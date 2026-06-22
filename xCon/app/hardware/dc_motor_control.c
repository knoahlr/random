/*
 * dc_motor_control.c
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 *
 * Independent 4-wheel drive over two L298N H-bridge modules. Each wheel has a
 * PWM enable (speed) and two direction GPIOs (polarity). Pin allocation and the
 * rationale for PG0 (vs PM0) are documented in docs/four-wheel-l298n-pinmap.
 *
 * Control scheme (car style): right trigger = forward velocity, left trigger =
 * brake (proportional, then active electronic brake when held hard), left stick
 * X = steering (differential mix into the left/right wheel pairs). A failsafe
 * actively brakes all wheels if no command frame arrives within the window.
 */

#include <hardware/dc_motor_control.h>
#include <uart_interface.h>
#include <input/gamepad_input.h>

#include <stdbool.h>
#include <stdint.h>

#include <xdc/std.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/drivers/PWM.h>

#include <driverlib/gpio.h>
#include <inc/hw_memmap.h>

#include "Board.h"

#define PWM_PERIOD_US          20000u  /* 20 ms PWM period; duty set in us. */
#define SPEED_MAX              255     /* command magnitude range is +/-255. */

/* Right-trigger counts below this read as no throttle (PS5 trigger rests near 0). */
#define TRIGGER_DEADZONE       10
/* Left-stick X magnitude (int16) below this reads as straight ahead. */
#define STEER_DEADZONE         4000
/* Maximum steering authority added/subtracted per side at full stick. */
#define STEER_MAX              255
/* Left trigger at/above this engages the active electronic brake. */
#define HARD_BRAKE_THRESHOLD   200

/* No command within this window -> failsafe brake (link-loss safety). */
#define MOTOR_FAILSAFE_TICKS   500
/* On a direction reversal, drop PWM and settle this long before flipping the
 * H-bridge, to bound current spikes / gearbox shock (system ticks). */
#define DIR_CHANGE_SETTLE_TICKS 20
/* Log a live sample at most this often (seconds). */
#define MOTOR_LOG_INTERVAL_S   3u

typedef enum {
    MOTOR_LEFT_FRONT = 0,
    MOTOR_LEFT_REAR,
    MOTOR_RIGHT_FRONT,
    MOTOR_RIGHT_REAR,
    MOTOR_COUNT
} MotorId;

typedef enum {
    STOP_COAST,   /* EN 0% -> bridge disabled, motor free-wheels */
    STOP_BRAKE    /* both IN low + EN 100% -> dynamic (short) brake */
} StopMode;

typedef struct {
    uint32_t   pwm_index;   /* Board_PWMx enable channel */
    uint32_t   in_a_port;   /* direction A GPIO port base */
    uint8_t    in_a_pin;    /* direction A pin mask */
    uint32_t   in_b_port;   /* direction B GPIO port base */
    uint8_t    in_b_pin;    /* direction B pin mask */
    bool       inverted;    /* flip forward/reverse if wired backwards */
    PWM_Handle pwm;         /* filled at init */
} motor_cfg_t;

/* Pin map mirrors docs/four-wheel-l298n-pinmap and the L298N truth tables.
 * Set `inverted` per wheel once forward rotation is verified on the bench. */
static motor_cfg_t motors[MOTOR_COUNT] = {
    [MOTOR_LEFT_FRONT]  = { Board_PWM7, GPIO_PORTM_BASE, GPIO_PIN_6,
                            GPIO_PORTQ_BASE, GPIO_PIN_1, false, NULL }, /* PK5; PM6/PQ1 */
    [MOTOR_LEFT_REAR]   = { Board_PWM4, GPIO_PORTP_BASE, GPIO_PIN_3,
                            GPIO_PORTM_BASE, GPIO_PIN_2, false, NULL }, /* PG0; PP3/PM2 */
    [MOTOR_RIGHT_FRONT] = { Board_PWM5, GPIO_PORTH_BASE, GPIO_PIN_0,
                            GPIO_PORTH_BASE, GPIO_PIN_1, false, NULL }, /* PG1; PH0/PH1 */
    [MOTOR_RIGHT_REAR]  = { Board_PWM6, GPIO_PORTK_BASE, GPIO_PIN_6,
                            GPIO_PORTK_BASE, GPIO_PIN_7, false, NULL }, /* PK4; PK6/PK7 */
};

