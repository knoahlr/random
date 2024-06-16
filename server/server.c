/*
 * randomDefaultServer.cc
 *
 *  Created on: Jun 28, 2021
 *      Author: Noah Workstation
 */

/* XDCtools Header files */
#include <communication/connection_manager.h>
#include <gamepad/gamepad_input.h>
#include <hardware/stepper_control.h>
#include <server/server.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/clock.h>

#include <ti/sysbios/hal/Seconds.h>
//#include <ti/sysbios/knl/time.h>

/* Local Platform Specific Header file */
#include "socket.h"

/*Application specific headers*/

#define TCPPORT         1000
#define TCPPACKETSIZE   256

static bool conn_active = false;

Clock_Params clock_params;
Clock_Handle clock_handle;

bool is_conn_active(int client_fd){
    uint8_t ping_message[] = "stormblessed";
    int bytesSent = send(client_fd, ping_message, strlen(ping_message), 0);
    if (bytesSent > 0){
        conn_active = true;
    }
    conn_active = false;
    uint8_t conn_status_string[] = conn_active ? "Active" : "Not Active";
    printf("Connection Status:%s\n", conn_status_string);
    return conn_active;
}


void tcp_message_handler(int server, Mailbox_Handle mail) {

  int         bytesRcvd;
  int         bytesSent;
  int         client_fd;

  sockaddr_in clientAddr;
  socklen_t   addrlen = sizeof(clientAddr);

  uint8_t        buffer[TCPPACKETSIZE];

  int listen_rc = listen(server, 0);

  //for(;;) {
  while(!listen_rc) {
    client_fd = accept(server, (sockaddr *)&clientAddr, &addrlen);


    uint32_t time_at_accept_s = Seconds_get();
    uint32_t time_since_last_byte = time_at_accept_s;

    while(client_fd) {

        GPIO_toggle(Board_LED0); // As a signal we're accepting incoming connections
        bytesRcvd = recv(client_fd, buffer, TCPPACKETSIZE, 4);

        Gamepad gamepad_state = { 0 };

        while ( bytesRcvd > 0 ) {
            time_since_last_byte = Seconds_get();
            //Process Command Frame from device and response. if valid gamepad state transmit to motor```
            bool command_frame_parse_rc = commandFrameParse(&gamepad_state, buffer, sizeof(buffer));
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

        //if 10 seconds has passed without a message check that connection is still active
        if( abs(Seconds_get()-time_since_last_byte) > CONN_RETRY_TIMER_S){
            if(!is_conn_active(client_fd)) {
                listen_rc = listen(server, 0);
                client_fd = accept(server, (sockaddr *)&clientAddr, &addrlen);
            }
        }
    }
  }
  printf("closing server. ooops!");
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

//    Clock_Params_init(&clock_params);
//    clock_params.period = 1000;
//    clock_params.startFlag = TRUE;
//    clkParams.arg = ;
//    clock_handle = Clock_create((Task_FuncPtr)clock_func, 3000, &clock_params, NULL);
//    Clock_start(clock_handle);
//    Clock_stop();

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

        tcp_message_handler(server, mail);
    }
}
