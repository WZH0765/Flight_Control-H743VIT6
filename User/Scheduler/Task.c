#include "Protocol.h"
#include "Calib.h"
#include "gpio.h"
#include "Task.h"
#include "AHRS.h"
#include "SysTime.h"
#include "hardware.h"
#include "Scheduler.h"

/**********
*   任务函数
**********/
void LED_Blink(void)
{
    HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);
}

void AHRS_Ctrl(void)
{
    uint32_t Now = micros();
    static uint32_t Last = 0;
    float Acc[3],Gyr[3],Mag[3];

    if(Last == 0)           //第一次执行记为无效
    {
        Last = Now;
        return;
    }

    float dt = (Now - Last)*1e-6f;
    Last = Now;
    if(dt < 0.0008f || dt > 0.0012f) return;        //保护执行周期1ms

    for(uint8_t i = 0;i < 3;i ++)
    {
        Acc[i] = HW_Sample.Acc[i];
        Gyr[i] = HW_Sample.Gyr[i];
        Mag[i] = HW_Sample.Mag[i];

        if(IMU_CalValid == true)
        {
            Acc[i] = (Acc[i] - Acc_Bias[i])/Acc_Scal[i];
            Gyr[i] =  Gyr[i] - Gyr_Bias[i];
        }
        if(MAG_CalValid == true)
        {
            Mag[i] = Mag[i] - Mag_Bias[i];
        }
    }

    /*使用校准后的机体系数据*/
    AHRS_Update(Acc[0],Acc[1],Acc[2],Gyr[0],Gyr[1],Gyr[2],Mag[0],Mag[1],Mag[2],dt);
}

/*
 * 任务注册函数
 */
void Task_Init(void)
{
    Scheduler_Add("AHRS_CTRL" ,AHRS_Ctrl,1000   ,TASK_PRIORITY_HIGH    );

    Scheduler_Add("IMU_READ"  ,IMU_Read    ,1000   ,TASK_PRIORITY_REALTIME);
    Scheduler_Add("MAG_READ"  ,MAG_Read    ,10000  ,TASK_PRIORITY_MED     );
    Scheduler_Add("BAR_READ"  ,BAR_Read    ,20000  ,TASK_PRIORITY_MED_LOW );
    Scheduler_Add("SENSOR_CAL",Cal_Run     ,1000   ,TASK_PRIORITY_LOW     );
    Scheduler_Add("PROTOCOL"  ,Prot_Receive,10000  ,TASK_PRIORITY_LOW     );
    Scheduler_Add("LED"       ,LED_Blink   ,1000000,TASK_PRIORITY_LOW     );
}
