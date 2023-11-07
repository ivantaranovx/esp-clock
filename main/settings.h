
#ifndef SETTINGS_H
#define SETTINGS_H

typedef struct 
{
    char ntp[64];
    char tz[64];

    uint32_t magic;
}
SETTINGS;

void settings_load(void);
void settings_save(void);

extern SETTINGS settings;

#endif /* SETTINGS_H */
