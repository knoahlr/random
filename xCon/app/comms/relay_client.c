/*
 * relay_client.c
 *
 * CLIENT model (segregated for future use; NOT wired into main()).
 *
 * Discovers a relay app via UDP beacons and connects OUT to it, then hands the
 * socket to the shared net_session loop. This is the inverse of server.c: rather
 * than accepting a controller, the device dials out to a known relay. Preserved
 * here so the dial-out path can be re-enabled later without resurrecting it from
 * git history.
 */

#include <comms/relay_client.h>
#include <comms/comms_config.h>
#include <comms/uart_interface.h>
#include <comms/udp_discovery.h>
#include <comms/inet.h>
#include <comms/net_session.h>

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
#define RELAY_LINK_WAIT_TICKS    (20000 * Clock_tickPeriod)
/* Poll cadence between discovery sweeps (system ticks). */
#define RELAY_DISCOVERY_POLL_TICKS  100

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

static void send_handshake(int fd)
{
    static const char handshake[] =
        "{\"type\":\"tm4c_connect\",\"device_id\":\"" APP_DEVICE_ID "\"}";
    send(fd, handshake, sizeof(handshake) - 1, 0);
}

void relay_client_task(UArg arg0, UArg arg1)
{
    Mailbox_Handle   mail = (Mailbox_Handle)arg0;
    Semaphore_Handle sem  = (Semaphore_Handle)arg1;

    /* Wait for the link before touching the SimpleLink socket layer. */
    if (!Semaphore_pend(sem, RELAY_LINK_WAIT_TICKS)) {
        uart_log("[relay] timed out waiting for WiFi link; exiting\n");
        return;
    }
    GPIO_write(Board_LED1, Board_LED_OFF);

    udp_discovery_init();

    uint32_t           last_beacon_s = 0;
    relay_app_beacon_t relay         = { 0 };

    for (;;) {
        uint32_t now_s = Seconds_get();

        if ((now_s - last_beacon_s) >= APP_UDP_BEACON_INTERVAL_S) {
            last_beacon_s = now_s;
            udp_send_beacon();

            if (udp_check_for_app_beacon(&relay) && relay.valid) {
                int fd = connect_to_relay_app(relay.ip, relay.port);
                if (fd >= 0) {
                    send_handshake(fd);
                    (void)net_service_session(fd, mail);
                }
                continue;   /* re-discover after the session ends */
            }
        }

        Task_sleep(RELAY_DISCOVERY_POLL_TICKS);
    }
}
