#include "SPI.h"
#include "I2C.h"
#include "DPS310.h"
#include "IST8310.h"
#include "ICM42605.h"
#include "QMC5883P.h"

/*************** 传感器数据与健康状态 ***************/
health_t ACC_Health;
health_t GYR_Health;
health_t MAG_Health;
health_t BAR_Health;

sample_t HW_Sample = {0};

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

/***************
*  硬件初始化：
*
*  形  参：无
*  返回值：无
*  说  明：初始化传感器健康状态，检测并初始化各外设
****************/
void Hardware_Init(void)
{
    Health_Init(&ACC_Health,100);
    Health_Init(&GYR_Health,100);
    Health_Init(&MAG_Health,250);
    Health_Init(&BAR_Health,250);

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

/***************
*  读取 IMU：
*
*  形  参：无
*  返回值：无
*  说  明：读取加速度/角速度，换算为机体系SI单位
****************/
void IMU_Read(void)
{
    ICM42605_AccRead(&ICM42605_Device1);
    Health_Update(&ACC_Health,ICM42605_Device1.Acc[0],ICM42605_Device1.Acc[1],ICM42605_Device1.Acc[2]);

    /*硬件坐标系 -> 算法坐标系*/
    HW_Sample.Acc[0] = -ICM42605_Device1.Acc[0]/ACC_SCALE*GRAVITY;
    HW_Sample.Acc[1] = -ICM42605_Device1.Acc[1]/ACC_SCALE*GRAVITY;
    HW_Sample.Acc[2] =  ICM42605_Device1.Acc[2]/ACC_SCALE*GRAVITY;

    ICM42605_GyrRead(&ICM42605_Device1);
    Health_Update(&GYR_Health,ICM42605_Device1.Gyr[0],ICM42605_Device1.Gyr[1],ICM42605_Device1.Gyr[2]);

    /*硬件坐标系 -> 算法坐标系*/
    HW_Sample.Gyr[0] = -ICM42605_Device1.Gyr[0]/GYR_SCALE;
    HW_Sample.Gyr[1] = -ICM42605_Device1.Gyr[1]/GYR_SCALE;
    HW_Sample.Gyr[2] =  ICM42605_Device1.Gyr[2]/GYR_SCALE;
}

/***************
*  读取磁力计：
*
*  形  参：无
*  返回值：无
*  说  明：读取三轴磁场
****************/
void MAG_Read(void)
{
#ifdef USE_IST8310
    IST8310_Read(&IST8310_Device);
    Health_Update(&MAG_Health,IST8310_Device.Mag[0],IST8310_Device.Mag[1],IST8310_Device.Mag[2]);

    HW_Sample.Mag[0] = (float)IST8310_Device.Mag[0];
    HW_Sample.Mag[1] = (float)IST8310_Device.Mag[1];
    HW_Sample.Mag[2] = (float)IST8310_Device.Mag[2];
#endif

#ifdef USE_QMC5883P
    QMC5883P_Read(&QMC5883P_Device);
    Health_Update(&MAG_Health,QMC5883P_Device.Mag[0],QMC5883P_Device.Mag[1],QMC5883P_Device.Mag[2]);

    HW_Sample.Mag[0] = (float)QMC5883P_Device.Mag[0];
    HW_Sample.Mag[1] = (float)QMC5883P_Device.Mag[1];
    HW_Sample.Mag[2] = (float)QMC5883P_Device.Mag[2];
#endif
}

/***************
*  读取气压计：
*
*  形  参：无
*  返回值：无
*  说  明：读取气压与温度
****************/
void BAR_Read(void)
{
    DPS310_Read(&DPS310_Device);
    Health_Update(&BAR_Health,DPS310_Device.Pressure,DPS310_Device.Temperature,0);

    HW_Sample.Pressure    = DPS310_Device.Pressure;
    HW_Sample.Temperature = DPS310_Device.Temperature;
}
