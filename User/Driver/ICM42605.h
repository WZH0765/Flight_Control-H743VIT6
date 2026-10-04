#pragma once

/***********************
* ICM42605_Device1:
* 挂载在SPI1总线
* SCK  - PA5
* MISO - PA6
* MOSI - PD7
* CS   - PC15
***********************/

/***********************
* ICM42605_Device2:
* 挂载在SPI4总线
* SCK  - PE12
* MISO - PE13
* MOSI - PE14
* CS   - PC13
***********************/

#include "bus.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware.h"

void ICM42605_Init(imu_t *Device);
bool ICM42605_Detect(imu_t *Device);

bool ICM42605_AccRead(imu_t *Device);
bool ICM42605_GyroRead(imu_t *Device);
bool ICM42605_TempRead(imu_t *Device);


#define ICM42605_WHO_AM_I                           0x42

/*********************** ICM42605 RegisterAddress *******************************/
#define ICM42605_RA_WHO_AM_I                        0x75
#define ICM42605_RA_PWR_MGMT0                       0x4E
#define ICM42605_RA_TEMP_DATA1                      0x1D
#define ICM42605_RA_INT_CONFIG                      0x14
#define ICM42605_RA_INT_CONFIG0                     0x63
#define ICM42605_RA_INT_CONFIG1                     0x64
#define ICM42605_RA_INT_SOURCE0                     0x65
#define ICM426XX_RA_REG_BANK_SEL                    0x76
#define ICM42605_RA_GYRO_CONFIG0                    0x4F
#define ICM42605_RA_GYRO_DATA_X1                    0x25
#define ICM42605_RA_ACCEL_CONFIG0                   0x50
#define ICM42605_RA_ACCEL_DATA_X1                   0x1F
#define ICM42605_RA_GYRO_ACCEL_CONFIG0              0x52

#define ICM426XX_RA_GYRO_CONFIG_STATIC3             0x0C  // User Bank 1
#define ICM426XX_RA_GYRO_CONFIG_STATIC4             0x0D  // User Bank 1
#define ICM426XX_RA_GYRO_CONFIG_STATIC5             0x0E  // User Bank 1
#define ICM426XX_RA_ACCEL_CONFIG_STATIC2            0x03  // User Bank 2
#define ICM426XX_RA_ACCEL_CONFIG_STATIC3            0x04  // User Bank 2
#define ICM426XX_RA_ACCEL_CONFIG_STATIC4            0x05  // User Bank 2


/*********************** ICM42605 Setting *******************************/
#define ICM426XX_BANK_SELECT0                       0x00
#define ICM426XX_BANK_SELECT1                       0x01
#define ICM426XX_BANK_SELECT2                       0x02
#define ICM426XX_BANK_SELECT3                       0x03
#define ICM426XX_BANK_SELECT4                       0x04

#define ICM42605_GYRO_AAF_DELT                      4
#define ICM42605_GYRO_AAF_DELTSQR                   16
#define ICM42605_GYRO_AAF_BITSHIFT                  11

#define ICM42605_ACCEL_AAF_DELT                     21
#define ICM42605_ACCEL_AAF_DELTSQR                  440
#define ICM42605_ACCEL_AAF_BITSHIFT                 6

#define ICM42605_INTF_CONFIG1                       0x4D
#define ICM42605_INTF_CONFIG1_AFSR_MASK             0xC0
#define ICM42605_INTF_CONFIG1_AFSR_DISABLE          0x40

#define ICM42605_INT1_MODE_PULSED                   (0 << 2)
#define ICM42605_INT1_MODE_LATCHED                  (1 << 2)
#define ICM42605_INT1_DRIVE_CIRCUIT_OD              (0 << 1)
#define ICM42605_INT1_DRIVE_CIRCUIT_PP              (1 << 1)
#define ICM42605_INT1_POLARITY_ACTIVE_LOW           (0 << 0)
#define ICM42605_INT1_POLARITY_ACTIVE_HIGH          (1 << 0)

#define ICM42605_INT_ASYNC_RESET_BIT                4
#define ICM42605_INT_TPULSE_DURATION_8              (1 << 6)
#define ICM42605_INT_TDEASSERT_ENABLED              (0 << 5)
#define ICM42605_INT_TDEASSERT_DISABLED             (1 << 5)
#define ICM42605_INT_TPULSE_DURATION_100            (0 << 6)

#define ICM42605_UI_DRDY_INT1_EN_ENABLED            (1 << 3)
#define ICM42605_UI_DRDY_INT1_EN_DISABLED           (0 << 3)

#define ICM42605_PWR_MGMT0_GYRO_MODE_LN             (3 << 2)
#define ICM42605_PWR_MGMT0_ACCEL_MODE_LN            (3 << 0)
#define ICM42605_PWR_MGMT0_TEMP_DISABLE_ON          (1 << 5)
#define ICM42605_PWR_MGMT0_TEMP_DISABLE_OFF         (0 << 5)

#define ICM42605_UI_DRDY_INT_CLEAR_ON_SBR           ((0 << 5) || (0 << 4))
#define ICM42605_UI_DRDY_INT_CLEAR_ON_F1BR          ((1 << 5) || (0 << 4))
#define ICM42605_UI_DRDY_INT_CLEAR_ON_SBR_AND_F1BR  ((1 << 5) || (1 << 4))
#define ICM42605_UI_DRDY_INT_CLEAR_ON_SBR_DUPLICATE ((0 << 5) || (0 << 4))

#define ICM42605_GYRO_UI_FILT_BW_LOW_LATENCY        (15 << 0)
#define ICM42605_ACCEL_UI_FILT_BW_LOW_LATENCY       (15 << 4)


/*********************** ICM42605 Define *******************************/
#define INT16_BE(p) ((int16_t)(((uint16_t)(p)[0] << 8) | (uint16_t)(p)[1]))
