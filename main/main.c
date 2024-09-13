
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "lwip/apps/sntp.h"
#include "driver/gpio.h"

#include "err.h"
#include "opt.h"
#include "settings.h"
#include "wifi.h"
#include "http.h"
#include "display.h"
#include "timezone.h"

#define TAG "main"

static void show_clock(void)
{
    static time_t old = 0;
    time_t now;
    time(&now);
    if (now == old)
        return;
    old = now;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < 100)
        return;
    display_set(DISPLAY_HOURS, timeinfo.tm_hour);
    display_set(DISPLAY_MINUTES, timeinfo.tm_min);

    if (settings.night_hour > settings.day_hour)
        clock_flags.day = (timeinfo.tm_hour >= settings.day_hour) && (timeinfo.tm_hour < settings.night_hour);
    else
        clock_flags.day = (timeinfo.tm_hour < settings.night_hour) || (timeinfo.tm_hour >= settings.day_hour);

    if (timeinfo.tm_sec == 0)
    {
        if ((timeinfo.tm_min == 0) && clock_flags.day && (settings.alarm_flags & HR_CHIME))
            clock_flags.ben = 1;
        if (((timeinfo.tm_hour == settings.alarm_hour) && (timeinfo.tm_min == settings.alarm_min)) && ((1 << timeinfo.tm_wday) & settings.alarm_flags))
            clock_flags.alarm = 1;
    }
}

void app_main()
{
    esp_err_t err;
    ESP_LOGI(TAG, "\n\f%s", APP_NAME);

    settings_load();

    ERR_CHK(gpio_install_isr_service(0));

    ERR_CHK(esp_netif_init());
    ERR_CHK(esp_event_loop_create_default());
    ERR_CHK(nvs_flash_init());
    ERR_CHK(wifi_init());
    ERR_CHK(httpd_init());

    display_init();

    timezone_set(settings.tz);

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, settings.ntp);
    sntp_init();

    for (;;)
    {
        vTaskDelay(100 / portTICK_RATE_MS);
        show_clock();
    }
}
