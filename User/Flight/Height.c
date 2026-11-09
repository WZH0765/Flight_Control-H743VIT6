#include <math.h>
#include "Height.h"
#include "Flight.h"
#include "Hardware.h"

float Speed_Z;              //Z方向速度-cm/s
float ZeroAlt;              //Z方向零点气压-Pa
static float Rate_Z;        //Z方向变化率-cm/s
static float SetAlt;        //Z方向目标高度-cm
static float ActAlt;        //Z方向实际高度-cm
float LastThro = ALT_IDLE;  //上次输出油门 (‰)

/***************
*  初始化
****************/
void Height_Init(void)
{
    Rate_Z   = 0.0f;
    SetAlt   = 0.0f;
    ActAlt   = 0.0f;
    Speed_Z  = 0.0f;
    ZeroAlt  = 0.0f;
    LastThro = ALT_IDLE;
}

/***************
*  定  高
*
*  形  参：杆量 0~1000(中位 500 = 定高) dt(s)
*  返  回：总油门（‰）
****************/
float Height_Update(float SetThro,float dt)
{
    if(ZeroAlt == 0.0f)
    {
        if(HW_Sample.Pressure == 0.0f)
        {
            return ALT_IDLE;
        }
        else
        {
            ZeroAlt = HW_Sample.Pressure;
        }
    }

    float Prev = ActAlt;
    ActAlt = LPF(44330.0f*(1.0f - powf(HW_Sample.Pressure/ZeroAlt,ALT_ISA_K))*100.0f,ActAlt,ALT_LP_ALT,dt);
    Rate_Z = LPF((ActAlt - Prev)/dt,Rate_Z,ALT_LP_RATE,dt);

    /*2) 杆量 → 升降速度 → 目标高度*/
    float Dir = SetThro - ALT_CENTER;

    Speed_Z = (fabsf(Dir) < ALT_DEADBAND) ? 0.0f : Dir/ALT_CENTER*ALT_VZ_MAX;

    SetAlt = fminf(SetAlt + Speed_Z*dt,ALT_MAX_UP);

    /*3) 已经落地、也没要求上升：只给怠速。不给悬停油门，否则停在地面上会自己起飞*/
    if(SetAlt < 2.0f && ActAlt < 5.0f && Speed_Z <= 0.0f)
    {
        SetAlt = 0.0f;
        return ALT_IDLE;
    }

    /*4) 高度 PD + 升降速度前馈 → 油门*/
    float Out = ALT_HOVER + Speed_Z*ALT_FF
              + ALT_KP*CLAMP(SetAlt - ActAlt,ALT_ERR_MAX)
              - ALT_KD*CLAMP(Rate_Z - Speed_Z,ALT_RATE_MAX);

    /*5) 限变化率、限下限（空中不允许掉到怠速以下）*/
    LastThro += CLAMP(Out - LastThro,ALT_SLEW*dt);

    return fmaxf(ALT_IDLE,fminf(1000.0f,LastThro));
}