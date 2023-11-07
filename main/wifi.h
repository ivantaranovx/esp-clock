
#ifndef WIFI_H
#define WIFI_H

#include "err.h"

esp_err_t wifi_init();
wifi_ap_record_t *wifi_get_ap_list(uint16_t *ap_num);
bool wifi_is_connected(void);

#endif /* WIFI_H */
