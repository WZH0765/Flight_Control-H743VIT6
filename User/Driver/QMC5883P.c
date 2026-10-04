#include "Bus.h"
#include <stdint.h>
#include <stdbool.h>
#include "IST8310.h"
#include "QMC5883P.h"
#include "stm32h7xx_hal.h"

bool QMC5883P_Detect(mag_t *Device)
{
    uint8_t ID = 0;
    uint8_t Retry_Num = 5;

    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_INIT);

    while(Retry_Num--)
    {
        Bus_Write(Device->BusDevice,QMC5883P_RA_CONF2,QMC5883P_CONF2_RESET);
        HAL_Delay(30);

        if(Bus_Read(Device->BusDevice,QMC5883P_RA_ID,&ID))
        {
            if(ID == QMC5883P_ID)
            {
                return true;
            }
        }
    }

    return false;
}

void QMC5883P_Init(mag_t *Device)
{
    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_INIT);

    Bus_Write(Device->BusDevice,QMC5883P_RA_CONF2,QMC5883P_CONF2_RESET);
    HAL_Delay(30);

    Bus_Write(Device->BusDevice,QMC5883P_RA_DATA_SIGN,QMC5883P_DATA_SIGN_MAGIC_VALUE);
    Bus_Write(Device->BusDevice,QMC5883P_RA_CONF2,QMC5883P_CONF2_RNG_8G);
    Bus_Write(Device->BusDevice,QMC5883P_RA_CONF1,QMC5883P_CONF1_OSR2_8|
                                                                 QMC5883P_CONF1_OSR1_8|
                                                                 QMC5883P_CONF1_ODR_200HZ|
                                                                 QMC5883P_CONF1_MODE_CONTINUOUS);
    HAL_Delay(10);

    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_FAST);
}

bool QMC5883P_Read(mag_t *Device)
{
    uint8_t Status = 0;
    uint8_t Data[6] = {0};

    if(!Bus_Read(Device->BusDevice,QMC5883P_RA_STATUS,&Status))
    {
        return false;
    }

    if((Status & QMC5883P_STATUS_DRDY_MASK) == 0)
    {
        return false;
    }

    if(!Bus_ReadBuf(Device->BusDevice,QMC5883P_RA_DATA_OUTPUT_X,Data,6))
    {
        return false;
    }

    Device->Mag[0] = -(int16_t)(Data[3]<<8|Data[2]);
    Device->Mag[1] = -(int16_t)(Data[1]<<8|Data[0]);
    Device->Mag[2] = -(int16_t)(Data[5]<<8|Data[4]);

    return true;
}
