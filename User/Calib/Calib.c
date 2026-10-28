#include "Calib.h"
#include "Flash.h"
#include <stdio.h>
#include <string.h>
#include "SysTime.h"
#include "Hardware.h"

sensor_t Mode  = IMU;
state_t  State = IDLE;

/*校准结果*/
float Acc_Bias[3];
float Gyr_Bias[3];
float Mag_Bias[3];
float Acc_Scal[3];

bool Next = false;
bool IMU_CalValid = false;
bool MAG_CalValid = false;

void Cal_Start(sensor_t Sensor)
{
    if((Sensor == IMU && Mode == IMU && State == WAIT)||
       (Sensor == MAG && Mode == MAG && State == WAIT))
    {
        Next = true;
        return;
    }

    if(State != IDLE)
    {
        printf("CAL,%s,BUSY\r\n",(Sensor == IMU) ? "IMU" : "MAG");
        return;
    }

    Next  = false;
    Mode  = Sensor;
    State = REQUEST;
}

void Cal_Abort(void)
{
    if(State == IDLE) return;

    Next  = false;
    State = IDLE;
    printf("CAL,%s,IDLE,ABORTED\r\n",(Mode == IMU) ? "IMU" : "MAG");
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

    static uint8_t  Face = 0;
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
        State     = COLLECT;

        if(Face < 6)
        {
            printf("CAL,IMU,COLLECT,FACE%d\r\n",Face + 1);
        }
        else
        {
            printf("CAL,IMU,COLLECT,GYRO\r\n");
        }
    }

    switch(State)
    {
        case REQUEST:
            Face      = 0;
            Samples   = 0;
            TimeStamp = Now;
            State     = CHECK;

            memset(Acc_Sum,0,sizeof(Acc_Sum));
            memset(Gyr_Sum,0,sizeof(Gyr_Sum));

            printf("CAL,IMU,CHECK\r\n");
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
                State     = COLLECT;

                printf("CAL,IMU,COLLECT,FACE1\r\n");
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
                    State     = WAIT;

                    if(Face < 6)
                    {
                        printf("CAL,IMU,WAIT,FACE%d\r\n",Face + 1);
                    }
                    else
                    {
                        printf("CAL,IMU,WAIT,GYRO\r\n");
                    }
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
                        IMU_CalValid = false;

                        printf("CAL,IMU,SCALE,%.2f,%.2f,%.2f\r\n",Acc_Scal[0],Acc_Scal[1],Acc_Scal[2]);
                        printf("CAL,IMU,ERROR\r\n");

                        State = IDLE;
                        break;
                    }

                    IMU_CalValid = true;
                    Saved = Flash_Save();
                    printf("CAL,IMU,DONE\r\n");

                    if(Saved == false)
                    {
                        printf("CAL,IMU,SAVE,ERROR\r\n");
                    }

                    State = IDLE;
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

    switch(State)
    {
        case REQUEST:
            Count   = 0;
            Divider = 0;
            State   = COLLECT;

            printf("CAL,MAG,COLLECT\r\n");
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
                printf("CAL,MAG,ERROR\r\n");
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
                printf("CAL,MAG,DONE\r\n");

                if(Saved == false)
                {
                    printf("CAL,MAG,SAVE,ERROR\r\n");
                }
            }

            State = IDLE;
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
