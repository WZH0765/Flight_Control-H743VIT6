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
  列顺序：Yaw Roll Pitch
  Pitch 抬头要靠【前侧】加油——后侧加油是压机头，符号反了就是正反馈
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

    PID_Init(&PID_YawRate  ,Kp[0],Ki[0],Kd[0]);
    PID_Init(&PID_RollRate ,Kp[1],Ki[1],Kd[1]);
    PID_Init(&PID_PitchRate,Kp[2],Ki[2],Kd[2]);
}

void Flight_Update(float Gx,float Gy,float Gz,float dt)
{
    float Corr[3];
    Corr[0] = PID_Update(&PID_YawRate  ,SetRate[0],Gz*RAD_TO_DEG,dt);
    Corr[1] = PID_Update(&PID_RollRate ,SetRate[1],Gx*RAD_TO_DEG,dt);
    Corr[2] = PID_Update(&PID_PitchRate,SetRate[2],Gy*RAD_TO_DEG,dt);

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

    float BaseThro = IDLE_THRO + SetThro*(1000.0f - IDLE_THRO)/1000.0f;
    BaseThro = fminf(BaseThro,1000.0f - Max);
    BaseThro = fmaxf(BaseThro,MIN_THRO - Min);

    for(uint8_t i = 0;i < 4;i ++)
    {
        Motor_Set(i,(uint16_t)fmaxf(MIN_THRO,fminf(1000.0f,BaseThro + Mix[i]*Scale)));
    }
}