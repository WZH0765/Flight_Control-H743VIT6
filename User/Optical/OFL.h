#pragma once

#include <stdint.h>
#include <stdbool.h>

/******************
*   光流 + 单点 TOF
*
*   注  意：光流只在 5m 以下可信
******************/

#define OFL_VALID     0xF5

#define OFL_SIGN_X    ( 1.0f)       //光流 X 轴方向-往右为正
#define OFL_SIGN_Y    (-1.0f)       //光流 Y 轴方向-往前为正

#define OFL_RX_SIZE    64

typedef struct
{
    float Alt;                      //离地高度(m)
    float Vf,Vr;                    //水平速度(m/s)

    bool New;                       //有新的一帧
    
    uint8_t Flow_Qual;              //光流置信度 0~100
    uint8_t Laser_Qual;             //激光置信度 0~100

} ofl_t;

extern ofl_t OFL;
extern uint8_t OFL_RxBuffer[OFL_RX_SIZE];

void OFL_Start(void);
void OFL_Parse(uint8_t *Buf,uint16_t Len);