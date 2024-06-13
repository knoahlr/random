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
#include "driverlib/interrupt.h"

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

Char server_stack[DEFAULT_SERVER_TASKSTACKSIZE];
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
    Task_Params conn_mgr_params;
    Task_Params server_task_params;
    Task_Params motor_control_params;
    Task_Params uart_params;
    Task_Handle conn_mgr_handle;
    Task_Handle server_handle;
    Task_Handle uart_handle;

    // Semaphore Declarations

    Semaphore_Handle semaphoreHandle = Semaphore_create(0, NULL, NULL);
    Mailbox_Handle mailboxHandle = Mailbox_create(sizeof(Gamepad), 10, NULL, NULL);
    Queue_Handle uart_queue_handle= Queue_create(NULL, NULL);

    /* Call board init functions */

    Board_initGeneral();
    Board_initGPIO();
    Board_initWiFi();
    Board_initPWM();
    Board_initUART();


    /* Construct  Task thread  --- TODO Rename to DC Motor Control Task*/
    Task_Params_init(&motor_control_params);
    motor_control_params.stackSize = MOTOR_DUTY_TASKSTACKSIZE;
    motor_control_params.stack = &task_motorDutyStack;
    motor_control_params.arg0 = mailboxHandle;
    motor_control_params.priority = 2;
    Task_construct(&task_PwmLedStruct, (Task_FuncPtr)pwm_motor_proc_init, &motor_control_params, NULL);

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    Task_Params_init(&conn_mgr_params);
    conn_mgr_params.arg0 = semaphoreHandle;
    conn_mgr_params.arg1 = uart_queue_handle;
    conn_mgr_params.stackSize = CM_TASKSTACKSIZE;
    conn_mgr_params.stack = &task_ConnectionManagerStack;
    conn_mgr_params.priority = 1;
    conn_mgr_handle = Task_create((Task_FuncPtr)CM_connection_mgr, &conn_mgr_params, NULL);

    if (!conn_mgr_handle) {
        printf("Failed to launch connection mgr task");
    }

    /* Construct Task to manage UART connection*/
    Task_Params_init(&uart_params);
    uart_params.stackSize = UART_TASKSTACKSIZE;
    uart_params.stack = &task_UARTStack;
    uart_params.arg0 = uart_queue_handle;
    uart_params.priority = 5;
    uart_handle = Task_create((Task_FuncPtr)uart_messaging_service, &uart_params, NULL);

    /* Construct serverTask Task thread */
    Task_Params_init(&server_task_params);
    server_task_params.arg0 = mailboxHandle;
    server_task_params.arg1 = semaphoreHandle;
    server_task_params.stackSize = DEFAULT_SERVER_TASKSTACKSIZE;
    server_task_params.stack = &server_stack;
    server_task_params.priority = 2;
    server_handle = Task_create((Task_FuncPtr)server_init, &server_task_params, NULL);

    if (!server_handle) {
        printf("Failed to start server task");
    }

    System_printf("Here NOW\n");

    /* SysMin will only print to the console when you call flush or exit */
    System_flush();


    /* Start BIOS */
    BIOS_start();
    IntMasterEnable();

//    while(Task_getMode(connectionManagerTaskHandle) == ti_sysbios_knl_Task_Mode_RUNNING || Task_getMode(connectionManagerTaskHandle) == ti_sysbios_knl_Task_Mode_READY){}

    return (0);
}
