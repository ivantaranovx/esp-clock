
#ifndef ERR_H
#define ERR_H

#include "esp_err.h"
#include "esp_wifi_types.h"
#include "tcpip_adapter.h"

#define ERR_CHK(x)                           \
    err = x;                                 \
    if (err != ESP_OK)                       \
    ESP_LOGE(TAG, "ERR: %s in %s: %d: %s\n", \
             esp_err_to_name(err), __ESP_FILE__, __LINE__, __ASSERT_FUNC)

#define ERR_RET(x)     \
    err = x;           \
    if (err != ESP_OK) \
        return err;

#define ERR_EX(x)      \
    err = x;           \
    if (err != ESP_OK) \
        goto exit;

char *wifi_errstr(wifi_event_t e);
char *ip_errstr(ip_event_t e);

void dump(void *buf, int len);

#endif // ERR_H
