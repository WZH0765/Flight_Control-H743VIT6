#pragma once

#include <stdint.h>

/***********************
* DShot300 电机输出
* M1 - PB0 - TIM3_CH3 - CCR3 - DMA1_Stream2
* M2 - PB1 - TIM3_CH4 - CCR4 - DMA1_Stream2
* M3 - PA0 - TIM2_CH1 - CCR1 - DMA1_Stream3
* M4 - PA1 - TIM2_CH2 - CCR2 - DMA1_Stream3
*
* TIM2 的 CCR 是 32 位，必须按 32 位写
***********************/

#define MOTOR_MIN       48          //DShot 最小油门
#define MOTOR_MAX       2047        //DShot 最大油门
#define MOTOR_BIT0      150         //37.5% 占空比，数据位 0
#define MOTOR_BIT1      300         //75%   占空比，数据位 1
#define MOTOR_BUF_LEN   64          //16 位数据 + 48 位空闲，约 4.7kHz 帧率

void Motor_Init(void);
void Motor_Stop(void);
void Motor_Set(uint8_t Index,uint16_t Throttle);