/*
 * hardwareController.c
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 */

#include <hardware/dcMotorControl.h>
#include <uart/uart_interface.h>

static PWM_Handle pwm1;
static PWM_Handle pwm2;
static PWM_Handle pwm3;

Mailbox_Handle mail;

void log_gamepad_input(Gamepad *gamepadState){
    printf("\nGamepad State:\n"
            "\n\tLeft Trigger:%d"
            "\n\tRight Trigger:%d"
            "\n\tLeft Analog  (x, y):%d, %d"
            "\n\tRight Analog (x, y):%d,%d"
            "\n\tLeft Button (x, y):%d,"
            "\n\tRight Button(x, y):%d,",
            gamepadState->LeftTrigger,
            gamepadState->RightTrigger,
            gamepadState->LeftAnalog_X,
            gamepadState->LeftAnalog_Y,
            gamepadState->RightAnalog_X,
            gamepadState->RightAnalog_Y,
            gamepadState->LeftButton,
            gamepadState->RightButton);
    UARTprintf("\nGamepad State:\n"
            "\n\tLeft Trigger:%d"
            "\n\tRight Trigger:%d"
            "\n\tLeft Analog  (x, y):%d, %d"
            "\n\tRight Analog (x, y):%d,%d"
            "\n\tLeft Button (x, y):%d,"
            "\n\tRight Button(x, y):%d,",
            gamepadState->LeftTrigger,
            gamepadState->RightTrigger,
            gamepadState->LeftAnalog_X,
            gamepadState->LeftAnalog_Y,
            gamepadState->RightAnalog_X,
            gamepadState->RightAnalog_Y,
            gamepadState->LeftButton,
            gamepadState->RightButton);
}


void pwm_motor_proc_init(UArg arg0, UArg arg1)
{
    PWM_Params params;
    uint16_t   pwm_period = 20000;      // Period and duty in microseconds
    uint16_t   duty = 0;
    mail = (Mailbox_Handle)arg0;
    //uint16_t   dutyInc = 100; //duty increment value


    PWM_init();
    PWM_Params_init(&params);
    params.period = pwm_period;
    params.dutyMode = PWM_DUTY_TIME;
    pwm1 = PWM_open(Board_PWM6, &params);
    if (pwm1 == NULL)
        printf("Board_PWM6 did not open");

    pwm2 = PWM_open(Board_PWM5, &params);
    if (pwm2 == NULL)
        printf("Board_PWM5 did not open");

    pwm3 = PWM_open(Board_PWM7, &params);
    if (pwm2 == NULL)
        printf("Board_PWM7 did not open");

    PWM_setDuty(pwm1, duty);
    PWM_setDuty(pwm2, duty);
    PWM_setDuty(pwm3, duty);

    update_motor_state(mail);

}

void update_motor_state() {
    uint32_t mailbox_timeout;
    mailbox_timeout = 20 * Clock_tickPeriod;

    for(;;) {
        Gamepad gamepad_state = { 0 };
        bool rc = Mailbox_pend(mail, &gamepad_state, mailbox_timeout);
        if(!rc) {
//            printf("Unsuccessful receipt: %p\n", &mail);
        } else {
            log_gamepad_input(&gamepad_state);
            //process gamepad state and update motor pwm
            //where 20000 = 100 % duty cycle.
            //Need to implement logarithmic relationship between pwm duty cyle and right trigger. Intial logarithmic equation designed using bounds of 100-255
            // and 10000 - 20000 respectively: scaledPWM = 10682 log(51/2000 * rightTrigger).

//            uint16_t scaledPwm = gamepadState.RightTrigger < 100 ? 10000: (float)(gamepadState.RightTrigger)/255 * 20000;

            uint16_t scaled_pwm = gamepad_state.RightTrigger < 50 ? 5000: 10682*log(0.0255*gamepad_state.RightTrigger);

            PWM_setDuty(pwm1, scaled_pwm);
            PWM_setDuty(pwm2, scaled_pwm);
            PWM_setDuty(pwm3, scaled_pwm);

        }
    }
}

