#pragma once

#define ACC_SCALE (2048.0f)
#define GYR_SCALE (16.4f/0.0174533f)

typedef enum
{
    TASK_PRIORITY_LOW      = 1,     //低优先级
    TASK_PRIORITY_MED_LOW  = 2,
    TASK_PRIORITY_MED      = 3,     //中优先级
    TASK_PRIORITY_MED_HIGH = 4,
    TASK_PRIORITY_HIGH     = 5,     //高优先级
    TASK_PRIORITY_REALTIME = 18     //实时优先级

} taskPri_t;

void Task_Init(void);
