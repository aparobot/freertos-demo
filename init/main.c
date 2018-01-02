#include "config.h"
#include "system.h"
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "led.h"
#include <stm32f10x.h>

int main()
{
    systemLaunch();
    vTaskStartScheduler();
}

/* For now, the stack depth of IDLE has 88 left. if want add func to here, 
   you should increase it. */
void vApplicationIdleHook(void)
{   /* ATTENTION: all funcs called within here, must not be blocked */

}
