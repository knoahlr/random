/*
 * uart_interface.c
 *
 *  Created on: Mar 2, 2023
 *      Author: Noah Workstation
 *
 * Console logging is decoupled: any task formats a line with uart_log() which
 * enqueues it (non-blocking) onto a SYS/BIOS Mailbox. uart_messaging_service
 * owns UART0, drains that queue with UARTwrite, and consumes command input when
 * the UART RX ISR posts the service semaphore. This serializes normal output
 * from concurrent producers and keeps producers off the wire. uartstdio is
 * built UART_BUFFERED, so UARTwrite itself is non-blocking (writes to the TX
 * ring buffer).
 *
 * UART0 itself is brought up at boot in EK_TM4C129EXL_initUART(); see
 * docs/uart-console-retarget.md.
 */

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#include <comms/uart_interface.h>
#include <console/console.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/drivers/GPIO.h>

#include <inc/hw_memmap.h>
#include <driverlib/uart.h>

#include "Board.h"

/* Depth of the console log queue (messages buffered before drops begin). */
#define UART_LOG_QUEUE_DEPTH 16

typedef struct {
    uint16_t len;
    char     data[UART_LOG_MSG_LEN];
} uart_log_msg;

static Mailbox_Struct  uart_log_mbox_struct;
static Mailbox_Handle  uart_log_mbox;
static Semaphore_Struct uart_service_sem_struct;
static Semaphore_Handle uart_service_sem;

void uart_console_rx_isr_hook(void)
{
    if (uart_service_sem != NULL) {
        Semaphore_post(uart_service_sem);
    }
}

void uart_log_init(void){
    Mailbox_Params params;
    Semaphore_Params sem_params;

    Mailbox_Params_init(&params);
    Mailbox_construct(&uart_log_mbox_struct, sizeof(uart_log_msg),
                      UART_LOG_QUEUE_DEPTH, &params, NULL);
    uart_log_mbox = Mailbox_handle(&uart_log_mbox_struct);

    Semaphore_Params_init(&sem_params);
    Semaphore_construct(&uart_service_sem_struct, 0, &sem_params);
    uart_service_sem = Semaphore_handle(&uart_service_sem_struct);
}

void uart_log(const char *fmt, ...){
    if (uart_log_mbox == NULL) {
        return; // uart_log_init() not called yet
    }

    uart_log_msg msg;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(msg.data, sizeof(msg.data), fmt, ap);
    va_end(ap);

    if (n <= 0) {
        return;
    }
    msg.len = (n >= (int)sizeof(msg.data)) ? (uint16_t)(sizeof(msg.data) - 1)
                                           : (uint16_t)n;

    /* Non-blocking: drop the line rather than stall the producer if the queue is full. */
    if (Mailbox_post(uart_log_mbox, &msg, BIOS_NO_WAIT)) {
        Semaphore_post(uart_service_sem);
    }
}

void uart_log_fatal(const char *s){
    /*
     * Blocking, polled write straight to the UART0 TX FIFO, bypassing the queue
     * and the uartstdio TX ring. For fatal paths that are about to System_abort
     * (which disables interrupts, so the ISR can no longer drain the ring) — this
     * guarantees the reason reaches the wire before the core halts.
     */
    while (*s) {
        if (*s == '\n') {
            UARTCharPut(UART0_BASE, '\r');
        }
        UARTCharPut(UART0_BASE, (unsigned char)*s++);
    }
}

void uart_messaging_service(UArg arg0){
    (void)arg0;
    uart_log_msg msg;

    /*
     * Proof-of-life banner: written directly (not via the queue) the moment the
     * consumer task starts, so it appears right after reset independent of any
     * producer or WiFi bring-up. If this shows on the console, the UART0 path
     * (clock, pins, Hwi, UART_BUFFERED, baud) is good and any later silence is a
     * producer-side problem.
     */
    const char banner[] = "\r\n[boot] UART console up @115200 8N1\r\n";
    UARTwrite(banner, sizeof(banner) - 1);
    GPIO_toggle(Board_LED0);
    console_init();

    for(;;) {
        bool wrote_log = false;
        while (Mailbox_pend(uart_log_mbox, &msg, BIOS_NO_WAIT)) {
            if (!wrote_log) {
                console_before_async_output();
            }
            UARTwrite(msg.data, msg.len);
            /* Board_LED0 is the UART activity indicator: toggles on every line
             * pushed to the wire. No other task drives this LED. */
            GPIO_toggle(Board_LED0);
            wrote_log = true;
        }
        if (wrote_log) {
            console_after_async_output();
        }

        console_poll();
        Semaphore_pend(uart_service_sem, BIOS_WAIT_FOREVER);
    }
}
