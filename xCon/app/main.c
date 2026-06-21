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

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Mailbox.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>

/* Board Header file */
#include "Board.h"

#include <hardware/dc_motor_control.h>
#include <comms/connection_manager.h>
#include <comms/server.h>
#include <comms/uart_interface.h>
/* <time.h> intentionally omitted: it is unused here and pulls newlib's
 * <sys/select.h>, whose select/fd_set/timeval collide with SimpleLink's BSD
 * socket layer (SL_INC_STD_BSD_API_NAMING). Time comes from SYS/BIOS Seconds. */
#include <ti/sysbios/hal/Seconds.h>

/* Local Platform Specific Header file */

#define CM_TASKSTACKSIZE     8192
#define DEFAULT_SERVER_TASKSTACKSIZE   8192
#define MOTOR_DUTY_TASKSTACKSIZE   2048
#define UART_LOG_TASKSTACKSIZE   1024

#define STARTTIME 1718248069 //12/06/24


static Task_Struct task_motor_control_struct;
static Task_Struct task_uart_log_struct;
static Task_Struct task_connection_manager_struct;

static Char server_stack[DEFAULT_SERVER_TASKSTACKSIZE];
static Char task_connection_manager_stack[CM_TASKSTACKSIZE];
static Char task_motor_duty_stack[MOTOR_DUTY_TASKSTACKSIZE];
static Char task_uart_log_stack[UART_LOG_TASKSTACKSIZE];


/*
 *  ======== main ========
 */
int main(void) {

    /* Task declarations */
    Task_Params conn_mgr_params;
    Task_Params server_task_params;
    Task_Params motor_control_params;
    Task_Params uart_log_params;
    Task_Handle server_handle;

    Semaphore_Handle semaphore_handle = Semaphore_create(0, NULL, NULL);
    Mailbox_Handle mailbox_handle = Mailbox_create(sizeof(Gamepad), 10, NULL, NULL);

    if (!semaphore_handle || !mailbox_handle) {
        System_abort("Failed to create RTOS synchronization objects");
    }

    /* Console log queue must exist before any task posts to it. */
    uart_log_init();

    Board_initGeneral();
    Board_initGPIO();
    Board_initWiFi();
    Board_initPWM();
    Board_initUART();

    Seconds_set(STARTTIME);

    /* Console log consumer: owns UART0, drains the log queue to the wire. */
    Task_Params_init(&uart_log_params);
    uart_log_params.stackSize = UART_LOG_TASKSTACKSIZE;
    uart_log_params.stack = task_uart_log_stack;
    uart_log_params.priority = 3;
    Task_construct(&task_uart_log_struct, (Task_FuncPtr)uart_messaging_service, &uart_log_params, NULL);

    /* Motor control task. */
    Task_Params_init(&motor_control_params);
    motor_control_params.stackSize = MOTOR_DUTY_TASKSTACKSIZE;
    motor_control_params.stack = task_motor_duty_stack;
    motor_control_params.arg0 = (UArg)mailbox_handle;
    motor_control_params.priority = 2;
    Task_construct(&task_motor_control_struct, (Task_FuncPtr)pwm_motor_proc_init, &motor_control_params, NULL);

    /* Turn on user LED */
    GPIO_write(Board_LED1, Board_LED_ON);

    /* WiFi connection manager starts when BIOS starts; server waits on its semaphore. */
    Task_Params_init(&conn_mgr_params);
    conn_mgr_params.arg0 = (UArg)semaphore_handle;
    conn_mgr_params.stackSize = CM_TASKSTACKSIZE;
    conn_mgr_params.stack = task_connection_manager_stack;
    conn_mgr_params.priority = 3;
    Task_construct(&task_connection_manager_struct,
                   (Task_FuncPtr)cm_connection_mgr, &conn_mgr_params, NULL);

    /* Server task blocks until the connection manager posts after WiFi/IP is up. */
    Task_Params_init(&server_task_params);
    server_task_params.arg0 = (UArg)mailbox_handle;
    server_task_params.arg1 = (UArg)semaphore_handle;
    server_task_params.stackSize = DEFAULT_SERVER_TASKSTACKSIZE;
    server_task_params.stack = server_stack;
    server_task_params.priority = 2;
    server_handle = Task_create((Task_FuncPtr)server_task, &server_task_params, NULL);

    if (!server_handle) {
        System_abort("Failed to start server task");
    }

    BIOS_start();
    return (0);
}
