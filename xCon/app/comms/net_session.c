/*
 * net_session.c
 *
 * Shared session servicing for an established TCP connection. Both the server
 * model (accepting a controller) and the future client model (dialling out to a
 * relay app) hand their connected socket here, so the recv/parse/post-to-motor
 * data path lives in exactly one place.
 */

#include <comms/net_session.h>
#include <comms/comms_config.h>
#include <comms/uart_interface.h>
#include <input/gamepad_input.h>

#include <stdbool.h>
#include <stdint.h>

#include <xdc/std.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/drivers/GPIO.h>

#include "Board.h"
#include "socket.h"

/* recv() wakes at least this often so the session can run keepalive/failsafe. */
#define SESSION_RECV_TIMEOUT_S    4
/* Drop a peer that has sent nothing for this long. */
#define SESSION_IDLE_TIMEOUT_S    10

void net_socket_set_nonblocking(int fd, uint32_t enable)
{
    SlSockNonblocking_t opt;
    opt.NonblockingEnabled = (_u32)enable;
    sl_SetSockOpt(fd, SL_SOL_SOCKET, SL_SO_NONBLOCKING, &opt, sizeof(opt));
}

void net_socket_set_recv_timeout_s(int fd, uint32_t seconds)
{
    struct SlTimeval_t tv;
    tv.tv_sec  = (SlTime_t)seconds;
    tv.tv_usec = 0;
    sl_SetSockOpt(fd, SL_SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv));
}

/* Lightweight keepalive: returns false if the link is clearly dead. */
static bool keepalive_ok(int fd)
{
    static const uint8_t ping[] = "stormblessed";
    return send(fd, ping, sizeof(ping) - 1, 0) > 0;
}

void net_service_session(int fd, Mailbox_Handle mail)
{
    uint8_t  buffer[APP_TCP_PACKET_SIZE];
    uint32_t last_rx_s = Seconds_get();

    /* A socket accepted from a non-blocking listener inherits that flag; force a
     * blocking socket with a bounded recv timeout so this loop wakes regularly. */
    net_socket_set_nonblocking(fd, 0);
    net_socket_set_recv_timeout_s(fd, SESSION_RECV_TIMEOUT_S);

    uart_log("[session] peer connected\n");

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
                    uart_log("[session] motor mailbox full; frame dropped\n");
                }
            }

            if (send(fd, gamepad_state.status, sizeof(gamepad_state.status), 0) < 0) {
                uart_log("[session] send failed; closing peer\n");
                break;
            }
        } else if (n == 0) {
            uart_log("[session] peer closed connection\n");
            break;
        } else if (n == SL_EAGAIN) {
            /* recv timed out: no data this window. Drop the peer if it has been
             * silent too long and a keepalive can't reach it. */
            if (((Seconds_get() - last_rx_s) > SESSION_IDLE_TIMEOUT_S) &&
                !keepalive_ok(fd)) {
                uart_log("[session] peer idle/unreachable; closing\n");
                break;
            }
        } else {
            uart_log("[session] recv error %d; closing peer\n", n);
            break;
        }
    }

    close(fd);
}
