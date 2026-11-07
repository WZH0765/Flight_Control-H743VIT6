#include "tim.h"
#include "Motor.h"
#include <string.h>

__attribute__((section(".dma_buffer")))
uint32_t Motor_Buffer[2][MOTOR_BUF_LEN * 4];

void Motor_Init(void)
{
    memset(Motor_Buffer,0,sizeof(Motor_Buffer));

    Motor_Stop();

    TIM2->DCR = (3 << 8) | (0x34 >> 2);
    TIM3->DCR = (3 << 8) | (0x34 >> 2);

    HAL_DMA_Start(&hdma_tim3_up,(uint32_t)Motor_Buffer[0],(uint32_t)&TIM3->DMAR,MOTOR_BUF_LEN * 4);
    HAL_DMA_Start(&hdma_tim2_up,(uint32_t)Motor_Buffer[1],(uint32_t)&TIM2->DMAR,MOTOR_BUF_LEN * 4);

    __HAL_TIM_ENABLE_DMA(&htim2,TIM_DMA_UPDATE);
    __HAL_TIM_ENABLE_DMA(&htim3,TIM_DMA_UPDATE);

    HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_4);
    HAL_TIM_PWM_Start(&htim2,TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2,TIM_CHANNEL_2);
}

/***************
*  设置单路油门
*
*  形  参：Index 0~3,Throttle 0~1000
****************/
void Motor_Set(uint8_t Index,uint16_t Throttle)
{
    uint16_t DShot = 0;
    uint16_t Check = 0;
    uint16_t Frame = 0;
    uint16_t Packet = 0;
    uint8_t  Timer = Index >> 1;
    uint8_t  Slot  = (uint8_t)((Index & 1) + (Timer == 0 ? 2 : 0));

    if(Throttle >= 1000)   DShot = MOTOR_MAX;
    else if(Throttle > 0)  DShot = MOTOR_MIN + (uint16_t)((uint32_t)Throttle*(MOTOR_MAX - MOTOR_MIN)/1000);

    Packet = (uint16_t)(DShot << 1);
    Check  = (Packet ^ (Packet >> 4) ^ (Packet >> 8)) & 0x0F;
    Frame  = (uint16_t)((Packet << 4) | Check);

    for(uint8_t i = 0;i < 16;i ++)
    {
        Motor_Buffer[Timer][4*i + Slot] = (Frame & 0x8000) ? MOTOR_BIT1 : MOTOR_BIT0;
        Frame <<= 1;
    }
}

void Motor_Stop(void)
{
    for(uint8_t i = 0;i < 4;i ++)
    {
        Motor_Set(i,0);
    }
}