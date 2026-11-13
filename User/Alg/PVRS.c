#include "PVRS.h"
#include <math.h>
#include "AHRS.h"
#include <string.h>
#include "Hardware.h"

/*Position and Velocity Reference System - 平移估计器*/

/******************
*    算法物理坐标-标准NED
*         X
*         |
*         ▲---Y
*
*      Z轴向下
******************/

pvrs_t PVRS = {0};

static float Phi[PVRS_N][PVRS_N];
static float Tmp[PVRS_N][PVRS_N];

void PVRS_Init(void)
{
    memset(&PVRS,0,sizeof(pvrs_t));

    for(uint8_t i = 0;i < 3;i ++)
    {
        PVRS.P[i  ][i  ] = P_POS *P_POS;
        PVRS.P[i+3][i+3] = P_VEL *P_VEL;
        PVRS.P[i+6][i+6] = P_BIAS*P_BIAS;
    }
}

/***************
*  预测
*
*  形  参：Ax/Ay/Az 机体系加速度（m/s²），dt 周期（s）
****************/
void PVRS_Update(float Ax,float Ay,float Az,float dt)
{
    float Cy = cosf(AHRS.Yaw)  , Sy = sinf(AHRS.Yaw);
    float Cr = cosf(AHRS.Roll) , Sr = sinf(AHRS.Roll);
    float Cp = cosf(AHRS.Pitch), Sp = sinf(AHRS.Pitch);

    float R[3][3];
    R[0][0] =  Cy*Cp; R[0][1] = Cy*Sp*Sr - Sy*Cr; R[0][2] = Cy*Sp*Cr + Sy*Sr;
    R[1][0] =  Sy*Cp; R[1][1] = Sy*Sp*Sr + Cy*Cr; R[1][2] = Sy*Sp*Cr - Cy*Sr;
    R[2][0] = -Sp;    R[2][1] = Cp*Sr;            R[2][2] = Cp*Cr;

    float Fx = Ax - PVRS.Bx;
    float Fy = Ay - PVRS.By;
    float Fz = Az - PVRS.Bz;

    float An = R[0][0]*Fx + R[0][1]*Fy + R[0][2]*Fz;
    float Ae = R[1][0]*Fx + R[1][1]*Fy + R[1][2]*Fz;
    float Ad = R[2][0]*Fx + R[2][1]*Fy + R[2][2]*Fz + GRAVITY;

    PVRS.Pn += PVRS.Vn*dt;
    PVRS.Pe += PVRS.Ve*dt;
    PVRS.Pd += PVRS.Vd*dt;

    PVRS.Vn += An*dt;
    PVRS.Ve += Ae*dt;
    PVRS.Vd += Ad*dt;

    memset(Phi,0,sizeof(Phi));

    for(uint8_t i = 0;i < 3;i ++)
    {
        Phi[i  ][i  ] = 1.0f;
        Phi[i+3][i+3] = 1.0f;
        Phi[i+6][i+6] = 1.0f;

        Phi[i][i+3] = dt;                       //p' = p + v·dt

        for(uint8_t j = 0;j < 3;j ++)
        {
            Phi[i+3][6+j] = -R[i][j]*dt;        //v' = v + (R·a)·dt
        }
    }

    /*P = Φ·P·Φᵀ + Q*/
    for(uint8_t i = 0;i < PVRS_N;i ++)
    {
        for(uint8_t j = 0;j < PVRS_N;j ++)
        {
            float Sum = 0.0f;

            for(uint8_t k = 0;k < PVRS_N;k ++) Sum += Phi[i][k]*PVRS.P[k][j];
            Tmp[i][j] = Sum;
        }
    }

    for(uint8_t i = 0;i < PVRS_N;i ++)
    {
        for(uint8_t j = 0;j < PVRS_N;j ++)
        {
            float Sum = 0.0f;

            for(uint8_t k = 0;k < PVRS_N;k ++) Sum += Tmp[i][k]*Phi[j][k];   //Φᵀ
            PVRS.P[i][j] = Sum;
        }
    }

    float Qa = Q_ACC *Q_ACC *dt;
    float Qb = Q_BIAS*Q_BIAS*dt;

    for(uint8_t i = 3;i < 6;i ++) PVRS.P[i][i] += Qa;
    for(uint8_t i = 6;i < 9;i ++) PVRS.P[i][i] += Qb;
}

void PVRS_Correct(uint8_t Index,float Meas,float Sigma)
{
    float Row[PVRS_N];
    float Gain[PVRS_N];
    float S = PVRS.P[Index][Index] + Sigma*Sigma;

    for(uint8_t i = 0;i < PVRS_N;i ++)
    {
        Row[i]  = PVRS.P[Index][i];
        Gain[i] = PVRS.P[i][Index]/S;
    }

    float Err = Meas - PVRS.X[Index];
    for(uint8_t i = 0;i < PVRS_N;i ++) PVRS.X[i] += Gain[i]*Err;

    /*P = (I - K·H)·P*/
    for(uint8_t i = 0;i < PVRS_N;i ++)
    {
        for(uint8_t j = 0;j < PVRS_N;j ++)
        {
            PVRS.P[i][j] -= Gain[i]*Row[j];
        }
    }

    for(uint8_t i = 0;i < PVRS_N;i ++)
    {
        for(uint8_t j = 0;j < i;j ++)
        {
            float Sym = (PVRS.P[i][j] + PVRS.P[j][i])*0.5f;

            PVRS.P[i][j] = Sym;
            PVRS.P[j][i] = Sym;
        }
    }
}

/***************
*  光流修正
*
*  形  参：Vf/Vr 机体系水平速度（前/右，m/s，光流测的原始方向）
*  说  明：按航向角转到 NED，倾角小的时候不修正不影响精度
****************/
void PVRS_Flow(float Vf,float Vr)
{
    float Cy = cosf(AHRS.Yaw), Sy = sinf(AHRS.Yaw);

    PVRS_Correct(3, Cy*Vf - Sy*Vr,R_FLOW);
    PVRS_Correct(4, Sy*Vf + Cy*Vr,R_FLOW);
}

/***************
*  高度修正
*
*  形  参：Alt 离地高度（m，向上为正）
****************/
void PVRS_Height(float Alt)
{
    PVRS_Correct(2,-Alt,R_ALT);             //D 向下为正，高度往上 → 取负
}

/***************
*  静止修正
*
*  说  明：落地/手持不动时调用，把三个速度拉回 0，顺便把零偏估准
****************/
void PVRS_Still(void)
{
    PVRS_Correct(3,0.0f,R_STILL);
    PVRS_Correct(4,0.0f,R_STILL);
    PVRS_Correct(5,0.0f,R_STILL);
}