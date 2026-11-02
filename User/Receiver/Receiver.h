#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
	地址 + 数据长度 + 帧类型 + 数据 + CRC校验码
*/

#define RC_BUFFER_SIZE 							64
#define RC_LINK_TIMEOUT							200			//超过该时间判定失控(ms)

/*****************ADDRESS BEGIN*****************/
#define CRSF_ADDRESS_FLIGHT_CONTROLLER			0xC8		//飞控地址

#define CRSF_ADDRESS_BROADCAS					0x00		//广播地址
#define CRSF_ADDRESS_USB						0x10		//USB地址
#define CRSF_ADDRESS_TBS_CORE_PNP_PRO			0x80		//TBS Core PNP Pro地址
#define CRSF_ADDRESS_RESERVED1					0x8A		//保留地址1
#define CRSF_ADDRESS_CURRENT_SENSOR				0xC0		//电流传感器地址
#define CRSF_ADDRESS_GPS						0xC2		//GPS地址
#define CRSF_ADDRESS_TBS_BLACKBOX				0xC4		//TBS黑匣子地址
#define CRSF_ADDRESS_RESERVED2					0xCA		//保留地址2
#define CRSF_ADDRESS_RACE_TAG					0xCC		//比赛标签地址
#define CRSF_ADDRESS_RADIO_TRANSMITTER			0xEA		//无线电发射器地址
#define CRSF_ADDRESS_CRSF_RECEIVER				0xEC		//CRSF接收机地址
#define CRSF_ADDRESS_CRSF_TRANSMITTER			0xEE		//CRSF发射机地址
/******************ADDRESS END******************/

/*****************FRAMETYPE BEGIN*****************/
#define CRSF_FRAMETYPE_RC_CHANNELS_PACKED		0x16		//遥控器通道打包帧类型
#define CRSF_FRAMETYPE_LINK_STATISTICS			0x14		//链路统计帧类型
#define CRSF_FRAMETYPE_HEARTBEAT				0x0B		//（CRSFv3）心跳

#define CRSF_FRAMETYPE_GPS						0x02		//GPS帧类型
#define CRSF_FRAMETYPE_BATTERY_SENSOR			0x08		//电池传感器帧类型
#define CRSF_FRAMETYPE_OPENTX_SYNC				0x10		//OpenTX同步帧类型
#define CRSF_FRAMETYPE_RADIO_ID					0x3A		//无线电ID帧类型
#define CRSF_FRAMETYPE_ATTITUDE					0x1E		//姿态帧类型
#define CRSF_FRAMETYPE_FLIGHT_MODE				0x21		//飞行模式帧类型
#define CRSF_FRAMETYPE_DEVICE_PING				0x28		//设备Ping帧类型
#define CRSF_FRAMETYPE_DEVICE_INFO				0x29		//设备信息帧类型
#define CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY 0x2B		//参数设置条目帧类型
#define CRSF_FRAMETYPE_PARAMETER_READ			0x2C		//参数读取帧类型
#define CRSF_FRAMETYPE_PARAMETER_WRITE			0x2D		//参数写入帧类型
#define CRSF_FRAMETYPE_COMMAND					0x32		//命令帧类型
#define CRSF_FRAMETYPE_MSP_REQ					0x7A		//使用MSP序列作为命令的响应请求
#define CRSF_FRAMETYPE_MSP_RESP					0x7B		//以58字节分块二进制形式回复
#define CRSF_FRAMETYPE_MSP_WRITE				0x7C		//以8字节分块二进制形式写入（OpenTX出站遥测缓冲区限制）
/******************FRAMETYPE END******************/

//所有数据
typedef struct
{
/******RC_CHANNELS_PACKED RC_DATA BEGIN******/

	uint16_t CHANNEL[16];	//CHANNEL 0~15数据

	float Left_X;			//左摇杆x轴
	float Left_Y;			//左摇杆y轴
	float Right_X;			//右摇杆x轴
	float Right_Y;			//右摇杆y轴

	float S1;				//左滑块
	float S2;				//右滑块

	uint8_t Button_A;		//按键A
	uint8_t Button_B;		//按键B

	uint8_t Lever_A;		//拨杆A
	uint8_t Lever_B;		//拨杆B
	uint8_t Lever_C;		//拨杆C
	uint8_t Lever_D;		//拨杆D

/*******RC_CHANNELS_PACKED RC_DATA END*******/

/********LINK_STATISTICS RC_DATA BEGIN********/
	/*飞行器 -> 地面站*/
	int8_t  UPLINK_SNR;			   //上行信噪比
	uint8_t UPLINK_RSSI_1;         //上行RSSI_1
	uint8_t UPLINK_RSSI_2;         //上行RSSI_2
	uint8_t UPLINK_TX_POWER;       //上行传输功率
	uint8_t UPLINK_LINK_QUALITY;   //上行链路质量

	/*地面站 -> 飞行器*/
	int8_t  DOWNLINK_SNR;          //下行信噪比
	uint8_t DOWNLINK_RSSI;         //下行RSSI
	uint8_t DOWNLINK_LINK_QUALITY; //下行链路质量

	/*其它*/
	uint8_t RF_MODE;               // 射频模式
	uint8_t ACTIVE_ANTENNA;        // 当前活跃天线
/*********LINK_STATISTICS RC_DATA END*********/

/********HEARTBEAT RC_DATA BEGIN********/
	uint16_t HEARTBEAT_COUNTER;			// 心跳计数器
/*********HEARTBEAT RC_DATA END*********/

} rc_data_t;

extern rc_data_t RC_Data;
extern uint8_t RC_RxBuffer[RC_BUFFER_SIZE];

bool Link_Status(void);
void Start_Receive(void);
void Receive_Parse(uint8_t *Input,uint16_t size,rc_data_t *Output);