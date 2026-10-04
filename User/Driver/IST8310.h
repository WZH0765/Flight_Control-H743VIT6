#pragma once

/***********************
* IST8310_Device:
* 挂载在I2C1总线
* SCL  - PB6
* SDA  - PB7
* ADDR - 0X0F
***********************/

#include "bus.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware.h"

void IST8310_Init(mag_t *Device);
bool IST8310_Read(mag_t *Device);
bool IST8310_Detect(mag_t *Device);

/**************** IST8310 RegisterAddress ************************/
#define IST8310_RA_WHOAMI       0x00
#define IST8310_RA_DATA         0x03
#define IST8310_RA_CNTRL1       0x0A
#define IST8310_RA_CNTRL2       0x0B
#define IST8310_RA_AVERAGE      0x41
#define IST8310_RA_PDCNTL       0x42

/********************** IST8310 ID *****************************/
#define IST8310_ID              0x10

/******************* IST8310 Setting ***************************/
#define IST8310_ODR_SINGLE      0x01
#define IST8310_ODR_10_HZ       0x03
#define IST8310_ODR_20_HZ       0x05
#define IST8310_ODR_50_HZ       0x07
#define IST8310_ODR_100_HZ      0x06

#define IST8310_AVG_16          0x24
#define IST8310_PULSE_DURATION  0xC0

#define IST8310_CNTRL2_RESET    0x01
#define IST8310_CNTRL2_DRPOL    0x04
#define IST8310_CNTRL2_DRENA    0x08