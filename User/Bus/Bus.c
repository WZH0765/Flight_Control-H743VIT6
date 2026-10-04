#include "Bus.h"
#include "stdio.h"
#include <string.h>
#include <stdbool.h>
#include "stm32h7xx_hal_def.h"

#define BUS_MAX_LEN     64

/***************
*  设置SPI速度：
*
*  形  参：总线类型、分频系数
*  返回值：无
****************/
static void SPI_SetSpeed(bus_t *Bus,uint32_t Prescaler)
{
    Bus->hspi->Init.BaudRatePrescaler = Prescaler;
    HAL_SPI_Init(Bus->hspi);
}

/***************
*  SPI写单字节
*
*  形  参：总线类型、目标寄存器地址、待写入的数据
*  返回值：bool-成功、失败
****************/
static bool SPI_Write(bus_t *Bus,uint8_t Reg,uint8_t Data)
{
    uint8_t TxBuf[2] = {0};

    TxBuf[0] = Reg & 0x7F;
    TxBuf[1] = Data;

    HAL_GPIO_WritePin(Bus->CsPort,Bus->CsPin,GPIO_PIN_RESET);
    HAL_StatusTypeDef Status = HAL_SPI_Transmit(Bus->hspi,TxBuf,2,100);
    HAL_GPIO_WritePin(Bus->CsPort,Bus->CsPin,GPIO_PIN_SET);

    if(Status == HAL_OK)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/***************
*  SPI读单字节
*
*  形  参：总线类型、目标寄存器地址、读出的数据
*  返回值：bool-成功、失败
****************/
static bool SPI_Read(bus_t *Bus,uint8_t Reg,uint8_t *Data)
{
    uint8_t TxBuf[2] = {0};
    uint8_t RxBuf[2] = {0};

    TxBuf[0] = Reg | 0x80;
    TxBuf[1] = 0x00;

    HAL_GPIO_WritePin(Bus->CsPort,Bus->CsPin,GPIO_PIN_RESET);
    HAL_StatusTypeDef Status = HAL_SPI_TransmitReceive(Bus->hspi,TxBuf,RxBuf,2,100);
    HAL_GPIO_WritePin(Bus->CsPort,Bus->CsPin,GPIO_PIN_SET);

    if(Status == HAL_OK)
    {
        *Data = RxBuf[1];
        return true;
    }
    else
    {
        return false;
    }
}

/***************
*  SPI读多字节
*
*  形  参：总线类型、目标寄存器地址、读出的数据、数据长度
*  返回值：bool-成功、失败
****************/
static bool SPI_ReadBuf(bus_t *Bus,uint8_t Reg,uint8_t *Data,uint16_t Len)
{
    uint8_t TxBuf[BUS_MAX_LEN + 1] = {0};
    uint8_t RxBuf[BUS_MAX_LEN + 1] = {0};

    TxBuf[0] = Reg | 0x80;

    HAL_GPIO_WritePin(Bus->CsPort,Bus->CsPin,GPIO_PIN_RESET);
    HAL_StatusTypeDef Status = HAL_SPI_TransmitReceive(Bus->hspi,TxBuf,RxBuf,Len + 1,100);
    HAL_GPIO_WritePin(Bus->CsPort,Bus->CsPin,GPIO_PIN_SET);

    if(Status == HAL_OK)
    {
        memcpy(Data,&RxBuf[1],Len);
        return true;
    }
    else
    {
        return false;
    }
}

/***************
*  设置I2C速度：
*
*  形  参：总线类型、时序值
*  返回值：无
****************/
static void I2C_SetSpeed(bus_t *Bus,uint32_t Timing)
{
    Bus->hi2c->Init.Timing = Timing;
    HAL_I2C_Init(Bus->hi2c);
}

/***************
*  I2C写单字节
*
*  形  参：总线类型、目标寄存器地址、待写入的数据
*  返回值：bool-成功、失败
****************/
static bool I2C_Write(bus_t *Bus,uint8_t Reg,uint8_t Data)
{
    HAL_StatusTypeDef Status = 
    HAL_I2C_Mem_Write(Bus->hi2c,Bus->Addr<<1,Reg,I2C_MEMADD_SIZE_8BIT,&Data,1,100);

    if(Status == HAL_OK)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/***************
*  I2C读单字节
*
*  形  参：总线类型、目标寄存器地址、读出的数据
*  返回值：bool-成功、失败
****************/
static bool I2C_Read(bus_t *Bus,uint8_t Reg,uint8_t *Data)
{
    HAL_StatusTypeDef Status =
    HAL_I2C_Mem_Read(Bus->hi2c,Bus->Addr<<1,Reg,I2C_MEMADD_SIZE_8BIT,Data,1,100);

    if(Status == HAL_OK)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/***************
*  I2C读多字节
*
*  形  参：总线类型、目标寄存器地址、读出的数据、数据长度
*  返回值：bool-成功、失败
****************/
static bool I2C_ReadBuf(bus_t *Bus,uint8_t Reg,uint8_t *Data,uint16_t Len)
{
    HAL_StatusTypeDef Status =
    HAL_I2C_Mem_Read(Bus->hi2c,Bus->Addr<<1,Reg,I2C_MEMADD_SIZE_8BIT,Data,Len,100);

    if(Status == HAL_OK)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/***************************外部调用******************************/
void Bus_SetSpeed(bus_t *Bus,uint32_t Speed)
{
    switch(Bus->Type)
    {
        case BUS_TYPE_SPI:
            SPI_SetSpeed(Bus,Speed);
            break;

        case BUS_TYPE_I2C:
            I2C_SetSpeed(Bus,Speed);
            break;

        default:
            break;
    }
}

bool Bus_Write(bus_t *Bus,uint8_t Reg,uint8_t Data)
{
    switch(Bus->Type)
    {
        case BUS_TYPE_SPI:
            return SPI_Write(Bus,Reg,Data);

        case BUS_TYPE_I2C:
            return I2C_Write(Bus,Reg,Data);

        default:
            return false;
    }
}

bool Bus_Read(bus_t *Bus,uint8_t Reg,uint8_t *Data)
{
    switch(Bus->Type)
    {
        case BUS_TYPE_SPI:
            return SPI_Read(Bus,Reg,Data);

        case BUS_TYPE_I2C:
            return I2C_Read(Bus,Reg,Data);

        default:
            return false;
    }
}

bool Bus_ReadBuf(bus_t *Bus,uint8_t Reg,uint8_t *Data,uint16_t Len)
{
    switch(Bus->Type)
    {
        case BUS_TYPE_SPI:
            return SPI_ReadBuf(Bus,Reg,Data,Len);

        case BUS_TYPE_I2C:
            return I2C_ReadBuf(Bus,Reg,Data,Len);

        default:
            return false;
    }
}