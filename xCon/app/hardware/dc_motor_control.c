/*
 * dc_motor_control.c
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 *
 * Consumes parsed Gamepad frames from a mailbox and drives three motor PWM
 * channels from the right-trigger value. If no frame arrives within the
 * failsafe window the motors are stopped, so a dropped link cannot leave them
 * running at the last commanded duty.
 */

#include <hardware/dc_motor_control.h>
#include <uart_interface.h>
#include <input/gamepad_input.h>

#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include <xdc/std.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/drivers/PWM.h>

#include "Board.h"

#define PWM_PERIOD_US           20000u  /* 20 ms period; duty in us (== 100%). */
#define MOTOR_MIN_DUTY_US       5000u   /* floor for a light trigger press. */
#define MOTOR_TRIGGER_DEADZONE  50      /* trigger counts below this => min duty. */
/* No command within this window stops the motors (failsafe on link loss). */
#define MOTOR_FAILSAFE_TICKS    500
/* Log an incoming gamepad frame at most once per this many seconds, so the
 * console shows a live sample of activity without flooding the log queue. */
#define MOTOR_LOG_INTERVAL_S    3u

static PWM_Handle pwm1;
static PWM_Handle pwm2;
static PWM_Handle pwm3;

static void log_gamepad_input(const Gamepad *g)
{
    uart_log("[motor] gamepad LT:%d RT:%d  L(%d,%d) R(%d,%d)  btnL:%d btnR:%d\n",
             g->left_trigger, g->right_trigger,
             g->left_analog_x, g->left_analog_y,
             g->right_analog_x, g->right_analog_y,
             g->left_button, g->right_button);
}

/* Map a right-trigger value (0..255) to a PWM duty in microseconds. */
static uint16_t trigger_to_duty(int right_trigger)
{
    double duty;

    if (right_trigger < MOTOR_TRIGGER_DEADZONE) {
        return MOTOR_MIN_DUTY_US;
    }

    /* Eventual goal is a logarithmic feel: duty = 10682 * ln(51/2000 * RT). */
    duty = 10682.0 * log(0.0255 * (double)right_trigger);

    if (duty < 0.0) {
        duty = 0.0;
    } else if (duty > PWM_PERIOD_US) {
        duty = PWM_PERIOD_US;
    }
    return (uint16_t)duty;
}

static void motors_set_duty(uint16_t duty)
{
    if (pwm1 != NULL) PWM_setDuty(pwm1, duty);
    if (pwm2 != NULL) PWM_setDuty(pwm2, duty);
    if (pwm3 != NULL) PWM_setDuty(pwm3, duty);
}

static void motor_run_loop(Mailbox_Handle mail)
{
    uint32_t last_log_s = 0;  /* 0 => log the first frame after boot */

    for (;;) {
        Gamepad gamepad_state = { 0 };

        if (Mailbox_pend(mail, &gamepad_state, MOTOR_FAILSAFE_TICKS)) {
            uint32_t now_s = Seconds_get();
            if ((now_s - last_log_s) >= MOTOR_LOG_INTERVAL_S) {
                last_log_s = now_s;
                log_gamepad_input(&gamepad_state);
            }
            motors_set_duty(trigger_to_duty(gamepad_state.right_trigger));
        } else {
            /* Failsafe: no command within the window -> stop the motors. */
            motors_set_duty(0);
        }
    }
}

void pwm_motor_proc_init(UArg arg0, UArg arg1)
{
    Mailbox_Handle mail = (Mailbox_Handle)arg0;
    PWM_Params     params;

    (void)arg1;

    PWM_init();
    PWM_Params_init(&params);
    params.period   = PWM_PERIOD_US;
    params.dutyMode = PWM_DUTY_TIME;

    pwm1 = PWM_open(Board_PWM6, &params);
    if (pwm1 == NULL) uart_log("[motor] Board_PWM6 did not open\n");
    pwm2 = PWM_open(Board_PWM5, &params);
    if (pwm2 == NULL) uart_log("[motor] Board_PWM5 did not open\n");
    pwm3 = PWM_open(Board_PWM7, &params);
    if (pwm3 == NULL) uart_log("[motor] Board_PWM7 did not open\n");

    motors_set_duty(0);

    motor_run_loop(mail);
}
