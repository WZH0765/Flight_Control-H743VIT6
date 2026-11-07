#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MIN_THRO    1.0f
#define MIX_GAIN    400.0f
#define IDLE_THRO   50.0f
#define RAD_TO_DEG  57.29578f

typedef enum
{
    ARM      = 0,        //解锁
    DISARM   = 1,        //上锁
    FAILSAFE = 2         //故障

} fltstate_e;

void Flight_Init(void);
void Flight_Update(float Gx,float Gy,float Gz,float dt);