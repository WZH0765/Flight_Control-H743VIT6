#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    int32_t  Last[3];         //上次值
    uint16_t Count;           //连续不变次数
    uint16_t Limit;           //报警阈值

} health_t;

bool Health_Status(health_t *H);
void Health_Init(health_t *H,uint16_t Limit);
void Health_Update(health_t *H,int32_t V0,int32_t V1,int32_t V2);
