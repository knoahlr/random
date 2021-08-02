
#include <stdbool.h>
#include <string.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/WiFi.h>

/* SimpleLink Wi-Fi Host Driver Header files */
#include <osi.h>
#include <simplelink.h>

/* Example/Board Header file */
#include "Board.h"
//#include "connectionManager.h"

#if defined(MSP430WARE) || defined(MSP432WARE)
#define SPI_BIT_RATE    2000000
#elif defined(CC32XXWARE)
#define SPI_BIT_RATE    20000000
#else
#define SPI_BIT_RATE    14000000
#endif

typedef struct
{
    bool ipAcquired;
    bool deviceConnected;
    _u32 ipv4Address;
}ConnectionStatus;

bool deviceConnected = false;
bool ipAcquired = false;
bool smartConfigFlag = false;

ConnectionStatus   connectionState;

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
            connectionState.ipAcquired = true;
            connectionState.ipv4Address = pArgs->EventData.ipAcquiredV4.ip;
            break;

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
            connectionState.deviceConnected = true;
            break;
        case SL_WLAN_DISCONNECT_EVENT:
            connectionState.deviceConnected = false;
            break;
        default:
            break;
    }
}


void CM_ConfigureWiFiParameters(void) {
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
    _u8 country[] = "US";
    _u8 apChannel = 6;
    _i16 wlanSetRC = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_COUNTRY_CODE, sizeof(country), &country[0]);
    wlanSetRC = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_CHANNEL, sizeof(apChannel), &apChannel);

    /* Set connection variables to initial values */
    deviceConnected = false;
    ipAcquired = false;
}


//############################ End of SimpleLink Asynchronous Event Handlers #################################################

Sl_WlanNetworkEntry_t CM_ReadAPBssid(_u8* hostname, _u8* apMACAddress)
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


void * CM_AddConnectionProfile(UArg arg0)
{
    WiFi_Params        wifiParams;
    WiFi_Handle        handle;
    _i8 _hostName[] = "Hidden Gotham Village";
    _i8 _password[] = "BatmanUchiha";

    Semaphore_Handle sem = (Semaphore_Handle)arg0;

    _i16 wlanConnectRC = -123;
    SlSecParams_t securityParameter;
    _u8 apMacAddr[SL_MAC_ADDR_LEN];
    _u8 deviceMACAddress[SL_MAC_ADDR_LEN];
    _u8 deviceMACAddressLen = SL_MAC_ADDR_LEN;

//    SlSecParamsExt_t extSecurityParameter;
//    Sl_WlanNetworkEntry_t specifiedAccessPoint; //Acces point specified by _hostName (SSID)

    securityParameter.Type = SL_SEC_TYPE_WPA_WPA2;
    securityParameter.Key = &_password[0];
    securityParameter.KeyLen = sizeof(_password);

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

    CM_ConfigureWiFiParameters();
//    CM_ReadAPBssid(&_hostName[0], &apMacAddr[0]);

    sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &deviceMACAddressLen, (_u8 *)deviceMACAddress);

    int connectionRetryCounter = 0;
    wlanConnectRC = sl_WlanProfileAdd(&_hostName[0], sizeof(_hostName), &deviceMACAddress[0], &securityParameter, NULL, 7, NULL);

    if (wlanConnectRC >= 0)
    {
        while ((connectionState.deviceConnected != true) || (connectionState.ipAcquired != true))
        {
            connectionRetryCounter ++;
            GPIO_toggle(Board_LED1);
            System_printf("Connection trial: %d.\nrc:%d",connectionRetryCounter, wlanConnectRC);
            Task_sleep(500);
        }
    }
    if(connectionState.deviceConnected && connectionState.ipAcquired)
    {
        GPIO_write(Board_LED1, Board_LED_ON);
        Semaphore_post(sem);
    }
}
