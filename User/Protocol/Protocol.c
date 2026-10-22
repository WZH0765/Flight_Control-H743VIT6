#include "Protocol.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define RX_BUF_SIZE     256

static char Cmd_Buffer[96];
static uint8_t Cmd_Index = 0;

static volatile uint16_t Rx_Head = 0;
static volatile uint16_t Rx_Tail = 0;
static volatile uint8_t  Rx_Buffer[RX_BUF_SIZE];

/***************
*  接收一个字节
*
*  形  参：待接收的字节
*  说  明：在USB中断里调用，把字节放进环形缓冲
****************/
void Protocol_WriteBuf(uint8_t Data)
{
    uint16_t Next_Head = (Rx_Head + 1) % RX_BUF_SIZE;

    if(Next_Head != Rx_Tail)
    {
        Rx_Buffer[Rx_Head] = Data;
        Rx_Head = Next_Head;
    }
}

/***************
*  取出一个字节
*
*  形  参：存放字节的地址
****************/
static bool Protocol_ReadBuf(uint8_t *Data)
{
    if(Rx_Head == Rx_Tail) return false;

    *Data = Rx_Buffer[Rx_Tail];
    Rx_Tail = (Rx_Tail + 1) % RX_BUF_SIZE;

    return true;
}

/*******************
* 解析一条命令
*
* 命令格式：CAL <IMU|MAG|BAR>
*******************/
static void Protocol_Process(char *Cmd)
{
    char Name[16] = {0};
    char Arg1[16] = {0};
    const char *Sensor = NULL;

    for(uint8_t i = 0;Cmd[i] != '\0';i ++)
    {
        Cmd[i] = (char)toupper((unsigned char)Cmd[i]);
        if(Cmd[i] == ',') Cmd[i] = ' ';
    }

    sscanf(Cmd,"%15s %15s",Name,Arg1);

    if(strcmp(Name,"CAL") != 0) return;
    if(strcmp(Arg1,"IMU") == 0) Sensor = "IMU";
    else if(strcmp(Arg1,"MAG") == 0) Sensor = "MAG";
    else if(strcmp(Arg1,"BAR") == 0) Sensor = "BAR";
    else return;

    printf("CAL,%s,IDLE,0,NOT_READY\r\n",Sensor);
}

/***************
*  协议接收
*
*  形  参：无
*  说  明：从环形缓冲取字节，攒够一行就解析
****************/
void Protocol_Receive(void)
{
    uint8_t Byte = 0;

    while(Protocol_ReadBuf(&Byte))
    {
        if(Byte == '\n' || Byte == '\r')
        {
            if(Cmd_Index > 0)
            {
                Cmd_Buffer[Cmd_Index] = '\0';
                Protocol_Process(Cmd_Buffer);
                Cmd_Index = 0;
            }
        }
        else if(Cmd_Index < sizeof(Cmd_Buffer) - 1)
        {
            Cmd_Buffer[Cmd_Index ++] = (char)Byte;
        }
    }
}
