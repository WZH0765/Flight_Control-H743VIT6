#include "PID.h"
#include <math.h>
#include "Motor.h"
#include "Calib.h"
#include "Flight.h"
#include "hardware.h"

float SetThro = 0.0f;
float SetRate[3] = {0.0f,0.0f,0.0f};

pid_t PID_YawRate;          //Yaw   角速度环
pid_t PID_RollRate;         //Roll  角速度环
pid_t PID_PitchRate;        //Pitch 角速度环
fltstate_e State = DISARM;

static const float Kp[3] = {0.0080f,0.0060f,0.0060f};
static const float Ki[3] = {0.0040f,0.0020f,0.0020f};
static const float Kd[3] = {0.0000f,0.0000f,0.0000f};

/*****************************
  M1 前右 顺时针  M2 后右 逆时针
  M4 前左 逆时针  M3 后左 顺时针
******************************/
const float Mix_Table[4][3] =
{
    {-1.0f,-1.0f,-1.0f},
    {+1.0f,-1.0f,+1.0f},
    {-1.0f,+1.0f,+1.0f},
    {+1.0f,+1.0f,-1.0f},
};

void Flight_Init(void)
{
    SetThro = 0.0f;
    State = DISARM;

    PID_Init(&PID_YawRate  ,Kp[0],Ki[0],Kd[0]);
    PID_Init(&PID_RollRate ,Kp[1],Ki[1],Kd[1]);
    PID_Init(&PID_PitchRate,Kp[2],Ki[2],Kd[2]);
}

void Flight_Update(float Gx,float Gy,float Gz,float dt)
{
    float Out[3];
    float Base;
    float Diff[4];
    float Scale;
    float MaxDiff = -10000.0f,MinDiff = 10000.0f;

    if(State != ARM)
    {
        Motor_Stop();
        return;
    }

    if(Health_Status(&GYR_Health) == false)
    {
        State = FAILSAFE;
        Motor_Stop();
        return;
    }

    Out[0] = PID_Update(&PID_YawRate  ,SetRate[0],Gz*RAD_TO_DEG,dt);
    Out[1] = PID_Update(&PID_RollRate ,SetRate[1],Gx*RAD_TO_DEG,dt);
    Out[2] = PID_Update(&PID_PitchRate,SetRate[2],Gy*RAD_TO_DEG,dt);

    Base = IDLE_THRO + SetThro*(1000.0f - IDLE_THRO)/1000.0f;

    for(uint8_t i = 0;i < 4;i ++)
    {
        Diff[i] = MIX_GAIN*(Mix_Table[i][0]*Out[0] + Mix_Table[i][1]*Out[1] + Mix_Table[i][2]*Out[2]);

        if(Diff[i] > MaxDiff) MaxDiff = Diff[i];
        if(Diff[i] < MinDiff) MinDiff = Diff[i];
    }

    /*行程不够时等比压缩，再把基准油门平移进可用行程*/
    Scale = fminf(1.0f,(1000.0f - MIN_THRO)/(MaxDiff - MinDiff));

    MaxDiff *= Scale;
    MinDiff *= Scale;

    for(uint8_t i = 0;i < 4;i ++) Diff[i] *= Scale;

    if(Base + MinDiff < MIN_THRO) Base = MIN_THRO - MinDiff;
    if(Base + MaxDiff > 1000.0f)  Base = 1000.0f - MaxDiff;

    for(uint8_t i = 0;i < 4;i ++)
    {
        float Thro = Base + Diff[i];

        if(Thro < MIN_THRO) Thro = MIN_THRO;
        if(Thro > 1000.0f)  Thro = 1000.0f;

        Motor_Set(i,(uint16_t)Thro);
    }
}