/*
 * crc8.c - 비트 단위 구현 (테이블 방식은 성능 최적화 과제)
 */
#include "crc8.h"

#define CRC8_POLY  0x1Du

uint8 Crc8_Calc(const uint8* data, uint32 len, uint8 start)
{
    uint8 crc = start;
    for (uint32 i = 0u; i < len; i++) {
        crc ^= data[i];
        for (uint8 b = 0u; b < 8u; b++) {
            crc = (uint8)((crc & 0x80u) != 0u ? (uint8)((crc << 1u) ^ CRC8_POLY)
                                              : (uint8)(crc << 1u));
        }
    }
    return (uint8)(crc ^ 0xFFu);
}

uint8 Crc8_J1850(const uint8* data, uint32 len)
{
    return Crc8_Calc(data, len, 0xFFu);
}
