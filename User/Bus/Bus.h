#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"

typedef enum
{
    BUS_TYPE_SPI = 0,
    BUS_TYPE_I2C = 1,

} busType_e;

typedef struct
{
    busType_e Type;             //总线类型

/******************SPI**********************/
    uint16_t  CsPin;            //片选引脚
    GPIO_TypeDef *CsPort;       //片选引脚
    SPI_HandleTypeDef *hspi;    //SPI句柄

/******************I2C**********************/
    uint16_t Addr;              //Addr存7位地址
    I2C_HandleTypeDef *hi2c;    //I2C句柄

} bus_t;

void Bus_SetSpeed(bus_t *Bus,uint32_t Speed);
bool Bus_Write(bus_t *Bus,uint8_t Reg,uint8_t Data);
bool Bus_Read(bus_t *Bus,uint8_t Reg,uint8_t *Data);
bool Bus_ReadBuf(bus_t *Bus,uint8_t Reg,uint8_t *Data,uint16_t Len);

#define I2C_SPEED_INIT            0x20303E5D    // 120MHz 下 100kHz
#define I2C_SPEED_FAST            0x2010091A    // 120MHz 下 400kHz

#define SPI_SPEED_INIT            SPI_BAUDRATEPRESCALER_8
#define SPI_SPEED_FAST            SPI_BAUDRATEPRESCALER_2
