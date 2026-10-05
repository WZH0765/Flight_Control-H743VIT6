#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    const char *Name;               //任务名
    bool       Enabled;             //任务启用状态
    void       (*Func_Name)(void);  //任务函数指针

    uint32_t   Period;              //任务执行周期
    uint8_t    Priority;            //任务静态优先级
    uint32_t   LastExecutedTime;    //任务上次执行时刻
    
} task_t;

void Scheduler_Run(void);
void Scheduler_Init(void);
void Scheduler_Add(const char *Name,void (*Func_Name)(void),uint32_t Period,uint8_t Priority);
