#ifndef UDP_DISCOVERY_H
#define UDP_DISCOVERY_H
#include <stdint.h>
#include <stdbool.h>

// UDP beaconing, device/app discovery, and negotiation state management.

#define UDP_BROADCAST_PORT 5051
#define UDP_BEACON_INTERVAL_S 2
#define DEVICE_ID "tm4c129x-001" // Customizable per device
#define DEFAULT_SERVER_PORT 1000

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
