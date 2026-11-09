#pragma once
#include <stdint.h>
#include <stdbool.h>

#define KP_ACC          0.5f        //加速度计修正增益：越小越"信陀螺"，机动时不被阻力/惯性力带偏
#define KI_ACC          0.0f
#define KP_MAG          0.1f
#define KI_MAG          0.0f
#define GRAVITY_G       9.80665f    //标准重力（m/s²）
#define ACC_TRUST_BAND  0.3f        //|A| 偏离重力超过这个值（m/s²）就认为在机动

typedef struct
{
    float Yaw;       // 弧度
    float Roll;      // 弧度
    float Pitch;     // 弧度

} euler_t;

extern euler_t AHRS;

void AHRS_Update(float Ax,float Ay,float Az,float Gx,float Gy,float Gz,float Mx,float My,float Mz,float dt);