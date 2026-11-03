#pragma once

/***********************
* AT7456E:(NTSC制式)
* 挂载在SPI2总线
* SCK  - PB13
* MISO - PB14
* MOSI - PB15
* CS   - PB12
***********************/

#include "bus.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware.h"

void AT7456E_Init(osd_t *Device);
void AT7456E_Clear(osd_t *Device);
bool AT7456E_Detect(osd_t *Device);
void AT7456E_WriteChar(osd_t *Device,uint8_t Row,uint8_t Col,uint8_t Ch);

/*********************** AT7456E RegisterAddress *******************************/
#define AT7456E_RA_VM0                          0x00
#define AT7456E_RA_VM1                          0x01
#define AT7456E_RA_HOS                          0x02
#define AT7456E_RA_VOS                          0x03
#define AT7456E_RA_DMM                          0x04
#define AT7456E_RA_DMAH                         0x05
#define AT7456E_RA_DMAL                         0x06
#define AT7456E_RA_DMDI                         0x07
#define AT7456E_RA_CMM                          0x08
#define AT7456E_RA_CMAH                         0x09
#define AT7456E_RA_CMAL                         0x0A
#define AT7456E_RA_CMDI                         0x0B
#define AT7456E_RA_OSDM                         0x0C
#define AT7456E_RA_STAT                         0x20

/*********************** AT7456E VM0 Setting *******************************/
#define AT7456E_VM0_VIDEO_NTSC                  (0 << 6)
#define AT7456E_VM0_VIDEO_PAL                   (1 << 6)
#define AT7456E_VM0_SYNC_AUTO                   (0 << 5)
#define AT7456E_VM0_DISPLAY_ON                  (1 << 4)
#define AT7456E_VM0_OSD_ON                      (1 << 3)
#define AT7456E_VM0_RESET                       (1 << 1)

/*********************** AT7456E Define *******************************/
#define AT7456E_COLS_MEM                        30
#define AT7456E_ROWS_MEM                        16
#define AT7456E_CH_BLANK                        0x00