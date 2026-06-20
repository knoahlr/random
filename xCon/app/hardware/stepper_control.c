/*
 * stepper_control.c
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include <hardware/stepper_control.h>


static stepper_phase stepper_1_state;

// Initialize Stepper
void stepper_init(void)
{
    GPIOPadConfigSet(GPIO_PORTH_BASE, STEPPER_IN1, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);
    GPIOPadConfigSet(GPIO_PORTH_BASE, STEPPER_IN2, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);
    GPIOPadConfigSet(GPIO_PORTK_BASE, STEPPER_IN3, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);
    GPIOPadConfigSet(GPIO_PORTK_BASE, STEPPER_IN4, GPIO_STRENGTH_12MA, GPIO_PIN_TYPE_STD_WPD);

    GPIOPinTypeGPIOOutput(GPIO_PORTH_BASE, STEPPER_IN1);
    GPIOPinTypeGPIOOutput(GPIO_PORTH_BASE, STEPPER_IN2);
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, STEPPER_IN3);
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, STEPPER_IN4);

    GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN1 | STEPPER_IN4, STEPPER_IN1 | STEPPER_IN4);

    stepper_1_state = PHASE1;
}

void toggle_stepper(void)
{
    int32_t l0;
    int32_t l1;
    int32_t l2;
    int32_t l3;

    GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_6, GPIO_PIN_6);

    l0 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_0);
    l1 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_1);
    l2 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_2);
    l3 = GPIOPinRead(GPIO_PORTL_BASE, GPIO_PIN_3);

    if (l0 > l1)
    {
        GPIOPinWrite(GPIO_PORTH_BASE, GPIO_PIN_0, 0);
        GPIOPinWrite(GPIO_PORTH_BASE, GPIO_PIN_1, GPIO_PIN_1);
        GPIO_toggle(Board_LED0);
    }
    else if (l1 > l2)
    {
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_1, 0);
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_2, GPIO_PIN_2);
        GPIO_toggle(Board_LED1);
    }
    else if (l2 > l3)
    {
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_2, 0);
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_3, GPIO_PIN_3);
        GPIO_toggle(Board_LED0);
    }
    else if (l3 > l1)
    {
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_3, 0);
        GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_1, GPIO_PIN_1);
        GPIO_toggle(Board_LED1);
    }
}

void full_stepper_toggle(void)
{
    switch (stepper_1_state)
    {
        case PHASE1:
            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN2, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN4, 0x0);

            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN1, STEPPER_IN1);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN4, STEPPER_IN4);
            stepper_1_state = PHASE2;
            GPIO_toggle(Board_LED0);
            break;
        case PHASE2:
            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN1, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN3, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN4, 0x0);

            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN1, STEPPER_IN1);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN3, STEPPER_IN3);
            stepper_1_state = PHASE3;
            GPIO_toggle(Board_LED1);
            break;
        case PHASE3:
            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN1, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN3, 0x0);

            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN2, STEPPER_IN2);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN3, STEPPER_IN3);
            stepper_1_state = PHASE4;
            GPIO_toggle(Board_LED0);
            break;
        case PHASE4:
            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN2, 0x0);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN3, 0x0);

            GPIOPinWrite(GPIO_PORTH_BASE, STEPPER_IN2, STEPPER_IN2);
            GPIOPinWrite(GPIO_PORTK_BASE, STEPPER_IN4, STEPPER_IN4);
            stepper_1_state = PHASE1;
            GPIO_toggle(Board_LED1);
            break;
    }
}
