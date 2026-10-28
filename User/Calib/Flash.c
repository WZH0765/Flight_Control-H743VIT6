#include "Flash.h"
#include "Calib.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_flash_ex.h"

_Static_assert(sizeof(flash_t) == 64, "flash_t size");
_Static_assert(offsetof(flash_t,Crc) == CAL_FLASH_DATA_SIZE, "flash_t crc offset");

/***************
*  Flash校准参数：
*
*  存储在 bank2 最后一个 128KB 扇区；
*  写入后回读校验，失败则清校准标志。
****************/

uint32_t Flash_CRC(const void *Data,uint32_t Length)
{
    const uint8_t *p = (const uint8_t *)Data;
    uint32_t crc = 0xFFFFFFFFU;

    while(Length --)
    {
        crc ^= *p ++;

        for(uint8_t i = 0;i < 8;i ++)
        {
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }

    return ~crc;
}

bool Flash_Valid(const flash_t *Record)
{
    if(Record->Size != CAL_FLASH_DATA_SIZE)
    {
        return false;
    }

    return Record->Crc == Flash_CRC(Record,CAL_FLASH_DATA_SIZE);
}

void Flash_Load(void)
{
    const flash_t *Record = (const flash_t *)CAL_FLASH_ADDR;

    if(Flash_Valid(Record) == false)
    {
        IMU_CalValid = false;
        MAG_CalValid = false;
        return;
    }

    for(uint8_t i = 0;i < 3;i ++)
    {
        Acc_Bias[i] = Record->Acc_Bias[i];
        Acc_Scal[i] = Record->Acc_Scal[i];
        Gyr_Bias[i] = Record->Gyr_Bias[i];
        Mag_Bias[i] = Record->Mag_Bias[i];
    }

    IMU_CalValid = (Record->Flags & CAL_FLASH_IMU) != 0;
    MAG_CalValid = (Record->Flags & CAL_FLASH_MAG) != 0;
}

bool Flash_Prog(uint32_t Address,const flash_t *Record)
{
    const uint8_t *Data = (const uint8_t *)Record;

    for(uint32_t Offset = 0;Offset < sizeof(flash_t);Offset += 32U)
    {
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,Address + Offset,
                             (uint32_t)(uintptr_t)(Data + Offset)) != HAL_OK)
        {
            return false;
        }
    }

    return true;
}

bool Flash_Save(void)
{
    uint32_t SectorError = 0;
    FLASH_EraseInitTypeDef Erase = {0};
    flash_t Record __attribute__((aligned(32)));

    Erase.NbSectors    = 1;
    Erase.Banks        = FLASH_BANK_2;
    Erase.Sector       = CAL_FLASH_SECTOR;
    Erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    Erase.TypeErase    = FLASH_TYPEERASE_SECTORS;

    memset(&Record,0,sizeof(Record));
    Record.Size = CAL_FLASH_DATA_SIZE;

    for(uint8_t i = 0;i < 3;i ++)
    {
        Record.Acc_Bias[i] = Acc_Bias[i];
        Record.Acc_Scal[i] = Acc_Scal[i];
        Record.Gyr_Bias[i] = Gyr_Bias[i];
        Record.Mag_Bias[i] = Mag_Bias[i];
    }

    Record.Flags = 0;

    if(IMU_CalValid == true) Record.Flags |= CAL_FLASH_IMU;
    if(MAG_CalValid == true) Record.Flags |= CAL_FLASH_MAG;

    Record.Crc = Flash_CRC(&Record,CAL_FLASH_DATA_SIZE);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS_BANK2);

    bool Success = HAL_FLASHEx_Erase(&Erase,&SectorError) == HAL_OK &&
                       Flash_Prog(CAL_FLASH_ADDR,&Record);

    HAL_FLASH_Lock();
    const flash_t *Saved = (const flash_t *)CAL_FLASH_ADDR;

    if(Success == true && Flash_Valid(Saved) == true)
    {
        return true;
    }

    IMU_CalValid = false;
    MAG_CalValid = false;
    return false;
}
