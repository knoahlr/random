/*
 * stepper_control.h
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
#include "Board.h"


#ifndef STEPPERCONTROL_H_
#define STEPPERCONTROL_H_

#define STEPPER_IN1 GPIO_PIN_0
#define STEPPER_IN2 GPIO_PIN_1
#define STEPPER_IN3 GPIO_PIN_6
#define STEPPER_IN4 GPIO_PIN_7

typedef enum stepper_phase
{
    PHASE1,
    PHASE2,
    PHASE3,
    PHASE4,
} stepper_phase;

void stepper_init(void);

void toggle_stepper(void);

void full_stepper_toggle(void);

#endif /* STEPPERCONTROL_H_ */
