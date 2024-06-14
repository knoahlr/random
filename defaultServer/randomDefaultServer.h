
/* Local Platform Specific Header file */
#include "socket.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <ti/sysbios/hal/Seconds.h>
/*
     Server Method to contionously listening for commands.
     Gamepad input sampled every 500ms
 */


#define MAILBOX_TIMEOUT (20 * Clock_tickPeriod)
#define CONN_RETRY_TIMER_S 10

bool is_conn_active(int client_fd);

void clock_func(UArg arg0);

void server_init(UArg arg0, UArg arg1);

void tcp_message_handler(int server, Mailbox_Handle mail);
