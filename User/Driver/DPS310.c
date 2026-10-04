#include "Bus.h"
#include "stdio.h"
#include "DPS310.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware.h"
#include "stm32h7xx_hal.h"

static uint8_t DPS310_RegRead(bus_t *Bus,uint8_t Reg)
{
    uint8_t Value = 0;
    Bus_Read(Bus,Reg,&Value);
    return Value;
}

static void DPS310_RegWrite(bus_t *Bus,uint8_t Reg,uint8_t Value)
{
    Bus_Write(Bus,Reg,Value);
}

static void DPS310_WriteBits(bus_t *Bus,uint8_t Reg,uint8_t Mask,uint8_t Bits)
{
    uint8_t Value = DPS310_RegRead(Bus,Reg);

    if((Value & Mask) != Bits)
    {
        Value = (uint8_t)((Value & (~Mask)) | Bits);
        DPS310_RegWrite(Bus, Reg, Value);
    }
}

static void DPS310_SetBits(bus_t *Bus,uint8_t Reg,uint8_t SetBits)
{
    DPS310_WriteBits(Bus,Reg,SetBits,SetBits);
}

static int32_t DPS310_GetSigned(uint32_t Raw,uint8_t Length)
{
    if (Raw&((uint32_t)1 << (Length - 1)))
    {
        return((int32_t)Raw) - ((int32_t)1 << Length);
    }

    return (int32_t)Raw;
}


bool DPS310_Detect(bar_t *Device)
{
    uint8_t Retry_Num = 5;

    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_INIT);

    while(Retry_Num--)
    {
        HAL_Delay(100);

        Bus_Read(Device->BusDevice,DPS310_RA_ID,&Device->ID);

        if(Device->ID == DPS310_ID)
        {
            return true;
        }
    }

    return false;
}

