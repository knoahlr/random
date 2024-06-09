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

/* Local Platform Specific Header file */
#include "socket.h"

/*Application specific headers*/

#define TCPPORT         1000
#define TCPPACKETSIZE   256

void defaultServerTask(UArg arg0, UArg arg1)
{
    Gamepad gamepadState = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int         bytesRcvd;
    int         bytesSent;
    int         status;
    int         clientfd;
    int         server;
    sockaddr_in localAddr;
    sockaddr_in clientAddr;
    socklen_t   addrlen = sizeof(clientAddr);
    char        buffer[TCPPACKETSIZE];
    _u32        semaphoreTimeout;
    bool        semphorePostStatus = false;
    _u32        mailboxTimeout;

    semaphoreTimeout = 20000 * Clock_tickPeriod;
    Semaphore_Handle sem = (Semaphore_Handle)arg1;
    Mailbox_Handle mail = (Mailbox_Handle)arg0;
    mailboxTimeout = 20 * Clock_tickPeriod;

    //use sufficiently large timeout since WiFi connection(s) is inherently unpredictable.
    //Note that this semaphore is posted after Ip has been acquired by the network driver.
    semphorePostStatus = Semaphore_pend(sem, semaphoreTimeout);
    if (semphorePostStatus)
    {
        GPIO_write(Board_LED0, 0);
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

        while ((clientfd = accept(server, (sockaddr *)&clientAddr, &addrlen)) != -1)
        {
            GPIO_toggle(Board_LED0);
            while ((bytesRcvd = recv(clientfd, buffer, TCPPACKETSIZE, 4)) > 0)
            {
                //Process Command Frame from device and response.
                bool commandFrameParse_rc = commandFrameParse(&gamepadState, buffer, sizeof(buffer));

                //free buffer
                memset(buffer, 0, TCPPACKETSIZE);

                //post gamepad state to mailbox. Message will be read and interpreted by dependent tasks
                // Current(and only) dependent task is pwm duty motor updater task
                if(commandFrameParse_rc)
                {
                    bool rc = Mailbox_post(mail, &gamepadState, mailboxTimeout);
                    if(!rc){System_printf("Error: Unable to post gamepad state to mailbox.\n");}
                }

                //Build message to display commands being read from Gamepad

                //Apply gamepad state
                //-- Left Analog for Servo
                //-- Right Trigger for Motor

                bytesSent = send(clientfd, gamepadState.status, sizeof(gamepadState.status), 0);

                System_printf("Message Received is %s\n", buffer);

                //My own version of TCP Echo
                if (bytesSent < 0)
                {
                    System_printf("Error: send failed.\n");
//                    break;
                }
            }
            addrlen = sizeof(clientAddr);
            close(clientfd);
            GPIO_write(Board_LED0, 0);
        }
    shutdown:
        if (server >= 0)
        {
            close(server);
            GPIO_write(Board_LED0, 0);
            /* Close the network - don't do this if other tasks are using it */
        }
    }
  }

