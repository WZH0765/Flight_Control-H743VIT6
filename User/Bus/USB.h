#pragma once

#include <stdint.h>
#include <stdbool.h>

#define RX_BUF_SIZE 256

typedef struct
{
    uint8_t Buffer[RX_BUF_SIZE];
    volatile uint16_t Head;
    volatile uint16_t Tail;

} usb_rxbuf_t;

void USB_WriteBuf(uint8_t Data);
bool USB_ReadBuf(uint8_t *Data);
