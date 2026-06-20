/* SimpleLink Wi-Fi Host Driver Header files */
#include <osi.h>
#include <simplelink.h>

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/WiFi.h>

#include <comms/uart_interface.h>

#include "Board.h"

#define CM_PROFILE_INVALID_ARG (-4)
#define CM_MAX_WIFI_PASSPHRASE_LENGTH 64

struct wlan_profile_info
{
    int8_t hostname[MAXIMAL_SSID_LENGTH + 1];
    int8_t password[CM_MAX_WIFI_PASSPHRASE_LENGTH + 1];
    int16_t host_name_len;
    uint8_t pass_len;
    uint32_t priority;
    uint8_t mac_address[SL_MAC_ADDR_LEN];
    SlSecParams_t sec_params;
    SlGetSecParamsExt_t sec_ext_params;
};

struct configured_profiles{
    struct wlan_profile_info profile_entries[20];
    uint8_t config_net_count;
};

struct connection_status{
    bool ip_acquired;
    bool device_connected;
    _u32 ipv4_address;
    _u32 ipv6_address[4];
};


/*
 Opens WiFi Driver and connects to WiFI AP by adding profile and setting auto connect policy.
 */
void cm_connection_mgr(UArg arg0, UArg arg1);


/*
 * Store a WLAN profile on the NWP (sl_WlanProfileAdd). Returns the stored index
 * (>=0) or a negative error. Adding an existing profile (same SSID/MAC/sec type)
 * replaces it at the same index. Used at boot for the default and, later, by the
 * console to add profiles on the fly.
 */
int16_t cm_add_connection_profile(struct wlan_profile_info *profile);

/* Store a WPA/WPA2 profile from plain console arguments. */
int16_t cm_add_wpa2_profile(const char *ssid, const char *passkey);

/*
 * Read the profiles stored on the NWP into saved_profiles. Note the NWP does not
 * return profile keys. Returns the number of profiles found.
 */
uint8_t cm_load_saved_profiles(void);

/* Print the currently loaded profiles to the console. */
void cm_print_configured_profiles(void);

/* Delete every stored profile (sl_WlanProfileDel(255)). For console use. */
void cm_remove_all_connection_profiles(void);

/*
 *  ======== cm_configure_wifi_parameters ========
 *  1. Sets WiFi to Station mode
 *  2. Enable DHCP client
 *  3. Sets the auto-connect policy
 */
void cm_configure_wifi_parameters(void);
