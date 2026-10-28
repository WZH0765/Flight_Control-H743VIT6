#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CAL_FLASH_ADDR       0x081E0000UL
#define CAL_FLASH_SECTOR     7U
#define CAL_FLASH_DATA_SIZE  56U

#define CAL_FLASH_IMU        0x01U
#define CAL_FLASH_MAG        0x02U

typedef struct
{
    uint16_t Size;
    uint8_t  Pad0[2];       //补位0

    float Acc_Bias[3];
    float Acc_Scal[3];
    float Gyr_Bias[3];
    float Mag_Bias[3];

    uint8_t Flags;
    uint8_t Pad1[3];        //补位1

    uint32_t Crc;
    uint8_t Pad2[4];        //补位2

} flash_t;

void Flash_Load(void);
bool Flash_Save(void);
