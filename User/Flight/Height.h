#pragma once

#define MAX_VZ        150.0f    //满杆速度
#define MAX_UP        500.0f    //高度上限
#define DEADBAND      20.0f     //中位死区
#define IDLE_THRO     50.0f     //怠速油门
#define HOVER_THRO    421.0f    //悬停油门

#define ALT_FF        0.05f     //前馈
#define ALT_KP        0.25f     //振荡
#define ALT_KD        1.20f     //阻尼
#define ALT_SLEW      4000.0f   //油门变化率上限
#define ALT_ERR_MAX   200.0f    //误差参与运算上限
#define ALT_RATE_MAX  200.0f    //爬升率参与运算上限

#define LP_ALT    0.10f     //高度低通（原来 0.15）
#define LP_RATE   0.05f     //升降速度低通

#define CLAMP(Val,Limit)        (fmaxf(-(Limit),fminf((Limit),(Val))))
#define LPF(Now,Last,Tau,dt)    ((Last) + ((Now) - (Last))*((dt)/((Tau) + (dt))))

void Height_Init(void);
float Height_Update(float StickThro,float dt);