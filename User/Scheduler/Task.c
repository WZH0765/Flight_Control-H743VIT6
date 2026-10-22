#include "Protocol.h"
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

void AHRS_Control(void)
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

    /*使用硬件层换算后的机体系数据*/
    AHRS_Update(HW_Sample.Acc[0],HW_Sample.Acc[1],HW_Sample.Acc[2],
                HW_Sample.Gyr[0],HW_Sample.Gyr[1],HW_Sample.Gyr[2],
                HW_Sample.Mag[0],HW_Sample.Mag[1],HW_Sample.Mag[2],
                dt);
}

/*
 * 任务注册函数
 */
void Task_Init(void)
{
    Scheduler_Add("AHRS_CTRL",AHRS_Control    ,1000   ,TASK_PRIORITY_HIGH    );

    Scheduler_Add("IMU_READ" ,Hardware_IMURead,1000   ,TASK_PRIORITY_REALTIME);
    Scheduler_Add("MAG_READ" ,Hardware_MAGRead,10000  ,TASK_PRIORITY_MED     );
    Scheduler_Add("BAR_READ" ,Hardware_BARRead,20000  ,TASK_PRIORITY_MED_LOW );
    Scheduler_Add("PROTOCOL" ,Protocol_Receive,10000  ,TASK_PRIORITY_LOW     );
    Scheduler_Add("LED"      ,LED_Blink       ,1000000,TASK_PRIORITY_LOW     );
}
