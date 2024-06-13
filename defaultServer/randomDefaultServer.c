/*
 * randomDefaultServer.cc
 *
 *  Created on: Jun 28, 2021
 *      Author: Noah Workstation
 */

/* XDCtools Header files */
#include <communication/connectionManager.h>
#include <gamepad/GamepadInput.h>
#include <hardware/stepperControl.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/clock.h>
//#include <ti/sysbios/knl/time.h>

/* Local Platform Specific Header file */
#include "socket.h"
#include "randomDefaultServer.h"
/*Application specific headers*/

#define TCPPORT         1000
#define TCPPACKETSIZE   256

Clock_Params clock_params;
Clock_Handle clock_handle;

void clock_func(UArg arg0) {
    printf("\nAt clock func: %d\n", Clock_getTicks());
}
void tcp_message_handler(int server, Mailbox_Handle mail) {

  int         bytesRcvd;
  int         bytesSent;
  int         client_fd;
  sockaddr_in clientAddr;

  socklen_t   addrlen = sizeof(clientAddr);

  uint8_t        buffer[TCPPACKETSIZE];

  for(;;) {

    //clock_t clock(void);
    client_fd = accept(server, (sockaddr *)&clientAddr, &addrlen);

    do{
        GPIO_toggle(Board_LED0);
        bytesRcvd = recv(client_fd, buffer, TCPPACKETSIZE, 4);

        Gamepad gamepad_state = { 0 };

        while ( bytesRcvd > 0 ) {
            //Process Command Frame from device and response.
            bool command_frame_parse_rc = commandFrameParse(&gamepad_state, buffer, sizeof(buffer));
            printf("%d: Bytes Received. Msg is %s\n", bytesRcvd, buffer);
            //free buffer
            memset(buffer, 0, TCPPACKETSIZE);

            //post gamepad state to mailbox. Message will be read and interpreted by dependent tasks
            // Current(and only) dependent task is pwm duty motor updater task
            if(command_frame_parse_rc) {
                bool rc = Mailbox_post(mail, &gamepad_state, MAILBOX_TIMEOUT);
                if(!rc) {
                    printf("Error: Unable to post gamepad state to mailbox.\n");
                }
            }

            //Build message to display commands being read from Gamepad
            //Apply gamepad state
            //-- Left Analog for Servo
            //-- Right Trigger for Motor
            bytesSent = send(client_fd, gamepad_state.status, sizeof(gamepad_state.status), 0);

            //My own version of TCP Echo
            if (bytesSent < 0) {
              printf("Error: send failed.\n");
              break;
            }
            bytesRcvd = recv(client_fd, buffer, TCPPACKETSIZE, 4);
        }
        Task_sleep(100);

        //check for new connection every 3 secs incase server was stopped
    } while(client_fd);
  }
  addrlen = sizeof(clientAddr);
  close(client_fd);
  GPIO_write(Board_LED0, 0);
}

void server_init(UArg arg0, UArg arg1)
{

    _u32        semaphoreTimeout;
    bool        semphorePostStatus = false;

    sockaddr_in localAddr;

    int         status;
    int server;

    semaphoreTimeout = 20000 * Clock_tickPeriod;
    Semaphore_Handle sem = (Semaphore_Handle)arg1;
    Mailbox_Handle mail = (Mailbox_Handle)arg0;

    Clock_Params_init(&clock_params);
    clock_params.period = 1000;
    clock_params.startFlag = TRUE;
//    clkParams.arg = ;
    clock_handle = Clock_create((Task_FuncPtr)clock_func, 3000, &clock_params, NULL);
    Clock_start(clock_handle);
    //Clock_stop();

    //use sufficiently large timeout since WiFi connection(s) is inherently unpredictable.
    //Note that this semaphore is posted after Ip has been acquired by the network driver.
    semphorePostStatus = Semaphore_pend(sem, semaphoreTimeout);
    if (semphorePostStatus) {
        GPIO_write(Board_LED0, 0);
        server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == -1) {
            printf("Error: socket not created.\n");
            return;
        }

        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = htons(TCPPORT);

        status = bind(server, (const sockaddr *)&localAddr, sizeof(localAddr));
        if (status == -1) {
            printf("Error: bind failed.\n");
            return;
        }

        status = listen(server, 0);
        if (status == -1){
            printf("Error: listen failed.\n");
            return;
        }
        tcp_message_handler(server, mail);
    }
}
