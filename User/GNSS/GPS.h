#pragma once

#include <stdint.h>

#define GPS_RX_SIZE 1024

typedef enum
{
    GPS_FIX_INVALID = 0,    //未定位
    GPS_FIX_2D      = 1,    //二维定位
    GPS_FIX_3D      = 2     //三维定位

} gpsFix_e;

typedef struct
{
    uint8_t  Fix;               //定位状态
    uint8_t  SatNum;            //参与定位的卫星数

    uint8_t  Hour;
    uint8_t  Minute;
    uint8_t  Second;

    float    Altitude;          //海拔（米）
    double   Latitude;          //纬度（度）
    double   Longitude;         //经度（度）

    float    GroundSpeed;       //对地速度（m/s）
    float    GroundCourse;      //对地航向（度，0~359.9）

    uint32_t TimeStamp;         //更新时间（ms）

} gpsData_t;

extern gpsData_t GPS_Data;
extern uint8_t GPS_RxBuffer[GPS_RX_SIZE];

void GPS_Start(void);
void GPS_Parse(uint8_t *Buffer,uint16_t Len);