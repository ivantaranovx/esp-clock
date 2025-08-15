
#ifndef DISPLAY_H
#define DISPLAY_H

typedef enum {
    DISPLAY_HOURS,
    DISPLAY_MINUTES
}
DISPLAY_D;

void display_init(void);
void display_set(DISPLAY_D d, unsigned v);

typedef union
{
    struct
    {
        unsigned day : 1;
        unsigned ap_pup : 1;
        unsigned alarm : 1;
        unsigned ben : 1;
    };
    uint8_t v;
} CLOCK_FLAGS;

extern CLOCK_FLAGS clock_flags;

#endif /* DISPLAY_H */
