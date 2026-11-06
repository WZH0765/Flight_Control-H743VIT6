#include "PID.h"
#include "Motor.h"
#include "Calib.h"
#include "Flight.h"
#include "hardware.h"

float SetThro = 0.0f;
float BaseThro = 0.0f;
float FinalThro = 0.0f;
float SetRate[3] = {0.0f,0.0f,0.0f};
float OutRate[3] = {0.0f,0.0f,0.0f};

pid_t PID_YawRate;       //Yaw   角速度环
pid_t PID_RollRate;      //Roll  角速度环
pid_t PID_PitchRate;     //Pitch 角速度环

static fltState_e State = FLIGHT_DISARM;

/*
 M1 前右 顺时针  M2 后右 逆时针
 M4 前左 逆时针  M3 后左 顺时针
*/
const float Mix_Table[4][3] =
{
    {-1.0f,-1.0f,-1.0f},    //M1 前右
    {-1.0f,+1.0f,+1.0f},    //M2 后右
    {+1.0f,+1.0f,-1.0f},    //M3 后左
    {+1.0f,-1.0f,+1.0f},    //M4 前左
};

const float Kp[3] = {0.0060f,0.0060f,0.0080f};
const float Ki[3] = {0.0020f,0.0020f,0.0040f};
const float Kd[3] = {0.0000f,0.0000f,0.0000f};

void Flight_Init(void)
{
    SetThro = 0.0f;
    State = FLIGHT_DISARM;

    PID_Init(&PID_YawRate  ,Kp[2],Ki[2],Kd[2]);
    PID_Init(&PID_RollRate ,Kp[0],Ki[0],Kd[0]);
    PID_Init(&PID_PitchRate,Kp[1],Ki[1],Kd[1]);
}

/***************
*  角速度环 + 混控
*
*  形  参：Gx/Gy/Gz 校准后的机体系角速度（rad/s），dt 执行周期（s）
****************/
void Flight_Update(float Gx,float Gy,float Gz,float dt)
{
    float MaxDiff = -10000.0f,MinDiff = 10000.0f;
    float Diff[4] = {0.0f,0.0f,0.0f,0.0f};

    if(State != FLIGHT_ARM)
    {
        Motor_Stop();
        return;
    }
    if(Health_Status(&GYR_Health) == false ||
       Health_Status(&ACC_Health) == false)
    {
        State = FLIGHT_FAILSAFE;
        Motor_Stop();
        return;
    }

    OutRate[0] = PID_Update(&PID_RollRate ,SetRate[0],Gx*RAD_TO_DEG,dt);
    OutRate[1] = PID_Update(&PID_PitchRate,SetRate[1],Gy*RAD_TO_DEG,dt);
    OutRate[2] = PID_Update(&PID_YawRate  ,SetRate[2],Gz*RAD_TO_DEG,dt);

    BaseThro = IDLE_THRO + SetThro*(1000.0f - IDLE_THRO)/1000.0f;

    for(uint8_t i = 0;i < 4;i ++)
    {
        Diff[i] = MIX_GAIN*(Mix_Table[i][0]*OutRate[0] + Mix_Table[i][1]*OutRate[1] + Mix_Table[i][2]*OutRate[2]);
        if(Diff[i] > MaxDiff) MaxDiff = Diff[i];
        if(Diff[i] < MinDiff) MinDiff = Diff[i];
    }

    if(MaxDiff - MinDiff > 1000.0f)
    {
        MaxDiff *= 1000.0f/(MaxDiff - MinDiff);
        MinDiff *= 1000.0f/(MaxDiff - MinDiff);
        for(uint8_t i = 0;i < 4;i ++) Diff[i] *= 1000.0f/(MaxDiff - MinDiff);
    }

    if(BaseThro + MinDiff < 1.0000f) BaseThro = 1.0f - MinDiff;
    if(BaseThro + MaxDiff > 1000.0f) BaseThro = 1000.0f - MaxDiff;

    for(uint8_t i = 0;i < 4;i ++)
    {
        FinalThro = BaseThro + Diff[i];

        if(FinalThro < 1.0000f) FinalThro = 1.0f;
        if(FinalThro > 1000.0f) FinalThro = 1000.0f;

        Motor_Set(i,(uint16_t)FinalThro);
    }
}

void Flight_Set(int32_t Roll,int32_t Pitch,int32_t Yaw,int32_t Throttle)
{
    int32_t Rate[3] = {Roll,Pitch,Yaw};

    for(uint8_t i = 0;i < 3;i ++)
    {
        if(Rate[i] >  (int32_t)RATE_LIMIT) Rate[i] =  (int32_t)RATE_LIMIT;
        if(Rate[i] < -(int32_t)RATE_LIMIT) Rate[i] = -(int32_t)RATE_LIMIT;

        SetRate[i] = (float)Rate[i];
    }

    if(Throttle < 0)    Throttle = 0;
    if(Throttle > 1000) Throttle = 1000;

    SetThro = (float)Throttle;
}