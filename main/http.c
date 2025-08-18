
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_server.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "rom/crc.h"

#include "json.h"
#include "http.h"
#include "wifi.h"
#include "mem.h"
#include "settings.h"
#include "opt.h"

#define TAG "http"

#define CHUNK_BUF_SZ 128

typedef esp_err_t (*uri_handler)(httpd_req_t *req, char *buf);
static esp_err_t http_send_index(httpd_req_t *req);
static esp_err_t http_send_settings(httpd_req_t *req);
static esp_err_t http_send_state(httpd_req_t *req);
static esp_err_t http_redirect(httpd_req_t *req);
static esp_err_t http_recv_settings(httpd_req_t *req);
static esp_err_t http_recv_upgrade(httpd_req_t *req);

#define HTTP_ROOT "/"
#define HTTP_REDIRECT "/"
#define HTTP_SETTINGS "/settings"
#define HTTP_STATE "/state"
#define HTTP_NOINDEX "<html><body><h1>no index</h1></body></html>"
#define HTTP_UPGRADE "/upgrade"

#define HTTP_CE "Content-Encoding"
#define HTTP_GZIP "gzip"

#define KEY_SSID "ssid"
#define KEY_PASS "pass"
#define KEY_NTP "ntp"
#define KEY_VER "ver"
#define KEY_TZ "tz"
#define KEY_DAY_HOUR "day_hour"
#define KEY_NIGHT_HOUR "night_hour"
#define KEY_ALARM_HOUR "alarm_hour"
#define KEY_ALARM_MIN "alarm_min"
#define KEY_ALARM_FLAGS "alarm_flags"

#define RES_OK "{\"result\":\"ok\"}"
#define UP_SIGN "eclk"

static wifi_config_t sta_cfg;

#define NEW_BUF(sz)   \
    pvPortMalloc(sz); \
    if (buf == 0)     \
        return ESP_ERR_NO_MEM;

typedef struct
{
    char *key;
    int sz;
    void *val;
} SET_ITEM;

static SET_ITEM set_items[] = {
    {KEY_SSID, sizeof(sta_cfg.sta.ssid), (char *)sta_cfg.sta.ssid},
    {KEY_PASS, sizeof(sta_cfg.sta.password), (char *)sta_cfg.sta.password},
    {KEY_NTP, sizeof(settings.ntp), settings.ntp},
    {KEY_TZ, sizeof(settings.tz), settings.tz},
    {KEY_DAY_HOUR, sizeof(settings.day_hour), &settings.day_hour},
    {KEY_NIGHT_HOUR, sizeof(settings.night_hour), &settings.night_hour},
    {KEY_ALARM_HOUR, sizeof(settings.alarm_hour), &settings.alarm_hour},
    {KEY_ALARM_MIN, sizeof(settings.alarm_min), &settings.alarm_min},
    {KEY_ALARM_FLAGS, sizeof(settings.alarm_flags), &settings.alarm_flags},
    {.key = 0}};

typedef struct
{
    uint32_t sign;
    uint32_t addr;
    uint32_t crc;
    uint32_t len;
} UPD_HEAD;

static UPD_HEAD upd_head;

static char *bool_to_string(bool b)
{
    return b ? "true" : "false";
}

static esp_err_t http_send_index(httpd_req_t *req)
{
    esp_err_t err;
    uint32_t index_sz;
    ERR_RET(spi_flash_read(INDEX_ADDR, &index_sz, sizeof(index_sz)));
    if (index_sz == 0xffffffff)
        return httpd_resp_send(req, HTTP_NOINDEX, strlen(HTTP_NOINDEX));
    ERR_RET(httpd_resp_set_hdr(req, HTTP_CE, HTTP_GZIP));
    char *buf = NEW_BUF(CHUNK_BUF_SZ);
    size_t addr = INDEX_ADDR + 4;
    while (index_sz)
    {
        int len = CHUNK_BUF_SZ;
        if (len > index_sz)
            len = index_sz;
        ERR_EX(spi_flash_read(addr, buf, len));
        ERR_EX(httpd_resp_send_chunk(req, buf, len));
        index_sz -= len;
        addr += len;
    }
    ERR_EX(httpd_resp_send_chunk(req, NULL, 0));
    err = ESP_OK;
exit:
    vPortFree(buf);
    return err;
}

