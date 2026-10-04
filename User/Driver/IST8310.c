#include "Bus.h"
#include <stdint.h>
#include "SysTime.h"
#include <stdbool.h>
#include "IST8310.h"
#include "hardware.h"
#include "stm32h7xx_hal.h"

bool IST8310_Detect(mag_t *Device)
{
    uint8_t ID = 0;
    uint8_t Retry_Num = 5;

    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_INIT);

    while(Retry_Num--)
    {
        HAL_Delay(10);

        if(Bus_Read(Device->BusDevice,IST8310_RA_WHOAMI,&ID))
        {
            if(ID == IST8310_ID)
            {
                return true;
            }
        }
    }

    return false;
}

void IST8310_Init(mag_t *Device)
{
    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_INIT);

    Bus_Write(Device->BusDevice,IST8310_RA_CNTRL1,IST8310_ODR_50_HZ);
    HAL_Delay(5);

    Bus_Write(Device->BusDevice,IST8310_RA_AVERAGE,IST8310_AVG_16);
    HAL_Delay(5);

    Bus_Write(Device->BusDevice,IST8310_RA_PDCNTL,IST8310_PULSE_DURATION);
    HAL_Delay(5);

    /*IST8310为板载磁力计时，I2C总线速度可设置为400KHz*/
    // Bus_SetSpeed(Device->BusDevice,I2C_SPEED_FAST);
}

bool IST8310_Read(mag_t *Device)
{
    uint8_t Data[6] = {0};

    if(!Bus_ReadBuf(Device->BusDevice,IST8310_RA_DATA,Data,6))
    {
        return false;
    }

    Device->Mag[0] =  (int16_t)(Data[1]<<8|Data[0])*3;
    Device->Mag[1] = -(int16_t)(Data[3]<<8|Data[2])*3;
    Device->Mag[2] =  (int16_t)(Data[5]<<8|Data[4])*3;

    return true;
}
