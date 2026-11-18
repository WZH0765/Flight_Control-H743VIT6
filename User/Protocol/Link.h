#pragma once

#include <stdint.h>
#include <stdbool.h>

/******************
*   下行数据链
*
*   帧格式：AA 55 | ID | LEN | 负载 | CRC16
*
*   帧头    2 字节 0xAA 0x55
*   ID      1 字节,消息类型
*   LEN     1 字节,负载字节数
*   负载    int16 数组
*   CRC16   2 字节,对 ID+LEN+负载 计算，多项式 0x1021，初值 0xFFFF
******************/

#define LINK_MAX        48          //最大负载字节数

#define LINK_ATT        0x01        //姿态 50Hz   yaw roll pitch(0.1°) gx gy gz(0.1°/s)
#define LINK_IMU        0x02        //九轴 10Hz   gx gy gz(0.1°/s) ax ay az(cm/s²) mx my mz(LSB) 气压偏移(Pa) 温度(0.1℃)
#define LINK_OFL        0x03        //光流 10Hz   vf vr(mm/s) alt(mm) 光流质量 激光质量
#define LINK_PVRS       0x04        //位置 10Hz   pn pe(cm) vn ve(cm/s) pd(cm) vd(cm/s)
#define LINK_STATUS     0x10        //状态  5Hz   六路健康位 + 解锁状态 + 保留

void Link_Run(void);
void Link_Send(uint8_t ID,const int16_t *Data,uint8_t Count);