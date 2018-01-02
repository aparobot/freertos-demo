#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cfassert.h"
#include "system.h"
#include "led.h"

static bool isInit = false;
static void systemTask(void *param);

void systemLaunch()
{
    ASSERT(xTaskCreate(systemTask, (const signed char *)"SYSTEM",
             configMINIMAL_STACK_SIZE << 1, NULL, configMAX_PRIORITIES - 1,  NULL) == pdPASS);
}

void systemInit()
{
    if(isInit)
    {
        return;
    }
    ledInit();
    isInit = true;
}

bool systemTest()
{
    return true;
}


void systemTask(void *param)
{
    systemInit();
    if(systemTest())
    {
        // LOG_DEBUG("system init success(%d FreeBytes)\n", xPortGetFreeHeapSize());
    }
    else
    {
        while(1)
        {
            ledSetGreen(1);
            vTaskDelay(M2T(20));
            ledSetGreen(0);
            vTaskDelay(M2T(20));
        }
    }
    
    //Should never reach this point!
    while(1)
        vTaskDelay(portMAX_DELAY);
}
