
/* Example/Board Header file */

#include <comms/connection_manager.h>
#include <comms/comms_config.h>
#include <utils/uartstdio.h>
#include <stdio.h>

#if defined(MSP430WARE) || defined(MSP432WARE)
#define SPI_BIT_RATE    2000000
#elif defined(CC32XXWARE)
#define SPI_BIT_RATE    20000000
#else
#define SPI_BIT_RATE    14000000
#endif

const static bool erase_network_list = true;

const static char default_hostname[] = APP_DEFAULT_WIFI_SSID;
const static char default_pass[] = APP_DEFAULT_WIFI_PASS;

static uint8_t ap_mac_addr[SL_MAC_ADDR_LEN];
static uint8_t device_mac_address[SL_MAC_ADDR_LEN];
static uint8_t device_mac_address_len = SL_MAC_ADDR_LEN;
static struct configured_profiles saved_profiles;

static bool connected = false;

bool device_connected = false;
bool ip_acquired = false;
bool smart_config_flag = false;

static struct connection_status connection_state;


/*
 * Structure to hold connection status information
 */
//############################ SimpleLink Asynchronous Event Handlers #################################################

/*
 *  ======== SimpleLinkGeneralEventHandler ========
 *  SimpleLink Host Driver callback for general device errors & events.
 */
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    uart_log("General Event Handler - ID=%d Sender=%d\n\n",
           pDevEvent->EventData.deviceEvent.status,  // status of the general event
           pDevEvent->EventData.deviceEvent.sender); // sender type
    uart_log("General event occurred, Event ID: %x\n", pDevEvent->Event);
}

/*
 *  ======== SimpleLinkHttpServerCallback ========
 *  SimpleLink Host Driver callback for HTTP server events.
 */
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
    SlHttpServerResponse_t *pHttpResponse)
{
    switch (pHttpEvent->Event) {
        case SL_NETAPP_HTTPGETTOKENVALUE_EVENT:
        case SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT:
        default:
            break;
    }
}

/*
 *  ======== SimpleLinkNetAppEventHandler ========
 *  SimpleLink Host Driver callback for asynchronous IP address events.
 */
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pArgs)
{
    if(pArgs == NULL){
        uart_log("Net APP Event Handler callback is NULL");
        return;
    }
    switch (pArgs->Event) {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
            connection_state.ip_acquired = true;
            connection_state.ipv4_address = pArgs->EventData.ipAcquiredV4.ip;
            uart_log("\nIP Address Acquired: %u\n", connection_state.ipv4_address);
            break;
        case SL_NETAPP_IPV6_IPACQUIRED_EVENT:
            connection_state.ip_acquired = true;
            memcpy(connection_state.ipv6_address, pArgs->EventData.ipAcquiredV6.ip, 4);
            uart_log("\nIP Address Acquired: %s\n", connection_state.ipv6_address);
        default:
            break;
    }
}

/*
 *  ======== SimpleLinkSockEventHandler ========
 *  SimpleLink Host Driver callback for socket event indication.
 */
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    switch(pSock->Event) {
        case SL_SOCKET_TX_FAILED_EVENT:
        default:
            break;
    }
}

/*
 *  ======== SimpleLinkWlanEventHandler ========
 *  SimpleLink Host Driver callback for handling WLAN connection or
 *  disconnection events.
 */
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pArgs)
{
    uart_log("WLAN EVENT:%d \n", pArgs->Event);

    switch (pArgs->Event) {
        case SL_WLAN_CONNECT_EVENT:
            connection_state.device_connected = true;
            break;
        case SL_WLAN_DISCONNECT_EVENT:
            connection_state.device_connected = false;
            break;
        default:
            break;
    }
}


