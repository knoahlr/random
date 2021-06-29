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
