/*
 * server.h
 *
 * WiFi command server: streams gamepad command frames from a controller into
 * the motor mailbox.
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
 * After the link is up the task picks a role every cycle via UDP discovery:
 *   - SERVER: listen on APP_TCP_SERVER_PORT and accept a controller, or
 *   - CLIENT: if a relay app announces itself on the LAN, connect out to it.
 * Either way an established session is serviced by the same frame loop.
 */
void server_task(UArg arg0, UArg arg1);

#endif /* APP_COMMS_SERVER_H */
