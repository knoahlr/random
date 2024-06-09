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

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Queue.h>


/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/PWM.h>
#include <ti/drivers/WiFi.h>

/* SimpleLink Wi-Fi Host Driver and board Header files */
#include <simplelink.h>
#include "Board.h"

#include <hardware/dcMotorControl.h>
#include <communication/connectionManager.h>
#include <uart/uart_interface.h>
#include <defaultServer/randomDefaultServer.h>
#include <hardware/stepperControl.h>

/* Local Platform Specific Header file */

#define GPIO_TASKSTACKSIZE     1024
#define UART_TASKSTACKSIZE 8192
#define CM_TASKSTACKSIZE     8192
#define DEFAULT_SERVER_TASKSTACKSIZE   8192
#define MOTOR_DUTY_TASKSTACKSIZE   2048

#define TCPPORT         1000
#define TCPPACKETSIZE   100


Task_Struct task_GPIOStruct;
Task_Struct task_DefaultServerStruct;
Task_Struct task_ConnectionManagerStruct;
Task_Struct task_PwmLedStruct;
Task_Struct task_uartStruct;

Char task_DefaultServerStack[DEFAULT_SERVER_TASKSTACKSIZE];
Char task_GPIOStack[GPIO_TASKSTACKSIZE];
Char task_ConnectionManagerStack[CM_TASKSTACKSIZE];
Char task_motorDutyStack[MOTOR_DUTY_TASKSTACKSIZE];
Char task_UARTStack[UART_TASKSTACKSIZE];


/*
 *  ======== main ========
 */
int main(void)
{

    // TASK Declarations
    Task_Params connectionManagerParams;
    Task_Params defaultServerTaskParams;
    Task_Params pwmLEDTaskParams;
    Task_Params uartTaskParams;
    Task_Handle connectionManagerTaskHandle;
    Task_Handle defaultServerTaskHandle;
    Task_Handle uartTaskHandle;

    // Semaphore Declarations

    Semaphore_Handle semaphoreHandle = Semaphore_create(0, NULL, NULL);
    Mailbox_Handle mailboxHandle = Mailbox_create(sizeof(Gamepad), 1, NULL, NULL);
    Queue_Handle uart_queue_handle= Queue_create(NULL, NULL);

    /* Call board init functions */

    Board_initGeneral();
    Board_initGPIO();
    Board_initWiFi();
    Board_initPWM();
    Board_initUART();

    /* Construct LED Task thread  --- TODO Rename to DC Motor Control Task*/
    Task_Params_init(&pwmLEDTaskParams);
    pwmLEDTaskParams.stackSize = MOTOR_DUTY_TASKSTACKSIZE;
    pwmLEDTaskParams.stack = &task_motorDutyStack;
    pwmLEDTaskParams.arg0 = mailboxHandle;
    pwmLEDTaskParams.priority = 2;
    Task_construct(&task_PwmLedStruct, (Task_FuncPtr)updateMotorPWMDuty, &pwmLEDTaskParams, NULL);

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    Task_Params_init(&connectionManagerParams);
    connectionManagerParams.arg0 = semaphoreHandle;
    connectionManagerParams.arg1 = uart_queue_handle;
    connectionManagerParams.stackSize = CM_TASKSTACKSIZE;
    connectionManagerParams.stack = &task_ConnectionManagerStack;
    connectionManagerParams.priority = 1;
    connectionManagerTaskHandle = Task_create((Task_FuncPtr)CM_connection_mgr, &connectionManagerParams, NULL);

    if (connectionManagerTaskHandle == NULL){ System_printf("Failed to connection mgr task");}

    /* Construct Task to manage UART connection*/
    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = UART_TASKSTACKSIZE;
    uartTaskParams.stack = &task_UARTStack;
    uartTaskParams.arg0 = uart_queue_handle;
    uartTaskParams.priority = 5;
    uartTaskHandle = Task_create((Task_FuncPtr)uart_messaging_service, &uartTaskParams, NULL);
    //Task_construct(&task_uartStruct, (Task_FuncPtr)uart_messaging_service, &uartTaskParams, NULL);

    /* Construct serverTask Task thread */
    Task_Params_init(&defaultServerTaskParams);
    defaultServerTaskParams.arg0 = mailboxHandle;
    defaultServerTaskParams.arg1 = semaphoreHandle;
    defaultServerTaskParams.stackSize = DEFAULT_SERVER_TASKSTACKSIZE;
    defaultServerTaskParams.stack = &task_DefaultServerStack;
    defaultServerTaskParams.priority = 2;
    defaultServerTaskHandle = Task_create((Task_FuncPtr)defaultServerTask, &defaultServerTaskParams, NULL);

    if (defaultServerTaskHandle == NULL){System_printf("Failed to start server task");}

    System_printf("Here NOW\n");

    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

//    while(Task_getMode(connectionManagerTaskHandle) == ti_sysbios_knl_Task_Mode_RUNNING || Task_getMode(connectionManagerTaskHandle) == ti_sysbios_knl_Task_Mode_READY){}

    return (0);
}
