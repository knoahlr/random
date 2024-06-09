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

#ifndef UART_IF_HEADER_FILE_H
#define UART_IF_HEADER_FILE_H
#define MAX_UART_RECORD 256

//typedef struct log_uart uart_rec;

struct log_uart{
    Queue_Elem elem;
    char data[MAX_UART_RECORD];
    uint8_t len;
};

void configure_uart_interface();

void uart_handler(void);

void * uart_messaging_service(UArg arg0);

//void print_uart_record(struct log_uart *rec);
#endif
