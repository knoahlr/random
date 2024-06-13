/*
 * uart_if.c
 *
 *  Created on: Mar 2, 2023
 *      Author: Noah Workstation
 */


#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <uart/uart_interface.h>

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include <driverlib/gpio.h>
#include <driverlib/interrupt.h>

#include <driverlib/sysctl.h>
#include <driverlib/uart.h>

#include "board.h"

uint32_t sys_clock;
 static bool uart_configured = true;
 static uint32_t baud_rate = 115200;

void configure_uart_interface(uint8_t uart_port){

  sys_clock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ|
                                  SYSCTL_OSC_MAIN |
                                    SYSCTL_USE_PLL |
                                    SYSCTL_CFG_VCO_480), 120000000);
  printf("Configuring UART: %d\n", uart_port);
//  UARTStdioConfig((uint32_t)uart_port, baud_rate, sys_clock);
  uart_configured = true;

}

//constantly looking at uart_queue for any messages to be transmitted
void  * uart_messaging_service(UArg arg0){

    struct log_uart *uart_rec;

    Queue_Handle uart_queue_handle = (Queue_Handle)arg0;

    if(!uart_configured)
      configure_uart_interface(0);

    for(;;) {
      if(Queue_empty(uart_queue_handle)){
	  //
//	  System_printf("Uart queue empty...\n");
//	  System_flush();
      } else {
	  //loop through queue and send chars to UART.
	  uart_rec = (struct log_uart *)Queue_dequeue(uart_queue_handle);
	  if(uart_rec != NULL){
	      UARTCharPutNonBlocking(UART0_BASE, *uart_rec->data);
	      free(uart_rec);
	      System_printf("Uart record received and freed\n");
	      System_flush();
	  }
      }
      Task_sleep(100);
    }
}


