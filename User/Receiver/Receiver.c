#include "main.h"
#include "usart.h"
#include "SysTime.h"
#include "Receiver.h"
#include "stm32h7xx_hal.h"

static const uint8_t CRC_Table[256] =
{
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54,
    0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06,
    0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0,
    0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2,
    0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9,
    0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B,
    0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D,
    0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F,
    0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB,
    0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9,
    0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F,
    0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D,
    0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26,
    0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74,
    0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82,
    0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0,
    0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9
};

static uint8_t RC_CRC(const uint8_t *data,uint16_t len)
{
    uint8_t crc = 0;
    for(uint16_t i = 0;i < len;i++)
    {
        crc = CRC_Table[crc^data[i]];
    }
    return crc;
}

float Map(float input_value,float input_min,float input_max,float Output_min,float Output_max)
{
	float Output_value;
	if(input_value < input_min)
	{
		Output_value = Output_min;
	}
	else if(input_value > input_max)
	{
		Output_value = Output_max;
	}
	else
	{
		Output_value = Output_min + (input_value - input_min) * (Output_max - Output_min) / (input_max - input_min);
	}
	return Output_value;
}

float Map_Median(float input_value,float input_min,float input_max,float median,float Output_min,float Output_max)
{
    float Output_median = (Output_max - Output_min) / 2 + Output_min;
    if(input_min >= input_max || Output_min >= Output_max || median <= input_min || median >= input_max)
    {
        return Output_min;
    }

    if(input_value < median)
    {
        return Map(input_value,input_min,median,Output_min,Output_median);
    }
    else
    {
        return Map(input_value,median,input_max,Output_median,Output_max);
    }
}

rc_data_t RC_Data;
uint32_t  LastFrame_Time;
uint8_t   RC_RxBuffer[RC_BUFFER_SIZE];

void Start_Receive(void)
{
	HAL_UARTEx_ReceiveToIdle_DMA(&huart6,RC_RxBuffer,sizeof(RC_RxBuffer));
	__HAL_DMA_DISABLE_IT(huart6.hdmarx,DMA_IT_HT);
}

