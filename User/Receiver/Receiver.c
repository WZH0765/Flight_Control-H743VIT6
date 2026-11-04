#include "main.h"
#include "usart.h"
#include "SysTime.h"
#include "Receiver.h"
#include "stm32h7xx_hal.h"

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

__attribute__((section(".dma_buffer")))
uint8_t   RC_RxBuffer[RC_BUFFER_SIZE];

void Receiver_Start(void)
{
	HAL_UARTEx_ReceiveToIdle_DMA(&huart6,RC_RxBuffer,sizeof(RC_RxBuffer));
	__HAL_DMA_DISABLE_IT(huart6.hdmarx,DMA_IT_HT);
}

void Receiver_Parse(uint8_t *Input,uint16_t size,rc_data_t *Output)
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

bool Receiver_Status(void)
{
	return LastFrame_Time != 0 && (millis() - LastFrame_Time) < RC_LINK_TIMEOUT;
}