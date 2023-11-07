
#ifndef DISPLAY_H
#define DISPLAY_H

typedef enum {
    DISPLAY_HOURS,
    DISPLAY_MINUTES
}
DISPLAY_D;

void display_init(void);
void display_set(DISPLAY_D d, uint16_t v);

#endif /* DISPLAY_H */
