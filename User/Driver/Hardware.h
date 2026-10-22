#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "Bus.h"
#include "Health.h"

/****************** 选择使用的硬件 ******************/

/*******************************
* 请注意：
* 使用 磁力计/气压计 等I2C设备
* 请使用较低（如：100kHz）总线速度
*******************************/

#define USE_ICM42605
#define USE_QMC5883P
#define USE_DPS310

/****************** 板级量程换算 ******************/

#define ACC_SCALE   (2048.0f)               //±16g
#define GYR_SCALE   (16.4f/0.0174533f)      //±2000dps
#define GRAVITY     (9.80665f)              //标准重力加速度

typedef struct
{
    float Acc[3];           //机体系加速度（m/s²）
    float Gyr[3];           //机体系角速度（rad/s）
    float Mag[3];           //机体系磁场（LSB）

    float Pressure;         //气压（Pa）
    float Temperature;      //温度（℃）

} sample_t;

typedef struct
{
    bus_t *BusDevice;

    float Acc[3];
    float Gyr[3];

    int16_t Temp;

} imu_t;

typedef struct
{
    bus_t *BusDevice;

    int16_t Mag[3];

} mag_t;

typedef struct
{
    bus_t *BusDevice;

    int16_t c0;
    int16_t c1;
    int32_t c00;
    int32_t c10;
    int16_t c01;
    int16_t c11;
    int16_t c20;
    int16_t c21;
    int16_t c30;

    float Pressure;
    float Temperature;

    uint8_t ID;
} bar_t;

extern bar_t DPS310_Device;
extern mag_t IST8310_Device;
extern mag_t QMC5883P_Device;
extern imu_t ICM42605_Device1;
extern imu_t ICM42605_Device2;

extern sample_t HW_Sample;
extern health_t ACC_Health;
extern health_t GYR_Health;
extern health_t MAG_Health;
extern health_t BAR_Health;

void Hardware_Init(void);
void Hardware_IMURead(void);
void Hardware_MAGRead(void);
void Hardware_BARRead(void);
