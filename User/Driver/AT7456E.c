#include "Bus.h"
#include <stdint.h>
#include <stdbool.h>
#include "AT7456E.h"
#include "hardware.h"
#include "stm32h7xx_hal.h"

/*
*   检测AT7456E
*/
bool AT7456E_Detect(osd_t *Device)
{
    uint8_t Value = 0;
    uint8_t Retry_Num = 5;

    Bus_SetSpeed(Device->BusDevice,SPI_SPEED_INIT);

    Bus_Write(Device->BusDevice,AT7456E_RA_VM0,AT7456E_VM0_RESET);

    while(Retry_Num--)
    {
        HAL_Delay(50);

        Bus_Write(Device->BusDevice,AT7456E_RA_VM0,AT7456E_VM0_OSD_ON);
        Bus_Read(Device->BusDevice,AT7456E_RA_VM0,&Value);

        if(Value == AT7456E_VM0_OSD_ON)
        {
            return true;
        }
    }

    return false;
}

/*
*   初始化AT7456E
*/
void AT7456E_Init(osd_t *Device)
{
    Bus_SetSpeed(Device->BusDevice,SPI_SPEED_INIT);

    Bus_Write(Device->BusDevice,AT7456E_RA_VM0,
              AT7456E_VM0_VIDEO_NTSC|AT7456E_VM0_SYNC_AUTO|AT7456E_VM0_DISPLAY_ON|AT7456E_VM0_OSD_ON);
    HAL_Delay(10);

    AT7456E_Clear(Device);
}

/*
*   清屏
*/
void AT7456E_Clear(osd_t *Device)
{
    for(uint16_t Addr = 0;Addr < AT7456E_COLS_MEM*AT7456E_ROWS_MEM;Addr++)
    {
        Bus_Write(Device->BusDevice,AT7456E_RA_DMAH,Addr >> 8);
        Bus_Write(Device->BusDevice,AT7456E_RA_DMAL,Addr & 0xFF);
        Bus_Write(Device->BusDevice,AT7456E_RA_DMDI,AT7456E_CH_BLANK);
    }
}

/*
*   写一个字符
*/
void AT7456E_WriteChar(osd_t *Device,uint8_t Row,uint8_t Col,uint8_t Ch)
{
    uint16_t Addr = Row*AT7456E_COLS_MEM + Col;

    Bus_Write(Device->BusDevice,AT7456E_RA_DMAH,Addr >> 8);
    Bus_Write(Device->BusDevice,AT7456E_RA_DMAL,Addr & 0xFF);
    Bus_Write(Device->BusDevice,AT7456E_RA_DMDI,Ch);
}
