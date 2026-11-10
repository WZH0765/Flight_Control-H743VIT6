#include <math.h>
#include "Height.h"
#include "Flight.h"
#include "Hardware.h"

float Speed_Z;              //Z方向速度-cm/s
float ZeroAlt;              //Z方向零点气压-Pa
static float Rate_Z;        //Z方向变化率-cm/s
static float SetAlt;        //Z方向目标高度-cm
static float ActAlt;        //Z方向实际高度-cm
float LastThro = IDLE_THRO; //上次输出油门 (‰)

void Height_Init(void)
{
    Rate_Z   = 0.0f;
    SetAlt   = 0.0f;
    ActAlt   = 0.0f;
    Speed_Z  = 0.0f;
    ZeroAlt  = 0.0f;
    LastThro = IDLE_THRO;
}

/***************
*  定  高
*
*  形  参：杆量 0~1000(中位 500 = 定高) dt(s)
*  返  回：总油门
****************/
float Height_Update(float SetThro,float dt)
{
    float Dir = 0.0f;       //摇杆偏离中位方向
    float PreAlt = 0.0f;    //上次高度,速度计算

    if(ZeroAlt == 0.0f)
    {
        if(HW_Sample.Pressure == 0.0f)
        {
            return IDLE_THRO;
        }
        else
        {
            ZeroAlt = HW_Sample.Pressure;
        }
    }

    PreAlt = ActAlt;
    ActAlt = LPF(44330.0f*(1.0f - powf(HW_Sample.Pressure/ZeroAlt,0.190295f))*100.0f,ActAlt,LP_ALT,dt);
    Rate_Z = LPF((ActAlt - PreAlt)/dt,Rate_Z,LP_RATE,dt);

    Dir = SetThro - 500.0f;
    Speed_Z = (fabsf(Dir) < DEADBAND) ? 0.0f : Dir/500.0f*MAX_VZ;
    SetAlt  = fminf(SetAlt + Speed_Z*dt,MAX_UP);

    if(SetAlt < 2.0f && ActAlt < 5.0f && Speed_Z <= 0.0f)
    {
        SetAlt = 0.0f;
        return IDLE_THRO;
    }

    float Out = HOVER_THRO + Speed_Z*ALT_FF + ALT_KP*CLAMP(SetAlt - ActAlt,ALT_ERR_MAX) - ALT_KD*CLAMP(Rate_Z - Speed_Z,ALT_RATE_MAX);
    LastThro += CLAMP(Out - LastThro,ALT_SLEW*dt);

    return fmaxf(IDLE_THRO,fminf(1000.0f,LastThro));
}