static esp_err_t http_send_settings(httpd_req_t *req)
{
    esp_err_t err;
    wifi_ap_record_t *ap_records = 0;
    int len;
    ERR_RET(esp_wifi_get_config(ESP_IF_WIFI_STA, &sta_cfg));
    ERR_RET(httpd_resp_set_type(req, HTTPD_TYPE_JSON));
    char *buf = NEW_BUF(CHUNK_BUF_SZ);
    len = sprintf(buf, "{\"ssid\": \"%s\""
                       ",\"ntp\": \"%s\""
                       ",\"ver\": \"%s\""
                       ",\"tz\": \"%s\""
                       ",\"ap_list\":[",
                  sta_cfg.sta.ssid,
                  settings.ntp,
                  APP_VER,
                  settings.tz);
    ERR_EX(httpd_resp_send_chunk(req, buf, len));
    uint16_t ap_num = 0;
    ap_records = wifi_get_ap_list(&ap_num);
    bool b = false;
    for (int i = 0; i < ap_num; i++)
    {
        if (strlen((char *)ap_records[i].ssid) == 0)
            continue;
        len = sprintf(buf, "%c{\"ssid\": \"%s\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}",
                      b ? ',' : ' ',
                      ap_records[i].ssid,
                      ap_records[i].bssid[0],
                      ap_records[i].bssid[1],
                      ap_records[i].bssid[2],
                      ap_records[i].bssid[3],
                      ap_records[i].bssid[4],
                      ap_records[i].bssid[5]);
        ERR_EX(httpd_resp_send_chunk(req, buf, len));
        b = true;
    }
    len = sprintf(buf, "]"
                       ",\"day_time\":\"%02d:00\""
                       ",\"night_time\":\"%02d:00\""
                       ",\"alarm_time\":\"%02d:%02d\""
                       ",\"alarm_flags\":%d"
                       "}",
                  settings.day_hour,
                  settings.night_hour,
                  settings.alarm_hour,
                  settings.alarm_min,
                  settings.alarm_flags);
    ERR_EX(httpd_resp_send_chunk(req, buf, len));

    ERR_EX(httpd_resp_send_chunk(req, 0, 0));
    err = ESP_OK;
exit:
    if (ap_records)
        vPortFree(ap_records);
    vPortFree(buf);
    return err;
}

static esp_err_t http_send_state(httpd_req_t *req)
{
    esp_err_t err;
    char *buf = NEW_BUF(CHUNK_BUF_SZ);
    int len = sprintf(buf, "{\"connected\": %s}",
                      bool_to_string(wifi_is_connected()));
    ERR_EX(httpd_resp_send(req, buf, len));
    wifi_ap_activity();
    err = ESP_OK;
exit:
    vPortFree(buf);
    return err;
}

static void settings_cb(char *key, char *value, int level)
{
    esp_err_t err;
    int len = strlen(value);
    switch (level)
    {
    case 1:
        for (int i = 0; set_items[i].key; i++)
        {
            if (strcmp(key, set_items[i].key) ||
                (len == 0))
                continue;
            switch (set_items[i].sz)
            {
            case 1:
                *((uint8_t *)set_items[i].val) = atoi(value);
                break;
            case 2:
                *((uint16_t *)set_items[i].val) = atoi(value);
                break;
            case 3:
            case 4:
                break;
            default:
                if (len >= set_items[i].sz)
                    break;
                memset(set_items[i].val, 0, set_items[i].sz);
                memcpy(set_items[i].val, value, len);
                break;
            }
            break;
        }
        break;
    case 0:
        ERR_CHK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_cfg));
        ERR_CHK(esp_wifi_disconnect());
        settings_save();
        break;
    }
}

static esp_err_t http_recv_settings(httpd_req_t *req)
{
    esp_err_t err;
    char *buf = NEW_BUF(CHUNK_BUF_SZ);
    json_parse_context *jctx = pvPortMalloc(sizeof(json_parse_context));
    if (jctx == 0)
    {
        err = ESP_ERR_NO_MEM;
        goto exit;
    }
    json_init(jctx);
    jctx->func = settings_cb;
    for (;;)
    {
        int len = httpd_req_recv(req, buf, CHUNK_BUF_SZ);
        if (len <= 0)
            break;
        json_parse(jctx, buf, len);
    }
    vPortFree(jctx);
    ERR_EX(httpd_resp_set_type(req, HTTPD_TYPE_JSON));
    ERR_EX(httpd_resp_send(req, RES_OK, strlen(RES_OK)));
    err = ESP_OK;
exit:
    vPortFree(buf);
    return err;
}

