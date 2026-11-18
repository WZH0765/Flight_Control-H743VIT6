#include "LED.h"
#include "main.h"
#include "SysTime.h"

static uint8_t  LED_Left   = 0;
static uint32_t LED_Tick   = 0;
static uint16_t LED_Period = 0;

/***************
*  设置闪烁
*
*  形  参：Count 次数,Period 总持续时间 ms
*  说  明：非阻塞
****************/
void LED_Flash(uint8_t Count,uint16_t Period)
{
    LED_Period = Period/(Count*2);
    if(LED_Period == 0) LED_Period = 1;

    LED_Left   = Count*2;
    LED_Tick   = millis();

    HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,GPIO_PIN_SET);
}

void LED_Run(void)
{
    if(LED_Left == 0) return;
    if(millis() - LED_Tick < LED_Period) return;

    LED_Left --;
    LED_Tick = millis();

    if(LED_Left == 0)
    {
        HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,GPIO_PIN_RESET);
        return;
    }

    HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);
}