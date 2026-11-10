#include "GPS.h"
#include "usart.h"
#include <string.h>
#include "minmea.h"
#include "SysTime.h"

gpsData_t GPS_Data;
static gpsData_t GPS_Buf;

__attribute__((section(".dma_buffer")))
uint8_t GPS_RxBuffer[GPS_RX_SIZE];

void GPS_Start(void)
{
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2,GPS_RxBuffer,GPS_RX_SIZE - 1);
    __HAL_DMA_DISABLE_IT(huart2.hdmarx,DMA_IT_HT);
}

/***************
*  解析单行数据
*
****************/
static void GPS_ParseLine(char *Line)
{
    struct minmea_sentence_rmc rmc;
    struct minmea_sentence_gga gga;

    if(minmea_check(Line,true) == false) return;    //严格模式：校验和不对直接丢

    switch(minmea_sentence_id(Line,false))
    {
    case MINMEA_SENTENCE_RMC:
        if(minmea_parse_rmc(&rmc,Line) == false) return;

        /*授时*/
        GPS_Buf.Hour   = (uint8_t)rmc.time.hours;
        GPS_Buf.Minute = (uint8_t)rmc.time.minutes;
        GPS_Buf.Second = (uint8_t)rmc.time.seconds;

        if(rmc.valid == true)
        {
            GPS_Buf.Latitude     = minmea_tocoord(&rmc.latitude);
            GPS_Buf.Longitude    = minmea_tocoord(&rmc.longitude);
            GPS_Buf.GroundSpeed  = minmea_tofloat(&rmc.speed)*0.514f;   //节 → m/s
            GPS_Buf.GroundCourse = minmea_tofloat(&rmc.course);
        }
        else
        {
            GPS_Buf.Latitude     = 0;
            GPS_Buf.Longitude    = 0;
            GPS_Buf.GroundSpeed  = 0;
            GPS_Buf.GroundCourse = 0;
        }
        break;

    case MINMEA_SENTENCE_GGA:
        if(minmea_parse_gga(&gga,Line) == false) return;

        if(gga.fix_quality > 0)
        {
            GPS_Buf.Fix       = (gga.fix_quality >= 2) ? GPS_FIX_3D : GPS_FIX_2D;
            GPS_Buf.SatNum    = (uint8_t)gga.satellites_tracked;
            GPS_Buf.Altitude  = minmea_tofloat(&gga.altitude);
            GPS_Buf.Latitude  = minmea_tocoord(&gga.latitude);
            GPS_Buf.Longitude = minmea_tocoord(&gga.longitude);
        }
        else
        {
            GPS_Buf.Fix       = GPS_FIX_INVALID;
            GPS_Buf.Altitude  = 0;
            GPS_Buf.Latitude  = 0;
            GPS_Buf.Longitude = 0;
        }
        break;

    default:
        return;
    }

    GPS_Buf.TimeStamp = millis();
    GPS_Data = GPS_Buf;
}

/***************
*  解析一包数据
*
*  形  参：Buffer 数据，Len 长度
****************/
void GPS_Parse(uint8_t *Buffer,uint16_t Len)
{
    char *Line;
    char *Next;
    uint16_t Length;

    if(Len == 0) return;

    Buffer[Len] = '\0';
    Line = (char*)Buffer;

    while(*Line != '\0')
    {
        Next = strchr(Line,'\n');
        if(Next != NULL) *Next = '\0';

        Length = (uint16_t)strlen(Line);
        if(Length > 0 && Line[Length - 1] == '\r') Line[Length - 1] = '\0';

        GPS_ParseLine(Line);

        if(Next == NULL) break;
        Line = Next + 1;
    }
}