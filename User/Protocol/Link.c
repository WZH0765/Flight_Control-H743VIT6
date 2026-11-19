#include "Crc.h"
#include "OFL.h"
#include <math.h>
#include "AHRS.h"
#include "Link.h"
#include "PVRS.h"
#include "Task.h"
#include "Flight.h"
#include "Health.h"
#include "SysTime.h"
#include "Hardware.h"
#include "usbd_cdc_if.h"

static uint8_t Link_Buf[LINK_MAX + 6];      //头2 + ID1 + LEN1 + 负载 + CRC2

/***************
*  发一帧
*
*  形  参：ID 消息类型，Data int16 数组，Count 个数
****************/
void Link_Send(uint8_t ID,const int16_t *Data,uint8_t Count)
{
    uint8_t Len = Count*2;

    Link_Buf[0] = 0xAA;
    Link_Buf[1] = 0x55;
    Link_Buf[2] = ID;
    Link_Buf[3] = Len;

    for(uint8_t i = 0;i < Len;i ++)
    {
        Link_Buf[4 + i] = ((const uint8_t *)Data)[i];
    }

    uint16_t Crc = CRC16(&Link_Buf[2],Len + 2);

    Link_Buf[4 + Len] = (uint8_t)Crc;
    Link_Buf[5 + Len] = (uint8_t)(Crc >> 8);

    CDC_Transmit_FS(Link_Buf,Len + 6);
}

void Link_Run(void)
{
    int16_t Buf[11];
    uint32_t Now = millis();
    static uint32_t Tick[5];

/*姿态 50Hz*/
    if(Now - Tick[0] >= 20)
    {
        Tick[0] = Now;

        Buf[0] = (int16_t)(AHRS.Yaw  *RAD_TO_DEG*10.0f);
        Buf[1] = (int16_t)(AHRS.Roll *RAD_TO_DEG*10.0f);
        Buf[2] = (int16_t)(AHRS.Pitch*RAD_TO_DEG*10.0f);
        Buf[3] = (int16_t)(HW_Body.Gyr[0]*RAD_TO_DEG*10.0f);
        Buf[4] = (int16_t)(HW_Body.Gyr[1]*RAD_TO_DEG*10.0f);
        Buf[5] = (int16_t)(HW_Body.Gyr[2]*RAD_TO_DEG*10.0f);

        Link_Send(LINK_ATT,Buf,6);
        return;
    }

/*九轴 + 气压 10Hz*/
    if(Now - Tick[1] >= 100)
    {
        Tick[1] = Now;

        Buf[0]  = (int16_t)(HW_Sample.Gyr[0]*RAD_TO_DEG*10.0f);
        Buf[1]  = (int16_t)(HW_Sample.Gyr[1]*RAD_TO_DEG*10.0f);
        Buf[2]  = (int16_t)(HW_Sample.Gyr[2]*RAD_TO_DEG*10.0f);
        Buf[3]  = (int16_t)(HW_Sample.Acc[0]*100.0f);
        Buf[4]  = (int16_t)(HW_Sample.Acc[1]*100.0f);
        Buf[5]  = (int16_t)(HW_Sample.Acc[2]*100.0f);
        Buf[6]  = (int16_t)HW_Sample.Mag[0];
        Buf[7]  = (int16_t)HW_Sample.Mag[1];
        Buf[8]  = (int16_t)HW_Sample.Mag[2];
        Buf[9]  = (int16_t)(HW_Sample.Pressure - 90000.0f);    //气压 0.001kPa（1Pa）
        Buf[10] = (int16_t)(HW_Sample.Temperature*10.0f);

        Link_Send(LINK_IMU,Buf,11);
        return;
    }

/*光流 + 激光 10Hz*/
    if(Now - Tick[2] >= 100)
    {
        Tick[2] = Now;

        Buf[0] = (int16_t)(OFL.Vf*1000.0f);
        Buf[1] = (int16_t)(OFL.Vr*1000.0f);
        Buf[2] = (int16_t)(OFL.Alt*1000.0f);
        Buf[3] = OFL.Flow_Qual;
        Buf[4] = OFL.Laser_Qual;

        Link_Send(LINK_OFL,Buf,5);
        return;
    }

/*位置速度 10Hz*/
    if(Now - Tick[3] >= 100)
    {
        Tick[3] = Now;

        Buf[0] = (int16_t)(PVRS.Pn*100.0f);
        Buf[1] = (int16_t)(PVRS.Pe*100.0f);
        Buf[2] = (int16_t)(PVRS.Vn*100.0f);
        Buf[3] = (int16_t)(PVRS.Ve*100.0f);
        Buf[4] = (int16_t)(PVRS.Pd*100.0f);
        Buf[5] = (int16_t)(PVRS.Vd*100.0f);

        Link_Send(LINK_PVRS,Buf,6);
        return;
    }

/*健康 状态 5Hz*/
    if(Now - Tick[4] >= 200)
    {
        Tick[4] = Now;

        Buf[0] = Health_Status(&GYR_Health);
        Buf[1] = Health_Status(&ACC_Health);
        Buf[2] = Health_Status(&MAG_Health);
        Buf[3] = Health_Status(&BAR_Health);
        Buf[4] = Health_Status(&OFL_Health);
        Buf[5] = Health_Status(&GPS_Health);
        Buf[6] = (int16_t)State;
        Buf[7] = 0;                                 //保留

        Link_Send(LINK_STATUS,Buf,8);
    }
}