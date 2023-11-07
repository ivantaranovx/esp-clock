
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_http_server.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "json.h"
#include "http.h"
#include "wifi.h"
#include "mem.h"
#include "settings.h"

#define TAG "http"

#define INDEX_BUF_SZ 512
#define CHUNK_BUF_SZ 128

typedef esp_err_t (*uri_handler)(httpd_req_t *req, char *buf);
static esp_err_t http_send_index(httpd_req_t *req);
static esp_err_t http_send_settings(httpd_req_t *req);
static esp_err_t http_send_state(httpd_req_t *req);
static esp_err_t http_redirect(httpd_req_t *req);
static esp_err_t http_recv_settings(httpd_req_t *req);

#define HTTP_ROOT "/"
#define HTTP_SETTINGS "/settings"
#define HTTP_STATE "/state"
#define HTTP_NOINDEX "<html><body><h1>no index</h1></body></html>"

#define HTTP_CE "Content-Encoding"
#define HTTP_GZIP "gzip"

#define KEY_SSID "ssid"
#define KEY_PASS "pass"
#define KEY_NTP "ntp"
#define KEY_TZ "tz"

static wifi_config_t sta_cfg;

#define NEW_BUF(sz)   \
    pvPortMalloc(sz); \
    if (buf == 0)     \
        return ESP_ERR_NO_MEM;

typedef struct
{
    char *key;
    int sz;
    char *val;
} SET_ITEM;

SET_ITEM set_items[] = {
    {KEY_SSID, sizeof(sta_cfg.sta.ssid), (char *)sta_cfg.sta.ssid},
    {KEY_PASS, sizeof(sta_cfg.sta.password), (char *)sta_cfg.sta.password},
    {KEY_NTP, sizeof(settings.ntp), settings.ntp},
    {KEY_TZ, sizeof(settings.tz), settings.tz},
    {.key = 0}};

static char *bool_to_string(bool b)
{
    return b ? "true" : "false";
}

static esp_err_t http_send_index(httpd_req_t *req)
{
    esp_err_t err;
    uint32_t index_sz;
    ERR_RET(spi_flash_read(INDEX_ADDR, &index_sz, sizeof(index_sz)));
    index_sz = htonl(index_sz);
    if (index_sz == 0xffffffff)
        return httpd_resp_send(req, HTTP_NOINDEX, strlen(HTTP_NOINDEX));
    ERR_RET(httpd_resp_set_hdr(req, HTTP_CE, HTTP_GZIP));
    char *buf = NEW_BUF(INDEX_BUF_SZ);
    size_t addr = INDEX_ADDR + 4;
    while (index_sz)
    {
        int len = INDEX_BUF_SZ;
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
    len = sprintf(buf, "{\"ssid\": \"%s\",\"ntp\": \"%s\",\"tz\": \"%s\",\"ap_list\":[",
                  sta_cfg.sta.ssid,
                  settings.ntp,
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
    len = sprintf(buf, "]}");
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
                (len == 0) ||
                (len >= set_items[i].sz))
                continue;
            memset(set_items[i].val, 0, set_items[i].sz);
            memcpy(set_items[i].val, value, len);
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
        return ESP_ERR_NO_MEM;
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
    int len = sprintf(buf, "{\"result\":\"ok\"}");
    ERR_EX(httpd_resp_send(req, buf, len));
    err = ESP_OK;
exit:
    vPortFree(buf);
    return err;
}

static esp_err_t http_redirect(httpd_req_t *req)
{
    esp_err_t err;
    ERR_RET(httpd_resp_set_status(req, "307 moved"));
    ERR_RET(httpd_resp_set_hdr(req, "Location", HTTP_ROOT));
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

static const httpd_uri_t uri_post = {
    .uri = HTTP_SETTINGS,
    .method = HTTP_POST,
    .handler = http_recv_settings};

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
    ERR_RET(httpd_register_uri_handler(server, &uri_post));
    ESP_LOGI(TAG, "started");
    return ESP_OK;
}
