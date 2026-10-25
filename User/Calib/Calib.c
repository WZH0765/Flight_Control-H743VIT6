#include "Calib.h"
#include <stdio.h>
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
bool IMU_Valid = false;
bool MAG_Valid = false;

void Cal_Start(sensor_t Sensor)
{
    if(Sensor == IMU && Mode == IMU && State == WAIT)
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
    static uint32_t PhaseStart = 0;

    static float    Gyr_Sum[3];
    static float    Acc_Sum[6][3];

    if(Next == true)
    {
        Next       = false;
        Samples    = 0;
        PhaseStart = Now;
        State      = COLLECT;

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
        case IDLE:
        case WAIT:
            break;

        case REQUEST:
            State      = CHECK;
            Face       = 0;
            Samples    = 0;
            PhaseStart = Now;

            for(uint8_t i = 0;i < 6;i ++)
            {
                for(uint8_t j = 0;j < 3;j ++)
                {
                    Acc_Sum[i][j] = 0.0f;
                }
            }

            for(uint8_t i = 0;i < 3;i ++)
            {
                Gyr_Sum[i] = 0.0f;
            }

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
                PhaseStart = Now;
            }
            else if(Now - PhaseStart >= STILL_MS)
            {
                State      = COLLECT;
                Face       = 0;
                Samples    = 0;
                PhaseStart = Now;

                printf("CAL,IMU,COLLECT,FACE1\r\n");
            }
            break;

        case COLLECT:
            if(Now - PhaseStart < SETTLE_MS) break;

            Samples ++;

            if(Face < 6)
            {
                for(uint8_t i = 0;i < 3;i ++)
                {
                    Acc_Sum[Face][i] += HW_Sample.Acc[i];
                }

                if(Samples >= FACE_SAMPLES)
                {
                    Face ++;
                    Samples    = 0;
                    PhaseStart = Now;
                    State      = WAIT;

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

                if(Samples >= GYRO_SAMPLES)
                {
                    /*AHRS期望比力向上（水平时Acc=(0,0,-g)），Plus取Acc为+g的面*/
                    const uint8_t Plus[3]  = {3,5,1};   //FACE4/6/2
                    const uint8_t Minus[3] = {2,4,0};   //FACE3/5/1

                    bool Valid = true;

                    for(uint8_t i = 0;i < 3;i ++)
                    {
                        float P = Acc_Sum[Plus[i]][i]/FACE_SAMPLES;
                        float M = Acc_Sum[Minus[i]][i]/FACE_SAMPLES;

                        Acc_Bias[i] = (P + M)*0.5f;
                        Acc_Scal[i] = (P - M)/(2.0f*GRAVITY);
                        Gyr_Bias[i] = Gyr_Sum[i]/GYRO_SAMPLES;

                        if(Acc_Scal[i] < 0.0f) Valid = false;
                    }

                    if(Valid == false)
                    {
                        IMU_Valid = false;
                        printf("CAL,IMU,SCALE,%.2f,%.2f,%.2f\r\n",Acc_Scal[0],Acc_Scal[1],Acc_Scal[2]);
                        printf("CAL,IMU,ERROR,7\r\n");
                        State = IDLE;
                        break;
                    }

                    IMU_Valid = true;
                    printf("CAL,IMU,DONE\r\n");
                    State = IDLE;
                }
            }
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
    static float    Min[3];
    static float    Max[3];
    
    static uint16_t Count = 0;
    static uint8_t  Divider = 0;

    switch(State)
    {
        case IDLE:
            break;

        case REQUEST:
            State   = COLLECT;
            Count   = 0;
            Divider = 0;

            printf("CAL,MAG,COLLECT\r\n");
            break;

        case COLLECT:
            Divider ++;
            if(Divider < 10) break;

            Divider = 0;
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
            if(Count >= MAG_SAMPLES) State = SOLVE;
            break;

        case SOLVE:
            if(Max[0] - Min[0] < MAG_MIN_RANGE ||
               Max[1] - Min[1] < MAG_MIN_RANGE ||
               Max[2] - Min[2] < MAG_MIN_RANGE)
            {
                printf("CAL,MAG,ERROR,6\r\n");
            }
            else
            {
                for(uint8_t i = 0;i < 3;i ++)
                {
                    Mag_Bias[i] = (Max[i] + Min[i])*0.5f;
                }

                MAG_Valid = true;
                printf("CAL,MAG,DONE\r\n");
            }

            State = IDLE;
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
