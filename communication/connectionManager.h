/* SimpleLink Wi-Fi Host Driver Header files */
#include <osi.h>
#include <simplelink.h>

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/WiFi.h>

//#include <communication/uart_interface.h>
#include <uart/uart_interface.h>

#include "Board.h"

struct wlan_profile_info
{
    int8_t hostname[MAXIMAL_SSID_LENGTH];
    int8_t password[MAXIMAL_SSID_LENGTH];
    int16_t host_name_len;
    uint8_t pass_len;
    uint32_t priority;
    uint8_t mac_address;
    SlSecParams_t sec_params;
    SlGetSecParamsExt_t sec_ext_params;
};

struct configured_profiles{
    struct wlan_profile_info profile_entries[20];
    uint8_t config_net_count;
};

struct connection_status{
    bool ipAcquired;
    bool deviceConnected;
    _u32 ipv4Address;
    _u32 ipv6Address;
};


/*
 Opens WiFi Driver and connects to WiFI AP by adding profile and setting auto connect policy.
 */
void * CM_connection_mgr(UArg arg0, UArg arg1);


/*
Adds profile of WiFI AP sets auto connect policy.
 */
int16_t CM_add_connection_profile(struct wlan_profile_info *profile);

/*
Find saved profiles
 */
uint8_t CM_load_saved_profiles();

/*
Find saved profiles
 */
void CM_print_configured_profiles();

/*
 *  ======== setStationMode ========
 *  1. Sets WiFi to Station mode
 *  2. Enable DHCP client
 *  3. Set auto connect policy
 */
void CM_configure_wifi_parameters(void);

/*
 * Calls sl_WlanGetNetworkList and loops through valid networks and extracts bssid from specified ssid.
 * Should fail if specified ssid isn't found
 */
Sl_WlanNetworkEntry_t CM_read_accesspoint_bssid(_u8* hostName, _u8* apMACAddress);

/*
 * monitor the state of the WiFi connection
 */
 void *CM_connectionStateMonitor(void);



