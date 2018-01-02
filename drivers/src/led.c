#include "led.h"

#include <stdbool.h>
#include "stm32f10x_conf.h"

/*FreeRtos includes*/
#include "FreeRTOS.h"
#include "task.h"

static bool isInit = false;
static void ledTask(void *param);
static GPIO_TypeDef* ledPorts[] = {
    LED_GPIO_GREEN_PORT
};

static unsigned int ledPins[] = {
    LED_GPIO_GREEN
};


void ledInit(void)
{
    int i;
    GPIO_InitTypeDef GPIO_InitStructure;
    
    if(isInit)
    {
        return;
    }

    RCC_APB2PeriphClockCmd(LED_GPIO_PERIF, ENABLE );
    for(i = 0; i < LED_NUM; i++) // DO MAKE SURE LED_NUM is legal
    {
        GPIO_InitStructure.GPIO_Pin = ledPins[i];
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
        GPIO_Init(ledPorts[i], &GPIO_InitStructure);
    }
    
    ledSet(LED_GREEN, 0);
    
    xTaskCreate(ledTask, (const signed char *)"ledBlink",  configMINIMAL_STACK_SIZE,  NULL,  1,  NULL);
    
    isInit = true;
}

bool ledTest(void)
{
    return isInit;
}


void ledSet(led_t led, bool value)
{
    if ((int)led >= LED_NUM)
    {
        return;
    }

    if(value)
    {
        GPIO_ResetBits(ledPorts[led], ledPins[led]);
    }
    else
    {
        GPIO_SetBits(ledPorts[led], ledPins[led]);
    }
}


void ledTask(void *param)
{
    while(1)
    {
        ledSetGreen(1);
        vTaskDelay(M2T(INTERVAL));
        ledSetGreen(0);
        vTaskDelay(M2T(INTERVAL));
    }
}

