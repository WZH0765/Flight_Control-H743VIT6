#include "scheduler.h"
#include "systime.h"
#include <string.h>

#define MAX_TASKS 16

static uint8_t TaskCount = 0;
static task_t  Task[MAX_TASKS];

/***************
*  调度器初始化：
*
*  形  参：无
*  返回值：无
****************/
void Scheduler_Init(void)
{
    TaskCount = 0;
    memset(Task,0,sizeof(Task));
}

/***************
*  注册任务：
*
*  形  参：任务名、任务函数指针、任务执行周期、任务静态优先级
*  返回值：无
****************/
void Scheduler_Add(const char *Name,void (*Func_Name)(void),uint32_t Period,uint8_t Priority)
{
    if(TaskCount >= MAX_TASKS || Func_Name == NULL) return;

    task_t *Task_Temp = &Task[TaskCount ++];

    Task_Temp->Name           = Name;
    Task_Temp->Enabled        = true;
    Task_Temp->Func_Name      = Func_Name;

    Task_Temp->Period           = Period;
    Task_Temp->Priority         = Priority;
    Task_Temp->LastExecutedTime = micros() - Period;
}

/***************
*  调度器运行：
*
*  形  参：无
*  返回值：无
*  说  明：遍历所有已到期任务，选动态优先级最高的一个执行;每轮只执行一个任务，保证任何任务都不会霸占 CPU
****************/
void Scheduler_Run(void)
{
    uint32_t now = micros();
    uint16_t Task_Priority = 0;
    task_t  *Task_Select = NULL;

    for(uint8_t i = 0;i < TaskCount;i ++)
    {
        task_t *Task_Temp = &Task[i];
        if(!Task_Temp->Enabled) continue;

        uint32_t Elapsed = now - Task_Temp->LastExecutedTime;
        if(Elapsed < Task_Temp->Period) continue;

        uint16_t Dynamic_Priority = Task_Temp->Priority + (Elapsed/Task_Temp->Period);
        if(Dynamic_Priority > Task_Priority)
        {
            Task_Select = Task_Temp;
            Task_Priority = Dynamic_Priority;
        }
    }

    if(Task_Select != NULL)
    {
        Task_Select->Func_Name();
        Task_Select->LastExecutedTime += Task_Select->Period;

        if(micros() - Task_Select->LastExecutedTime > Task_Select->Period)
        {
            Task_Select->LastExecutedTime = micros();
        }
    }
}
