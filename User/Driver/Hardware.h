#pragma once

/****************** 选择使用的硬件 ******************/

/*******************************
* 请注意：
* 使用外置 磁力计/气压计 等I2C设备
* 请使用较低（如：100kHz）总线速度
*******************************/

#define USE_ICM42605
#define USE_QMC5883P
#define USE_DPS310

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

void Hardware_Init(void);
