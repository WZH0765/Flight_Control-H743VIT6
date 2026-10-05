#include "gpio.h"
#include "Task.h"
#include "AHRS.h"
#include "DPS310.h"
#include "SysTime.h"
#include "IST8310.h"
#include "hardware.h"
#include "ICM42605.h"
#include "Scheduler.h"
#include "stm32h7xx_hal_gpio.h"

/*
* 任务函数
*/
void IMU_Read(void)
{
    ICM42605_AccRead(&ICM42605_Device1);
    ICM42605_GyroRead(&ICM42605_Device1);
}

void MAG_Read(void)
{
    IST8310_Read(&IST8310_Device);
}

void BAR_Read(void)
{
    DPS310_Read(&DPS310_Device);
}

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
    if(dt < 0.0008f || dt > 0.0012f) return;        //保护执行周期1ms

    float Ax = ICM42605_Device1.Acc[0]/ACC_SCALE;
    float Ay = ICM42605_Device1.Acc[1]/ACC_SCALE;
    float Az = ICM42605_Device1.Acc[2]/ACC_SCALE;

    float Gx = ICM42605_Device1.Gyro[0]/GYR_SCALE;
    float Gy = ICM42605_Device1.Gyro[1]/GYR_SCALE;
    float Gz = ICM42605_Device1.Gyro[2]/GYR_SCALE;

    float Mx = (float)IST8310_Device.Mag[0];
    float My = (float)IST8310_Device.Mag[1];
    float Mz = (float)IST8310_Device.Mag[2];

    AHRS_Update(Ax,Ay,Az,Gx,Gy,Gz,Mx,My,Mz,dt);
    Last = Now;
}

/*
 * 任务注册函数
 */
void Task_Init(void)
{
    Scheduler_Add("AHRS_CTRL",AHRS_Ctrl,1000   ,TASK_PRIORITY_HIGH    );

    Scheduler_Add("IMU_READ" ,IMU_Read ,1000   ,TASK_PRIORITY_REALTIME);
    Scheduler_Add("MAG_READ" ,MAG_Read ,10000  ,TASK_PRIORITY_MED     );
    Scheduler_Add("BAR_READ" ,BAR_Read ,20000  ,TASK_PRIORITY_MED_LOW );
    Scheduler_Add("LED"      ,LED_Blink,1000000,TASK_PRIORITY_LOW     );
}
