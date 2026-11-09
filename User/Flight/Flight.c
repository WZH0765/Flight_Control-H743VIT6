#include "PID.h"
#include <math.h>
#include "AHRS.h"
#include "Motor.h"
#include "Calib.h"
#include "Flight.h"
#include "Height.h"
#include "SysTime.h"
#include "hardware.h"

float SetThro = 0.0f;
float SetYawRate = 0.0f;

pid_t PID_YawRate;          //Yaw   角速度环
pid_t PID_RollRate;         //Roll  角速度环
pid_t PID_PitchRate;        //Pitch 角速度环
fltstate_e State = DISARM;

static float Stk_Roll;
static float Stk_Pitch;

static const float Kp[3] = {0.0053f,0.0040f,0.0040f};
static const float Ki[3] = {0.0027f,0.0013f,0.0013f};
static const float Kd[3] = {0.0000f,0.0000f,0.0000f};

/*****************************
  M1 前右 顺时针  M2 后右 逆时针
  M4 前左 逆时针  M3 后左 顺时针

******************************/
const float Mix_Table[4][3] =
{
    {-1.0f,-1.0f,+1.0f},    //M1 前右
    {+1.0f,-1.0f,-1.0f},    //M2 后右
    {-1.0f,+1.0f,-1.0f},    //M3 后左
    {+1.0f,+1.0f,+1.0f},    //M4 前左
};

void Flight_Init(void)
{
    SetThro = 0.0f;
    State = DISARM;

    Stk_Roll   = 0.0f;
    Stk_Pitch  = 0.0f;

    PID_Init(&PID_YawRate  ,Kp[0],Ki[0],Kd[0]);
    PID_Init(&PID_RollRate ,Kp[1],Ki[1],Kd[1]);
    PID_Init(&PID_PitchRate,Kp[2],Ki[2],Kd[2]);
}

/***************
*  遥控输入
*
*  形  参：Thro 0~1000，Yaw/Roll/Pitch −100~100（杆量百分比）
****************/
void Flight_Stick(float Thro,float Yaw,float Roll,float Pitch)
{
    SetThro = fmaxf(0.0f,fminf(1000.0f,Thro));

    Stk_Roll  = fmaxf(-100.0f,fminf(100.0f,Roll ));
    Stk_Pitch = fmaxf(-100.0f,fminf(100.0f,Pitch));

    SetYawRate = fmaxf(-100.0f,fminf(100.0f,Yaw))*RC_RATE_MAX/100.0f;
}

void Flight_Update(float Gx,float Gy,float Gz,float dt)
{
    float RollSp  = Stk_Roll *ANGLE_MAX/RAD_TO_DEG/100.0f;
    float PitchSp = Stk_Pitch*ANGLE_MAX/RAD_TO_DEG/100.0f;

    float RateSp[3];
    RateSp[0] = SetYawRate;
    RateSp[1] = fmaxf(-RC_RATE_MAX,fminf(RC_RATE_MAX,ANGLE_KP*(RollSp  - AHRS.Roll )*RAD_TO_DEG));
    RateSp[2] = fmaxf(-RC_RATE_MAX,fminf(RC_RATE_MAX,ANGLE_KP*(PitchSp - AHRS.Pitch)*RAD_TO_DEG));

    float Corr[3];
    Corr[0] = PID_Update(&PID_YawRate  ,RateSp[0],Gz*RAD_TO_DEG,dt);
    Corr[1] = PID_Update(&PID_RollRate ,RateSp[1],Gx*RAD_TO_DEG,dt);
    Corr[2] = PID_Update(&PID_PitchRate,RateSp[2],Gy*RAD_TO_DEG,dt);

    float Mix[4];
    float Max = -10000.0f,Min = 10000.0f;
    for(uint8_t i = 0;i < 4;i ++)
    {
        Mix[i] = MIX_GAIN*(Mix_Table[i][0]*Corr[0] + Mix_Table[i][1]*Corr[1] + Mix_Table[i][2]*Corr[2]);

        Max = fmaxf(Max,Mix[i]);
        Min = fminf(Min,Mix[i]);
    }
    float Scale = fminf(1.0f,(1000.0f - MIN_THRO)/(Max - Min));

    Max *= Scale;
    Min *= Scale;

    float BaseThro = Height_Update(SetThro,dt);
    BaseThro = fminf(BaseThro,1000.0f - Max);
    BaseThro = fmaxf(BaseThro,MIN_THRO - Min);

    for(uint8_t i = 0;i < 4;i ++)
    {
        Motor_Set(i,(uint16_t)fmaxf(MIN_THRO,fminf(1000.0f,BaseThro + Mix[i]*Scale)));
    }
}