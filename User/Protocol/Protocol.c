#include <ctype.h>
#include "Calib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Flight.h"
#include <stdbool.h>
#include "Protocol.h"
#include "Simulator.h"

static volatile uint16_t Head = 0;
static volatile uint16_t Tail = 0;
static volatile uint8_t  Buffer[BUF_SIZE];

/***************
*  接收一个字节
*
*  形  参：待接收的字节
*  说  明：在USB中断里调用，把字节放进环形缓冲
****************/
void Prot_WriteBuf(uint8_t Data)
{
    uint16_t Next_Head = (Head + 1) % BUF_SIZE;

    if(Next_Head != Tail)
    {
        Buffer[Head] = Data;
        Head = Next_Head;
    }
}

/***************
*  取出一个字节
*
*  形  参：存放字节的地址
****************/
bool Prot_ReadBuf(uint8_t *Data)
{
    if(Head == Tail) return false;

    *Data = Buffer[Tail];
    Tail = (Tail + 1) % BUF_SIZE;

    return true;
}

void Prot_Process(char *Cmd)
{
    sensor_t Sensor;
    int  Val[4]   = {0};
    char Name[16] = {0};
    char Arg1[16] = {0};
    char Arg2[16] = {0};

    for(uint8_t i = 0;Cmd[i] != '\0';i ++)
    {
        Cmd[i] = (char)toupper((unsigned char)Cmd[i]);
        if(Cmd[i] == ',') Cmd[i] = ' ';
    }
    sscanf(Cmd,"%15s %15s %15s",Name,Arg1,Arg2);

#ifdef USE_SIM
/*虚拟遥控*/
    if(strcmp(Name,"RC") == 0)
    {
        sscanf(Cmd,"%*s %d %d %d %d",&Val[0],&Val[1],&Val[2],&Val[3]);
        Flight_Stick((float)Val[0],(float)Val[1],(float)Val[2],(float)Val[3]);
        return;
    }

    if(strcmp(Name,"CAL") != 0) return;
#endif

/*校准参数加载*/
    if(strcmp(Arg1,"STATUS") == 0)
    {
        printf("CAL,STATUS,IMU=%d,MAG=%d\r\n",IMU_CalValid ? 1 : 0,MAG_CalValid ? 1 : 0);
        return;
    }

/*校准外设选择*/
    if(strcmp(Arg1,"IMU") == 0) Sensor = IMU;
    else if(strcmp(Arg1,"MAG") == 0) Sensor = MAG;
    else return;

/*校准开始&终止*/
    if(strcmp(Arg2,"ABORT") == 0) Cal_Abort();
    else if(strcmp(Arg2,"START") == 0 || strcmp(Arg2,"NEXT") == 0) Cal_Start(Sensor);
}

/***************
*  数据接收
*
*  形  参：无
*  说  明：从环形缓冲取字节
****************/
void Prot_Run(void)
{
    uint8_t Byte = 0;
    static uint8_t Cmd_Index = 0;
    static char    Cmd_Buffer[96];

    while(Prot_ReadBuf(&Byte))
    {
        if(Byte == '\n' || Byte == '\r')
        {
            if(Cmd_Index > 0)
            {
                Cmd_Buffer[Cmd_Index] = '\0';
                Prot_Process(Cmd_Buffer);

                Cmd_Index = 0;
            }
        }
        else if(Cmd_Index < sizeof(Cmd_Buffer) - 1)
        {
            Cmd_Buffer[Cmd_Index ++] = (char)Byte;
        }
    }
}