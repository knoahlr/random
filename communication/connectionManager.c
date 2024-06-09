
/* Example/Board Header file */

//#include <communication/uart_interface.h>
#include <communication/connectionManager.h>

#if defined(MSP430WARE) || defined(MSP432WARE)
#define SPI_BIT_RATE    2000000
#elif defined(CC32XXWARE)
#define SPI_BIT_RATE    20000000
#else
#define SPI_BIT_RATE    14000000
#endif

const static bool erase_network_list = true;

const static char default_hostname[] = "tatakae";
const static char default_pass[] = "itadoriyuji";

static uint8_t apMacAddr[SL_MAC_ADDR_LEN];
static uint8_t deviceMACAddress[SL_MAC_ADDR_LEN];
static uint8_t deviceMACAddressLen = SL_MAC_ADDR_LEN;
static struct configured_profiles saved_profiles;

bool deviceConnected = false;
bool ipAcquired = false;
bool smartConfigFlag = false;

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
    System_printf("General event occurred, Event ID: %x", pDevEvent->Event);
    System_flush();
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
    switch (pArgs->Event) {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
            connection_state.ipAcquired = true;
            connection_state.ipv4Address = pArgs->EventData.ipAcquiredV4.ip;
            break;
        case SL_NETAPP_IPV6_IPACQUIRED_EVENT:
            connection_state.ipAcquired = true;
            connection_state.ipv6Address = pArgs->EventData.ipAcquiredV6.ip;
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
    switch (pArgs->Event) {
        case SL_WLAN_CONNECT_EVENT:
            connection_state.deviceConnected = true;
            break;
        case SL_WLAN_DISCONNECT_EVENT:
            connection_state.deviceConnected = false;
            break;
        default:
            break;
    }
}


void CM_configure_wifi_parameters(void) {
    int           mode;
    int           response;
    unsigned char param;

    mode = sl_Start(0, 0, 0);
    if (mode < 0) {
        System_abort("Could not initialize SimpleLink Wi-Fi");
    }

    /* Change network processor to station mode */
    if (mode != ROLE_STA) {
        sl_WlanSetMode(ROLE_STA);

        /* Restart network processor */
        sl_Stop(BIOS_WAIT_FOREVER);
        mode = sl_Start(0, 0, 0);
        if (mode != ROLE_STA) {
            System_abort("Failed to set SimpleLink Wi-Fi to Station mode");
        }
    }

    sl_WlanDisconnect();

    /* Set auto connect policy */
    response = sl_WlanPolicySet(SL_POLICY_CONNECTION,
            SL_CONNECTION_POLICY(1, 0, 0, 0, 0), NULL, 0);
    if (response < 0) {
        System_abort("Failed to set connection policy to auto");
    }

    /* Enable DHCP client */
    param = 1;
    response = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE, 1, 1, &param);
    if(response < 0) {
        System_abort("Could not enable DHCP client");
    }

    //set country code and channel
//    _u8 country[] = "US";
//    _u8 apChannel = 6;
//    _i16 wlanSetRC = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE, sizeof(country), &country[0]);
//    _i16 wlanSetRC = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_CHANNEL, sizeof(apChannel), &apChannel);

    /* Set connection variables to initial values */
    deviceConnected = false;
    ipAcquired = false;
}


//############################ End of SimpleLink Asynchronous Event Handlers #################################################

Sl_WlanNetworkEntry_t CM_read_accesspoint_bssid(_u8* hostname, _u8* apMACAddress)
{
    Sl_WlanNetworkEntry_t networkEntries[20];
    _i8 validNetworkCount = sl_WlanGetNetworkList(0, 20, &networkEntries[0]);
    _i8 ssidIndex = 0;
    _i8 ssidCount = 0;

    //extract hidden gotham village bssid and save into apMacAddr
    for(;; ssidIndex++)
    {
        if(strcmp(hostname, networkEntries[ssidIndex].ssid) == 0)
        {
            memcpy(apMACAddress, &networkEntries[ssidIndex].bssid, sizeof(networkEntries[ssidIndex].bssid));
            break;
        }

         ssidCount = validNetworkCount-ssidIndex-1;
        if( ssidCount == 0 || validNetworkCount == -1)
        {
             Task_sleep(1000);
             memset(&networkEntries[0], 0, sizeof(networkEntries)); //clear WiFi network entries.
             validNetworkCount = sl_WlanGetNetworkList(0, 20, &networkEntries[0]); //
             ssidIndex = -1;
        }
    }
    return networkEntries[ssidIndex];
}

void CM_remove_all_connnection_profiles(){

   uint8_t delete_all_index = 255;

   int16_t ret = sl_WlanProfileDel(delete_all_index);
   if(!ret) {
       System_printf("Failed to delete profiles\n");
       System_flush();
       return;
   }
   System_printf("Removing all saved profiles\n");
   System_flush();
}

void CM_print_configured_profiles(){

    if(saved_profiles.config_net_count){
        uint8_t profile_index = 0;
        for(; profile_index < saved_profiles.config_net_count; profile_index++){
            uint8_t profile_mac[6];
            memcpy(&profile_mac, saved_profiles.profile_entries[profile_index].mac_address, 6);
            System_printf("\nSSID: %s,\nPassword:%s \nMAC: %x\nSSID Len: %d\n", (char *)saved_profiles.profile_entries[profile_index].hostname,
                          saved_profiles.profile_entries[profile_index].sec_params.Key,
                          &profile_mac,
                          saved_profiles.profile_entries[profile_index].host_name_len);
        }
        System_flush();
    }
}

