
#include <stdlib.h>
#include <time.h>
#include <string.h>

void timezone_set(char *tzname)
{
    setenv("TZ", tzname, 1);
    tzset();
}
