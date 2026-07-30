#ifndef CRC8_H
#define CRC8_H
/*
 * crc8.h - CRC-8 SAE J1850 (ECU 신호 무결성 보호의 기본 부품)
 *   다항식 0x1D, 초기값 0xFF, 최종 XOR 0xFF, 반사 없음
 *   검증 벡터: ASCII "123456789" -> 0x4B
 * 실무에서는 CRC 테이블을 RTE 생성/최적화(다항식 테이블)로 돌린다.
 */
#include "Std_Types.h"

uint8 Crc8_J1850(const uint8* data, uint32 len);              /* start=0xFF, xorout=0xFF */
uint8 Crc8_Calc(const uint8* data, uint32 len, uint8 start);  /* 커스텀 초기값 */

#endif /* CRC8_H */
