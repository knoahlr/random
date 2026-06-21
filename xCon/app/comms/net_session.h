/*
 * net_session.h
 *
 * Shared transport-agnostic session servicing. Once a TCP socket is connected
 * (whether we accepted it as a server or dialled out as a client) the same loop
 * streams parsed gamepad command frames into the motor mailbox. Keeping it here
 * lets the server model (server.c) and the future client model (relay_client.c)
 * share one well-tested data path.
 */
#ifndef APP_COMMS_NET_SESSION_H
#define APP_COMMS_NET_SESSION_H

#include <xdc/std.h>
#include <stdint.h>

#include <ti/sysbios/knl/Mailbox.h>

/* ---- socket option helpers (native SimpleLink: no BSD setsockopt alias) ---- */

/* Toggle a socket's non-blocking flag. */
void net_socket_set_nonblocking(int fd, uint32_t enable);
/* Bound recv() so a session loop wakes regularly for keepalive/failsafe. */
void net_socket_set_recv_timeout_s(int fd, uint32_t seconds);

/*
 * Service one connected peer until it disconnects or goes idle. Streams gamepad
 * frames into the motor mailbox and echoes the parsed status back. The socket is
 * closed before returning.
 */
void net_service_session(int fd, Mailbox_Handle mail);

#endif /* APP_COMMS_NET_SESSION_H */
