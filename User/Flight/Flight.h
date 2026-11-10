#pragma once

#include "math.h"
#include <stdint.h>
#include <stdbool.h>

#define MIN_THRO      1.0000f   //最小油门
#define MAX_THRO      1000.0f   //最大油门

#define MIX_GAIN      400.0f    //混控增益
#define ANGLE_KP      8.0f      //角度增益

#define MAX_RATE   200.0f       //满杆角速度限幅
#define MAX_ANGLE  35.00f       //满杆角度  限幅
#define RAD_TO_DEG 57.29578f

#define MAX4(a,b,c,d)   fmaxf(fmaxf((a),(b)), fmaxf((c),(d)))
#define MIN4(a,b,c,d)   fminf(fminf((a),(b)), fminf((c),(d)))

typedef enum
{
    ARM      = 0,        //解锁
    DISARM   = 1,        //上锁
    FAILSAFE = 2         //故障

} fltState_e;

extern fltState_e State;

void Flight_Init(void);
void Flight_Update(float Gx,float Gy,float Gz,float dt);
void Flight_Stick(float Thro,float Yaw,float Roll,float Pitch);