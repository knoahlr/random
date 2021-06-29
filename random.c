/*
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY D\
 *
 * GES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== empty_min.c ========
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

/* SimpleLink Wi-Fi Host Driver Header files */
#include <simplelink.h>
#include <driverlib/gpio.h>
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include "driverlib/pin_map.h"
#include <driverlib/sysctl.h>
#include <randomDefaultServer.h>
#include "driverlib/pwm.h"
#include "Board.h"


/* Local Platform Specific Header file */
#include "StepperControl.h"
#include "connectionManager.h"

#define GPIO_TASKSTACKSIZE     1024
#define CM_TASKSTACKSIZE     1024
#define DEFAULT_SERVER_TASKSTACKSIZE   2048

#define TCPPORT         1000
#define TCPPACKETSIZE   256


Task_Struct task_GPIOStruct;
Task_Struct task_DefaultServerStruct;
Task_Struct task_ConnectionManagerStruct;

Char task_DefaultServerStack[DEFAULT_SERVER_TASKSTACKSIZE];
Char task_GPIOStack[GPIO_TASKSTACKSIZE];
Char task_ConnectionManagerStack[CM_TASKSTACKSIZE];

//
//void runStepper()
//{
//    while(true)
//    {
////        toggleStepper();
//        fullStepperToggle();
//        Task_sleep(1);
//    }
//
//}
//void initPWMPins()
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
//
//
//void initPWMRTOS()
//{
//    PWM_Handle handle;
//    PWM_Params params;
////    PWM_init();
//    PWM_Params_init(&params);
//
//    // Output low when PWM is not running
//    params.period = 1000000;            // 1MHz
// // Duty is fraction of period
//    params.dutyMode = PWM_DUTY_TIME;
//
//    handle = PWM_open(Board_PWM6, &params);
//
//    if (handle == NULL) {
//        System_printf("PWM did not open");
//        Task_exit();
//    }
//    PWM_setDuty(handle, 500000);
//
//
////    PWM_start(handle);
//}

/*
 *  ======== main ========
 */
int main(void)
{

    Task_Params gpioTaskParams;
    Task_Params connectionManagerParams;
    Task_Params defaultServerTaskParams;

    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initWiFi();
    Board_initPWM();

    //    stepperInit();
    //    initPWMRTOS();

//    /* Construct Stepper Control Task thread */
//    Task_Params_init(&gpioTaskParams);
//    gpioTaskParams.stackSize = GPIO_TASKSTACKSIZE;
//    gpioTaskParams.stack = &taskGPIOStack;
//    Task_construct(&taskGPIOStruct, (Task_FuncPtr)runStepper, &gpioTaskParams, NULL);
//
    /* Construct serverTask Task thread */
//    Task_Params_init(&defaultServerTaskParams);
//    defaultServerTaskParams.arg0 = 1000;
//    defaultServerTaskParams.stackSize = DEFAULT_SERVER_TASKSTACKSIZE;
//    defaultServerTaskParams.stack = &task_DefaultServerStack;
//    defaultServerTaskParams.priority = 1;
//    Task_construct(&task_DefaultServerStruct, (Task_FuncPtr)defaultServerTask, &defaultServerTaskParams, NULL);

    Task_Params_init(&connectionManagerParams);
    connectionManagerParams.stackSize = CM_TASKSTACKSIZE;
    connectionManagerParams.stack = &task_ConnectionManagerStack;
    Task_construct(&task_ConnectionManagerStruct, (Task_FuncPtr)CM_AddConnectionProfile, &connectionManagerParams, NULL);


/* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
