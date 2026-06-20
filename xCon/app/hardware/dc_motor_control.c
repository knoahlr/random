/*
 * dc_motor_control.c
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 */

#include <hardware/dc_motor_control.h>
#include <comms/uart_interface.h>

static PWM_Handle pwm1;
static PWM_Handle pwm2;
static PWM_Handle pwm3;

static Mailbox_Handle mail;

void log_gamepad_input(Gamepad *gamepad_state){
    UARTprintf("\nGamepad State:\n"
            "\n\tLeft Trigger:%d"
            "\n\tRight Trigger:%d"
            "\n\tLeft Analog  (x, y):%d, %d"
            "\n\tRight Analog (x, y):%d,%d"
            "\n\tLeft Button (x, y):%d,"
            "\n\tRight Button(x, y):%d,",
            gamepad_state->left_trigger,
            gamepad_state->right_trigger,
            gamepad_state->left_analog_x,
            gamepad_state->left_analog_y,
            gamepad_state->right_analog_x,
            gamepad_state->right_analog_y,
            gamepad_state->left_button,
            gamepad_state->right_button);
}


void pwm_motor_proc_init(UArg arg0, UArg arg1)
{
    PWM_Params params;
    uint16_t   pwm_period = 20000;      // Period and duty in microseconds
    uint16_t   duty = 0;
    mail = (Mailbox_Handle)arg0;

    PWM_init();
    PWM_Params_init(&params);
    params.period = pwm_period;
    params.dutyMode = PWM_DUTY_TIME;
    pwm1 = PWM_open(Board_PWM6, &params);
    if (pwm1 == NULL)
        UARTprintf("Board_PWM6 did not open");

    pwm2 = PWM_open(Board_PWM5, &params);
    if (pwm2 == NULL)
        UARTprintf("Board_PWM5 did not open");

    pwm3 = PWM_open(Board_PWM7, &params);
    if (pwm3 == NULL)
        UARTprintf("Board_PWM7 did not open");

    PWM_setDuty(pwm1, duty);
    PWM_setDuty(pwm2, duty);
    PWM_setDuty(pwm3, duty);

    update_motor_state();
}

void update_motor_state(void) {
    uint32_t mailbox_timeout = 20 * Clock_tickPeriod;

    for(;;) {
        Gamepad gamepad_state = { 0 };
        bool rc = Mailbox_pend(mail, &gamepad_state, mailbox_timeout);
        if(rc) {
            log_gamepad_input(&gamepad_state);
            // Process gamepad state and update motor PWM, where 20000 = 100% duty.
            // A logarithmic map between duty and right trigger is the eventual goal:
            // scaled_pwm = 10682 * log(51/2000 * right_trigger).
            uint16_t scaled_pwm = gamepad_state.right_trigger < 50
                                ? 5000
                                : 10682 * log(0.0255 * gamepad_state.right_trigger);

            PWM_setDuty(pwm1, scaled_pwm);
            PWM_setDuty(pwm2, scaled_pwm);
            PWM_setDuty(pwm3, scaled_pwm);
        }
    }
}
