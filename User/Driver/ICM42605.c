#include "Bus.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ICM42605.h"
#include "hardware.h"
#include "stm32h7xx_hal.h"

/*
*   切换ICM42605 Bank
*/
static void ICM42605_BankSet(bus_t *Bus,const uint8_t Bank)
{
    Bus_Write(Bus,ICM426XX_RA_REG_BANK_SEL,Bank&7);
}

bool ICM42605_Detect(imu_t *Device)
{
    uint8_t ID = 0;
    uint8_t Retry_Num = 5;

    Bus_SetSpeed(Device->BusDevice,SPI_SPEED_INIT);

    Bus_Write(Device->BusDevice,ICM42605_RA_PWR_MGMT0,0x00);

    while(Retry_Num--)
    {
        HAL_Delay(150);

        Bus_Read(Device->BusDevice,ICM42605_RA_WHO_AM_I,&ID);

        if(ID == ICM42605_WHO_AM_I)
        {
            return true;
        }
    }

    return false;
}

/*
*   初始化 ICM42605 Acc&Gyro
*/
void ICM42605_Init(imu_t *Device)
{
    uint8_t INT_CONFIG1_Value = 0;
    uint8_t INTF_CONFIG1_Value = 0;

    Bus_SetSpeed(Device->BusDevice,SPI_SPEED_INIT);
    ICM42605_BankSet(Device->BusDevice,ICM426XX_BANK_SELECT0);

    //打开陀螺仪和加速度计，低噪声模式
    Bus_Write(Device->BusDevice,ICM42605_RA_PWR_MGMT0,
              ICM42605_PWR_MGMT0_GYRO_MODE_LN|ICM42605_PWR_MGMT0_ACCEL_MODE_LN|ICM42605_PWR_MGMT0_TEMP_DISABLE_OFF);
    HAL_Delay(15);

    //量程&ODR: ±2000dps/±16g,1kHz
    Bus_Write(Device->BusDevice,ICM42605_RA_GYRO_CONFIG0,
              0x06);
    HAL_Delay(15);

    Bus_Write(Device->BusDevice,ICM42605_RA_ACCEL_CONFIG0,
              0x06);
    HAL_Delay(15);

    //LPF低延迟
    Bus_Write(Device->BusDevice,ICM42605_RA_GYRO_ACCEL_CONFIG0,
              ICM42605_ACCEL_UI_FILT_BW_LOW_LATENCY|ICM42605_GYRO_UI_FILT_BW_LOW_LATENCY);
    HAL_Delay(15);

    //陀螺仪抗混叠滤波器 AAF: Bank1,42Hz
    ICM42605_BankSet(Device->BusDevice,ICM426XX_BANK_SELECT1);
    Bus_Write(Device->BusDevice,ICM426XX_RA_GYRO_CONFIG_STATIC3,ICM42605_GYRO_AAF_DELT);
    Bus_Write(Device->BusDevice,ICM426XX_RA_GYRO_CONFIG_STATIC4,ICM42605_GYRO_AAF_DELTSQR & 0xFF);
    Bus_Write(Device->BusDevice,ICM426XX_RA_GYRO_CONFIG_STATIC5,(ICM42605_GYRO_AAF_DELTSQR >> 8) | (ICM42605_GYRO_AAF_BITSHIFT << 4));

    //加速度计抗混叠滤波器 AAF: Bank2,256Hz
    ICM42605_BankSet(Device->BusDevice,ICM426XX_BANK_SELECT2);
    Bus_Write(Device->BusDevice,ICM426XX_RA_ACCEL_CONFIG_STATIC2,ICM42605_ACCEL_AAF_DELT << 1);
    Bus_Write(Device->BusDevice,ICM426XX_RA_ACCEL_CONFIG_STATIC3,ICM42605_ACCEL_AAF_DELTSQR & 0xFF);
    Bus_Write(Device->BusDevice,ICM426XX_RA_ACCEL_CONFIG_STATIC4,(ICM42605_ACCEL_AAF_DELTSQR >> 8) | (ICM42605_ACCEL_AAF_BITSHIFT << 4));

    //配置 ICM42605 中断输出为脉冲模式，推挽输出，高电平有效
    ICM42605_BankSet(Device->BusDevice,ICM426XX_BANK_SELECT0);
    Bus_Write(Device->BusDevice,ICM42605_RA_INT_CONFIG,
             ICM42605_INT1_MODE_PULSED|ICM42605_INT1_DRIVE_CIRCUIT_PP|ICM42605_INT1_POLARITY_ACTIVE_HIGH);
    HAL_Delay(15);

    Bus_Write(Device->BusDevice,ICM42605_RA_INT_CONFIG0,
              ICM42605_UI_DRDY_INT_CLEAR_ON_SBR);
    HAL_Delay(100);

    Bus_Write(Device->BusDevice,ICM42605_RA_INT_SOURCE0,
              ICM42605_UI_DRDY_INT1_EN_ENABLED);

    //配置 INT1 异步复位、脉冲宽度和去激活时间
    Bus_Read(Device->BusDevice,ICM42605_RA_INT_CONFIG1,
             &INT_CONFIG1_Value);

    INT_CONFIG1_Value &= ~(1 << ICM42605_INT_ASYNC_RESET_BIT);
    INT_CONFIG1_Value |=  (ICM42605_INT_TPULSE_DURATION_8|ICM42605_INT_TDEASSERT_DISABLED);

    Bus_Write(Device->BusDevice,ICM42605_RA_INT_CONFIG1,
              INT_CONFIG1_Value);
    HAL_Delay(15);

    //配置 ICM42605 接口配置寄存器，禁用自动范围切换
    Bus_Read(Device->BusDevice,ICM42605_INTF_CONFIG1,
             &INTF_CONFIG1_Value);

    INTF_CONFIG1_Value &= ~ICM42605_INTF_CONFIG1_AFSR_MASK;
    INTF_CONFIG1_Value |=  ICM42605_INTF_CONFIG1_AFSR_DISABLE;

    Bus_Write(Device->BusDevice,ICM42605_INTF_CONFIG1,
              INTF_CONFIG1_Value);
    HAL_Delay(15);

    Bus_SetSpeed(Device->BusDevice,SPI_SPEED_FAST);
}

