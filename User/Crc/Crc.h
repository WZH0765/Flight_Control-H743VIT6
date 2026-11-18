#pragma once

#include <stdint.h>

uint32_t CRC32(const void *Data,uint32_t Len);
uint16_t CRC16(const uint8_t *Data,uint16_t Len);
uint8_t CRC_Xor(const uint8_t *Data,uint16_t Len);
uint8_t CRC8_DvbS2(const uint8_t *Data,uint16_t Len);