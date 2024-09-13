
#include "esp_spi_flash.h"
#include "esp_log.h"

#include "settings.h"
#include "mem.h"
#include "timezone.h"
#include "esp_wifi.h"

#include <string.h>

#define TAG "settings"
#define MAGIC 0x1234dcba

SETTINGS settings;

static const SETTINGS settings_default = {
    .ntp = "pool.ntp.org",
    .tz = "GMT-0",
    .alarm_flags = HR_CHIME,
    .alarm_hour = 7,
    .alarm_min = 30,
    .day_hour = 7,
    .night_hour = 22};

void settings_load(void)
{
    spi_flash_read(SETTINGS_ADDR, &settings, sizeof(settings));
    if (settings.magic == MAGIC)
        return;
    memcpy(&settings, &settings_default, sizeof(settings));
    ESP_LOGI(TAG, "defaults loaded");
}

void settings_save(void)
{
    settings.magic = MAGIC;
    spi_flash_erase_sector(SETTINGS_ADDR / SPI_FLASH_SEC_SIZE);
    spi_flash_write(SETTINGS_ADDR, &settings, sizeof(settings));
    timezone_set(settings.tz);
    ESP_LOGI(TAG, "ntp: %s", settings.ntp);
    ESP_LOGI(TAG, "tz: %s", settings.tz);

    esp_wifi_connect();
}

SETTINGS *settings_get(void)
{
    return &settings;
}
