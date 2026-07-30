/*
 * [EX4-1] CRC-8 SAE J1850 + E2E Profile 1 테스트
 * 요구사항 추적: [SRS_E2E_010] CRC 검증 벡터, [SRS_E2E_020] 카운터 시퀀스 감시,
 *               [SRS_E2E_030] 오염 데이터 검출
 */
#include "mini_test.h"
#include <string.h>
#include "crc8.h"
#include "e2e_p01.h"

static E2E_P01Config s_cfg;
static E2E_P01State  s_tx, s_rx;
static uint8 s_frame[8];

static void e2e_init(void)
{
    s_cfg.dataId = 0x0123u; s_cfg.maxDeltaCounter = 2u;
    s_tx.txCounter = 0u;
    s_rx.rxWaitForFirst = 1u; s_rx.rxLastCounter = 0u;
    /* 페이로드: 속도 raw = 0x0C8 등이 data[2..] 에 실긴다고 가정 */
    s_frame[0] = 0u; s_frame[1] = 0u; s_frame[2] = 0x00u; s_frame[3] = 0xC8u;
    for (uint8 i = 4u; i < 8u; i++) { s_frame[i] = 0u; }
}

int main(void)
{
    MT_SECTION("TC_E2E_010: CRC-8 SAE J1850 검증 벡터");
    MT_CHECK(Crc8_J1850((const uint8*)"123456789", 9u) == 0x4Bu, "ASCII 1..9 -> 0x4B");

    MT_SECTION("TC_E2E_020: 정상 시퀀스 + 랩(14->0)");
    e2e_init();
    for (uint8 round = 0u; round < 16u; round++) {
        uint8 f[8]; memcpy(f, s_frame, 8u);
        E2E_P01Protect(&s_cfg, &s_tx, f, 8u);
        E2E_P01Status st = E2E_P01Check(&s_cfg, &s_rx, f, 8u);
        if (round == 0u) {
            MT_CHECK(st == E2E_P01_INITIAL, "frame0: INITIAL");
        } else {
            if (st != E2E_P01_OK) { MT_CHECK(false, "랩 포함 16프레임 모두 OK 여야 함"); break; }
        }
    }
    MT_CHECK(true, "frame1..15: OK (카운터 랩 포함)");

    MT_SECTION("TC_E2E_021: 중복/유실/순서이상 분류");
    e2e_init();
    {
        uint8 f1[8], f[8];
        memcpy(f1, s_frame, 8u); E2E_P01Protect(&s_cfg, &s_tx, f1, 8u);   /* cnt0 */
        (void)E2E_P01Check(&s_cfg, &s_rx, f1, 8u);                        /* INITIAL */
        MT_CHECK(E2E_P01Check(&s_cfg, &s_rx, f1, 8u) == E2E_P01_REPEATED, "같은 프레임 재수신 -> REPEATED");

        memcpy(f, s_frame, 8u);  E2E_P01Protect(&s_cfg, &s_tx, f, 8u);    /* cnt1 (유실 가정을 위해 버림) */
        memcpy(f, s_frame, 8u);  E2E_P01Protect(&s_cfg, &s_tx, f, 8u);    /* cnt2 */
        MT_CHECK(E2E_P01Check(&s_cfg, &s_rx, f, 8u) == E2E_P01_OKSOMELOST,
                 "1프레임 유실(delta=2, 허용 2) -> OKSOMELOST");

        for (uint8 i = 0u; i < 4u; i++) { memcpy(f, s_frame, 8u); E2E_P01Protect(&s_cfg, &s_tx, f, 8u); }
        MT_CHECK(E2E_P01Check(&s_cfg, &s_rx, f, 8u) == E2E_P01_WRONGSEQUENCE,
                 "큰 유실(delta>2) -> WRONGSEQUENCE");
    }

    MT_SECTION("TC_E2E_030: 데이터 오염 -> ERROR");
    e2e_init();
    {
        uint8 f[8]; memcpy(f, s_frame, 8u);
        E2E_P01Protect(&s_cfg, &s_tx, f, 8u);
        f[3] ^= 0x01u;                                     /* 전송 중 1비트 오염 */
        MT_CHECK(E2E_P01Check(&s_cfg, &s_rx, f, 8u) == E2E_P01_ERROR, "CRC 불일치 검출");
        /* 다른 DataID 로 설정된 수신측도 실패해야 함 */
        E2E_P01Config badCfg = s_cfg; badCfg.dataId = 0x9999u;
        E2E_P01State st = { 0u, 0u, 1u };
        uint8 g[8]; memcpy(g, s_frame, 8u); E2E_P01Protect(&s_cfg, &s_tx, g, 8u);
        MT_CHECK(E2E_P01Check(&badCfg, &st, g, 8u) == E2E_P01_ERROR, "DataID 불일치도 검출");
    }

    MT_SUMMARY();
}
