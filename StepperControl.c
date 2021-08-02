/*
 * StepperControl.c
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include "StepperControl.h"


stepperPhase stepper_1_State;

//Initialize Stepper
void stepperInit()
{

    GPIOPadConfigSet(GPIO_PORTH_BASE, stepperIN1, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);
    GPIOPadConfigSet(GPIO_PORTH_BASE, stepperIN2, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);
    GPIOPadConfigSet(GPIO_PORTK_BASE, stepperIN3, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);
    GPIOPadConfigSet(GPIO_PORTK_BASE, stepperIN4, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);

    GPIOPinTypeGPIOOutput(GPIO_PORTH_BASE, stepperIN1);
    GPIOPinTypeGPIOOutput(GPIO_PORTH_BASE, stepperIN2);
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, stepperIN3);
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, stepperIN4);

    GPIOPinWrite(GPIO_PORTH_BASE, stepperIN1|stepperIN4, stepperIN1|stepperIN4);

    stepper_1_State = PHASE1;
//    GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_2, 0x0);

}

void toggleStepper()
{
    int32_t L0;
    int32_t L1;
    int32_t L2;
    int32_t L3;

    GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_6, GPIO_PIN_6);

    L0 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_0);
    L1 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_1);
    L2 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_2);
    L3 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_3);

    if(L0 >L1)
    {
        GPIOPinWrite(GPIO_PORTH_BASE, GPIO_PIN_0, 0);
        GPIOPinWrite(GPIO_PORTH_BASE, GPIO_PIN_1, GPIO_PIN_1);
        GPIO_toggle(Board_LED0);
    }
    else if (L1 > L2)
    {
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_1, 0);
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_2, GPIO_PIN_2);
        GPIO_toggle(Board_LED1);
    }
    else if(L2 > L3)
    {
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_2, 0);
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_3, GPIO_PIN_3);
        GPIO_toggle(Board_LED0);

    }
    else if(L3 > L1)
    {
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_3, 0);
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_1, GPIO_PIN_1);
        GPIO_toggle(Board_LED1);
    }
}

void fullStepperToggle()
{
    switch(stepper_1_State)
    {
        case PHASE1:
//            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN1, 0x0);
            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN2, 0x0);
//            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN3, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN4, 0x0);

            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN1, stepperIN1);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN4, stepperIN4);
            stepper_1_State = PHASE2;
            GPIO_toggle(Board_LED0);
            break;
        case PHASE2:

            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN1, 0x0);
//            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN2, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN3, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN4, 0x0);

//            GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0);
            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN1, stepperIN1);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN3, stepperIN3);
            stepper_1_State = PHASE3;
            GPIO_toggle(Board_LED1);
            break;

        case PHASE3:

            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN1, 0x0);
//            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN2, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN3, 0x0);
//            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN4, 0x0);

//            GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, 0);
            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN2, stepperIN2);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN3, stepperIN3);
            stepper_1_State = PHASE4;
            GPIO_toggle(Board_LED0);
            break;

        case PHASE4:

//            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN1, 0x0);
            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN2, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN3, 0x0);
//            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN4, 0x0);


            GPIOPinWrite(GPIO_PORTH_BASE, stepperIN2, stepperIN2);
            GPIOPinWrite(GPIO_PORTK_BASE, stepperIN4, stepperIN4);
            stepper_1_State = PHASE1;
            GPIO_toggle(Board_LED1);
            break;
    }
}


//
//void runStepper()
//
//{
//    while(true)
//    {
////        toggleStepper();
//        fullStepperToggle();
//        Task_sleep(1);
//    }
//
//}
//
//void initializePWMPins()
//{
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
//
//    SysCtlPWMClockSet(SYSCTL_PWMDIV_1);
//
//    //Configure GPIO Pins - Port Base G and K, pins 5, 6 and 7
//    GPIOPinConfigure(GPIO_PG1_M0PWM5);
//    GPIOPinConfigure(GPIO_PK4_M0PWM6);
//    GPIOPinConfigure(GPIO_PK5_M0PWM7);
//
//    GPIOPinTypePWM(GPIO_PORTG_BASE, GPIO_PIN_1);
//    GPIOPinTypePWM(GPIO_PORTK_BASE, GPIO_PIN_4);
//    GPIOPinTypePWM(GPIO_PORTK_BASE, GPIO_PIN_5);
//
//    PWMGenConfigure(PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN|PWM_GEN_MODE_NO_SYNC);
//    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, 400);
//    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_5, 200);
//    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_6, 200);
//    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_7, 200);
//
//    PWMGenEnable(PWM0_BASE, PWM_GEN_0);
//    PWMOutputState(PWM0_BASE, (PWM_OUT_5_BIT | PWM_OUT_6_BIT | PWM_OUT_7_BIT), true);
//
//
//}


//void initializePWMDriver()
//{
//    PWM_Handle handle;
//    PWM_Params params;
////    PWM_init();
//    PWM_Params_init(&params);
//
//    // Output low when PWM is not running
//    params.period = 20000;            // - Period in microseconds
// // Duty is fraction of period
//    params.dutyMode = PWM_DUTY_TIME;
//
//    handle = PWM_open(Board_PWM0, &params);
//
//    if (handle == NULL) {
//        System_printf("PWM did not open");
//        Task_exit();
//    }
//    PWM_setDuty(handle, 5000);
//
//
////    PWM_start(handle);
//}
