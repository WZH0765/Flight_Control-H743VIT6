#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PVRS_N      9           //9状态-位置3 速度3 零偏3

#define Q_ACC       0.05f       //加速度计白噪声（m/s²）
#define Q_BIAS      0.001f      //零偏随机游走（m/s²/√s）

#define P_POS       1.0f        //位置（m）
#define P_VEL       1.0f        //速度（m/s）
#define P_BIAS      0.5f        //零偏（m/s²）

#define R_ALT       0.05f       //高度（m）
#define R_FLOW      0.05f       //光流测速（m/s）
#define R_STILL     0.02f       //落地判零（m/s）

typedef struct
{
  union
  {
    float X[PVRS_N];
    struct
    {
      float Pn,Pe,Pd;     //位置 NED
      float Vn,Ve,Vd;     //速度 NED
      float Bx,By,Bz;     //加速度计零偏
    };
  };

  float P[PVRS_N][PVRS_N];    //协方差

} pvrs_t;

extern pvrs_t PVRS;

void PVRS_Init(void);
void PVRS_Still(void);
void PVRS_Height(float Alt);
void PVRS_Flow(float Vf,float Vr);
void PVRS_Update(float Ax,float Ay,float Az,float dt);