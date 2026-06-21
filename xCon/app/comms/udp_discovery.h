#ifndef UDP_DISCOVERY_H
#define UDP_DISCOVERY_H
#include <stdint.h>
#include <stdbool.h>

#include <comms/comms_config.h>

// UDP beaconing, device/app discovery, and negotiation state management.

#define UDP_BROADCAST_PORT APP_UDP_BROADCAST_PORT
#define UDP_BEACON_INTERVAL_S APP_UDP_BEACON_INTERVAL_S
#define DEVICE_ID APP_DEVICE_ID
#define DEFAULT_SERVER_PORT APP_TCP_SERVER_PORT

// Discovery beacon from relay app
typedef struct {
    bool valid;
    char ip[32];
    int  port;
    char app_id[32];
} relay_app_beacon_t;

void udp_discovery_init();
void udp_send_beacon();
// Listen for a relay app beacon, returns true and fills beacon struct if found
bool udp_check_for_app_beacon(relay_app_beacon_t* out);

#endif
