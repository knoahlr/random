/*
 * randomDefaultServer.cc
 *
 *  Created on: Jun 28, 2021
 *      Author: Noah Workstation
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* Local Platform Specific Header file */
#include "socket.h"

/*Application specific headers*/
#include "StepperControl.h"
#include "GamepadInput.h"
#include "connectionManager.h"

#define TCPPORT         1000
#define TCPPACKETSIZE   256

void defaultServerTask(UArg arg0, UArg arg1)
{
    Gamepad gamepadState;
    int         bytesRcvd;
    int         bytesSent;
    int         status;
    int         clientfd;
    int         server;
    sockaddr_in localAddr;
    sockaddr_in clientAddr;
    socklen_t   addrlen = sizeof(clientAddr);
    char        buffer[TCPPACKETSIZE];

    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == -1) {
        System_printf("Error: socket not created.\n");
        goto shutdown;
    }

    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(TCPPORT);

    status = bind(server, (const sockaddr *)&localAddr, sizeof(localAddr));
    if (status == -1) {
        System_printf("Error: bind failed.\n");
        goto shutdown;
    }

    status = listen(server, 0);
    if (status == -1){

        System_printf("Error: listen failed.\n");
        goto shutdown;
    }

    while ((clientfd =
            accept(server, (sockaddr *)&clientAddr, &addrlen)) != -1)
    {

        while ((bytesRcvd = recv(clientfd, buffer, TCPPACKETSIZE, 0)) > 0)
        {
//            GPIOPinWrite(GPIO_PORTK_BASE, GPIO_PIN_6, GPIO_PIN_6);
//            int32_t value = GPIOPinRead(GPIO_PORTK_BASE, GPIO_PIN_7);
//            char myBuffer[] = "The voltage is %d";
//            char tempString[100];
//            sprintf(tempString , tempString, value);

            //Process Command Frame from device and response.

            commandFrameParse(&gamepadState, buffer);
            //Build message to display commands being read from Gamepad




            //Apply gamepad state
            //-- Left Analog for Servo
            //-- Right Trigger for Motor

            bytesSent = send(clientfd, buffer, bytesRcvd, 0);

            System_printf("Message Received is %s\n", buffer);
            GPIO_toggle(Board_LED1);
            //Task_sleep(1000);

            //My own version of TCP Echo
            if (bytesSent < 0)
            {
                System_printf("Error: send failed.\n");
                break;
            }
        }
        addrlen = sizeof(clientAddr);
        close(clientfd);
    }
shutdown:
    if (server >= 0) {
        close(server);
        /* Close the network - don't do this if other tasks are using it */
//        socketsShutDown(netIF);
    }
  }

