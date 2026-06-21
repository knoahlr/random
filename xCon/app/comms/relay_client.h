/*
 * relay_client.h
 *
 * CLIENT model (segregated for future use; NOT wired into main()).
 *
 * Instead of accepting an inbound controller, this role discovers a relay app on
 * the LAN via UDP beacons and connects OUT to it, then streams gamepad frames
 * over that link using the shared net_session loop. Kept separate from the
 * SERVER model (server.c) so either can be selected without entangling the two.
 */
#ifndef APP_COMMS_RELAY_CLIENT_H
#define APP_COMMS_RELAY_CLIENT_H

#include <xdc/std.h>

/*
 * Relay-client task entry.
 *   arg0 = motor Mailbox_Handle (parsed gamepad frames are posted here)
 *   arg1 = connection Semaphore_Handle (posted by the connection manager once
 *          WiFi is associated with an IP)
 *
 * Blocks on the semaphore, then periodically beacons over UDP; when a relay app
 * announces itself it dials out, handshakes, and services the session.
 */
void relay_client_task(UArg arg0, UArg arg1);

#endif /* APP_COMMS_RELAY_CLIENT_H */
