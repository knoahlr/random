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
#include <ti/sysbios/knl/Queue.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>

/* Board Header file */
#include "Board.h"

#include <hardware/dc_motor_control.h>
#include <comms/connection_manager.h>
#include <comms/server.h>
/* <time.h> intentionally omitted: it is unused here and pulls newlib's
 * <sys/select.h>, whose select/fd_set/timeval collide with SimpleLink's BSD
 * socket layer (SL_INC_STD_BSD_API_NAMING). Time comes from SYS/BIOS Seconds. */
#include <ti/sysbios/hal/Seconds.h>

/* Local Platform Specific Header file */

#define CM_TASKSTACKSIZE     8192
#define DEFAULT_SERVER_TASKSTACKSIZE   8192
#define MOTOR_DUTY_TASKSTACKSIZE   2048

#define STARTTIME 1718248069 //12/06/24


static Task_Struct task_MotorControlStruct;

static Char server_stack[DEFAULT_SERVER_TASKSTACKSIZE];
static Char task_ConnectionManagerStack[CM_TASKSTACKSIZE];
static Char task_motorDutyStack[MOTOR_DUTY_TASKSTACKSIZE];


/*
 *  ======== main ========
 */
int main(void) {

    /* Task declarations */
    Task_Params conn_mgr_params;
    Task_Params server_task_params;
    Task_Params motor_control_params;
    Task_Handle conn_mgr_handle;
    Task_Handle server_handle;

    Semaphore_Handle semaphoreHandle = Semaphore_create(0, NULL, NULL);
    Mailbox_Handle mailboxHandle = Mailbox_create(sizeof(Gamepad), 10, NULL, NULL);
    Queue_Handle uart_queue_handle = Queue_create(NULL, NULL);

    if (!semaphoreHandle || !mailboxHandle || !uart_queue_handle) {
        System_abort("Failed to create RTOS synchronization objects");
    }

    Board_initGeneral();
    Board_initGPIO();
    Board_initWiFi();
    Board_initPWM();
    Board_initUART();

    Seconds_set(STARTTIME);

    /* Motor control task. */
    Task_Params_init(&motor_control_params);
    motor_control_params.stackSize = MOTOR_DUTY_TASKSTACKSIZE;
    motor_control_params.stack = task_motorDutyStack;
    motor_control_params.arg0 = (UArg)mailboxHandle;
    motor_control_params.priority = 2;
    Task_construct(&task_MotorControlStruct, (Task_FuncPtr)pwm_motor_proc_init, &motor_control_params, NULL);

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    /* WiFi connection manager starts when BIOS starts; server waits on its semaphore. */
    Task_Params_init(&conn_mgr_params);
    conn_mgr_params.arg0 = (UArg)semaphoreHandle;
    conn_mgr_params.arg1 = (UArg)uart_queue_handle;
    conn_mgr_params.stackSize = CM_TASKSTACKSIZE;
    conn_mgr_params.stack = task_ConnectionManagerStack;
    conn_mgr_params.priority = 2;
    conn_mgr_handle = Task_create((Task_FuncPtr)CM_connection_mgr, &conn_mgr_params, NULL);

    if (!conn_mgr_handle) {
        System_abort("Failed to launch connection manager task");
    }

    /* Server task blocks until the connection manager posts after WiFi/IP is up. */
    Task_Params_init(&server_task_params);
    server_task_params.arg0 = (UArg)mailboxHandle;
    server_task_params.arg1 = (UArg)semaphoreHandle;
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
