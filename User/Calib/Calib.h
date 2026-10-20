#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    SYS_STATE_NORMAL = 0, 
    SYS_STATE_CAL_ACC,    
    SYS_STATE_CAL_MAG,    
    SYS_STATE_CAL_BAR     
} sys_state_t;

extern sys_state_t SystemState;

extern float Acc_Offset[3];
extern float Mag_Offset[3];
extern float Bar_Offset;

void Calibration_Init(void);        // 从 Flash 读取偏移量
void Calibration_Task(void);        // 注册给 Scheduler 的任务
void Calibration_ProcessCmd(char* cmd); // 给 USB_Read 调用的命令解析函数