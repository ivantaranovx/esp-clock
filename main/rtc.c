
#include "FreeRTOS.h"
#include "task.h"

#include "esp_log.h"
#include "rtc.h"
#include "i2c.h"

#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#define TAG "rtc"

#define DS1307_ADDR 0xD0
#define DS1307_REG_START 0x00
#define DS1307_RAM_START 0x08
#define DS1307_PULSE_OUT 1

typedef struct __attribute__((packed))
{
    unsigned ones : 4;
    unsigned tens : 3;
    unsigned ch : 1;
} ds1307_reg_sec_t;

typedef struct __attribute__((packed))
{
    unsigned ones : 4;
    unsigned tens : 3;
    unsigned : 1;
} ds1307_reg_min_t;

typedef struct __attribute__((packed))
{
    unsigned ones : 4;
    unsigned tens : 2;
    unsigned h12 : 1;
    unsigned : 1;
} ds1307_reg_hour_t;

typedef struct __attribute__((packed))
{
    unsigned ones : 4;
    unsigned tens : 2;
    unsigned : 2;
} ds1307_reg_date_t;

typedef struct __attribute__((packed))
{
    unsigned ones : 4;
    unsigned tens : 1;
    unsigned : 3;
} ds1307_reg_mon_t;

typedef struct __attribute__((packed))
{
    unsigned ones : 4;
    unsigned tens : 4;
} ds1307_reg_year_t;

typedef struct __attribute__((packed))
{
    unsigned rs0 : 1;
    unsigned rs1 : 1;
    unsigned : 1;
    unsigned : 1;
    unsigned sqwe : 1;
    unsigned : 1;
    unsigned : 1;
    unsigned out : 1;
} ds1307_reg_ctl_t;

typedef struct
{
    ds1307_reg_sec_t sec;
    ds1307_reg_min_t min;
    ds1307_reg_hour_t hour;
    uint8_t day;
    ds1307_reg_date_t date;
    ds1307_reg_mon_t mon;
    ds1307_reg_year_t year;
    ds1307_reg_ctl_t ctl;
} ds1307_reg_t;

void rtc_load(void)
{
    ds1307_reg_t ds1307_regs;
    struct tm timeinfo;
    struct timeval tv;

    for (;;)
    {
        if (ESP_OK == i2c_read(DS1307_ADDR, DS1307_REG_START, &ds1307_regs, sizeof(ds1307_regs)))
            break;
        vTaskDelay(10 / portTICK_RATE_MS);
    }

    if (ds1307_regs.sec.ch)
    {
        memset(&ds1307_regs, 0, sizeof(ds1307_regs));
        ds1307_regs.ctl.sqwe = DS1307_PULSE_OUT;

        ds1307_regs.date.ones = 1;
        ds1307_regs.mon.ones = 1;
        ds1307_regs.year.tens = 2;
        ds1307_regs.year.ones = 5;
        i2c_write(DS1307_ADDR, DS1307_REG_START, &ds1307_regs, sizeof(ds1307_regs));
    }

    timeinfo.tm_year = (ds1307_regs.year.tens * 10) + ds1307_regs.year.ones;
    timeinfo.tm_mon = (ds1307_regs.mon.tens * 10) + ds1307_regs.mon.ones;
    timeinfo.tm_mday = (ds1307_regs.date.tens * 10) + ds1307_regs.date.ones;

    timeinfo.tm_hour = (ds1307_regs.hour.tens * 10) + ds1307_regs.hour.ones;
    timeinfo.tm_min = (ds1307_regs.min.tens * 10) + ds1307_regs.min.ones;
    timeinfo.tm_sec = (ds1307_regs.sec.tens * 10) + ds1307_regs.sec.ones;

    timeinfo.tm_year += 100;
    timeinfo.tm_isdst = 0;

    tv.tv_sec = mktime(&timeinfo);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    ESP_LOGI(TAG, "load %ld %d-%d-%d %d:%d:%d %d\n",
             tv.tv_sec,
             timeinfo.tm_year,
             timeinfo.tm_mon,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec,
             timeinfo.tm_wday);
}

void rtc_save(void)
{
    time_t now;
    struct tm timeinfo;
    ds1307_reg_t ds1307_regs;

    time(&now);
    gmtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "save %ld %d-%d-%d %d:%d:%d %d\n",
             now,
             timeinfo.tm_year,
             timeinfo.tm_mon,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec,
             timeinfo.tm_wday);

    memset(&ds1307_regs, 0, sizeof(ds1307_regs));
    ds1307_regs.ctl.sqwe = DS1307_PULSE_OUT;

    timeinfo.tm_year %= 100;
    ds1307_regs.year.tens = timeinfo.tm_year / 10;
    ds1307_regs.year.ones = timeinfo.tm_year % 10;
    ds1307_regs.mon.tens = timeinfo.tm_mon / 10;
    ds1307_regs.mon.ones = timeinfo.tm_mon % 10;
    ds1307_regs.date.tens = timeinfo.tm_mday / 10;
    ds1307_regs.date.ones = timeinfo.tm_mday % 10;
    ds1307_regs.day = timeinfo.tm_wday + 1;
    ds1307_regs.hour.tens = timeinfo.tm_hour / 10;
    ds1307_regs.hour.ones = timeinfo.tm_hour % 10;
    ds1307_regs.min.tens = timeinfo.tm_min / 10;
    ds1307_regs.min.ones = timeinfo.tm_min % 10;
    ds1307_regs.sec.tens = timeinfo.tm_sec / 10;
    ds1307_regs.sec.ones = timeinfo.tm_sec % 10;

    i2c_write(DS1307_ADDR, DS1307_REG_START, &ds1307_regs, sizeof(ds1307_regs));
}
