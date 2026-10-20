#include "USB.h"
#include "gpio.h"
#include "Task.h"
#include "AHRS.h"
#include "DPS310.h"
#include "SysTime.h"
#include "IST8310.h"
#include "QMC5883P.h"
#include "ICM42605.h"
#include "hardware.h"
#include "Scheduler.h"
#include "scheduler.h"
#include "usbd_cdc_if.h"
#include "stm32h7xx_hal_gpio.h"

/*******数据健康管理*******/
health_t ACC_Health;
health_t GYR_Health;
health_t MAG_Health;
health_t BAR_Health;

/**********
*   任务函数
**********/
void IMU_Read(void)
{
    ICM42605_AccRead(&ICM42605_Device1);
    Health_Update(&ACC_Health,ICM42605_Device1.Acc[0],ICM42605_Device1.Acc[1],ICM42605_Device1.Acc[2]);

    ICM42605_GyrRead(&ICM42605_Device1);
    Health_Update(&GYR_Health,ICM42605_Device1.Gyr[0],ICM42605_Device1.Gyr[1],ICM42605_Device1.Gyr[2]);
}

void MAG_Read(void)
{
#ifdef USE_IST8310
    IST8310_Read(&IST8310_Device);
    Health_Update(&MAG_Health,IST8310_Device.Mag[0],IST8310_Device.Mag[1],IST8310_Device.Mag[2]);
#endif
#ifdef USE_QMC5883P
    QMC5883P_Read(&QMC5883P_Device);
    Health_Update(&MAG_Health,QMC5883P_Device.Mag[0],QMC5883P_Device.Mag[1],QMC5883P_Device.Mag[2]);
#endif
}

void BAR_Read(void)
{
    DPS310_Read(&DPS310_Device);
    Health_Update(&BAR_Health,DPS310_Device.Pressure,DPS310_Device.Temperature,0);
}

void USB_Read(void)
{
    uint8_t Byte;
    static char Cmd_Buffer[64];
    static uint8_t Cmd_Index = 0;

    while(USB_ReadBuf(&Byte))
    {
        if(Byte == '\n' || Byte == '\r')
        {
            if(Cmd_Index > 0)
            {
                Cmd_Buffer[Cmd_Index] = '\0';

        //---------------------------------------//
                
        //---------------------------------------//

                Cmd_Index = 0;
            }
        }
        else
        {
            if(Cmd_Index < sizeof(Cmd_Buffer) - 1)
            {
                Cmd_Buffer[Cmd_Index ++] = Byte;
            }
        }
    }
}

// void IMU_Calib(void)
// {
//     static uint32_t sample_count = 0;
//     static int32_t acc_sum[3] = {0};

//     if(SystemState == SYS_STATE_NORMAL) return;

//     if(SystemState == SYS_STATE_CAL_ACC)
//     {
//         acc_sum[0] += ICM42605_Device1.Acc[0];
//         acc_sum[1] += ICM42605_Device1.Acc[1];
//         acc_sum[2] += ICM42605_Device1.Acc[2];
//         sample_count++;

//         if(sample_count % 500 == 0)
//         {
//             printf("CAL,ACC,%d\r\n", sample_count * 100 / 2000);
//         }

//         if(sample_count >= 2000)
//         {
//             Acc_Offset[0] = (float)acc_sum[0] / 2000.0f;
//             Acc_Offset[1] = (float)acc_sum[1] / 2000.0f;
//             Acc_Offset[2] = ((float)acc_sum[2] / 2000.0f) - ACC_SCALE; 

//             printf("CAL,ACC,DONE,%.2f,%.2f,%.2f\r\n", Acc_Offset[0], Acc_Offset[1], Acc_Offset[2]);
            
//             SystemState = SYS_STATE_NORMAL;
//             sample_count = 0;
//             acc_sum[0] = acc_sum[1] = acc_sum[2] = 0;
//         }
//     }
// }


void LED_Blink(void)
{
    HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);
}

void AHRS_Ctrl(void)
{
    uint32_t Now = micros();
    static uint32_t Last = 0;

    if(Last == 0)           //第一次执行记为无效
    {
        Last = Now;
        return;
    }

    float dt = (Now - Last)*1e-6f;
    Last = Now;
    if(dt < 0.0008f || dt > 0.0012f) return;        //保护执行周期1ms

    /*硬件坐标系 -> 算法坐标系*/
    float Ax = -ICM42605_Device1.Acc[0]/ACC_SCALE;
    float Ay = -ICM42605_Device1.Acc[1]/ACC_SCALE;
    float Az =  ICM42605_Device1.Acc[2]/ACC_SCALE;

    float Gx = -ICM42605_Device1.Gyr[0]/GYR_SCALE;
    float Gy = -ICM42605_Device1.Gyr[1]/GYR_SCALE;
    float Gz =  ICM42605_Device1.Gyr[2]/GYR_SCALE;

#ifdef USE_IST8310
    float Mx = (float)IST8310_Device.Mag[0];
    float My = (float)IST8310_Device.Mag[1];
    float Mz = (float)IST8310_Device.Mag[2];
#endif
#ifdef USE_QMC5883P
    /*硬件坐标系 -> 算法坐标系*/
    float Mx = (float)QMC5883P_Device.Mag[0];
    float My = (float)QMC5883P_Device.Mag[1];
    float Mz = (float)QMC5883P_Device.Mag[2];
#endif

    AHRS_Update(Ax,Ay,Az,Gx,Gy,Gz,Mx,My,Mz,dt);
}

/*
 * 任务注册函数
 */
void Task_Init(void)
{
    Health_Init(&ACC_Health,100);
    Health_Init(&GYR_Health,100);
    Health_Init(&MAG_Health,250);
    Health_Init(&BAR_Health,250);

    Scheduler_Add("AHRS_CTRL",AHRS_Ctrl,1000   ,TASK_PRIORITY_HIGH    );

    Scheduler_Add("IMU_READ" ,IMU_Read ,1000   ,TASK_PRIORITY_REALTIME);
    Scheduler_Add("MAG_READ" ,MAG_Read ,10000  ,TASK_PRIORITY_MED     );
    Scheduler_Add("BAR_READ" ,BAR_Read ,20000  ,TASK_PRIORITY_MED_LOW );
    Scheduler_Add("USB_Read" ,USB_Read ,10000  ,TASK_PRIORITY_LOW     );
    Scheduler_Add("LED"      ,LED_Blink,1000000,TASK_PRIORITY_LOW     );
}
