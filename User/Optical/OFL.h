#pragma once

#include <stdint.h>
#include <stdbool.h>

/******************
*   光流 + 单点 TOF
*
*   帧格式（固定 18 字节，小端）：
*     FE 0E | flow_x i32 | flow_y i32 | span u16 | dist u16 | valid | conf | XOR | 55
*                              ↑ rad×1e6      ↑ us        ↑ mm
*
*   注  意：光流只在 5m 以下可信
******************/

#define OFL_VALID     0xF5

#define OFL_SIGN_X    (-1.0f)       //光流 X 轴方向
#define OFL_SIGN_Y    ( 1.0f)       //光流 Y 轴方向

typedef struct
{
    float Alt;                      //离地高度(m)
    float Vf,Vr;                    //水平速度(m/s)
    
    uint8_t Quality;                //激光置信度 0~100

    bool New;                       //有新的一帧
    bool Valid;                     //光流是否可用

} ofl_t;

extern ofl_t OFL;
extern uint8_t OFL_RxBuffer[64];

void OFL_Start(void);
void OFL_Parse(uint8_t *Buffer,uint16_t Len);