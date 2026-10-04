#include "gpio.h"
#include "DPS310.h"
#include "IST8310.h"
#include "hardware.h"
#include "ICM42605.h"
#include "scheduler.h"
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

/*
 * 任务注册函数
 */
void Task_Init(void)
{
    Scheduler_Add("IMU_READ",IMU_Read,1000 ,TASK_PRIORITY_REALTIME);
    Scheduler_Add("MAG_READ",MAG_Read,10000,TASK_PRIORITY_HIGH);
    Scheduler_Add("BAR_READ",BAR_Read,20000,TASK_PRIORITY_MED);
    Scheduler_Add("LED",LED_Blink,1000000,TASK_PRIORITY_LOW);
}
