#include "Calib.h"
#include "Flash.h"
#include <stdio.h>
#include <string.h>
#include "SysTime.h"
#include "Hardware.h"

uint8_t  Face = 0;
sensor_t Mode = IMU;
calstate_e CAL_State = IDLE;

/*****校准结果*****/
float Acc_Bias[3];
float Gyr_Bias[3];
float Mag_Bias[3];
float Acc_Scal[3];

bool Next = false;
bool IMU_CalValid = false;
bool MAG_CalValid = false;

void Cal_Start(sensor_t Sensor)
{
    if((Sensor == IMU && Mode == IMU && CAL_State == WAIT)||
       (Sensor == MAG && Mode == MAG && CAL_State == WAIT))
    {
        Next = true;
        return;
    }

    if(CAL_State != IDLE)
    {
        return;
    }

    Next  = false;
    Mode  = Sensor;
    CAL_State = REQUEST;
}

void Cal_Abort(void)
{
    if(CAL_State == IDLE) return;

    Next  = false;
    CAL_State = IDLE;
}

/***************
*  IMU校准：
*
*  1kHz调用，六面采集 + 陀螺静止采集
****************/
static void Cal_IMU(void)
{
    float    G2,A2;
    uint32_t Now = millis();

    static uint16_t Samples = 0;
    static uint32_t TimeStamp = 0;

    static float    Gyr_Sum[3];
    static float    Acc_Sum[6][3];

    //允许进行下一步操作
    if(Next == true)
    {
        Samples   = 0;
        TimeStamp = Now;
        Next      = false;
        CAL_State = COLLECT;
    }

    switch(CAL_State)
    {
        case REQUEST:
            Face      = 0;
            Samples   = 0;
            TimeStamp = Now;
            CAL_State = CHECK;

            memset(Acc_Sum,0,sizeof(Acc_Sum));
            memset(Gyr_Sum,0,sizeof(Gyr_Sum));

            break;

        case CHECK:
            G2 = HW_Sample.Gyr[0]*HW_Sample.Gyr[0]
               + HW_Sample.Gyr[1]*HW_Sample.Gyr[1]
               + HW_Sample.Gyr[2]*HW_Sample.Gyr[2];

            A2 = HW_Sample.Acc[0]*HW_Sample.Acc[0]
               + HW_Sample.Acc[1]*HW_Sample.Acc[1]
               + HW_Sample.Acc[2]*HW_Sample.Acc[2];

            if(G2 >= ERROR_GYR                   ||
               A2 <  ERROR_ACC_MIN*ERROR_ACC_MIN ||
               A2 >  ERROR_ACC_MAX*ERROR_ACC_MAX)
            {
                TimeStamp = Now;
            }
            else if(Now - TimeStamp >= STILL_MS)
            {
                Face      = 0;
                Samples   = 0;
                TimeStamp = Now;
                CAL_State = COLLECT;
            }
            break;

        case COLLECT:
            if(Now - TimeStamp < SETTLE_MS) break;

            Samples ++;
            if(Face < 6)
            {
                for(uint8_t i = 0;i < 3;i ++)
                {
                    Acc_Sum[Face][i] += HW_Sample.Acc[i];
                }

                //切换等待下一面
                if(Samples >= ACC_SAMPLES)
                {
                    Face ++;
                    Samples   = 0;
                    TimeStamp = Now;
                    CAL_State = WAIT;
                }
            }
            else
            {
                for(uint8_t i = 0;i < 3;i ++)
                {
                    Gyr_Sum[i] += HW_Sample.Gyr[i];
                }

                if(Samples >= GYR_SAMPLES)
                {
                    const uint8_t Plus[3]  = {3,5,1};
                    const uint8_t Minus[3] = {2,4,0};

                    bool Saved = true;
                    bool Valid = true;

                    for(uint8_t i = 0;i < 3;i ++)
                    {
                        float P = Acc_Sum[Plus[i]][i]/ACC_SAMPLES;
                        float M = Acc_Sum[Minus[i]][i]/ACC_SAMPLES;

                        Acc_Bias[i] = (P + M)*0.5f;
                        Acc_Scal[i] = (P - M)/(2.0f*GRAVITY);
                        Gyr_Bias[i] = Gyr_Sum[i]/GYR_SAMPLES;

                        if(Acc_Scal[i] < 0.0f) Valid = false;
                    }

                    if(Valid == false)
                    {
                        CAL_State = IDLE;
                        IMU_CalValid = false;
                        break;
                    }

                    IMU_CalValid = true;
                    Saved = Flash_Save();

                    if(Saved == false) IMU_CalValid = false;

                    CAL_State = IDLE;
                }
            }
            break;

        case IDLE:
            break;
        case WAIT:
            break;
        default:
            break;
    }
}

/***************
*  MAG校准：
*
*  1kHz调用，按100Hz记录三轴极值
****************/
static void Cal_MAG(void)
{
    static float Min[3];
    static float Max[3];

    static uint16_t Count = 0;
    static uint8_t  Divider = 0;

    switch(CAL_State)
    {
        case REQUEST:
            Count   = 0;
            Divider = 0;
            CAL_State = COLLECT;

            break;

        case COLLECT:
            //采样频率降低为执行频率1/10
            if((++ Divider) < 10)
            {
                break;
            }
            else
            {
                Divider = 0;
            }

            if(Count == 0)
            {
                for(uint8_t i = 0;i < 3;i ++)
                {
                    Min[i] = HW_Sample.Mag[i];
                    Max[i] = HW_Sample.Mag[i];
                }
            }
            else
            {
                for(uint8_t i = 0;i < 3;i ++)
                {
                    if(HW_Sample.Mag[i] < Min[i]) Min[i] = HW_Sample.Mag[i];
                    if(HW_Sample.Mag[i] > Max[i]) Max[i] = HW_Sample.Mag[i];
                }
            }
            Count ++;

            if(Count < MAG_SAMPLES) break;

            if(Max[0] - Min[0] < RANGE_MAG_MIN ||
               Max[1] - Min[1] < RANGE_MAG_MIN ||
               Max[2] - Min[2] < RANGE_MAG_MIN)
            {
                MAG_CalValid = false;
            }
            else
            {
                bool Saved = true;
                for(uint8_t i = 0;i < 3;i ++)
                {
                    Mag_Bias[i] = (Max[i] + Min[i])*0.5f;
                }

                MAG_CalValid = true;
                Saved = Flash_Save();

                if(Saved == false) MAG_CalValid = false;
            }

            CAL_State = IDLE;
            break;

        case IDLE:
            break;
        default:
            break;
    }
}

void Cal_Run(void)
{
    if(Mode == IMU)
    {
        Cal_IMU();
    }
    else
    {
        Cal_MAG();
    }
}