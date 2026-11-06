#pragma once

#include <stdint.h>
#include <stdbool.h>

/****************** 可调参数 ******************/
#define IDLE_THRO   50.0f
#define MIX_GAIN    400.0f
#define RATE_LIMIT  800.0f

#define RAD_TO_DEG  57.29578f



/****************** 接口 ******************/

typedef enum
{
    FLIGHT_DISARM = 0,          //上锁，四路输出 DShot 0
    FLIGHT_ARM,                 //解锁，角速度环工作
    FLIGHT_FAILSAFE             //故障，强制停转

} fltState_e;

void Flight_Init(void);
void Flight_Update(float Gx,float Gy,float Gz,float dt);
void Flight_Set(int32_t Roll,int32_t Pitch,int32_t Yaw,int32_t Throttle);