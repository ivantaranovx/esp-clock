
#include "err.h"
#include "esp_log.h"

typedef struct
{
    wifi_event_t e;
    char *s;
} wifi_event_s;

static const wifi_event_s we[] = {
    {WIFI_EVENT_WIFI_READY, "WiFi ready"},
    {WIFI_EVENT_SCAN_DONE, "finish scanning AP"},
    {WIFI_EVENT_STA_START, "station start"},
    {WIFI_EVENT_STA_STOP, "station stop"},
    {WIFI_EVENT_STA_CONNECTED, "station connected to AP"},
    {WIFI_EVENT_STA_DISCONNECTED, "station disconnected from AP"},
    {WIFI_EVENT_STA_AUTHMODE_CHANGE, "the auth mode of AP connected by station changed"},
    {WIFI_EVENT_STA_BSS_RSSI_LOW, "AP's RSSI crossed configured threshold"},
    {WIFI_EVENT_STA_WPS_ER_SUCCESS, "station wps succeeds in enrollee mode"},
    {WIFI_EVENT_STA_WPS_ER_FAILED, "station wps fails in enrollee mode"},
    {WIFI_EVENT_STA_WPS_ER_TIMEOUT, "station wps timeout in enrollee mode"},
    {WIFI_EVENT_STA_WPS_ER_PIN, "station wps pin code in enrollee mode"},
    {WIFI_EVENT_AP_START, "soft-AP start"},
    {WIFI_EVENT_AP_STOP, "soft-AP stop"},
    {WIFI_EVENT_AP_STACONNECTED, "a station connected to soft-AP"},
    {WIFI_EVENT_AP_STADISCONNECTED, "a station disconnected from soft-AP"},
    {WIFI_EVENT_AP_PROBEREQRECVED, "Receive probe request packet in soft-AP interface"},
    {0, 0}};

char *wifi_errstr(wifi_event_t e)
{
    for (int i = 0; we[i].s; i++)
        if (we[i].e == e)
            return we[i].s;
    return "unknown";
}

typedef struct
{
    ip_event_t e;
    char *s;
} ip_event_s;

static const ip_event_s ie[] = {
    {IP_EVENT_STA_GOT_IP, "station got IP from connected AP"},
    {IP_EVENT_STA_LOST_IP, "station lost IP and the IP is reset to 0"},
    {IP_EVENT_AP_STAIPASSIGNED, "soft-AP assign an IP to a connected station"},
    {IP_EVENT_GOT_IP6, "station or ap or ethernet interface v6IP addr is preferred"},
    {0, 0}};

char *ip_errstr(ip_event_t e)
{
    for (int i = 0; ie[i].s; i++)
        if (ie[i].e == e)
            return ie[i].s;
    return "unknown";
}
    
void dump(void *buf, int len)
{
    printf("dump: %p %d\n", buf, len);
    int l = 0;
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", *((char *)buf++));
        l++;
        if (l > 15)
        {
            l = 0;
            printf("\n");
        }
    }
    if (l > 0)
        printf("\n");
}
