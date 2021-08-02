/*
 * hardwareController.c
 *
 *  Created on: Jul 20, 2021
 *      Author: Noah Workstation
 */

#include <hardware/dcMotorControl.h>


void updateMotorPWMDuty(UArg arg0, UArg arg1)
{
    PWM_Handle pwm1;
    PWM_Handle pwm2 = NULL;
    PWM_Params params;
    uint16_t   pwmPeriod = 20000;      // Period and duty in microseconds
    uint16_t   duty = 0;
    uint16_t   dutyInc = 100; //duty increment value
    uint32_t mailboxTimeout;
    Gamepad gamepadState = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    mailboxTimeout = 20 * Clock_tickPeriod;
    Mailbox_Handle mail = (Mailbox_Handle)arg0;

    PWM_init();
    PWM_Params_init(&params);
    params.period = pwmPeriod;
    params.dutyMode = PWM_DUTY_TIME;
    pwm1 = PWM_open(Board_PWM6, &params);
    if (pwm1 == NULL) {
        System_abort("Board_PWM6 did not open");
    }
    pwm2 = PWM_open(Board_PWM5, &params);
    if (pwm2 == NULL) {
        System_abort("Board_PWM5 did not open");
    }

    PWM_setDuty(pwm1, duty);
//    PWM_setDuty(pwm2, duty);

    while(1)
    {
        bool rc = Mailbox_pend(mail, &gamepadState, mailboxTimeout);
        if(rc)
        {
            //process gamepad state and update motor pwm
            //where 20000 = 100 % duty cycle.
            //Need to implement logarithmic relationship between pwm duty cyle and right trigger. Intial logarithmic equation designed using bounds of 100-255
            // and 10000 - 20000 respectively: scaledPWM = 10682 log(51/2000 * rightTrigger).

//            uint16_t scaledPwm = gamepadState.RightTrigger < 100 ? 10000: (float)(gamepadState.RightTrigger)/255 * 20000;

            uint16_t scaledPwm = gamepadState.RightTrigger < 100 ? 10000: 10682*log(0.0255*gamepadState.RightTrigger);

            PWM_setDuty(pwm1, scaledPwm);
            System_printf("Success: Gamepad state transmitted.\n");
//            Task_sleep(500); //0.5 s ?
        }
    }

}
