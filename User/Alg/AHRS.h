#pragma once
#include <stdint.h>
#include <stdbool.h>

#define KP_ACC   1.2f
#define KI_ACC   0.0f
#define KP_MAG   0.1f
#define KI_MAG   0.0f

typedef struct
{
    float Yaw;       // 弧度
    float Roll;      // 弧度
    float Pitch;     // 弧度

} euler_t;

extern euler_t AHRS;

void AHRS_Update(float Ax,float Ay,float Az,float Gx,float Gy,float Gz,float Mx,float My,float Mz,float dt);
