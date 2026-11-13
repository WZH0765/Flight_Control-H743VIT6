#include "PID.h"
#include <math.h>
#include "AHRS.h"
#include "PVRS.h"
#include "Motor.h"
#include "Calib.h"
#include "Flight.h"
#include "Height.h"
#include "SysTime.h"
#include "hardware.h"

float RC_SetRoll;              //遥控器(模拟器)设定Roll
float RC_SetPitch;             //遥控器(模拟器)设定Pitch
float RC_SetThro = 0.0f;       //遥控器(模拟器)设定Throttle
float RC_SetYawRate = 0.0f;    //遥控器(模拟器)设定Yaw角速度

pid_t PID_YawRate;          //Yaw   角速度环
pid_t PID_RollRate;         //Roll  角速度环
pid_t PID_PitchRate;        //Pitch 角速度环
fltState_e State = DISARM;

const float Kp[3] = {0.0053f,0.0040f,0.0040f};
const float Ki[3] = {0.0027f,0.0013f,0.0013f};
const float Kd[3] = {0.0000f,0.0000f,0.0000f};

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
    State = DISARM;
    RC_SetThro = 0.0f;
    RC_SetRoll  = 0.0f;
    RC_SetPitch = 0.0f;

    AHRS_Init();
    PVRS_Init();
    Height_Init();

    PID_Init(&PID_YawRate  ,Kp[0],Ki[0],Kd[0]);
    PID_Init(&PID_RollRate ,Kp[1],Ki[1],Kd[1]);
    PID_Init(&PID_PitchRate,Kp[2],Ki[2],Kd[2]);
}

/***************
*  遥控输入
*
*  形  参：Thro 0~1000，Yaw/Roll/Pitch −100~100
****************/
void Flight_Stick(float Thro,float Yaw,float Roll,float Pitch)
{
    RC_SetThro = fmaxf(0.0f,fminf(MAX_THRO,Thro));
    RC_SetRoll  = fmaxf(-100.0f,fminf(100.0f,Roll));
    RC_SetPitch = fmaxf(-100.0f,fminf(100.0f,Pitch));
    RC_SetYawRate = fmaxf(-100.0f,fminf(100.0f,Yaw))*MAX_RATE/100.0f;
}

void Flight_Update(float Gx,float Gy,float Gz,float dt)
{
    switch(State)
    {
/**************************遥控器(模拟器)解锁**************************/
        case ARM:
        //遥控器(模拟器)摇杆量映射到弧度
        float SetRoll  = RC_SetRoll /100.0f*MAX_ANGLE/RAD_TO_DEG;
        float SetPitch = RC_SetPitch/100.0f*MAX_ANGLE/RAD_TO_DEG;

        //遥控器(模拟器)摇杆量映射到角速度
        float SetRate[3];
        SetRate[0] = RC_SetYawRate;
        SetRate[1] = fmaxf(-MAX_RATE,fminf(MAX_RATE,ANGLE_KP*(SetRoll  - AHRS.Roll )*RAD_TO_DEG));
        SetRate[2] = fmaxf(-MAX_RATE,fminf(MAX_RATE,ANGLE_KP*(SetPitch - AHRS.Pitch)*RAD_TO_DEG));

        //内环更新
        float Corr[3];
        Corr[0] = PID_Update(&PID_YawRate  ,SetRate[0],Gz*RAD_TO_DEG,dt);
        Corr[1] = PID_Update(&PID_RollRate ,SetRate[1],Gx*RAD_TO_DEG,dt);
        Corr[2] = PID_Update(&PID_PitchRate,SetRate[2],Gy*RAD_TO_DEG,dt);

        //姿态修正量
        float Mix[4];
        for(uint8_t i = 0;i < 4;i ++)
        {
            Mix[i] = MIX_GAIN*(Mix_Table[i][0]*Corr[0] + Mix_Table[i][1]*Corr[1] + Mix_Table[i][2]*Corr[2]);
        }
        float Max = MAX4(Mix[0],Mix[1],Mix[2],Mix[3]);
        float Min = MIN4(Mix[0],Mix[1],Mix[2],Mix[3]);

        //放缩
        float Scale = fminf(1.0f,(MAX_THRO - MIN_THRO)/(Max - Min));
        Max *= Scale;
        Min *= Scale;

        //基准油门-基于高度环
        float BaseThro = Height_Update(RC_SetThro,dt);
        BaseThro = fminf(BaseThro,MAX_THRO - Max);
        BaseThro = fmaxf(BaseThro,MIN_THRO - Min);

        for(uint8_t i = 0;i < 4;i ++)
        {
            Motor_Set(i,(uint16_t)fmaxf(MIN_THRO,fminf(MAX_THRO,BaseThro + Mix[i]*Scale)));
        }
            break;
/*************************遥控器(模拟器)未解锁*************************/
        case DISARM:

        //加入未解锁逻辑

/******************************飞控失效******************************/
        case FAILSAFE:
            Motor_Stop();
            return;
    }
}