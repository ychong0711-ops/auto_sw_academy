#ifndef CAN_SIGNAL_H
#define CAN_SIGNAL_H
/*
 * can_signal.h - DBC 스타일 CAN 신호 코덱
 * 바이트 오더:
 *  - Intel(little-endian): LSB 부터 start bit, 이후 start+1, +2 ... 위치에 채움
 *  - Motorola(big-endian): MSB 가 start bit, 이후 "낮은 유효 위치" 규칙
 *      다음 비트 위치 = (pos % 8 == 0) ? pos + 15 : pos - 1   (sawtooth 번호 체계)
 * 이 규칙은 cantools/CANdb++ 와 같은 널리 쓰이는 해석과 일치한다.
 * DBC 파일의 @ 표기 규약: @0 = Motorola(big), @1 = Intel(little)  (Vector/cantools)
 */
#include "Std_Types.h"

typedef enum { CAN_SIG_INTEL = 0, CAN_SIG_MOTOROLA = 1 } CanSig_ByteOrder;

/* [SRS_SIG_010] raw 추출: data[0..len-1] 에서 startBit 기준 sigLen 비트 */
Std_ReturnType CanSig_ExtractRaw(const uint8* data, uint8 len,
                                 uint16 startBit, uint8 sigLen,
                                 CanSig_ByteOrder order, uint32* out);

/* [SRS_SIG_020] raw 삽입 (다른 비트 보존) */
Std_ReturnType CanSig_InsertRaw(uint8* data, uint8 len,
                                uint16 startBit, uint8 sigLen,
                                CanSig_ByteOrder order, uint32 value);

/* [SRS_SIG_030] 물리량 변환 (factor/offset) */
double CanSig_ToPhysical(uint32 raw, double factor, double offset);
uint32 CanSig_FromPhysical(double phys, double factor, double offset);

#endif /* CAN_SIGNAL_H */
