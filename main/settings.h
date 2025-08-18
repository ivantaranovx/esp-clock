
#ifndef SETTINGS_H
#define SETTINGS_H

#define ALARM_SUN 0x01
#define ALARM_MON 0x02
#define ALARM_TUE 0x04
#define ALARM_WED 0x08
#define ALARM_THU 0x10
#define ALARM_FRI 0x20
#define ALARM_SAT 0x40
#define HR_CHIME 0x80
#define HR_24FMT 0x100

#define SET_STR_SZ 64

typedef struct __attribute__((packed))
{
    char ntp[SET_STR_SZ];
    char tz[SET_STR_SZ];
    uint8_t day_hour;
    uint8_t night_hour;
    uint8_t alarm_hour;
    uint8_t alarm_min;
    uint16_t alarm_flags;
    uint32_t magic;
} SETTINGS;

void settings_load(void);
void settings_save(void);

extern SETTINGS settings;

#endif /* SETTINGS_H */
