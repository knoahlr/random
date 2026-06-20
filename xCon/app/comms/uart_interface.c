/*
 * uart_interface.c
 *
 *  Created on: Mar 2, 2023
 *      Author: Noah Workstation
 */


#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <comms/uart_interface.h>

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include <driverlib/gpio.h>
#include <driverlib/interrupt.h>
#include <driverlib/sysctl.h>
#include <driverlib/uart.h>

#include "Board.h"

#define UART_BUFFER_LEN 256

/*
 * System clock the catalog Boot module establishes at reset (120 MHz). Matches
 * EK_TM4C129EXL_SYSTEM_CLOCK in the board file; UARTStdioConfig needs it to set
 * the baud divisor.
 */
#define UART_SYSTEM_CLOCK 120000000U

static bool    uart_configured = false;
static uint8_t uart_rx_buffer[UART_BUFFER_LEN];

/*
 * UART0 is already brought up at boot by EK_TM4C129EXL_initUART(). This remains
 * as an idempotent guard for callers that may run before board init; the system
 * clock is owned by the catalog Boot module, so we only (re)configure stdio.
 */
void configure_uart_interface(uint8_t uart_port){
    if (uart_configured) {
        return;
    }
    UARTStdioConfig((uint32_t)uart_port, 115200, UART_SYSTEM_CLOCK);
    uart_configured = true;
}

// Continuously drains the uart_queue, transmitting any queued log records.
void uart_messaging_service(UArg arg0){

    struct log_uart *uart_rec;
    const uint8_t shell_prompt_str[] = "uart0:~$ ";
    Queue_Handle uart_queue_handle = (Queue_Handle)arg0;

    configure_uart_interface(0);

    for(;;) {
        uint32_t bytes_recv = UARTgets((char *)uart_rx_buffer, UART_BUFFER_LEN);
        if(bytes_recv < 3){
            UARTwrite(shell_prompt_str, strlen((const char *)shell_prompt_str));
        }
        if(!Queue_empty(uart_queue_handle)){
            // loop through queue and send chars to UART.
            uart_rec = (struct log_uart *)Queue_dequeue(uart_queue_handle);
            if(uart_rec != NULL){
                UARTprintf(uart_rec->data);
                free(uart_rec);
            }
        }
        Task_sleep(100);
    }
}
