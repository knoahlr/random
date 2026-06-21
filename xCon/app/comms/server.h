/*
 * server.h
 *
 * WiFi command server (SERVER model): listens on APP_TCP_SERVER_PORT, accepts an
 * inbound controller and streams its gamepad command frames into the motor
 * mailbox via the shared net_session loop. One controller is served at a time.
 */
#ifndef APP_COMMS_SERVER_H
#define APP_COMMS_SERVER_H

#include <xdc/std.h>

/*
 * Server task entry.
 *   arg0 = motor Mailbox_Handle (parsed gamepad frames are posted here)
 *   arg1 = connection Semaphore_Handle (posted by the connection manager once
 *          WiFi is associated with an IP)
 *
 * Blocks on the semaphore until the link is up, then loops accepting and
 * servicing one controller connection at a time. The dial-out CLIENT model
 * lives separately in relay_client.c.
 */
void server_task(UArg arg0, UArg arg1);

/*
 * Log the current server status (listening state/port, the connected client if
 * any, and cumulative connection/frame counters) to the console log. Safe to
 * call from another task (e.g. the console); it only reads the stats snapshot.
 */
void server_status(void);

#endif /* APP_COMMS_SERVER_H */
