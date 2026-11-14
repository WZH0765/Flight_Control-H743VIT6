#include "OFL.h"
#include "usart.h"
#include <string.h>
#include "Hardware.h"

ofl_t OFL = {0};

static uint8_t OFL_Buf[18];
static uint8_t OFL_Count = 0;

__attribute__((section(".dma_buffer")))
uint8_t OFL_RxBuffer[64];

static uint8_t OFL_Xor(uint8_t *Buf,uint8_t Len)
{
    uint8_t Sum = 0;
    while(Len --) Sum ^= *Buf ++;

    return Sum;
}

void OFL_Start(void)
{
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1,OFL_RxBuffer,sizeof(OFL_RxBuffer));
    __HAL_DMA_DISABLE_IT(huart1.hdmarx,DMA_IT_HT);
}

static void OFL_Frame(uint8_t *Buf)
{
    int32_t FlowX = (int32_t)((uint32_t)Buf[2] | ((uint32_t)Buf[3] << 8  )|
                    ((uint32_t)Buf[4] << 16)   | ((uint32_t)Buf[5] << 24));
    int32_t FlowY = (int32_t)((uint32_t)Buf[6] | ((uint32_t)Buf[7] << 8  )|
                    ((uint32_t)Buf[8] << 16)   | ((uint32_t)Buf[9] << 24));

    uint16_t Span = (uint16_t)Buf[10] | ((uint16_t)Buf[11] << 8);
    uint16_t Dist = (uint16_t)Buf[12] | ((uint16_t)Buf[13] << 8);

    OFL.Quality = Buf[15];
    OFL.Alt     = Dist/1000.0f;
    OFL.Valid   = (Buf[14] == OFL_VALID);

    if(OFL.Valid == true && Span > 0 && OFL.Alt > 0.05f)
    {
        float RateX = FlowX/1e6f/(Span*1e-6f);
        float RateY = FlowY/1e6f/(Span*1e-6f);

        OFL.Vr = OFL_SIGN_X*RateX*OFL.Alt;                  //机体系右向
        OFL.Vf = OFL_SIGN_Y*RateY*OFL.Alt;                  //机体系前向
    }
    else
    {
        OFL.Vf = 0.0f;
        OFL.Vr = 0.0f;
    }

    OFL.New = true;
    Health_Update(&OFL_Health,(int32_t)(OFL.Vf *1000.0f),(int32_t)(OFL.Vr *1000.0f),(int32_t)(OFL.Alt*1000.0f));
}

void OFL_Parse(uint8_t *Buffer,uint16_t Len)
{
    for(uint16_t i = 0;i < Len;i ++)
    {
        uint8_t Byte = Buffer[i];

        if(OFL_Count == 0)
        {
            if(Byte != 0xFE) continue;
        }
        else if(OFL_Count == 1)
        {
            if(Byte != 0x0E) {OFL_Count = 0; continue;}
        }

        OFL_Buf[OFL_Count ++] = Byte;
        if(OFL_Count < sizeof(OFL_Buf)) continue;

        if(OFL_Buf[17] == 0x55 && OFL_Buf[16] == OFL_Xor(&OFL_Buf[2],14))
        {
            OFL_Frame(OFL_Buf);
        }

        OFL_Count = 0;
    }
}