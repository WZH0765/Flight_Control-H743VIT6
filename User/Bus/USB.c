#include "USB.h"
#include <string.h>
#include <stdio.h>
#include "usbd_cdc_if.h"

static usb_rxbuf_t USB_Buffer = {0};

void USB_WriteBuf(uint8_t Data)
{
    uint16_t next_head = (USB_Buffer.Head + 1) % RX_BUF_SIZE;
    if(next_head != USB_Buffer.Tail)
    {
        USB_Buffer.Buffer[USB_Buffer.Head] = Data;
        USB_Buffer.Head = next_head;
    }
}

bool USB_ReadBuf(uint8_t *Data)
{
    if(USB_Buffer.Head != USB_Buffer.Tail)
    {
        *Data = USB_Buffer.Buffer[USB_Buffer.Tail];
        USB_Buffer.Tail = (USB_Buffer.Tail + 1) % RX_BUF_SIZE;
        
        return true;
    }
    else
    {
        return false;
    }
}
