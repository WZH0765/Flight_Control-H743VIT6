#pragma once

#include <stdint.h>
#include <stdbool.h>

#define USE_SIM

/****************** 机体参数（典型 5 寸机） ******************/
#define SIM_ARM       0.078f    //电机到机身的 x/y 投影距离（m），轴距约 220mm
#define SIM_TAU       0.020f    //电机一阶滞后（s）
#define SIM_MASS      0.65f     //整机质量（kg）
#define SIM_TMAX      8.000f    //单电机最大推力（N）
#define SIM_YAW_T     0.012f    //每牛推力的反扭矩（N·m/N）
#define SIM_INERTIA   0.005f    //单轴转动惯量（kg·m²）
#define SIM_ACC_NOISE 0.100f    //m/s²
#define SIM_GYR_NOISE 0.002f    //rad/s，约 0.11°/s，接近 ICM42605 在 1kHz 下的实际噪声
#define SIM_BAR_NOISE 0.200f    //Pa，折算约 ±1.6cm，给高度环一点真实噪声
#define SIM_CEIL      2.000f    //仿真天花板（m），避免没有高度环时一直往上飞

extern uint16_t SimuThro[4];

void Simulator(void);
void Simulator_Init(void);