uint8_t CM_load_saved_profiles(){

   uint8_t profile_index = 0;
   while(1){
       int16_t ret = sl_WlanProfileGet(profile_index, &saved_profiles.profile_entries[saved_profiles.config_net_count].hostname,
                                       &saved_profiles.profile_entries[saved_profiles.config_net_count].host_name_len,
                                       &saved_profiles.profile_entries[saved_profiles.config_net_count].mac_address,
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

//int16_t CM_add_connection_profile(uint8_t *hostname, uint8_t *password, uint8_t host_name_len, uint8_t pass_len)
int16_t CM_add_connection_profile(struct wlan_profile_info *profile)
{

    _i16 wlanConnectRC = -123;
//    SlSecParams_t securityParameter;
//
//    profile->sec_params.Type = SL_SEC_TYPE_WPA_WPA2;
//    profile->sec_params.Key = &profile->password[0];
//    profile->sec_params.KeyLen = profile->pass_len;
//
    System_printf("Adding profile.\nHostname:%s\npassword:%s\n", profile->hostname, profile->sec_params.Key);

    wlanConnectRC = sl_WlanProfileAdd(profile->hostname, profile->host_name_len, &deviceMACAddress[0], &profile->sec_params, NULL, profile->priority, NULL);

    return wlanConnectRC;
}

void * CM_connection_mgr(UArg arg0, UArg arg1){

    Semaphore_Handle sem = (Semaphore_Handle)arg0;
    Queue_Handle uart_queue_handle = (Queue_Handle)arg1;

    WiFi_Params        wifiParams;
    WiFi_Handle        handle;

    saved_profiles.config_net_count = 0;

    /*
     * Board_LED1 is used as a connection indicator.  It will blink until a
     * connection is establish with the AP.
     */
    GPIO_write(Board_LED1, Board_LED_OFF);

    /* Open WiFi driver */
    WiFi_Params_init(&wifiParams);
    wifiParams.bitRate = SPI_BIT_RATE;
    handle = WiFi_open(Board_WIFI, Board_WIFI_SPI, NULL, &wifiParams);
    if (handle == NULL) {
        System_abort("WiFi driver failed to open.");
    }

    CM_configure_wifi_parameters();
    //CM_read_accesspoint_bssid(&_hostNambe[0], &apMacAddr[0]);

    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &deviceMACAddressLen, (_u8 *)deviceMACAddress);

    //printing MAC Address of device
    System_printf("MAC Address-");
    int byte = 0;
    for(;byte< deviceMACAddressLen;byte++){
        System_printf("%x:",deviceMACAddress[byte]);
    }
    System_printf("\n");
    System_flush();

    struct wlan_profile_info default_profile = { 0 };
    memcpy(default_profile.hostname, &default_hostname[0], strlen(default_hostname));
    memcpy(default_profile.password, &default_pass[0], strlen(default_pass));
    default_profile.sec_params.Key = &default_profile.password[0];
    //default_profile.sec_params.Key = &default_profile.password[0];
    default_profile.host_name_len = strlen(default_hostname); //sizeof(default_hostname);
    default_profile.pass_len = strlen(default_pass); //sizeof(default_pass);
    default_profile.priority = 5;
    default_profile.sec_params.Type = SL_SEC_TYPE_WPA_WPA2;
    default_profile.sec_params.KeyLen = default_profile.pass_len;


//    System_printf("host len %d: \npass len: %d\n", default_profile.host_name_len, default_profile.pass_len);

    if(erase_network_list)
        CM_remove_all_connnection_profiles();

    uint8_t configured_access_point_count = CM_load_saved_profiles();
    int16_t ret = -150;
    if(!configured_access_point_count)
        ret = CM_add_connection_profile(&default_profile);

    System_printf("profile add rc: %d", ret);
    configured_access_point_count = CM_load_saved_profiles();

    if(configured_access_point_count)
        System_printf("%d configured access points found.",configured_access_point_count);
    System_flush();

    CM_print_configured_profiles();

    uint32_t connectionRetryCounter = 0;

    for(;;) {

        if(!configured_access_point_count){
            //Log that no network is configured;
            System_printf("No APs configured.");
            System_flush();

        } else {

            if ((connection_state.deviceConnected != true) || (connection_state.ipAcquired != true))
             {
                 connectionRetryCounter++;
                 GPIO_toggle(Board_LED1);
                 struct log_uart *uart_log = (struct log_uart *)malloc(sizeof(struct log_uart));
                 if(uart_log != NULL) {
                   sprintf(uart_log->data, "Connection trial: %d.\n", connectionRetryCounter);
		   Queue_enqueue(uart_queue_handle,  &uart_log->elem);
                 }
                 int16_t ret = sl_WlanConnect(default_profile.hostname, default_profile.host_name_len,
					      &deviceMACAddress[0], &default_profile.sec_params, NULL);
                 System_printf("Connection trial: %d.\nRet: %d\n",connectionRetryCounter, ret);
             } else {
                 // successful connection. Release sem to kick start server
                 System_printf("Successfully connected to AP.\nServer Task to start...");
                 GPIO_write(Board_LED1, Board_LED_ON);
                 Semaphore_post(sem);
             }
        }
        System_flush();
        Task_sleep(500);
    }

}
