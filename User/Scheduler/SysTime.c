#include "systime.h"
#include "stm32h7xx_hal.h"

void SysTime_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/*
 * 获取微秒级时间戳
 */
uint32_t micros(void)
{
    return DWT->CYCCNT/(SystemCoreClock/1000000U);
}

/*
 * 获取毫秒级时间戳
 */
uint32_t millis(void)
{
    return micros()/1000U;
}
