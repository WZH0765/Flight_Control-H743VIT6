#pragma once

#define I_LIMIT     0.30f
#define O_LIMIT     1.00f

typedef struct
{
    float Kp;           //比例
    float Ki;           //积分
    float Kd;           //微分

    float Iterm;        //积分累计
    float Last;         //上次误差

} pid_t;

void PID_Init(pid_t *Pid,float Kp,float Ki,float Kd);
float PID_Update(pid_t *Pid,float Target,float Actual,float dt);