void DPS310_Init(bar_t *Device)
{
    uint8_t Status = 0;
    uint8_t Param[21] = {0};

    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_INIT);

    DPS310_SetBits(Device->BusDevice,DPS310_RA_RESET,DPS310_RESET_BIT_SOFT_RST);
    HAL_Delay(40);

    Status = DPS310_RegRead(Device->BusDevice,DPS310_RA_MEAS_CFG);

    if((Status&DPS310_MEAS_CFG_COEF_RDY) == 0) return;
    if((Status&DPS310_MEAS_CFG_SENSOR_RDY) == 0) return;
    if(!Bus_ReadBuf(Device->BusDevice,DPS310_RA_COEF,Param,18)) return;

    Device->c0  = DPS310_GetSigned
    (((uint32_t)Param[0] << 4)|(((uint32_t)Param[1] >> 4) & 0x0F),12);

    Device->c1  = DPS310_GetSigned
    ((((uint32_t)Param[1] & 0x0F) << 8) | (uint32_t)Param[2],12);

    Device->c00 = DPS310_GetSigned
    (((uint32_t)Param[3] << 12)|((uint32_t)Param[4] << 4)|(((uint32_t)Param[5] >> 4) & 0x0F),20);

    Device->c10 = DPS310_GetSigned
    ((((uint32_t)Param[5] & 0x0F) << 16)|((uint32_t)Param[6] << 8)|(uint32_t)Param[7],20);

    Device->c01 = DPS310_GetSigned
    (((uint32_t)Param[8] << 8)|(uint32_t)Param[9],16);

    Device->c11 = DPS310_GetSigned
    (((uint32_t)Param[10] << 8)|(uint32_t)Param[11],16);

    Device->c20 = DPS310_GetSigned
    (((uint32_t)Param[12] << 8)|(uint32_t)Param[13],16);

    Device->c21 = DPS310_GetSigned
    (((uint32_t)Param[14] << 8)|(uint32_t)Param[15], 16);

    Device->c30 = DPS310_GetSigned
    (((uint32_t)Param[16] << 8)|(uint32_t)Param[17], 16);

    DPS310_WriteBits(Device->BusDevice,DPS310_RA_MEAS_CFG,
    DPS310_MEAS_CFG_MEAS_CTRL_MASK,
    DPS310_MEAS_CFG_MEAS_IDLE);

    DPS310_RegWrite(Device->BusDevice,0x0E,0xA5);
    DPS310_RegWrite(Device->BusDevice,0x0F,0x96);
    DPS310_RegWrite(Device->BusDevice,0x62,0x02);
    DPS310_RegWrite(Device->BusDevice,0x0E,0x00);
    DPS310_RegWrite(Device->BusDevice,0x0F,0x00);

    DPS310_WriteBits(Device->BusDevice,DPS310_RA_MEAS_CFG,
    DPS310_MEAS_CFG_MEAS_CTRL_MASK,
    DPS310_MEAS_CFG_MEAS_TEMP_SING);
    HAL_Delay(40);

    DPS310_SetBits(Device->BusDevice,DPS310_RA_PRS_CFG,
    DPS310_PRS_CFG_BIT_PM_RATE_32HZ|DPS310_PRS_CFG_BIT_PM_PRC_16);

    const uint8_t TMP_COEF_SRCE =
    DPS310_RegRead(Device->BusDevice,DPS310_RA_COEF_SRCE) & DPS310_COEF_SRCE_BIT_TMP_COEF_SRCE;

    DPS310_SetBits(Device->BusDevice,DPS310_RA_TMP_CFG,
    DPS310_TMP_CFG_BIT_TMP_RATE_32HZ|DPS310_TMP_CFG_BIT_TMP_PRC_16|TMP_COEF_SRCE);

    DPS310_SetBits(Device->BusDevice,DPS310_RA_CFG_REG,
    DPS310_CFG_REG_BIT_T_SHIFT|DPS310_CFG_REG_BIT_P_SHIFT);

    DPS310_WriteBits(Device->BusDevice,DPS310_RA_MEAS_CFG,
    DPS310_MEAS_CFG_MEAS_CTRL_MASK,
    DPS310_MEAS_CFG_MEAS_CTRL_CONT);

    Bus_SetSpeed(Device->BusDevice,I2C_SPEED_FAST);
}

bool DPS310_Read(bar_t *Device)
{
    uint8_t Data[6] = {0};

    if((DPS310_RegRead(Device->BusDevice,DPS310_RA_MEAS_CFG)&DPS310_MEAS_CFG_PRS_RDY) == 0)
    {
        return false;
    }

    if(!Bus_ReadBuf(Device->BusDevice,DPS310_RA_PSR_B2,Data,6))
    {
        return false;
    }

    const int32_t Praw = DPS310_GetSigned
    (((uint32_t)Data[0] << 16)|((uint32_t)Data[1] << 8)|(uint32_t)Data[2],24);

    const int32_t Traw = DPS310_GetSigned
    (((uint32_t)Data[3] << 16)|((uint32_t)Data[4] << 8)|(uint32_t)Data[5],24);

    const float kT = 253952.0f;
    const float kP = 253952.0f;

    const float Praw_sc = (float)Praw / kP;
    const float Traw_sc = (float)Traw / kT;

    const float c00 = (float)Device->c00;
    const float c01 = (float)Device->c01;
    const float c10 = (float)Device->c10;
    const float c11 = (float)Device->c11;
    const float c20 = (float)Device->c20;
    const float c21 = (float)Device->c21;
    const float c30 = (float)Device->c30;

    Device->Pressure = c00+Praw_sc*(c10+Praw_sc*(c20 + Praw_sc*c30))+Traw_sc*c01+Traw_sc*Praw_sc * (c11 + Praw_sc * c21);

    const float c0 = (float)Device->c0;
    const float c1 = (float)Device->c1;

    Device->Temperature = c0 * 0.5f + c1 * Traw_sc;

    return true;
}