void cm_configure_wifi_parameters(void) {
    int           mode;
    int           response;
    unsigned char param;

    uart_log("[wifi] sl_Start (this blocks until the CC3x00 responds)...\n");
    mode = sl_Start(0, 0, 0);
    uart_log("[wifi] sl_Start returned mode=%d\n", mode);
    if (mode < 0) {
        uart_log_fatal("[wifi] FATAL: sl_Start < 0 (NWP did not initialize)\n");
        System_abort("Could not initialize SimpleLink Wi-Fi");
    }

    /* Change network processor to station mode */
    if (mode != ROLE_STA) {
        uart_log("[wifi] not station mode (%d); switching to ROLE_STA...\n", mode);
        sl_WlanSetMode(ROLE_STA);

        /* Restart network processor */
        sl_Stop(BIOS_WAIT_FOREVER);
        mode = sl_Start(0, 0, 0);
        uart_log("[wifi] restart returned mode=%d\n", mode);
        if (mode != ROLE_STA) {
            uart_log_fatal("[wifi] FATAL: still not station mode after restart\n");
            System_abort("Failed to set SimpleLink Wi-Fi to Station mode");
        }
    }

    sl_WlanDisconnect();

    /* Set auto connect policy */
    response = sl_WlanPolicySet(SL_POLICY_CONNECTION,
            SL_CONNECTION_POLICY(1, 0, 0, 1, 0), NULL, 0);
    if (response < 0) {
        uart_log_fatal("[wifi] FATAL: sl_WlanPolicySet failed\n");
        System_abort("Failed to set connection policy to auto");
    }

    /* Enable DHCP client */
    param = 1;
    response = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &param);
    if(response < 0) {
        uart_log_fatal("[wifi] FATAL: sl_NetCfgSet (DHCP enable) failed\n");
        System_abort("Could not enable DHCP client");
    }
    uart_log("[wifi] station mode + DHCP client configured\n");

    /* Set connection variables to initial values */
    device_connected = false;
    ip_acquired = false;
}


//############################ End of SimpleLink Asynchronous Event Handlers #################################################

Sl_WlanNetworkEntry_t cm_read_accesspoint_bssid(_u8* host_name, _u8* ap_mac_address)
{
    Sl_WlanNetworkEntry_t network_entries[20];
    _i8 valid_network_count = sl_WlanGetNetworkList(0, 20, &network_entries[0]);
    _i8 ssid_index = 0;
    _i8 ssid_count = 0;

    // extract hidden access point bssid and save into ap_mac_address
    for(;; ssid_index++)
    {
        if(strcmp((char *)host_name, (char *)network_entries[ssid_index].ssid) == 0)
        {
            memcpy(ap_mac_address, &network_entries[ssid_index].bssid, sizeof(network_entries[ssid_index].bssid));
            break;
        }

        ssid_count = valid_network_count - ssid_index - 1;
        if( ssid_count == 0 || valid_network_count == -1)
        {
             Task_sleep(1000);
             memset(&network_entries[0], 0, sizeof(network_entries)); // clear WiFi network entries.
             valid_network_count = sl_WlanGetNetworkList(0, 20, &network_entries[0]);
             ssid_index = -1;
        }
    }
    return network_entries[ssid_index];
}

void cm_remove_all_connection_profiles(void){

   uint8_t delete_all_index = 255;

   int16_t ret = sl_WlanProfileDel(delete_all_index);
   if(ret < 0) {
       uart_log("Failed to delete profiles\n");
       return;
   }
   uart_log("Removing all saved profiles\n");
}

void cm_print_configured_profiles(void){

    if(saved_profiles.config_net_count){
        uint8_t profile_index = 0;
        uart_log("\nSaved profiles found");
        for(; profile_index < saved_profiles.config_net_count; profile_index++){
            uint8_t profile_mac[6];
            memcpy(profile_mac, saved_profiles.profile_entries[profile_index].mac_address, sizeof(profile_mac));
            uart_log("\t\nSSID: %s,\t\nPassword:%s \t\nMAC: %02x:%02x:%02x:%02x:%02x:%02x\t\nSSID Len: %d\n",
                          (char *)saved_profiles.profile_entries[profile_index].hostname,
                          saved_profiles.profile_entries[profile_index].sec_params.Key,
                          profile_mac[0], profile_mac[1], profile_mac[2],
                          profile_mac[3], profile_mac[4], profile_mac[5],
                          saved_profiles.profile_entries[profile_index].host_name_len);
        }
    }
}

uint8_t cm_load_saved_profiles(void){

   uint8_t profile_index = 0;
   while(1){
       int16_t ret = sl_WlanProfileGet(profile_index, saved_profiles.profile_entries[saved_profiles.config_net_count].hostname,
                                       &saved_profiles.profile_entries[saved_profiles.config_net_count].host_name_len,
                                       saved_profiles.profile_entries[saved_profiles.config_net_count].mac_address,
                                       &saved_profiles.profile_entries[saved_profiles.config_net_count].sec_params,
                                       &saved_profiles.profile_entries[saved_profiles.config_net_count].sec_ext_params,
                                       &saved_profiles.profile_entries[saved_profiles.config_net_count].priority);
       if(ret < 0){
           break;
       } else {
           saved_profiles.config_net_count += 1;
           profile_index += 1;
       }
   }
   return saved_profiles.config_net_count;
}

