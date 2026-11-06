#include "PID.h"

void PID_Init(pid_t *Pid,float Kp,float Ki,float Kd)
{
    Pid->Kp    = Kp;
    Pid->Ki    = Ki;
    Pid->Kd    = Kd;
    Pid->Last  = 0.0f;
    Pid->Iterm = 0.0f;
}

/***************
*  单轴 PID 计算
*
*  形  参：Target 目标值，Actual 反馈值，dt 执行周期（s）
****************/
float PID_Update(pid_t *Pid,float Target,float Actual,float dt)
{
    float Out;
    float Err = Target - Actual;

    Pid->Iterm += Pid->Ki*Err*dt;
    if(Pid->Iterm >  I_LIMIT) Pid->Iterm =  I_LIMIT;
    if(Pid->Iterm < -I_LIMIT) Pid->Iterm = -I_LIMIT;

    Out = Pid->Kp*Err + Pid->Iterm + Pid->Kd*(Err - Pid->Last)/dt;

    Pid->Last = Err;
    if(Out >  O_LIMIT) Out =  O_LIMIT;
    if(Out < -O_LIMIT) Out = -O_LIMIT;

    return Out;
}