void Receive_Parse(uint8_t *Input,uint16_t size,rc_data_t *Output)
{
	if(Input[1] < 2 || size < Input[1] + 2)return;
	if(size < 4 || Input[0] != CRSF_ADDRESS_FLIGHT_CONTROLLER)return;
	if(Input[Input[1] + 1] != RC_CRC(&Input[2],Input[1] - 1))return;

	LastFrame_Time = millis();

	/*判断数据类型 CRSF_FRAMETYPE_RC_CHANNELS_PACKED*/
	if(Input[2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
	{
		/***************** CHANNEL 0-15 PROCESSING BEGIN *****************/
		Output->CHANNEL[0]  = ((uint16_t)Input[3]  >> 0 | ((uint16_t)Input[4] << 8))  & 0x07FF;
		Output->CHANNEL[1]  = ((uint16_t)Input[4]  >> 3 | ((uint16_t)Input[5] << 5))  & 0x07FF;
		Output->CHANNEL[2]  = ((uint16_t)Input[5]  >> 6 | ((uint16_t)Input[6] << 2)   | ((uint16_t)Input[7] << 10)) & 0x07FF;
		Output->CHANNEL[3]  = ((uint16_t)Input[7]  >> 1 | ((uint16_t)Input[8] << 7))  & 0x07FF;
		Output->CHANNEL[4]  = ((uint16_t)Input[8]  >> 4 | ((uint16_t)Input[9] << 4))  & 0x07FF;
		Output->CHANNEL[5]  = ((uint16_t)Input[9]  >> 7 | ((uint16_t)Input[10] << 1)  | ((uint16_t)Input[11] << 9)) & 0x07FF;
		Output->CHANNEL[6]  = ((uint16_t)Input[11] >> 2 | ((uint16_t)Input[12] << 6)) & 0x07FF;
		Output->CHANNEL[7]  = ((uint16_t)Input[12] >> 5 | ((uint16_t)Input[13] << 3)) & 0x07FF;
		Output->CHANNEL[8]  = ((uint16_t)Input[14] >> 0 | ((uint16_t)Input[15] << 8)) & 0x07FF;
		Output->CHANNEL[9]  = ((uint16_t)Input[15] >> 3 | ((uint16_t)Input[16] << 5)) & 0x07FF;
		Output->CHANNEL[10] = ((uint16_t)Input[16] >> 6 | ((uint16_t)Input[17] << 2)  | ((uint16_t)Input[18] << 10)) & 0x07FF;
		Output->CHANNEL[11] = ((uint16_t)Input[18] >> 1 | ((uint16_t)Input[19] << 7)) & 0x07FF;
		Output->CHANNEL[12] = ((uint16_t)Input[19] >> 4 | ((uint16_t)Input[20] << 4)) & 0x07FF;
		Output->CHANNEL[13] = ((uint16_t)Input[20] >> 7 | ((uint16_t)Input[21] << 1)  | ((uint16_t)Input[22] << 9)) & 0x07FF;
		Output->CHANNEL[14] = ((uint16_t)Input[22] >> 2 | ((uint16_t)Input[23] << 6)) & 0x07FF;
		Output->CHANNEL[15] = ((uint16_t)Input[23] >> 5 | ((uint16_t)Input[24] << 3)) & 0x07FF;
		/****************** CHANNEL 0-15 PROCESSING END ******************/

		/***************** 摇杆数据映射 -100~100 BEGIN *****************/
		Output->Left_X  = Map_Median(Output->CHANNEL[3],174,1808,992,-100, 100);
		Output->Left_Y  = Map_Median(Output->CHANNEL[2],174,1811,992,0, 100);
		Output->Right_X = Map_Median(Output->CHANNEL[0],174,1811,992,-100, 100);
		Output->Right_Y = Map_Median(Output->CHANNEL[1],174,1808,992,-100, 100);
		/****************** 摇杆数据映射 -100~100 END ******************/

		/***************** 滑块数据映射 000~100 BEGIN *****************/
		Output->S1 = Map_Median(Output->CHANNEL[8],191,1792,992,0, 100);
		Output->S2 = Map_Median(Output->CHANNEL[9],191,1792,992,0, 100);
		/****************** 滑块数据映射 000~100 END ******************/

		/***************** 按键数据映射 0,1 BEGIN *****************/
		Output->Button_A = Output->CHANNEL[10] > 1000 ? 1 : 0;
		Output->Button_B = Output->CHANNEL[11] > 1000 ? 1 : 0;
		/***************** 按键数据映射 0,1 END *****************/

		/***************** 拨杆数据映射 0,1,2 BEGIN *****************/
		Output->Lever_A = Output->CHANNEL[5] == 992 ? 1 : (Output->CHANNEL[5] == 1792 ? 2 : 0);
		Output->Lever_B = Output->CHANNEL[6] == 992 ? 1 : (Output->CHANNEL[6] == 1792 ? 2 : 0);
		Output->Lever_C = Output->CHANNEL[4] == 992 ? 1 : (Output->CHANNEL[4] == 1792 ? 2 : 0);
		Output->Lever_D = Output->CHANNEL[7] == 992 ? 1 : (Output->CHANNEL[7] == 1792 ? 2 : 0);
		/***************** 拨杆数据映射 0,1,2 END *****************/
	}
	/*判断数据类型 CRSF_FRAMETYPE_LINK_STATISTICS*/
	else if(Input[2] == CRSF_FRAMETYPE_LINK_STATISTICS)
	{
		/************ UPLINK BEGIN ************/
		Output->UPLINK_SNR          = Input[6];
		Output->UPLINK_RSSI_1       = Input[3];
		Output->UPLINK_RSSI_2       = Input[4];
		Output->UPLINK_TX_POWER     = Input[9];
		Output->UPLINK_LINK_QUALITY = Input[5];
		/************ UPLINK END ************/

		/************ DOWNLINK BEGIN ************/
		Output->DOWNLINK_SNR          = Input[12];
		Output->DOWNLINK_RSSI         = Input[10];
		Output->DOWNLINK_LINK_QUALITY = Input[11];
		/************ DOWNLINK END ************/

		/************ OTHERS BEGIN ************/
		Output->RF_MODE        = Input[8];
		Output->ACTIVE_ANTENNA = Input[7];
		/************ OTHERS END ************/
	}
	/*判断数据类型 CRSF_FRAMETYPE_HEARTBEAT*/
	else if(Input[2] == CRSF_FRAMETYPE_HEARTBEAT)
	{
		Output->HEARTBEAT_COUNTER = (uint16_t)Input[3] | ((uint16_t)Input[4] << 8);
	}
}

bool Link_Status(void)
{
	return LastFrame_Time != 0 && (millis() - LastFrame_Time) < RC_LINK_TIMEOUT;
}