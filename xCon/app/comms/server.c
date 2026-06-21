/*
 * server.c
 *
 *  Created on: Jun 28, 2021
 *      Author: Noah Workstation
 *
 * WiFi command server. Once the connection manager reports an IP, this task
 * loops as a small state machine driven by UDP discovery:
 *
 *   - no relay app seen  -> SERVER role: accept an inbound controller
 *   - relay app announced -> CLIENT role: connect out to it
 *
 * Both roles hand the connected socket to the same session loop, which streams
 * gamepad command frames into the motor mailbox. Accept is non-blocking so the
 * UDP beacon cadence (and therefore role re-selection) never stalls.
 */

#include <comms/server.h>
#include <comms/comms_config.h>
#include <comms/uart_interface.h>
#include <comms/udp_discovery.h>
#include <comms/inet.h>
#include <input/gamepad_input.h>

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
/* recv() wakes at least this often so the session can run keepalive/failsafe. */
#define SESSION_RECV_TIMEOUT_S    4
/* Drop a client that has sent nothing for this long. */
#define CLIENT_IDLE_TIMEOUT_S     10

/* ---- socket option helpers (native SimpleLink: no BSD setsockopt alias) ---- */

static void socket_set_nonblocking(int fd, _u32 enable)
{
    SlSockNonblocking_t opt;
    opt.NonblockingEnabled = enable;
    sl_SetSockOpt(fd, SL_SOL_SOCKET, SL_SO_NONBLOCKING, &opt, sizeof(opt));
}

static void socket_set_recv_timeout_s(int fd, uint32_t seconds)
{
    struct SlTimeval_t tv;
    tv.tv_sec  = (SlTime_t)seconds;
    tv.tv_usec = 0;
    sl_SetSockOpt(fd, SL_SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv));
}

/* ---- helpers ---- */

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
    if (listen(fd, 0) < 0) {
        close(fd);
        return -1;
    }

    socket_set_nonblocking(fd, 1);
    uart_log("[server] listening on port %u\n", (unsigned)APP_TCP_SERVER_PORT);
    return fd;
}

/* Connect out to a discovered relay app. Returns the fd or -1. */
static int connect_to_relay_app(const char *app_ip, int app_port)
{
    struct sockaddr_in serv_addr;
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_port        = htons(app_port);
    serv_addr.sin_addr.s_addr = inet_addr_simple(app_ip);

    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Lightweight keepalive: returns false if the link is clearly dead. */
static bool client_keepalive_ok(int fd)
{
    static const uint8_t ping[] = "stormblessed";
    return send(fd, ping, sizeof(ping) - 1, 0) > 0;
}

/*
 * Service one connected controller until it disconnects or goes idle. Streams
 * gamepad frames into the motor mailbox and echoes the parsed status back.
 * The socket is closed before returning.
 */
static void service_client_session(int fd, Mailbox_Handle mail)
{
    uint8_t  buffer[APP_TCP_PACKET_SIZE];
    uint32_t last_rx_s = Seconds_get();

    /* Inbound sockets may inherit the listener's non-blocking flag; force a
     * blocking socket with a bounded recv timeout so this loop wakes regularly. */
    socket_set_nonblocking(fd, 0);
    socket_set_recv_timeout_s(fd, SESSION_RECV_TIMEOUT_S);

    uart_log("[server] client session started\n");

    for (;;) {
        GPIO_toggle(Board_LED1);

        int n = recv(fd, buffer, sizeof(buffer), 0);

        if (n > 0) {
            last_rx_s = Seconds_get();

            Gamepad gamepad_state = { 0 };
            if (command_frame_parse(&gamepad_state, buffer, sizeof(buffer))) {
                /* Non-blocking: a full motor mailbox means the consumer is
                 * behind; drop this frame rather than stall the network loop —
                 * the next frame supersedes it anyway. */
                if (!Mailbox_post(mail, &gamepad_state, BIOS_NO_WAIT)) {
                    uart_log("[server] motor mailbox full; frame dropped\n");
                }
            }

            if (send(fd, gamepad_state.status, sizeof(gamepad_state.status), 0) < 0) {
                uart_log("[server] send failed; closing client\n");
                break;
            }
        } else if (n == 0) {
            uart_log("[server] client closed connection\n");
            break;
        } else if (n == SL_EAGAIN) {
            /* recv timed out: no data this window. Drop the client if it has
             * been silent too long and a keepalive can't reach it. */
            if (((Seconds_get() - last_rx_s) > CLIENT_IDLE_TIMEOUT_S) &&
                !client_keepalive_ok(fd)) {
                uart_log("[server] client idle/unreachable; closing\n");
                break;
            }
        } else {
            uart_log("[server] recv error %d; closing client\n", n);
            break;
        }
    }

    close(fd);
}

static void send_handshake(int fd)
{
    static const char handshake[] =
        "{\"type\":\"tm4c_connect\",\"device_id\":\"" APP_DEVICE_ID "\"}";
    send(fd, handshake, sizeof(handshake) - 1, 0);
}

void server_task(UArg arg0, UArg arg1)
{
    Mailbox_Handle   mail = (Mailbox_Handle)arg0;
    Semaphore_Handle sem  = (Semaphore_Handle)arg1;

    /*
     * Wait for the link BEFORE touching the SimpleLink socket layer.
     * udp_discovery_init() opens a socket (sl_Socket); doing that before the
     * link is up makes this task contend the single-threaded NWP command pipe
     * with the connection manager's bring-up commands.
     */
    if (!Semaphore_pend(sem, SERVER_LINK_WAIT_TICKS)) {
        uart_log("[server] timed out waiting for WiFi link; exiting\n");
        return;
    }
    GPIO_write(Board_LED1, Board_LED_OFF);

    udp_discovery_init();

    int                listen_fd      = open_listen_socket();
    uint32_t           last_beacon_s  = 0;
    relay_app_beacon_t relay          = { 0 };

    for (;;) {
        uint32_t now_s = Seconds_get();

        /* Periodic UDP beacon + relay-app discovery. */
        if ((now_s - last_beacon_s) >= APP_UDP_BEACON_INTERVAL_S) {
            last_beacon_s = now_s;
            udp_send_beacon();

            if (udp_check_for_app_beacon(&relay) && relay.valid) {
                /* CLIENT role: a relay app is present — connect out to it. */
                int client_fd = connect_to_relay_app(relay.ip, relay.port);
                if (client_fd >= 0) {
                    send_handshake(client_fd);
                    service_client_session(client_fd, mail);
                }
                continue;   /* re-evaluate role after the session ends */
            }
        }

        /* SERVER role: accept an inbound controller (non-blocking). */
        if (listen_fd < 0) {
            listen_fd = open_listen_socket();
            if (listen_fd < 0) {
                Task_sleep(SERVER_ACCEPT_POLL_TICKS);
                continue;
            }
        }

        struct sockaddr_in client_addr;
        socklen_t          addrlen = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);

        if (client_fd >= 0) {
            service_client_session(client_fd, mail);
        } else {
            /* SL_EAGAIN: nothing pending. Sleep so the beacon cadence holds. */
            Task_sleep(SERVER_ACCEPT_POLL_TICKS);
        }
    }
}
