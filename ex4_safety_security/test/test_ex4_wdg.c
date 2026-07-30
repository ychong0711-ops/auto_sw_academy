/*
 * [EX4-3] Watchdog Manager 테스트
 * 요구사항 추적: [SRS_WDG_010] deadline 내 체크포인트 시 OK 유지,
 *               [SRS_WDG_020] 마감 초과 SE 는 EXPIRED, 글로벌 상태 전파,
 *               [SRS_WDG_030] EXPIRED 는 자동 복구하지 않고 리셋으로만 해제
 */
#include "mini_test.h"
#include "wdg.h"

int main(void)
{
    WdgM_Init();
    (void)WdgM_RegisterEntity(0u /*SensorSwc*/, 30u);   /* 30ms 마감 */
    (void)WdgM_RegisterEntity(1u /*Com Stack */, 50u);  /* 50ms 마감 */

    MT_SECTION("TC_WDG_010: 정상 급식(alive)");
    for (uint8 t = 0u; t < 3u; t++) {
        WdgM_Checkpoint(0u);
        WdgM_Checkpoint(1u);
        WdgM_MainFunction(10u);
    }
    MT_CHECK(WdgM_GetGlobalStatus() == WDGM_OK, "주기 체크포인트: 글로벌 OK");

    MT_SECTION("TC_WDG_020/030 FEC: Com 스택 행업 -> EXPIRED");
    for (uint8 t = 0u; t < 5u; t++) {                  /* 50ms 동안 1번만 급식 안 함 */
        WdgM_Checkpoint(0u);                           /* 0번은 건강 */
        WdgM_MainFunction(10u);
    }
    MT_CHECK(WdgM_GetLocalStatus(0u) == WDGM_OK, "건강한 SE 는 OK");
    MT_CHECK(WdgM_GetLocalStatus(1u) == WDGM_EXPIRED, "행업 SE 는 EXPIRED");
    MT_CHECK(WdgM_GetGlobalStatus() == WDGM_EXPIRED, "글로벌 상태 전파");
    MT_CHECK(WdgM_GetViolationCount() >= 1u, "위반 누적 기록");

    WdgM_Checkpoint(1u);                               /* 뒤늦은 체크포인트 */
    WdgM_MainFunction(10u);
    MT_CHECK(WdgM_GetGlobalStatus() == WDGM_EXPIRED, "자동 복구 금지 (리셋 필요)");

    WdgM_Reset();
    WdgM_Checkpoint(0u); WdgM_Checkpoint(1u);
    WdgM_MainFunction(10u);
    MT_CHECK(WdgM_GetGlobalStatus() == WDGM_OK, "명시적 리셋 후 복구");

    MT_SUMMARY();
}
