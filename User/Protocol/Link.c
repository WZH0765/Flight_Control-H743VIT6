#include "Crc.h"
#include "OFL.h"
#include <math.h>
#include "AHRS.h"
#include "Link.h"
#include "PVRS.h"
#include "Task.h"
#include "Motor.h"
#include "Calib.h"
#include "Flight.h"
#include "Health.h"
#include "SysTime.h"
#include "Hardware.h"
#include "usbd_cdc_if.h"

static uint8_t Tx_Buf[LINK_MAX + 6];      //头2 + ID1 + LEN1 + 负载 + CRC2
static uint8_t Rx_Buf[LINK_MAX + 6];

static volatile uint16_t Rx_Head = 0;           //Rx头
static volatile uint16_t Rx_Tail = 0;           //Rx尾
static volatile uint8_t Rx_Ring[RX_SIZE];       //Rx环形缓冲区

/***************
*  发一帧
*
*  形  参：ID 消息类型，Data int16 数组，Count 个数
****************/
void Link_Tx(uint8_t ID,const int16_t *Data,uint8_t Count)
{
    uint8_t Len = Count*2;

    Tx_Buf[0] = 0xAA;
    Tx_Buf[1] = 0x55;
    Tx_Buf[2] = ID;
    Tx_Buf[3] = Len;

    for(uint8_t i = 0;i < Len;i ++)
    {
        Tx_Buf[4 + i] = ((const uint8_t *)Data)[i];
    }

    uint16_t Crc = CRC16(&Tx_Buf[2],Len + 2);

    Tx_Buf[4 + Len] = (uint8_t)Crc;
    Tx_Buf[5 + Len] = (uint8_t)(Crc >> 8);

    CDC_Transmit_FS(Tx_Buf,Len + 6);
}

/***************
*  收字节
*
*  形  参：Buf 数据，Len 长度
****************/
void Link_Rx(uint8_t *Buf,uint32_t Len)
{
    for(uint32_t i = 0;i < Len;i ++)
    {
        uint16_t Next = (Rx_Head + 1) % RX_SIZE;

        if(Next == Rx_Tail) return;
        Rx_Ring[Rx_Head] = Buf[i];
        Rx_Head = Next;
    }
}

void Link_Handle(uint8_t ID,const uint8_t *Data)
{
    switch(ID)
    {
        case LINK_CMD_STATE:                    //解锁/上锁
            if(Data[0] == 0)
            {
                Motor_Stop();
                State = DISARM;
            }
            else
            {
                Flight_Init();
                State = ARM;
            }
            break;

        case LINK_CMD_CAL:                      //校准
            if(Data[1] == 0)
            {
                Cal_Start(Data[0] ? MAG : IMU);
            }
            else
            {
                Cal_Abort();
            }
            break;

        case LINK_CMD_SIMU:                     //模拟器摇杆
            Flight_Stick(GET16(&Data[0]),GET16(&Data[2]),GET16(&Data[4]),GET16(&Data[6]));
            break;
    }
}

void Link_Parse(void)
{
    uint8_t Rx_Len = 0;

    while(Rx_Tail != Rx_Head)
    {
        if(Rx_Len >= sizeof(Rx_Buf)) Rx_Len = 0;

        Rx_Buf[Rx_Len ++] = Rx_Ring[Rx_Tail];
        Rx_Tail = (Rx_Tail + 1) % RX_SIZE;

        if(Rx_Len < 4) continue;
        if(Rx_Buf[0] != 0xAA || Rx_Buf[1] != 0x55)
        {
            Rx_Len = 0;
            continue;
        }

        uint8_t Total = Rx_Buf[3] + 6;
        if(Total > sizeof(Rx_Buf))
        {
            Rx_Len = 0;
            continue;
        }

        if(Rx_Len < Total) continue;
        uint16_t Crc = Rx_Buf[4 + Rx_Buf[3]] | ((uint16_t)Rx_Buf[5 + Rx_Buf[3]] << 8);
        if(CRC16(&Rx_Buf[2],Rx_Buf[3] + 2) == Crc) Link_Handle(Rx_Buf[2],&Rx_Buf[4]);

        Rx_Len = 0;
    }
}

void Link_Run(void)
{
    int16_t Buf[11];
    uint32_t Now = millis();
    static uint32_t Tick[6];

    Link_Parse();

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

        Link_Tx(LINK_ATT,Buf,6);
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

        Link_Tx(LINK_IMU,Buf,11);
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

        Link_Tx(LINK_OFL,Buf,5);
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

        Link_Tx(LINK_PVRS,Buf,6);
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

        Link_Tx(LINK_STATUS,Buf,8);
        return;
    }

/*校准状态 5Hz；结束后再补发 0.4 秒，让上位机收到"已结束"这一帧*/
    static uint32_t Cal_Last = 0;

    if(CAL_State != IDLE) Cal_Last = Now;

    if(Now - Cal_Last < 400 && Now - Tick[5] >= 200)
    {
        Tick[5] = Now;

        Buf[0] = (int16_t)Mode;
        Buf[1] = (int16_t)CAL_State;
        Buf[2] = (int16_t)Face;
        Buf[3] = IMU_CalValid;
        Buf[4] = MAG_CalValid;

        Link_Tx(LINK_CAL,Buf,5);
    }
}