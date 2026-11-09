#include <math.h>
#include "AHRS.h"
#include <stdio.h>
#include "Calib.h"
#include <string.h>
#include "Flight.h"
#include "SysTime.h"
#include "hardware.h"
#include "Simulator.h"

extern float SetThro;
extern fltstate_e State;

uint16_t SimuThro[4];           //模拟量油门（0~1000）

static float    Sim_Q[4];       //姿态四元数，与 AHRS 同一约定（q0 为实部）
static float    Sim_W[3];       //机体系角速度（rad/s）
static float    Sim_V[3];       //NED 速度（m/s）
static float    Sim_P[3];       //NED 位置（m，Z 向下为正）
static float    Sim_U[4];       //电机实际油门（0~1，含滞后）

/*电机位置：x 前、y 右（m）*/
static const float Sim_Pos[4][2] =
{
    {+SIM_ARM,+SIM_ARM},        //M1 前右 顺时针
    {-SIM_ARM,+SIM_ARM},        //M2 后右 逆时针
    {-SIM_ARM,-SIM_ARM},        //M3 后左 顺时针
    {+SIM_ARM,-SIM_ARM},        //M4 前左 逆时针
};

/*偏航反扭矩符号：逆时针桨为正（与混控表 Yaw 列一致）*/
static const float Sim_Yaw[4] = {-1.0f,+1.0f,-1.0f,+1.0f};

/***************
*  伪随机噪声
*
*  说  明：线性同余，够用就行，不用 rand()
****************/
static uint32_t Sim_Seed = 12345;

static float Simulator_Rand(void)
{
    Sim_Seed = Sim_Seed*1103515245u + 12345u;

    return (float)((Sim_Seed >> 16) & 0x7FFF)/16383.5f - 1.0f;
}

/***************
*  初始化
*
*  说  明：清状态、关掉校准修正、直接解锁到接近悬停的油门
****************/
void Simulator_Init(void)
{
    memset(Sim_Q,0,sizeof(Sim_Q));
    memset(Sim_W,0,sizeof(Sim_W));
    memset(Sim_V,0,sizeof(Sim_V));
    memset(Sim_P,0,sizeof(Sim_P));
    memset(Sim_U,0,sizeof(Sim_U));

    Sim_Q[0] = 1.0f;
    IMU_CalValid = false;       //合成数据不能再被零偏/刻度修正
    MAG_CalValid = false;

    State   = ARM;
    SetThro = 490.0f;           //悬停点（Tmax=6N 时 base 515‰，对应 SetThro≈490）
    printf("SIM,start\r\n");
}

