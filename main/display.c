
#include "driver/gpio.h"
#include "driver/hw_timer.h"
#include "esp_event.h"

#include "display.h"
#include "timer.h"

ESP_EVENT_DEFINE_BASE(TIMER_EVENT);

#include <stdbool.h>
#include <string.h>

static void ap_play(uint8_t grp);
static void ap_pup(void);
static void out_ap(uint8_t b);
static void out_disp(uint8_t b);

#define TIMER_VALUE_US 200
#define EVENT_PERIOD (1000000 / TIMER_VALUE_US)

#define DIGITS 4
#define SEG_OFF 10

#define STROBE GPIO_NUM_12
#define SHIFT GPIO_NUM_15
#define SDATA GPIO_NUM_13
#define AP_CS GPIO_NUM_16

#define AP_CMD_PUP1 0xC5
#define AP_CMD_PLAY 0x55
#define SND_BEN 0
#define SND_ALARM 1

static const uint8_t dk1[4] = {0x04, 0x02, 0, 0};
static const uint8_t dk2[4] = {0, 0, 0x40, 0x80};
static const uint8_t ds1[11] = {0x10, 0x40, 0, 0, 0, 0x20, 0x80, 0x08, 0, 0, 0};
static const uint8_t ds2[11] = {0, 0, 0x02, 0x08, 0x20, 0, 0, 0, 0x04, 0x10, 0};

CLOCK_FLAGS clock_flags;

static uint8_t disp[DIGITS];

static void display_cb(void *arg)
{
    static int tmr = 0;
    static uint8_t dc = 0;
    static bool dd = false;

    uint8_t n = disp[dc];
    if (n > 9)
        n = SEG_OFF;
    if (!clock_flags.day && dd)
    {
        out_disp(0);
        out_disp(0);
    }
    else
    {
        out_disp(dk1[dc] | ds1[n]);
        out_disp(dk2[dc] | ds2[n]);
    }
    if (!dd)
        dc++;
    dc &= 3;
    dd ^= 1;

    gpio_set_level(STROBE, 1);
    os_delay_us(1);
    gpio_set_level(STROBE, 0);

    if (clock_flags.ap_pup)
    {
        clock_flags.ap_pup = 0;
        ap_pup();
    }
    else if (clock_flags.ben)
    {
        clock_flags.ben = 0;
        ap_play(SND_BEN);
    }
    else if (clock_flags.alarm)
    {
        clock_flags.alarm = 0;
        ap_play(SND_ALARM);
    }

    tmr++;
    if (tmr >= EVENT_PERIOD)
    {
        tmr = 0;
        esp_event_post(TIMER_EVENT, 1, NULL, 0, portMAX_DELAY);
    }
}

static void out_disp(uint8_t b)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        gpio_set_level(SDATA, (b & 0x80) ? 1 : 0);
        b <<= 1;
        gpio_set_level(SHIFT, 1);
        os_delay_us(1);
        gpio_set_level(SHIFT, 0);
    }
    gpio_set_level(SDATA, 0);
}

static void out_ap(uint8_t b)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        gpio_set_level(SHIFT, b & 1);
        b >>= 1;
        gpio_set_level(SDATA, 1);
        os_delay_us(1);
        gpio_set_level(SDATA, 0);
    }
}

static void ap_pup(void)
{
    gpio_set_level(AP_CS, 1);
    out_ap(AP_CMD_PUP1);
    gpio_set_level(AP_CS, 0);
    gpio_set_level(SHIFT, 0);
}

static void ap_play(uint8_t grp)
{
    gpio_set_level(AP_CS, 1);
    out_ap(AP_CMD_PLAY);
    out_ap(grp);
    gpio_set_level(AP_CS, 0);
    gpio_set_level(SHIFT, 0);
}

void display_set(DISPLAY_D d, uint16_t v)
{
    switch (d)
    {
    case DISPLAY_HOURS:
        if (v > 23)
            break;
        disp[1] = v % 10;
        disp[0] = v / 10;
        break;
    case DISPLAY_MINUTES:
        if (v > 59)
            break;
        disp[3] = v % 10;
        disp[2] = v / 10;
        break;
    }
}

void display_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(STROBE) | BIT(SDATA) | BIT(SHIFT) | BIT(AP_CS);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    memset(disp, SEG_OFF, sizeof(disp));

    clock_flags.v = 0;
    clock_flags.ap_pup = 1;

    hw_timer_init(display_cb, NULL);
    hw_timer_alarm_us(TIMER_VALUE_US, true);
}
