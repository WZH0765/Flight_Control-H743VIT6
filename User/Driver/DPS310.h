#pragma once

/***********************
* DPS310_Device:
* 挂载在I2C2总线
* SCL  - PB10
* SDA  - PB11
* ADDR - 0X76
***********************/

#include "bus.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware.h"

void DPS310_Init(bar_t *Device);
bool DPS310_Read(bar_t *Device);
bool DPS310_Detect(bar_t *Device);

/*********************** DPS310 RegisterAddress *******************************/
#define DPS310_RA_ID                        0x0D
#define DPS310_RA_COEF                      0x10
#define DPS310_RA_RESET                     0x0C
#define DPS310_RA_PSR_B2                    0x00
#define DPS310_RA_PSR_B1                    0x01
#define DPS310_RA_PSR_B0                    0x02
#define DPS310_RA_TMP_B2                    0x03
#define DPS310_RA_TMP_B1                    0x04
#define DPS310_RA_TMP_B0                    0x05
#define DPS310_RA_PRS_CFG                   0x06
#define DPS310_RA_TMP_CFG                   0x07
#define DPS310_RA_CFG_REG                   0x09
#define DPS310_RA_MEAS_CFG                  0x08
#define DPS310_RA_COEF_SRCE                 0x28

/*********************** DPS310 ID *******************************/
#define DPS310_ID                           0x10

/*********************** DPS310 Setting *******************************/
#define DPS310_RESET_BIT_SOFT_RST           0x09

#define DPS310_MEAS_CFG_TMP_RDY             (1 << 5)
#define DPS310_MEAS_CFG_PRS_RDY             (1 << 4)
#define DPS310_MEAS_CFG_COEF_RDY            (1 << 7)
#define DPS310_MEAS_CFG_SENSOR_RDY          (1 << 6)

#define DPS310_MEAS_CFG_MEAS_IDLE           (0x0)
#define DPS310_MEAS_CFG_MEAS_CTRL_MASK      (0x7)
#define DPS310_MEAS_CFG_MEAS_CTRL_CONT      (0x7)
#define DPS310_MEAS_CFG_MEAS_TEMP_SING      (0x2)

#define DPS310_PRS_CFG_BIT_PM_PRC_16        (0x04)
#define DPS310_PRS_CFG_BIT_PM_RATE_32HZ     (0x50)

#define DPS310_TMP_CFG_BIT_TMP_EXT          (0x80)
#define DPS310_TMP_CFG_BIT_TMP_PRC_16       (0x04)
#define DPS310_TMP_CFG_BIT_TMP_RATE_32HZ    (0x50)

#define DPS310_CFG_REG_BIT_P_SHIFT          (0x04)
#define DPS310_CFG_REG_BIT_T_SHIFT          (0x08)

#define DPS310_COEF_SRCE_BIT_TMP_COEF_SRCE  (0x80)