/***************
*  推进一步仿真
*
*  说  明：由调度器每毫秒调用。用上一拍的电机油门积分机体，再合成传感器；
*         天然一拍延迟，不构成代数环
****************/
void Simulator(void)
{
    float R[3][3];
    float T[4];
    float Tq[3] = {0.0f,0.0f,0.0f};
    float q0 = Sim_Q[0],q1 = Sim_Q[1],q2 = Sim_Q[2],q3 = Sim_Q[3];
    float Sum = 0.0f;

    /*1) 电机推力：一阶滞后 + 油门平方特性*/
    for(uint8_t i = 0;i < 4;i ++)
    {
        float U = (float)SimuThro[i]/1000.0f;

        Sim_U[i] += (U - Sim_U[i])*(0.001f/SIM_TAU);
        if(Sim_U[i] < 0.0f) Sim_U[i] = 0.0f;

        T[i] = SIM_TMAX*Sim_U[i]*Sim_U[i];
        Sum += T[i];
    }

    /*2) 力矩：力臂 + 偏航反扭矩（Z 向下为正，绕 +Z 右转）*/
    for(uint8_t i = 0;i < 4;i ++)
    {
        Tq[0] -= Sim_Pos[i][1]*T[i];                //Roll：右侧力大 → +X
        Tq[1] += Sim_Pos[i][0]*T[i];                //Pitch：后侧力大 → 抬头
        Tq[2] += SIM_YAW_T*Sim_Yaw[i]*T[i];         //Yaw：逆时针桨右转
    }

    /*3) 角速度 + 四元数积分（与 AHRS.c 同一套公式，便于对照）*/
    for(uint8_t i = 0;i < 3;i ++)
    {
        Sim_W[i] += Tq[i]/SIM_INERTIA*0.001f;
    }

    float q0d = 0.5f*(-q1*Sim_W[0] - q2*Sim_W[1] - q3*Sim_W[2]);
    float q1d = 0.5f*( q0*Sim_W[0] + q2*Sim_W[2] - q3*Sim_W[1]);
    float q2d = 0.5f*( q0*Sim_W[1] - q1*Sim_W[2] + q3*Sim_W[0]);
    float q3d = 0.5f*( q0*Sim_W[2] + q1*Sim_W[1] - q2*Sim_W[0]);

    Sim_Q[0] += q0d*0.001f;
    Sim_Q[1] += q1d*0.001f;
    Sim_Q[2] += q2d*0.001f;
    Sim_Q[3] += q3d*0.001f;

    float N = sqrtf(Sim_Q[0]*Sim_Q[0] + Sim_Q[1]*Sim_Q[1] + Sim_Q[2]*Sim_Q[2] + Sim_Q[3]*Sim_Q[3]);

    Sim_Q[0] /= N; Sim_Q[1] /= N; Sim_Q[2] /= N; Sim_Q[3] /= N;

    q0 = Sim_Q[0]; q1 = Sim_Q[1]; q2 = Sim_Q[2]; q3 = Sim_Q[3];

    /*4) 姿态矩阵（机体系 → NED）*/
    R[0][0] = 1.0f - 2.0f*(q2*q2 + q3*q3);
    R[0][1] = 2.0f*(q1*q2 - q0*q3);
    R[0][2] = 2.0f*(q1*q3 + q0*q2);
    R[1][0] = 2.0f*(q1*q2 + q0*q3);
    R[1][1] = 1.0f - 2.0f*(q1*q1 + q3*q3);
    R[1][2] = 2.0f*(q2*q3 - q0*q1);
    R[2][0] = 2.0f*(q1*q3 - q0*q2);
    R[2][1] = 2.0f*(q2*q3 + q0*q1);
    R[2][2] = 1.0f - 2.0f*(q1*q1 + q2*q2);

    /*5) 线运动：推力沿机体 -Z、重力沿 NED +Z、气动阻力沿速度反方向（∝v²）*/
    float An[3];
    float Speed = sqrtf(Sim_V[0]*Sim_V[0] + Sim_V[1]*Sim_V[1] + Sim_V[2]*Sim_V[2]);

    An[0] = -Sum*R[0][2]/SIM_MASS;
    An[1] = -Sum*R[1][2]/SIM_MASS;
    An[2] = GRAVITY - Sum*R[2][2]/SIM_MASS;

    for(uint8_t i = 0;i < 3;i ++)
    {
        An[i] -= SIM_DRAG*Speed*Sim_V[i]/SIM_MASS;

        Sim_V[i] += An[i]*0.001f;
        Sim_P[i] += Sim_V[i]*0.001f;
    }

    /*6) 地面约束：落地就把速度清掉，角速度按 50ms 时间常数衰减
           （原来每毫秒减半太狠，贴地时打杆几乎看不到角速度响应）*/
    if(Sim_P[2] > 0.0f)
    {
        Sim_P[2] = 0.0f;

        Sim_V[0] = 0.0f; Sim_V[1] = 0.0f; Sim_V[2] = 0.0f;

        Sim_W[0] *= 0.98f; Sim_W[1] *= 0.98f; Sim_W[2] *= 0.98f;
    }

    /*7) 合成传感器：AHRS 期望"机体系比力"，水平时 Acc=(0,0,-g)*/
    HW_Sample.Gyr[0] = Sim_W[0] + SIM_GYR_NOISE*Simulator_Rand();
    HW_Sample.Gyr[1] = Sim_W[1] + SIM_GYR_NOISE*Simulator_Rand();
    HW_Sample.Gyr[2] = Sim_W[2] + SIM_GYR_NOISE*Simulator_Rand();

    HW_Sample.Acc[0] = An[0]*R[0][0] + An[1]*R[1][0] + An[2]*R[2][0] - GRAVITY*R[2][0] + SIM_ACC_NOISE*Simulator_Rand();
    HW_Sample.Acc[1] = An[0]*R[0][1] + An[1]*R[1][1] + An[2]*R[2][1] - GRAVITY*R[2][1] + SIM_ACC_NOISE*Simulator_Rand();
    HW_Sample.Acc[2] = An[0]*R[0][2] + An[1]*R[1][2] + An[2]*R[2][2] - GRAVITY*R[2][2] + SIM_ACC_NOISE*Simulator_Rand();

    HW_Sample.Mag[0] =  R[0][0]*0.20f + R[1][0]*0.0f + R[2][0]*0.40f;
    HW_Sample.Mag[1] =  R[0][1]*0.20f + R[1][1]*0.0f + R[2][1]*0.40f;
    HW_Sample.Mag[2] =  R[0][2]*0.20f + R[1][2]*0.0f + R[2][2]*0.40f;

    /*7b) 气压计：真机 DPS310 只给 Pa，所以按 ISA 由高度反算，让高度环走真实路径*/
    float AltM = -Sim_P[2];

    HW_Sample.Pressure    = 101325.0f*powf(1.0f - AltM/44330.0f,5.25588f) + SIM_BAR_NOISE*Simulator_Rand();
    HW_Sample.Temperature = 25.0f;

    /*8) 50Hz 遥测：t, 估计姿态(x10°), 机体系角速度(x10°/s), 四路油门, 高度(cm), 真实姿态(x10°)*/
    static uint32_t Last = 0;

    if(millis() - Last >= 20)
    {
        Last = millis();

        /*模型自己的姿态（真值），用来和 AHRS 估计值对照*/
        float T_Roll  = atan2f(2.0f*(q0*q1 + q2*q3),1.0f - 2.0f*(q1*q1 + q2*q2));
        float T_Pitch = asinf(fmaxf(-1.0f,fminf(1.0f,2.0f*(q0*q2 - q3*q1))));
        float T_Yaw   = atan2f(2.0f*(q0*q3 + q1*q2),1.0f - 2.0f*(q2*q2 + q3*q3));

        printf("SIM,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
               Last,
               (int)(AHRS.Roll *RAD_TO_DEG*10.0f),
               (int)(AHRS.Pitch*RAD_TO_DEG*10.0f),
               (int)(AHRS.Yaw  *RAD_TO_DEG*10.0f),
               (int)(Sim_W[0]*RAD_TO_DEG*10.0f),
               (int)(Sim_W[1]*RAD_TO_DEG*10.0f),
               (int)(Sim_W[2]*RAD_TO_DEG*10.0f),
               SimuThro[0],SimuThro[1],SimuThro[2],SimuThro[3],
               (int)(-Sim_P[2]*100.0f),
               (int)(T_Roll *RAD_TO_DEG*10.0f),
               (int)(T_Pitch*RAD_TO_DEG*10.0f),
               (int)(T_Yaw  *RAD_TO_DEG*10.0f));
    }
}