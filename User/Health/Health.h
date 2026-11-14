#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    int32_t  Last[3];         //上次值
    uint16_t Count;           //连续不变次数
    uint16_t Limit;           //报警阈值

    uint32_t Tick;            //最后一次更新的时间（ms）
    uint32_t Timeout;         //超时时间（ms）

} health_t;

bool Health_Status(health_t *H);
void Health_Init(health_t *H,uint16_t Limit,uint32_t Timeout);
void Health_Update(health_t *H,int32_t V0,int32_t V1,int32_t V2);
