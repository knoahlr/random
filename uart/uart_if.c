/*
 * uart_if.c
 *
 *  Created on: Mar 2, 2023
 *      Author: Noah Workstation
 */


#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"

#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"

#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "board.h"

#include <uart/uart_interface.h>

uint32_t sys_clock;
 static bool uart_configured = true;

void uart_input_handler(void){

    uint32_t status;

    // Get the interrrupt status.
    status = UARTIntStatus(UART0_BASE, true);

    System_printf("UART Int Status: %x\n", status);
    System_flush();

//    // Clear the asserted interrupts.
//    UARTIntClear(UART0_BASE, status);

    // Loop while there are characters in the receive FIFO.
//    while(UARTCharsAvail(UART0_BASE))
//    {
//        //Read the next character from the UART and write it back to the UART.
//        UARTCharPutNonBlocking(UART0_BASE, UARTCharGetNonBlocking(UART0_BASE));
//
//        SysCtlDelay(sys_clock / (1000 * 3));
//    }
    if(status & UART_INT_RX){
	if(UARTCharsAvail(UART0_BASE)){
	    System_printf("UART has data. Int Status: %d\n", status);
	}
	System_flush();
    }
}

void configure_uart_interface(){


  System_printf("Configuring UART...\n");
  System_flush();
  sys_clock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ|
                                  SYSCTL_OSC_MAIN |
                                    SYSCTL_USE_PLL |
                                    SYSCTL_CFG_VCO_480), 120000000);
  SysCtlMOSCConfigSet (SYSCTL_MOSC_PWR_DIS);


//    //enable UART interface 0 ? What UART interface is connected to micro-usb port
//    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
//
  UARTEnable(UART0_BASE);
//
//    //[TODO] - Set UART Pins -- What are the UART pins for the Micro-USB connection.
//  UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_TX);
//  UARTIntRegister(UART0_BASE, &uart_input_handler);

  //enable processor interrupts
  //    IntMasterEnable();
  uart_configured = true;
}

//constantly looking at uart_queue for any messages to be transmitted
void  * uart_messaging_service(UArg arg0){

    struct log_uart *uart_rec;

    Queue_Handle uart_queue_handle = (Queue_Handle)arg0;

    if(!uart_configured)
      configure_uart_interface();

    for(;;) {
      if(Queue_empty(uart_queue_handle)){
	  //
//	  System_printf("Uart queue empty...\n");
//	  System_flush();
      } else {
	  //loop through queue and send chars to UART.
	  uart_rec = (struct log_uart *)Queue_dequeue(uart_queue_handle);
	  if(uart_rec != NULL){
	      UARTCharPutNonBlocking(UART0_BASE, (unsigned char)uart_rec->data);
	      free(uart_rec);
	      System_printf("Uart record received and freed\n");
	      System_flush();
	  }
      }
      Task_sleep(100);
    }

}


