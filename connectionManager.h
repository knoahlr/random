
/*
 Opens WiFi Driver and connects to WiFI AP by adding profile and setting auto connect policy.
 */
void * CM_AddConnectionProfile(void);

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
Sl_WlanNetworkEntry_t CM_ReadAPBssid(_i8* hostName, _u8* apMACAddress);

/*
 * monitor the state of the WiFi connection
 */
 void *CM_connectionStateMonitor(void);



