#include "Task.h"
#include "Calib.h"
#include <stdio.h>
#include <string.h>
#include "DPS310.h"
#include "ICM42605.h"
#include "QMC5883P.h"

sys_state_t SystemState = SYS_STATE_NORMAL;

float Acc_Offset[3] = {0};
float Mag_Offset[3] = {0};
float Bar_Offset = 0;

void Calibration_Init(void)
{
    // 以后在这里从 Flash 读取校准参数
}

void Calibration_ProcessCmd(char* cmd)
{
    if (strcmp(cmd, "CAL,ACC") == 0)
    {
        SystemState = SYS_STATE_CAL_ACC;
        printf("CAL,ACC,START\r\n");
    } 
    else if (strcmp(cmd, "CAL,STOP") == 0)
    {
        SystemState = SYS_STATE_NORMAL;
        printf("CAL,STOP,DONE\r\n");
    }
}
