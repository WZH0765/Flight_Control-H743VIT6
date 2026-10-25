#pragma once

#include <stdbool.h>

#define STILL_MS      500
#define SETTLE_MS     500
#define ERROR_GYR     0.0025f
#define MAG_SAMPLES   1000
#define ACC_SAMPLES   500
#define GYR_SAMPLES   3000
#define RANGE_MAG_MIN 100.0f
#define ERROR_ACC_MIN 9.30f
#define ERROR_ACC_MAX 10.3f

typedef enum
{
    IMU = 0,
    MAG = 1

} sensor_t;

typedef enum
{
    IDLE = 0,
    CHECK   ,
    WAIT    ,
    REQUEST ,
    COLLECT

} state_t;

extern float Acc_Bias[3];
extern float Gyr_Bias[3];
extern float Mag_Bias[3];
extern float Acc_Scal[3];
extern bool  IMU_CalValid;
extern bool  MAG_CalValid;

void Cal_Run(void);
void Cal_Abort(void);
void Cal_Start(sensor_t Sensor);
