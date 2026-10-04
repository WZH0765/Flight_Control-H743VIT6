#pragma once

/***********************
* QMC5883P_Device:
* 挂载在I2C1总线
* SCL  - PB6
* SDA  - PB7
* ADDR - 0x2C
***********************/

#include "bus.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware.h"

void QMC5883P_Init(mag_t *Device);
bool QMC5883P_Read(mag_t *Device);
bool QMC5883P_Detect(mag_t *Device);


/****************** QMC5883P RegisterAddress ******************/
#define QMC5883P_RA_ID                  0x00
#define QMC5883P_RA_CONF1               0x0A
#define QMC5883P_RA_CONF2               0x0B
#define QMC5883P_RA_STATUS              0x09
#define QMC5883P_RA_DATA_SIGN           0x29
#define QMC5883P_RA_DATA_OUTPUT_X       0x01

/********************* QMC5883P ID *****************************/
#define QMC5883P_ID                     0x80
#define QMC5883P_DATA_SIGN_MAGIC_VALUE  0x06

/******************* QMC5883P Setting **************************/
#define QMC5883P_CONF1_OSR1_8           (0x00 << 4)
#define QMC5883P_CONF1_OSR1_4           (0x01 << 4)
#define QMC5883P_CONF1_OSR1_2           (0x02 << 4)
#define QMC5883P_CONF1_OSR1_1           (0x03 << 4)

#define QMC5883P_CONF1_OSR2_1           (0x00 << 6)
#define QMC5883P_CONF1_OSR2_2           (0x01 << 6)
#define QMC5883P_CONF1_OSR2_4           (0x02 << 6)
#define QMC5883P_CONF1_OSR2_8           (0x03 << 6)

#define QMC5883P_CONF1_ODR_10HZ         (0x00 << 2)
#define QMC5883P_CONF1_ODR_50HZ         (0x01 << 2)
#define QMC5883P_CONF1_ODR_100HZ        (0x02 << 2)
#define QMC5883P_CONF1_ODR_200HZ        (0x03 << 2)

#define QMC5883P_CONF1_MODE_NORMAL      0x01
#define QMC5883P_CONF1_MODE_SINGLE      0x02
#define QMC5883P_CONF1_MODE_SUSPEND     0x00
#define QMC5883P_CONF1_MODE_CONTINUOUS  0x03


#define QMC5883P_CONF2_RNG_30G          (0x00 << 2)
#define QMC5883P_CONF2_RNG_12G          (0x01 << 2)
#define QMC5883P_CONF2_RNG_8G           (0x02 << 2)
#define QMC5883P_CONF2_RNG_2G           (0x03 << 2)

#define QMC5883P_CONF2_RESET            0x80
#define QMC5883P_CONF2_SELF_TEST        0x40
#define QMC5883P_STATUS_DRDY_MASK       0x01
#define QMC5883P_STATUS_OVFL_MASK       0x02
#define QMC5883P_CONF2_SET_RESET_ON     0x00
#define QMC5883P_CONF2_SET_RESET_OFF    0x02
#define QMC5883P_CONF2_SET_ON_RESET_OFF 0x01