/* Last commanded direction per wheel (-1 reverse, 0 stopped, +1 forward), used
 * to detect reversals for the direction-change settle. */
static int last_dir[MOTOR_COUNT];

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void pin_write(uint32_t port, uint8_t pin, bool high)
{
    GPIOPinWrite(port, pin, high ? pin : 0);
}

static void motor_pwm_duty(const motor_cfg_t *m, int speed_0_255)
{
    if (m->pwm != NULL) {
        uint32_t duty = (uint32_t)speed_0_255 * PWM_PERIOD_US / SPEED_MAX;
        PWM_setDuty(m->pwm, duty);
    }
}

/* Drive one wheel. `speed` in [-255,255]; 0 stops per `stop`. */
static void motor_set(MotorId id, int speed, StopMode stop)
{
    motor_cfg_t *m = &motors[id];

    speed = clampi(speed, -SPEED_MAX, SPEED_MAX);

    if (speed == 0) {
        if (stop == STOP_BRAKE) {
            pin_write(m->in_a_port, m->in_a_pin, false);
            pin_write(m->in_b_port, m->in_b_pin, false);
            motor_pwm_duty(m, SPEED_MAX);   /* EN 100% -> active brake */
        } else {
            motor_pwm_duty(m, 0);           /* EN 0% -> coast */
            pin_write(m->in_a_port, m->in_a_pin, false);
            pin_write(m->in_b_port, m->in_b_pin, false);
        }
        return;
    }

    bool forward = (speed > 0);
    int  duty    = forward ? speed : -speed;
    if (m->inverted) {
        forward = !forward;
    }

    pin_write(m->in_a_port, m->in_a_pin, forward);
    pin_write(m->in_b_port, m->in_b_pin, !forward);
    motor_pwm_duty(m, duty);
}

static void motors_brake_all(void)
{
    int i;
    for (i = 0; i < MOTOR_COUNT; i++) {
        motor_set((MotorId)i, 0, STOP_BRAKE);
        last_dir[i] = 0;
    }
}

/* Commit per-wheel speeds with a direction-change settle: any wheel reversing
 * direction has its PWM cut first, then a single short settle, then all wheels
 * are driven to their new targets. */
static void apply_wheel_speeds(const int s[MOTOR_COUNT])
{
    bool settle = false;
    int  i;

    for (i = 0; i < MOTOR_COUNT; i++) {
        int dir = (s[i] > 0) - (s[i] < 0);
        if (dir != 0 && last_dir[i] != 0 && dir != last_dir[i]) {
            motor_set((MotorId)i, 0, STOP_COAST);   /* drop PWM before flip */
            settle = true;
        }
    }

    if (settle) {
        Task_sleep(DIR_CHANGE_SETTLE_TICKS);
    }

    for (i = 0; i < MOTOR_COUNT; i++) {
        motor_set((MotorId)i, s[i], STOP_COAST);
        last_dir[i] = (s[i] > 0) - (s[i] < 0);
    }
}

/* Map a gamepad frame to four wheel speeds. Returns true if the left trigger is
 * held hard enough to demand a full active brake (s[] is then ignored). */
