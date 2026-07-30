/*
 * can_signal.c - 비트 단위 신호 배치/추출
 * Motorola 시퀀스 검증 예:
 *   start=7, len=12 -> 위치: 7,6,5,4,3,2,1,0,(0+15)=15,14,13,12
 *   => byte0 = 상위 8비트, byte1 상위 니블 = 하위 4비트  (포털 튜토리얼들의 정석 그림)
 */
#include "can_signal.h"

static uint8 get_bit_at(const uint8* data, uint16 pos)
{
    return (uint8)((data[pos >> 3u] >> (pos & 7u)) & 1u);
}

static void set_bit_at(uint8* data, uint16 pos, uint8 bit)
{
    uint8 m = (uint8)(1u << (pos & 7u));
    if (bit != 0u) { data[pos >> 3u] |=  m; }
    else           { data[pos >> 3u] &= (uint8)(~m); }
}

static uint16 next_pos(CanSig_ByteOrder order, uint16 pos)
{
    if (order == CAN_SIG_INTEL) { return (uint16)(pos + 1u); }
    /* Motorola: 바이트 경계(LSB)에서 다음 바이트의 MSB 로 점프 */
    return (uint16)((pos % 8u == 0u) ? (pos + 15u) : (pos - 1u));
}

static uint8 range_ok(uint8 dlc, uint16 startBit, uint8 sigLen, CanSig_ByteOrder order)
{
    /* 모든 비트 위치가 프레임 안에 있는지 검사 */
    uint16 pos = startBit;
    if ((sigLen == 0u) || (sigLen > 32u)) { return 0u; }
    for (uint8 i = 0u; i < sigLen; i++) {
        if (pos >= (uint16)dlc * 8u) { return 0u; }
        pos = next_pos(order, pos);
    }
    return 1u;
}

/* [SRS_SIG_010] */
Std_ReturnType CanSig_ExtractRaw(const uint8* data, uint8 len,
                                 uint16 startBit, uint8 sigLen,
                                 CanSig_ByteOrder order, uint32* out)
{
    uint16 pos;
    if ((data == NULL) || (out == NULL)) { return E_NOT_OK; }
    if (range_ok(len, startBit, sigLen, order) == 0u) { return E_NOT_OK; }

    *out = 0u;
    pos = startBit;
    for (uint8 i = 0u; i < sigLen; i++) {
        uint8 bit = get_bit_at(data, pos);
        if (order == CAN_SIG_INTEL) {
            *out |= ((uint32)bit << i);                        /* i번째 비트 = LSB부터 */
        } else {
            *out |= ((uint32)bit << (sigLen - 1u - i));        /* 첫 비트가 MSB */
        }
        pos = next_pos(order, pos);
    }
    return E_OK;
}

/* [SRS_SIG_020] */
Std_ReturnType CanSig_InsertRaw(uint8* data, uint8 len,
                                uint16 startBit, uint8 sigLen,
                                CanSig_ByteOrder order, uint32 value)
{
    uint16 pos;
    if (data == NULL) { return E_NOT_OK; }
    if (range_ok(len, startBit, sigLen, order) == 0u) { return E_NOT_OK; }

    pos = startBit;
    for (uint8 i = 0u; i < sigLen; i++) {
        uint8 bit;
        if (order == CAN_SIG_INTEL) { bit = (uint8)((value >> i) & 1u); }
        else                        { bit = (uint8)((value >> (sigLen - 1u - i)) & 1u); }
        set_bit_at(data, pos, bit);
        pos = next_pos(order, pos);
    }
    return E_OK;
}

/* [SRS_SIG_030] */
double CanSig_ToPhysical(uint32 raw, double factor, double offset)
{
    return ((double)raw * factor) + offset;
}

uint32 CanSig_FromPhysical(double phys, double factor, double offset)
{
    double r = (phys - offset) / factor;
    return (uint32)(r + ((r >= 0.0) ? 0.5 : -0.5));   /* 반올림 */
}