/*
*   读取ICM42605 Acc
*/
bool ICM42605_AccRead(imu_t *Device)
{
    uint8_t Data[6] = {0};

    const bool ack = Bus_ReadBuf(Device->BusDevice,ICM42605_RA_ACCEL_DATA_X1,Data,6);
    if(!ack)
    {
        return false;
    }

    Device->Acc[0] = (float)INT16_BE(&Data[0]);
    Device->Acc[1] = (float)INT16_BE(&Data[2]);
    Device->Acc[2] = (float)INT16_BE(&Data[4]);

    return true;
}

/*
*   读取ICM42605 Gyro
*/
bool ICM42605_GyroRead(imu_t *Device)
{
    uint8_t Data[6] = {0};

    const bool ack = Bus_ReadBuf(Device->BusDevice,ICM42605_RA_GYRO_DATA_X1,Data,6);
    if(!ack)
    {
        return false;
    }

    Device->Gyro[0] = (float)INT16_BE(&Data[0]);
    Device->Gyro[1] = (float)INT16_BE(&Data[2]);
    Device->Gyro[2] = (float)INT16_BE(&Data[4]);

    return true;
}

/*
*   读取ICM42605 Temperature
*/
bool ICM42605_TempRead(imu_t *Device)
{
    uint8_t Data[2] = {0};

    const bool ack = Bus_ReadBuf(Device->BusDevice,ICM42605_RA_TEMP_DATA1,Data,2);
    if(!ack)
    {
        return false;
    }
    Device->Temp = (INT16_BE(&Data[0]) / 13.248) + 250;

    return true;
}
