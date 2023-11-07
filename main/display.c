
#include "driver/gpio.h"
#include "driver/hw_timer.h"
#include "esp_event.h"

#include "display.h"
#include "timer.h"

ESP_EVENT_DEFINE_BASE(TIMER_EVENT);

#include <stdbool.h>
#include <string.h>

#define DIGITS 4

#define SEG_A GPIO_NUM_3
#define SEG_B GPIO_NUM_2
#define SEG_C GPIO_NUM_1
#define SEG_D GPIO_NUM_4
#define SEG_E GPIO_NUM_5
#define SEG_F GPIO_NUM_15
#define SEG_G GPIO_NUM_0

#define DIG_1 GPIO_NUM_16
#define DIG_2 GPIO_NUM_14
#define DIG_3 GPIO_NUM_12
#define DIG_4 GPIO_NUM_13

static const uint8_t digits[DIGITS] = {
    DIG_1, DIG_2, DIG_3, DIG_4};

static const uint16_t translate[] = {
    BIT(SEG_A) | BIT(SEG_B) | BIT(SEG_C) | BIT(SEG_D) | BIT(SEG_E) | BIT(SEG_F),
    BIT(SEG_B) | BIT(SEG_C),
    BIT(SEG_A) | BIT(SEG_B) | BIT(SEG_D) | BIT(SEG_E) | BIT(SEG_G),
    BIT(SEG_A) | BIT(SEG_B) | BIT(SEG_C) | BIT(SEG_D) | BIT(SEG_G),
    BIT(SEG_B) | BIT(SEG_C) | BIT(SEG_F) | BIT(SEG_G),
    BIT(SEG_A) | BIT(SEG_C) | BIT(SEG_D) | BIT(SEG_F) | BIT(SEG_G),
    BIT(SEG_A) | BIT(SEG_C) | BIT(SEG_D) | BIT(SEG_E) | BIT(SEG_F) | BIT(SEG_G),
    BIT(SEG_A) | BIT(SEG_B) | BIT(SEG_C),
    BIT(SEG_A) | BIT(SEG_B) | BIT(SEG_C) | BIT(SEG_D) | BIT(SEG_E) | BIT(SEG_F) | BIT(SEG_G),
    BIT(SEG_A) | BIT(SEG_B) | BIT(SEG_C) | BIT(SEG_D) | BIT(SEG_F) | BIT(SEG_G),
    0, BIT(SEG_G)};

#define DIG_BLANK 10
#define DIG_MINUS 11

static uint8_t display[DIGITS];

static void display_cb(void *arg)
{
    static int tmr = 0;
    static uint8_t dig = 0;

    for (int i = 0; i < DIGITS; i++)
        gpio_set_level(digits[i], 1);

    uint16_t d = 0;
    if (display[dig] < sizeof(translate))
        d = translate[display[dig]];

    gpio_set_level(SEG_A, (d & BIT(SEG_A)) ? 1 : 0);
    gpio_set_level(SEG_B, (d & BIT(SEG_B)) ? 1 : 0);
    gpio_set_level(SEG_C, (d & BIT(SEG_C)) ? 1 : 0);
    gpio_set_level(SEG_D, (d & BIT(SEG_D)) ? 1 : 0);
    gpio_set_level(SEG_E, (d & BIT(SEG_E)) ? 1 : 0);
    gpio_set_level(SEG_F, (d & BIT(SEG_F)) ? 1 : 0);
    gpio_set_level(SEG_G, (d & BIT(SEG_G)) ? 1 : 0);

    gpio_set_level(digits[dig], 0);

    dig++;
    if (dig >= DIGITS)
        dig = 0;

    tmr++;
    if (tmr >= 1000)
    {
        tmr = 0;
        esp_event_post(TIMER_EVENT, 1, NULL, 0, portMAX_DELAY);
    }
}

void display_set(DISPLAY_D d, uint16_t v)
{
    switch (d)
    {
    case DISPLAY_HOURS:
        if (v > 23)
            break;
        display[1] = v % 10;
        display[0] = v / 10;
        if (display[0] == 0)
            display[0] = DIG_BLANK;
        break;
    case DISPLAY_MINUTES:
        if (v > 59)
            break;
        display[3] = v % 10;
        display[2] = v / 10;
        break;
    }
}

void display_init()
{
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(SEG_A) | BIT(SEG_B) | BIT(SEG_C) | BIT(SEG_D) | BIT(SEG_E) | BIT(SEG_F) | BIT(SEG_G) | BIT(DIG_1) | BIT(DIG_2) | BIT(DIG_3) | BIT(DIG_4);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    memset(display, DIG_MINUS, sizeof(display));

    hw_timer_init(display_cb, NULL);
    hw_timer_alarm_us(1000, true);
}
