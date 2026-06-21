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
#include <uart_interface.h>
#include <input/gamepad_input.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
/* Diagnostic raw-frame dump cadence (seconds). 0 disables the dump. */
#define SESSION_RX_DUMP_INTERVAL_S 3
/* Bytes of each dumped frame to show. */
#define SESSION_RX_DUMP_BYTES      24

/*
 * Wire framing (see docs/gamepad-protocol.md): a continuous TCP stream of
 * messages, each a fixed 16-byte little-endian envelope followed by `length`
 * payload bytes. envelope[0] is the message type; envelope[2:4] is the uint16
 * payload length. There are no delimiters, so we length-prefix-parse.
 */
#define NET_ENVELOPE_SIZE   16
#define NET_MSG_GAMEPAD     0x01
/* Largest payload we accept (base 26, +12 sensors = 38); cap for resync sanity. */
#define NET_MAX_PAYLOAD     64
#define NET_FRAME_MAX       (NET_ENVELOPE_SIZE + NET_MAX_PAYLOAD)

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

/* Dump the first bytes of a received frame as hex, so the on-wire encoding can
 * be compared against the relay app / parser's expected layout. */
static void log_rx_frame(const uint8_t *buf, int n)
{
    char line[3 * SESSION_RX_DUMP_BYTES + 1];
    int  shown = (n < SESSION_RX_DUMP_BYTES) ? n : SESSION_RX_DUMP_BYTES;
    int  pos   = 0;
    int  i;

    for (i = 0; i < shown; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02x ", buf[i]);
    }
    line[(pos > 0) ? pos - 1 : 0] = '\0';
    uart_log("[session] rx n=%d: %s\n", n, line);
}

/* Lightweight keepalive: returns false if the link is clearly dead. */
static bool keepalive_ok(int fd)
{
    static const uint8_t ping[] = "stormblessed";
    return send(fd, ping, sizeof(ping) - 1, 0) > 0;
}

uint32_t net_service_session(int fd, Mailbox_Handle mail)
{
    /* Room for one full frame of carry-over plus a whole recv. */
    uint8_t  buffer[NET_FRAME_MAX + APP_TCP_PACKET_SIZE];
    size_t   have        = 0;   /* bytes buffered (partial-frame carry + new) */
    uint32_t last_rx_s   = Seconds_get();
    uint32_t last_dump_s = 0;
    uint32_t frames      = 0;

    /* A socket accepted from a non-blocking listener inherits that flag; force a
     * blocking socket with a bounded recv timeout so this loop wakes regularly. */
    net_socket_set_nonblocking(fd, 0);
    net_socket_set_recv_timeout_s(fd, SESSION_RECV_TIMEOUT_S);

    for (;;) {
        GPIO_toggle(Board_LED1);

        int n = recv(fd, buffer + have, APP_TCP_PACKET_SIZE, 0);

        if (n > 0) {
            last_rx_s = Seconds_get();

            /* Periodically dump the raw bytes just received so the wire encoding
             * can be checked against docs/gamepad-protocol.md. */
            if (SESSION_RX_DUMP_INTERVAL_S > 0 &&
                (last_rx_s - last_dump_s) >= SESSION_RX_DUMP_INTERVAL_S) {
                last_dump_s = last_rx_s;
                log_rx_frame(buffer + have, n);
            }

            have += (size_t)n;

            /* Frame-walk: pull every complete envelope+payload out of the buffer.
             * Motor control is state (not events), so we keep only the most
             * recent gamepad frame from this batch and post that one. */
            size_t  off        = 0;
            Gamepad gamepad_state = { 0 };
            bool    have_frame = false;

            while (have - off >= NET_ENVELOPE_SIZE) {
                const uint8_t *env    = buffer + off;
                uint16_t       length = (uint16_t)(env[2] | ((uint16_t)env[3] << 8));

                if (length > NET_MAX_PAYLOAD) {
                    /* Implausible length: the stream desynced. Skip a byte and
                     * re-scan rather than trusting a corrupt frame. */
                    off++;
                    continue;
                }
                if ((have - off) < (NET_ENVELOPE_SIZE + (size_t)length)) {
                    break;  /* rest of this frame hasn't arrived yet */
                }

                if (env[0] == NET_MSG_GAMEPAD &&
                    gamepad_parse_payload(&gamepad_state,
                                          env + NET_ENVELOPE_SIZE, length)) {
                    have_frame = true;
                    frames++;
                }
                off += NET_ENVELOPE_SIZE + (size_t)length;
            }

            if (have_frame) {
                /* Non-blocking: a full motor mailbox means the consumer is
                 * behind; drop this frame rather than stall the network loop. */
                if (!Mailbox_post(mail, &gamepad_state, BIOS_NO_WAIT)) {
                    uart_log("[session] motor mailbox full; frame dropped\n");
                }
                if (send(fd, gamepad_state.status,
                         sizeof(gamepad_state.status), 0) < 0) {
                    uart_log("[session] send failed; closing peer\n");
                    break;
                }
            }

            /* Carry the partial-frame remainder to the next recv. */
            have -= off;
            if (off > 0 && have > 0) {
                memmove(buffer, buffer + off, have);
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
    return frames;
}