int16_t cm_add_connection_profile(struct wlan_profile_info *profile)
{
    _i16 wlan_connect_rc = -123;
    uart_log("Adding profile.\nHostname:%s\npassword:%s\n", profile->hostname, profile->sec_params.Key);

    if(profile->sec_params.Type == SL_SEC_TYPE_OPEN) {
        wlan_connect_rc = sl_WlanProfileAdd(profile->hostname, profile->host_name_len, NULL, NULL, NULL, profile->priority, 0);
    } else {
        wlan_connect_rc = sl_WlanProfileAdd(profile->hostname, profile->host_name_len, NULL, &profile->sec_params, NULL, profile->priority, 0);
    }
    return wlan_connect_rc;
}

void cm_connection_mgr(UArg arg0, UArg arg1){

    Semaphore_Handle sem = (Semaphore_Handle)arg0;
    (void)arg1;

    WiFi_Params        wifi_params;
    WiFi_Handle        handle;

    saved_profiles.config_net_count = 0;

    /*
     * Board_LED1 is used as a connection indicator.  It will blink until a
     * connection is establish with the AP.
     */
    GPIO_write(Board_LED1, Board_LED_OFF);

    /* Open WiFi driver */
    uart_log("[wifi] opening driver (Board_WIFI, SPI bitrate=%d)...\n", SPI_BIT_RATE);
    WiFi_Params_init(&wifi_params);
    wifi_params.bitRate = SPI_BIT_RATE;
    handle = WiFi_open(Board_WIFI, Board_WIFI_SPI, NULL, &wifi_params);
    if (handle == NULL) {
        uart_log_fatal("[wifi] FATAL: WiFi_open returned NULL (SPI/driver init failed)\n");
        System_abort("WiFi driver failed to open.");
    }
    uart_log("[wifi] driver open; configuring station mode...\n");

    cm_configure_wifi_parameters();
    uart_log("[wifi] configured; reading MAC...\n");

    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &device_mac_address_len, (_u8 *)device_mac_address);

    // print the device MAC address as a single line
    uart_log("MAC Address-%02x:%02x:%02x:%02x:%02x:%02x\n",
             device_mac_address[0], device_mac_address[1], device_mac_address[2],
             device_mac_address[3], device_mac_address[4], device_mac_address[5]);

    struct wlan_profile_info default_profile = { 0 };
    memcpy(default_profile.hostname, &default_hostname[0], strlen(default_hostname));
    memcpy(default_profile.password, &default_pass[0], strlen(default_pass));
    default_profile.sec_params.Key = &default_profile.password[0];
    default_profile.host_name_len = strlen(default_hostname);
    default_profile.pass_len = strlen(default_pass);
    default_profile.priority = 5;
    default_profile.sec_params.Type = SL_SEC_TYPE_WPA_WPA2;
    default_profile.sec_params.KeyLen = default_profile.pass_len;

    if(erase_network_list)
        cm_remove_all_connection_profiles();

    uint8_t configured_access_point_count = cm_load_saved_profiles();
    int16_t ret = -150;
    if(!configured_access_point_count)
        ret = cm_add_connection_profile(&default_profile);

    uart_log("profile add rc: %d\n", ret);
    configured_access_point_count = cm_load_saved_profiles();

    if(configured_access_point_count)
        uart_log("\n%d configured access points found.", configured_access_point_count);

    cm_print_configured_profiles();

    uint32_t connection_retry_counter = 0;

    for(;;) {

        if(!configured_access_point_count){
            // Log that no network is configured.
            uart_log("No APs configured.");

        } else {

            if ((connection_state.device_connected != true) || (connection_state.ip_acquired != true))
             {
                 connection_retry_counter++;
                 GPIO_toggle(Board_LED1);
                 int16_t ret = sl_WlanConnect(default_profile.hostname, default_profile.host_name_len,
                                              NULL, &default_profile.sec_params, NULL);
                 uart_log("Connection trial: %d. Ret: %d\n", connection_retry_counter, ret);
             } else {
                 // successful connection. Release sem to kick start server
                 if(!connected) {
                     GPIO_write(Board_LED1, Board_LED_ON);
                     Semaphore_post(sem);
                     connected = true;
                 }
             }
        }
        Task_sleep(500);
    }
}
