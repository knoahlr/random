/* Local Platform Specific Header file */
#include "socket.h"
#include "udp_discovery.h"

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <ti/sysbios/hal/Seconds.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Semaphore.h>

/*
     Server method to continuously listen for commands.
     Gamepad input sampled every 500ms.
 */

#define MAILBOX_TIMEOUT (20 * Clock_tickPeriod)
#define CONN_RETRY_TIMER_S 10

// Device operation mode
typedef enum {
    SERVER_MODE = 0,
    CLIENT_MODE = 1
} server_state_t;

bool is_conn_active(int client_fd);

void server_task(UArg arg0, UArg arg1);

void tcp_message_handler(int server, Mailbox_Handle mail);
