/*
 * [EX3-1] DBC 스타일 CAN 신호 코덱 테스트 (Intel/Motorola 바이트 오더)
 * 요구사항 추적: [SRS_SIG_010/020/030] — can_signal.c 참조
 */
#include "mini_test.h"
#include "can_signal.h"

static void clear(uint8* d) { for (int i = 0; i < 8; i++) { d[i] = 0u; } }

int main(void)
{
    uint8 f[8];
    uint32 v = 0u;

    MT_SECTION("Intel(little-endian) 배치");
    clear(f);
    (void)CanSig_InsertRaw(f, 8u, 0u, 12u, CAN_SIG_INTEL, 0xABCu);
    MT_CHECK(f[0] == 0xBCu && f[1] == 0x0Au, "TC_SIG_001 start0,len12 -> BC 0A");
    (void)CanSig_ExtractRaw(f, 8u, 0u, 12u, CAN_SIG_INTEL, &v);
    MT_CHECK(v == 0xABCu, "TC_SIG_002 추출 왕복 정합");

    clear(f);
    (void)CanSig_InsertRaw(f, 8u, 20u, 8u, CAN_SIG_INTEL, 0x5Au);
    MT_CHECK(f[2] == 0xA0u && f[3] == 0x05u, "TC_SIG_003 바이트 경계 교차 -> A0 05");

    MT_SECTION("Motorola(big-endian) 배치 — CANdb/cantools 해석과 동일 규칙");
    clear(f);
    /* start=7 MSB 기준: byte0=상위8b, byte1 상위니블=하위4b */
    (void)CanSig_InsertRaw(f, 8u, 7u, 12u, CAN_SIG_MOTOROLA, 0xABCu);
    MT_CHECK(f[0] == 0xABu && f[1] == 0xC0u, "TC_SIG_010 start7,len12 -> AB C0");
    (void)CanSig_ExtractRaw(f, 8u, 7u, 12u, CAN_SIG_MOTOROLA, &v);
    MT_CHECK(v == 0xABCu, "TC_SIG_011 Motorola 추출 왕복 정합");

    clear(f);
    /* start=11: byte1 하위니블=상위4b, byte2=하위8b */
    (void)CanSig_InsertRaw(f, 8u, 11u, 12u, CAN_SIG_MOTOROLA, 0xABCu);
    MT_CHECK(f[1] == 0x0Au && f[2] == 0xBCu, "TC_SIG_012 start11,len12 -> 0A BC");

    MT_SECTION("범위 검사 / 무결성");
    clear(f); f[7] = 0x55u;
    (void)CanSig_InsertRaw(f, 8u, 0u, 8u, CAN_SIG_INTEL, 0xFFu);
    MT_CHECK(f[7] == 0x55u, "TC_SIG_020 인접 비트 보존");
    MT_CHECK(CanSig_InsertRaw(f, 8u, 60u, 8u, CAN_SIG_INTEL, 1u) == E_NOT_OK,
             "TC_SIG_021 프레임 밖 신호 거부");
    MT_CHECK(CanSig_ExtractRaw(f, 8u, 0u, 33u, CAN_SIG_INTEL, &v) == E_NOT_OK,
             "TC_SIG_022 32bit 초과 신호 거부");

    MT_SECTION("물리량 변환 (factor/offset)");
    double phys = CanSig_ToPhysical(200u, 0.5, -40.0);          /* 차속: 0.5kph/bit, -40 */
    MT_CHECK(phys > 59.99 && phys < 60.01, "TC_SIG_030 raw200 -> 60.0");
    MT_CHECK(CanSig_FromPhysical(60.0, 0.5, -40.0) == 200u, "TC_SIG_031 60.0 -> raw200");

    MT_SUMMARY();
}
