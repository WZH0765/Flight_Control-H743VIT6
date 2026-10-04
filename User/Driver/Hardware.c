#include "SPI.h"
#include "I2C.h"
#include "DPS310.h"
#include "IST8310.h"
#include "ICM42605.h"
#include "QMC5883P.h"

/****************** 选择使用的硬件 ******************/

/*******************************
* 请注意：
* 使用外置 磁力计/气压计 等I2C设备
* 请使用较低（如：100kHz）总线速度
*******************************/

#define USE_ICM42605
#define USE_IST8310
#define USE_DPS310

/************* 中间抽象层-用户注册硬件外设 **************/

//IMU 1
bus_t ICM42605_Bus1 = 
{
  .Type = BUS_TYPE_SPI,
  .hspi = &hspi1,
  .CsPort = GPIOC,
  .CsPin = GPIO_PIN_15
};
imu_t ICM42605_Device1 = 
{
  .BusDevice = &ICM42605_Bus1
};

//IMU 2
bus_t ICM42605_Bus2 = 
{
  .Type = BUS_TYPE_SPI,
  .hspi = &hspi4,
  .CsPort = GPIOC,
  .CsPin = GPIO_PIN_13
};
imu_t ICM42605_Device2 = 
{
  .BusDevice = &ICM42605_Bus2
};

//MAG 1
bus_t IST8310_Bus = 
{
  .Type = BUS_TYPE_I2C,
  .hi2c = &hi2c1,
  .Addr = 0x0F
};
mag_t IST8310_Device = 
{
  .BusDevice = &IST8310_Bus
};

//MAG 2
bus_t QMC5883P_Bus = 
{
  .Type = BUS_TYPE_I2C,
  .hi2c = &hi2c1,
  .Addr = 0x2C
};
mag_t QMC5883P_Device = 
{
  .BusDevice = &QMC5883P_Bus
};

//BAR
bus_t DPS310_Bus = 
{
  .Type = BUS_TYPE_I2C,
  .hi2c = &hi2c2,
  .Addr = 0x76
};
bar_t DPS310_Device = 
{
  .BusDevice = &DPS310_Bus
};

void Hardware_Init(void)
{
#ifdef USE_ICM42605
  if(ICM42605_Detect(&ICM42605_Device1) == true)
  {
    ICM42605_Init(&ICM42605_Device1);
  }
  if(ICM42605_Detect(&ICM42605_Device2) == true)
  {
    ICM42605_Init(&ICM42605_Device2);
  }
#endif

#ifdef USE_QMC5883P
  if(QMC5883P_Detect(&QMC5883P_Device) == true)
  {
    QMC5883P_Init(&QMC5883P_Device);
  }
#endif

#ifdef USE_IST8310
  if(IST8310_Detect(&IST8310_Device) == true)
  {
    IST8310_Init(&IST8310_Device);
  }
#endif

#ifdef USE_DPS310
  if(DPS310_Detect(&DPS310_Device) == true)
  {
    DPS310_Init(&DPS310_Device);
  }
#endif
}
