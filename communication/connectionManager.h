/* SimpleLink Wi-Fi Host Driver Header files */
#include <osi.h>
#include <simplelink.h>

#include <stdbool.h>
#include <string.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/WiFi.h>

#include "Board.h"
/*
 Opens WiFi Driver and connects to WiFI AP by adding profile and setting auto connect policy.
 */
void * CM_AddConnectionProfile(UArg arg0);

/*
 *  ======== setStationMode ========
 *  1. Sets WiFi to Station mode
 *  2. Enable DHCP client
 *  3. Set auto connect policy
 */
void CM_ConfigureWiFiParameters(void);

/*
 * Calls sl_WlanGetNetworkList and loops through valid networks and extracts bssid from specified ssid.
 * Should fail if specified ssid isn't found
 */
Sl_WlanNetworkEntry_t CM_ReadAPBssid(_u8* hostName, _u8* apMACAddress);

/*
 * monitor the state of the WiFi connection
 */
 void *CM_connectionStateMonitor(void);



