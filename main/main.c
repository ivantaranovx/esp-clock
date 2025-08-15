
#include "esp_netif.h"
#include "esp_event.h"
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
#include "i2c.h"
#include "rtc.h"

#include <string.h>

#define TAG "main"

static bool rtc_sync = false;
static int ntp_check = 0;

#define NTP_CHK_TIMEOUT 3

static void main_task(void *arg)
{
    static time_t now_t = 0;
    static time_t now;
    static struct tm timeinfo;

    for (;; vTaskDelay(100 / portTICK_RATE_MS))
    {
        time(&now);
        if (now == now_t)
            continue;
        now_t = now;

        wifi_tick();

        if (!rtc_sync && (SNTP_SYNC_STATUS_COMPLETED == sntp_get_sync_status()))
        {
            rtc_save();
            rtc_sync = true;
            ntp_check = 0;
        }

        if (ntp_check >= NTP_CHK_TIMEOUT)
        {
            ntp_check = 0;
            rtc_load();
        }

        localtime_r(&now, &timeinfo);

        display_set(DISPLAY_HOURS, timeinfo.tm_hour);
        display_set(DISPLAY_MINUTES, timeinfo.tm_min);

        if (settings.night_hour > settings.day_hour)
            clock_flags.day = (timeinfo.tm_hour >= settings.day_hour) && (timeinfo.tm_hour < settings.night_hour);
        else
            clock_flags.day = (timeinfo.tm_hour < settings.night_hour) || (timeinfo.tm_hour >= settings.day_hour);

        if (timeinfo.tm_sec == 0)
        {
            if (timeinfo.tm_min == 0)
            {
                rtc_sync = false;
                ntp_check++;
                if (clock_flags.day &&
                    (settings.alarm_flags & HR_CHIME))
                    clock_flags.ben = 1;
            }
            if (((timeinfo.tm_hour == settings.alarm_hour) &&
                 (timeinfo.tm_min == settings.alarm_min)) &&
                ((1 << timeinfo.tm_wday) & settings.alarm_flags))
                clock_flags.alarm = 1;
        }
    }
}

void app_main()
{
    esp_err_t err;
    ESP_LOGI(TAG, "%s", APP_NAME);

    ERR_CHK(gpio_install_isr_service(0));
    ERR_CHK(esp_netif_init());
    ERR_CHK(esp_event_loop_create_default());
    ERR_CHK(wifi_init());
    ERR_CHK(httpd_init());
    ERR_CHK(i2c_init());

    rtc_load();

    settings_load();
    timezone_set(settings.tz);

    display_init();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, settings.ntp);
    sntp_init();

    xTaskCreate(main_task, "main_task", 2048, NULL, 10, NULL);
}