static bool compute_wheel_speeds(const Gamepad *g, int s[MOTOR_COUNT])
{
    int rt = clampi(g->right_trigger, 0, SPEED_MAX);
    int lt = clampi(g->left_trigger, 0, SPEED_MAX);
    int lx = g->left_analog_x;
    int throttle, steer, left, right;

    if (lt >= HARD_BRAKE_THRESHOLD) {
        return true;
    }
    if (rt < TRIGGER_DEADZONE) {
        rt = 0;
    }

    /* Left trigger bleeds throttle proportionally before full braking. */
    throttle = rt * (SPEED_MAX - lt) / SPEED_MAX;

    steer = 0;
    if (lx > STEER_DEADZONE || lx < -STEER_DEADZONE) {
        steer = lx * STEER_MAX / 32767;
    }

    /* Stick right (lx > 0) -> right pair slower: turns right. Allowing a side to
     * go negative gives skid-steer pivots at low throttle. */
    left  = clampi(throttle + steer, -SPEED_MAX, SPEED_MAX);
    right = clampi(throttle - steer, -SPEED_MAX, SPEED_MAX);

    s[MOTOR_LEFT_FRONT]  = left;
    s[MOTOR_LEFT_REAR]   = left;
    s[MOTOR_RIGHT_FRONT] = right;
    s[MOTOR_RIGHT_REAR]  = right;
    return false;
}

static void log_drive(const Gamepad *g, const int s[MOTOR_COUNT], bool braking)
{
    if (braking) {
        uart_log("[motor] BRAKE (LT:%d)\n", g->left_trigger);
    } else {
        uart_log("[motor] RT:%d LT:%d steerX:%d -> L(%d,%d) R(%d,%d)\n",
                 g->right_trigger, g->left_trigger, g->left_analog_x,
                 s[MOTOR_LEFT_FRONT], s[MOTOR_LEFT_REAR],
                 s[MOTOR_RIGHT_FRONT], s[MOTOR_RIGHT_REAR]);
    }
}

static void motors_gpio_init(void)
{
    int i;
    for (i = 0; i < MOTOR_COUNT; i++) {
        GPIOPinTypeGPIOOutput(motors[i].in_a_port, motors[i].in_a_pin);
        GPIOPinTypeGPIOOutput(motors[i].in_b_port, motors[i].in_b_pin);
        pin_write(motors[i].in_a_port, motors[i].in_a_pin, false);
        pin_write(motors[i].in_b_port, motors[i].in_b_pin, false);
        last_dir[i] = 0;
    }
}

static void motors_pwm_init(void)
{
    PWM_Params params;
    int        i;

    PWM_init();
    PWM_Params_init(&params);
    params.period   = PWM_PERIOD_US;
    params.dutyMode = PWM_DUTY_TIME;

    for (i = 0; i < MOTOR_COUNT; i++) {
        motors[i].pwm = PWM_open(motors[i].pwm_index, &params);
        if (motors[i].pwm == NULL) {
            uart_log("[motor] PWM index %u did not open\n",
                     (unsigned)motors[i].pwm_index);
        } else {
            PWM_setDuty(motors[i].pwm, 0);
        }
    }
}

static void motor_run_loop(Mailbox_Handle mail)
{
    uint32_t last_log_s = 0;  /* 0 => log the first frame after boot */

    for (;;) {
        Gamepad gamepad_state = { 0 };

        if (Mailbox_pend(mail, &gamepad_state, MOTOR_FAILSAFE_TICKS)) {
            int  s[MOTOR_COUNT] = { 0 };
            bool braking = compute_wheel_speeds(&gamepad_state, s);

            if (braking) {
                motors_brake_all();
            } else {
                apply_wheel_speeds(s);
            }

            uint32_t now_s = Seconds_get();
            if ((now_s - last_log_s) >= MOTOR_LOG_INTERVAL_S) {
                last_log_s = now_s;
                log_drive(&gamepad_state, s, braking);
            }
        } else {
            /* Failsafe: no command within the window -> brake all wheels. */
            motors_brake_all();
        }
    }
}

void pwm_motor_proc_init(UArg arg0, UArg arg1)
{
    Mailbox_Handle mail = (Mailbox_Handle)arg0;

    (void)arg1;

    /* Startup safety: direction pins low first, then PWM channels at 0% duty,
     * before any command is serviced. */
    motors_gpio_init();
    motors_pwm_init();

    motor_run_loop(mail);
}
