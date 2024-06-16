/*
 * StepperControl.h
 *
 *  Created on: Mar 18, 2020
 *      Author: Noah Workstation
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files */
// #include <ti/drivers/EMAC.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/PWM.h>
#include <ti/drivers/WiFi.h>

#include <driverlib/gpio.h>
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include "board.h"


#ifndef STEPPERCONTROL_H_
#define STEPPERCONTROL_H_

#define stepperIN1 GPIO_PIN_0
#define stepperIN2 GPIO_PIN_1
#define stepperIN3 GPIO_PIN_6
#define stepperIN4 GPIO_PIN_7

typedef enum stepperPhase
{
    PHASE1,
    PHASE2,
    PHASE3,
    PHASE4,
}stepperPhase;

void stepperInit();

void toggleStepper();

void fullStepperToggle();

#endif /* STEPPERCONTROL_H_ */
