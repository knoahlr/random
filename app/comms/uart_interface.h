/*
 * uart_if.h
 *
 *  Created on: Mar 5, 2023
 *      Author: Noah Workstation
 */

#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Task.h>
#include <osi.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <utils/uartstdio.h>

#ifndef UART_IF_HEADER_FILE_H
#define UART_IF_HEADER_FILE_H
#define MAX_UART_RECORD 256

struct log_uart{
    Queue_Elem elem;
    char data[MAX_UART_RECORD];
    uint8_t len;
};

void configure_uart_interface(uint8_t uart_port);

void uart_handler(void);

void uart_messaging_service(UArg arg0);

#endif
