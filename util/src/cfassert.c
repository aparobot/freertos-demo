#include "cfassert.h"
#include "led.h"

void assertFail(char *exp, char *file, int line)
{
    // LOG_ERROR("Assert failed [%s] in file %s, line %d\n", exp, file, line);   
    while (1) {
        ledSet(LED_GREEN, 1);
    }
}
