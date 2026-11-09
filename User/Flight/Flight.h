#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MIN_THRO      1.0f
#define MIX_GAIN      400.0f
#define ANGLE_KP      8.0f      //角度外环增益
#define ANGLE_MAX     35.0f     //自稳模式满杆倾角（°）
#define RAD_TO_DEG    57.29578f
#define RC_RATE_MAX   200.0f    //满杆角速度（°/s）

typedef enum
{
    ARM      = 0,        //解锁
    DISARM   = 1,        //上锁
    FAILSAFE = 2         //故障

} fltstate_e;

extern fltstate_e State;

void Flight_Init(void);
void Flight_Update(float Gx,float Gy,float Gz,float dt);
void Flight_Stick(float Thro,float Yaw,float Roll,float Pitch);