
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "wifi.h"
#include "opt.h"
#include "dnsd.h"

#include <string.h>

#define TAG "wifi"
#define SCAN_TIME 1000
#define BTN_AP GPIO_NUM_14
#define AP_TIMEOUT 60

static bool is_connected = false;
static int reconnect = 0;
static bool ap_start_f = false;
static int ap_ttl = 0;

static void start_ap(void)
{
    if (ap_ttl)
        return;
    ap_ttl = AP_TIMEOUT;
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    ESP_LOGI(TAG, "start AP");
}

void wifi_ap_activity(void)
{
    if (ap_ttl)
        ap_ttl = AP_TIMEOUT;
}

static void wifi_scan_start()
{
    esp_wifi_scan_stop();
    wifi_scan_config_t scan_config = {0};
    esp_wifi_scan_start(&scan_config, false);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "connecting to WiFi");
        wifi_scan_start();
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        reconnect = 15;
        if (!is_connected)
            break;
        is_connected = false;
        ESP_LOGI(TAG, "disconnected from WiFi");
        break;
    case WIFI_EVENT_AP_START:
        dnsd_init();
        ESP_LOGI(TAG, "AP started");
        break;
    case WIFI_EVENT_AP_STOP:
        dnsd_stop();
        ESP_LOGI(TAG, "AP stopped");
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "station got ip: %s",
                 ip4addr_ntoa(&event->ip_info.ip));
        is_connected = true;
    }
}

void wifi_tick(void)
{
    if (is_connected && ap_ttl)
    {
        ap_ttl--;
        if (ap_ttl == 0)
        {
            esp_wifi_set_mode(WIFI_MODE_STA);
            ESP_LOGI(TAG, "stop AP");
        }
    }

    if (ap_start_f)
    {
        ap_start_f = false;
        start_ap();
    }

    if (reconnect)
    {
        reconnect--;
        if (reconnect == 0)
            esp_wifi_connect();
    }
}

static const wifi_config_t ap_config_default = {
    .ap = {
        .ssid = APP_NAME,
        .ssid_len = sizeof(APP_NAME) - 1,
        .password = "12345678",
        .max_connection = 1,
        .authmode = WIFI_AUTH_WPA2_PSK}};

bool wifi_is_connected(void)
{
    return is_connected;
}

wifi_ap_record_t *wifi_get_ap_list(uint16_t *ap_num)
{
    *ap_num = 0;
    wifi_ap_record_t *ap_records = 0;
    esp_wifi_scan_get_ap_num(ap_num);
    if (*ap_num == 0)
        goto exit;
    ap_records = pvPortCalloc(*ap_num, sizeof(wifi_ap_record_t));
    if (ap_records == 0)
        goto exit;
    esp_wifi_scan_get_ap_records(ap_num, ap_records);
exit:
    wifi_scan_start();
    return ap_records;
}

void gpio_isr(void *p)
{
    ap_start_f = true;
}

esp_err_t wifi_init()
{
    esp_err_t err;
    ERR_CHK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                       &wifi_event_handler, NULL));
    ERR_CHK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                       &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ERR_RET(esp_wifi_init(&cfg));

    wifi_config_t ap_config;
    ERR_RET(esp_wifi_get_config(ESP_IF_WIFI_AP, &ap_config));
    if (strcmp((char *)&ap_config.ap.ssid, APP_NAME))
    {
        memcpy(&ap_config, &ap_config_default, sizeof(ap_config));
        ERR_RET(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
        ESP_LOGI(TAG, "AP config set default\n");
    }

    ERR_RET(esp_wifi_start());

    start_ap();

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BIT(BTN_AP);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_isr_handler_add(BTN_AP, gpio_isr, 0);

    return ESP_OK;
}
