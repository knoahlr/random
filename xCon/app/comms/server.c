/*
 * server.c
 *
 *  Created on: Jun 28, 2021
 *      Author: Noah Workstation
 *
 * WiFi command server (SERVER model). Once the connection manager reports an IP,
 * this task listens on APP_TCP_SERVER_PORT and accepts an inbound controller,
 * then hands the socket to the shared session loop (net_session) which streams
 * gamepad command frames into the motor mailbox.
 *
 * Only one controller is served at a time (SERVER_MAX_CONNECTIONS): accept is
 * non-blocking and a connection is serviced to completion before the next is
 * accepted. The CLIENT model (dial OUT to a discovered relay app) has been
 * segregated into relay_client.c for future use and is not started here.
 */

#include <comms/server.h>
#include <comms/comms_config.h>
#include <comms/uart_interface.h>
#include <comms/net_session.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <xdc/std.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/drivers/GPIO.h>

#include "Board.h"
#include "socket.h"

/* How long to wait for the connection manager to report WiFi/IP up. */
#define SERVER_LINK_WAIT_TICKS    (20000 * Clock_tickPeriod)
/* Poll cadence while idly waiting for an inbound connection (system ticks). */
#define SERVER_ACCEPT_POLL_TICKS  100
/* Controllers served concurrently. One for now; sessions run to completion. */
#define SERVER_MAX_CONNECTIONS    1

/*
 * Status snapshot for the `server` console command. Written only by the server
 * task; read by the console task. Fields are word-sized so a torn read at worst
 * shows a momentarily stale value, which is fine for a human-facing readout.
 */
typedef struct {
    bool     listening;          /* listen socket is open */
    bool     client_connected;   /* a client session is in progress */
    uint32_t client_ip;          /* current/last client IP (host order) */
    uint16_t client_port;        /* current/last client port */
    uint32_t session_start_s;    /* Seconds when the current client connected */
    uint32_t total_connections;  /* clients accepted since boot */
    uint32_t total_frames;       /* gamepad frames parsed since boot */
} server_stats_t;

static server_stats_t g_stats;

void server_status(void)
{
    uint32_t ip = g_stats.client_ip;

    uart_log("[server] listening=%d port=%u\n",
             g_stats.listening, (unsigned)APP_TCP_SERVER_PORT);

    if (g_stats.client_connected) {
        uart_log("[server] client connected: %u.%u.%u.%u:%u  session=%us\n",
                 (unsigned)((ip >> 24) & 0xFF), (unsigned)((ip >> 16) & 0xFF),
                 (unsigned)((ip >> 8) & 0xFF), (unsigned)(ip & 0xFF),
                 (unsigned)g_stats.client_port,
                 (unsigned)(Seconds_get() - g_stats.session_start_s));
    } else {
        uart_log("[server] no client connected\n");
    }

    uart_log("[server] total_connections=%u total_frames=%u\n",
             (unsigned)g_stats.total_connections, (unsigned)g_stats.total_frames);
}

/* Open, bind and listen the server socket, set non-blocking so accept() polls.
 * Returns the fd or -1. */
static int open_listen_socket(void)
{
    struct sockaddr_in local_addr;
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return -1;
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port        = htons(APP_TCP_SERVER_PORT);

    if (bind(fd, (const struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, SERVER_MAX_CONNECTIONS) < 0) {
        close(fd);
        return -1;
    }

    net_socket_set_nonblocking(fd, 1);
    uart_log("[server] listening on port %u\n", (unsigned)APP_TCP_SERVER_PORT);
    return fd;
}

void server_task(UArg arg0, UArg arg1)
{
    Mailbox_Handle   mail = (Mailbox_Handle)arg0;
    Semaphore_Handle sem  = (Semaphore_Handle)arg1;

    /*
     * Wait for the link BEFORE touching the SimpleLink socket layer. Opening a
     * socket before the link is up makes this task contend the single-threaded
     * NWP command pipe with the connection manager's bring-up commands.
     */
    if (!Semaphore_pend(sem, SERVER_LINK_WAIT_TICKS)) {
        uart_log("[server] timed out waiting for WiFi link; exiting\n");
        return;
    }
    GPIO_write(Board_LED1, Board_LED_OFF);

    int listen_fd = open_listen_socket();
    g_stats.listening = (listen_fd >= 0);

    for (;;) {
        if (listen_fd < 0) {
            listen_fd = open_listen_socket();
            g_stats.listening = (listen_fd >= 0);
            if (listen_fd < 0) {
                Task_sleep(SERVER_ACCEPT_POLL_TICKS);
                continue;
            }
        }

        struct sockaddr_in client_addr;
        socklen_t          addrlen = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);

        if (client_fd >= 0) {
            uint32_t ip = ntohl(client_addr.sin_addr.s_addr);

            g_stats.client_ip        = ip;
            g_stats.client_port      = ntohs(client_addr.sin_port);
            g_stats.session_start_s  = Seconds_get();
            g_stats.client_connected = true;
            g_stats.total_connections++;

            uart_log("[server] client connected from %u.%u.%u.%u:%u\n",
                     (unsigned)((ip >> 24) & 0xFF), (unsigned)((ip >> 16) & 0xFF),
                     (unsigned)((ip >> 8) & 0xFF), (unsigned)(ip & 0xFF),
                     (unsigned)g_stats.client_port);

            /* Serve this controller to completion before accepting another,
             * which bounds us to SERVER_MAX_CONNECTIONS at a time. */
            g_stats.total_frames += net_service_session(client_fd, mail);

            g_stats.client_connected = false;
            uart_log("[server] client disconnected\n");
        } else {
            /* SL_EAGAIN: nothing pending. Sleep so we don't spin the CPU. */
            Task_sleep(SERVER_ACCEPT_POLL_TICKS);
        }
    }
}