static esp_err_t recv_upgrade_block(httpd_req_t *req)
{
    esp_err_t err = ESP_OK;
    int len = httpd_req_recv(req, (char *)&upd_head, sizeof(upd_head));
    if (len < sizeof(upd_head))
        return ESP_ERR_NOT_FOUND;
    if (memcmp(UP_SIGN, &upd_head, strlen(UP_SIGN)))
    {
        ESP_LOGI(TAG, "invalid signature");
        return ESP_FAIL;
    }
    upd_head.addr &= FLASH_ADDR_MASK;
    for (;;)
    {
        if ((upd_head.addr == UP_MAIN_ADDR) && (upd_head.len < UP_MAIN_LEN))
            break;
        if ((upd_head.addr == UP_INDEX_ADDR) && (upd_head.len < UP_INDEX_LEN))
            break;
        ESP_LOGI(TAG, "invalid addr");
        return ESP_FAIL;
    }
    char *buf = NEW_BUF(CHUNK_BUF_SZ);
    uint32_t rx_len = ((upd_head.len & FLASH_ADDR_MASK) | FLASH_BLOCK_MASK) + 1;
    ESP_LOGI(TAG, "flash erase: %08X, len: %08X",
             upd_head.addr, rx_len);
    spi_flash_erase_range(upd_head.addr, rx_len);
    ESP_LOGI(TAG, "flash: start writing");
    rx_len = upd_head.len;
    uint32_t crc = 0;
    for (;;)
    {
        len = httpd_req_recv(req, buf, (rx_len > CHUNK_BUF_SZ) ? CHUNK_BUF_SZ : rx_len);
        if (len <= 0)
        {
            err = ESP_FAIL;
            break;
        }
        spi_flash_write(upd_head.addr, buf, len);
        spi_flash_read(upd_head.addr, buf, len);
        crc = crc32_le(crc, (uint8_t *)buf, len);
        upd_head.addr += len;
        rx_len -= len;
        if (rx_len == 0)
            break;
    }
    if (rx_len)
    {
        ESP_LOGI(TAG, "invalid length");
        err = ESP_FAIL;
        goto exit;
    }
    if (upd_head.crc ^ crc)
    {
        ESP_LOGI(TAG, "invalid CRC");
        err = ESP_FAIL;
        goto exit;
    }
    ESP_LOGI(TAG, "flash: ok");
exit:
    vPortFree(buf);
    return err;
}

static esp_err_t http_recv_upgrade(httpd_req_t *req)
{
    esp_err_t err = ESP_OK;
    for (;;)
    {
        if (recv_upgrade_block(req) != ESP_OK)
            break;
    }
    ERR_EX(httpd_resp_set_type(req, HTTPD_TYPE_JSON));
    ERR_EX(httpd_resp_send(req, RES_OK, strlen(RES_OK)));
exit:
    return err;
}

static esp_err_t http_redirect(httpd_req_t *req)
{
    esp_err_t err;
    ERR_RET(httpd_resp_set_status(req, "307 moved"));
    ERR_RET(httpd_resp_set_hdr(req, "Location", HTTP_REDIRECT));
    return httpd_resp_send(req, 0, 0);
}

static const httpd_uri_t uri_get_index = {
    .uri = HTTP_ROOT,
    .method = HTTP_GET,
    .handler = http_send_index};

static const httpd_uri_t uri_get_settings = {
    .uri = HTTP_SETTINGS,
    .method = HTTP_GET,
    .handler = http_send_settings};

static const httpd_uri_t uri_get_state = {
    .uri = HTTP_STATE,
    .method = HTTP_GET,
    .handler = http_send_state};

static const httpd_uri_t uri_get_unknown = {
    .uri = "*",
    .method = HTTP_GET,
    .handler = http_redirect};

static const httpd_uri_t uri_post_settings = {
    .uri = HTTP_SETTINGS,
    .method = HTTP_POST,
    .handler = http_recv_settings};

static const httpd_uri_t uri_post_upgrade = {
    .uri = HTTP_UPGRADE,
    .method = HTTP_POST,
    .handler = http_recv_upgrade};

esp_err_t httpd_init(void)
{
    esp_err_t err;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ERR_RET(httpd_start(&server, &config));
    ERR_RET(httpd_register_uri_handler(server, &uri_get_index));
    ERR_RET(httpd_register_uri_handler(server, &uri_get_settings));
    ERR_RET(httpd_register_uri_handler(server, &uri_get_state));
    ERR_RET(httpd_register_uri_handler(server, &uri_get_unknown));
    ERR_RET(httpd_register_uri_handler(server, &uri_post_settings));
    ERR_RET(httpd_register_uri_handler(server, &uri_post_upgrade));
    ESP_LOGI(TAG, "started");
    return ESP_OK;
}
