#include <math.h>
#include <stdbool.h>
#include "Height.h"
#include "Flight.h"
#include "hardware.h"

#define ALT_ISA_K     0.190295f     //1/5.25588

static float Alt_Zero;      //地面零点气压（Pa），上电第一次有效采样记下
static float Alt_Filt;      //低通后的高度（cm）
static float Alt_Rate;      //低通后的升降速度（cm/s）

static float Alt_Sp;                //目标高度（cm）
static float Alt_Vz;                //升降速度指令（cm/s）
static float Out_Last = ALT_IDLE;   //上一拍油门（‰），用于变化率限制

float Height_Update(float StickThro,float dt)
{
    float P = HW_Sample.Pressure;
    Alt_Zero  = P;

    float Raw = 44330.0f*(1.0f - powf(P/Alt_Zero,ALT_ISA_K))*100.0f;
    float Prev = Alt_Filt;

    Alt_Filt += (Raw - Alt_Filt)*(dt/(ALT_LP_ALT + dt));
    Alt_Rate += ((Alt_Filt - Prev)/dt - Alt_Rate)*(dt/(ALT_LP_RATE + dt));

/*杆量 → 升降速度指令（中位附近当 0）*/
    float Dir = StickThro - ALT_CENTER;

    if(fabsf(Dir) < ALT_DEADBAND) Dir = 0.0f;

    Alt_Vz = Dir/ALT_CENTER*ALT_VZ_MAX;

/*目标高度按指令积分，限制在 0 ~ ALT_MAX_UP*/
    Alt_Sp += Alt_Vz*dt;

    if(Alt_Sp < 0.0f)      Alt_Sp = 0.0f;
    if(Alt_Sp > ALT_MAX_UP) Alt_Sp = ALT_MAX_UP;

/*已经落地、也没要求上升：目标清零并停机（中位停在地面时电机不转，推上去才起转）*/
    if(Alt_Sp < 2.0f && Alt_Filt < 5.0f && Alt_Vz <= 0.0f)
    {
        Alt_Sp = 0.0f;
        return MIN_THRO;
    }

    float Err  = fmaxf(-ALT_ERR_MAX ,fminf(ALT_ERR_MAX ,Alt_Sp - Alt_Filt));
    float Rate = fmaxf(-ALT_RATE_MAX,fminf(ALT_RATE_MAX,Alt_Rate - Alt_Vz));

    float Out = ALT_HOVER + Alt_Vz*ALT_FF + ALT_KP*Err - ALT_KD*Rate;

    /*变化率限制 + 油门下限：空中不允许掉到怠速以下*/
    float Step = ALT_SLEW*dt;

    Out_Last += fmaxf(-Step,fminf(Step,Out - Out_Last));

    return fmaxf(ALT_IDLE,fminf(1000.0f,Out_Last));
}