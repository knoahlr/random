#include "udp_discovery.h"
#include <comms/comms_config.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ti/sysbios/hal/Seconds.h>
#include <uart_interface.h>
#include "socket.h"
#include "EK_TM4C129EXL.h"

// --- Fix for missing macros on some embedded toolchains ---
#ifndef SO_BROADCAST
#define SO_BROADCAST 0x0020
#endif

#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST ((uint32_t)0xFFFFFFFF)
#endif
// -----------------------------------------------------------

static int udp_broadcast_sock = -1;
static struct sockaddr_in broadcast_addr;

void udp_discovery_init()
{
    udp_broadcast_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_broadcast_sock == -1) {
        uart_log("UDP broadcast socket create failed\n");
        return;
    }
    int broadcastEnable = 1;
    setsockopt(udp_broadcast_sock, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable));

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(UDP_BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
}

void udp_send_beacon()
{
    char beacon[256];
    snprintf(beacon, sizeof(beacon),
        "{\"type\":\"tm4c_firmware\",\"device_id\":\"%s\",\"default_server_port\":%d,\"capabilities\":[\"server\",\"client\"],\"status\":\"available\"}",
        APP_DEVICE_ID, APP_TCP_SERVER_PORT);
    sendto(udp_broadcast_sock, beacon, strlen(beacon), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
}

bool udp_check_for_app_beacon(relay_app_beacon_t* out) {
    if (!out) return false;
    char buffer[256];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    // MSG_DONTWAIT and flags are sometimes not available. Use 0 if not.
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
    int n = recvfrom(udp_broadcast_sock, buffer, sizeof(buffer) - 1, MSG_DONTWAIT, (struct sockaddr *)&sender_addr, &addr_len);
    out->valid = false;
    if (n <= 0) return false;
    buffer[n] = '\0';

    // Naive pattern match for relay app beacon
    if (strstr(buffer, "relay_app") && strstr(buffer, "request_mode\":\"client\"")) {
        // Extract \"ip\":\"...\" (look for actual value between quotes)
        char *ip_ptr = strstr(buffer, "\"ip\":\"");
        char *port_ptr = strstr(buffer, "server_port");
        char *app_ptr = strstr(buffer, "app_id\":\"");

        if (ip_ptr) {
            ip_ptr += 6; // move pointer right after \"ip\":\"
            char *ip_end = strchr(ip_ptr, '"');
            if (ip_end && (ip_end - ip_ptr) < (int)sizeof(out->ip)) {
                memcpy(out->ip, ip_ptr, ip_end - ip_ptr);
                out->ip[ip_end - ip_ptr] = '\0';
            } else {
                out->ip[0] = '\0';
            }
        } else {
            out->ip[0] = '\0';
        }

        if (port_ptr) {
            // Expect format: server_port":1234 (possibly with or without quotes)
            port_ptr = strchr(port_ptr, ':');
            if (port_ptr) {
                port_ptr++; // advance past ':'
                out->port = atoi(port_ptr);
            } else {
                out->port = 0;
            }
        } else {
            out->port = 0;
        }

        if (app_ptr) {
            app_ptr += 9; // move pointer right after \"app_id\":\"
            char *app_end = strchr(app_ptr, '"');
            if (app_end && (app_end - app_ptr) < (int)sizeof(out->app_id)) {
                memcpy(out->app_id, app_ptr, app_end - app_ptr);
                out->app_id[app_end - app_ptr] = '\0';
            } else {
                out->app_id[0] = '\0';
            }
        } else {
            strcpy(out->app_id, "?");
        }

        out->valid = true;
        return true;
    }
    return false;
}
