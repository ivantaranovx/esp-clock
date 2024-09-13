
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

typedef struct __attribute__((packed))
{
    char ntp[64];
    char tz[64];
    int day_hour;
    int night_hour;
    int alarm_hour;
    int alarm_min;
    int alarm_flags;
    uint32_t magic;
} SETTINGS;

void settings_load(void);
void settings_save(void);

extern SETTINGS settings;

#endif /* SETTINGS_H */
