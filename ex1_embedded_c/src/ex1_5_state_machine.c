/*
 * [EX1-5] 상태기계 - 모든 AUTOSAR SWC/BSW 모듈의 기본 설계 패턴
 *
 * 실무 배경
 *  - Dcm(진단 세션), CanSM(통신 상태), WdgM(감시 상태) ... 전부 상태기계.
 *  - "테이블 기반 전이 + 상태별 진입/유지/탈출 동작" 패턴을 익히면
 *    AUTOSAR 모듈 스펙을 읽는 속도가 달라진다.
 *
 * 시나리오: 이그니션 키 상태기계 (OFF -> ACC -> ON -> CRANK -> RUN)
 * 요구사항 추적: [SRS_SM_010] 정의되지 않은 이벤트는 무시하고 상태 유지
 *               [SRS_SM_020] 상태별 출력(릴레이)은 상태 함수에서만 결정
 */
#include <stdint.h>
#include "mini_test.h"

typedef enum { IGN_OFF = 0, IGN_ACC, IGN_ON, IGN_CRANK, IGN_RUN, IGN_STATE_MAX } IgnState;
typedef enum { EV_KEY_OFF = 0, EV_KEY_ACC, EV_KEY_ON, EV_START_BTN, EV_ENGINE_STARTED,
               EV_ENGINE_STALLED, EV_INVALID } IgnEvent;

typedef struct { IgnState state; } IgnCtx;

/* [SRS_SM_020] 상태 -> 출력 매핑 (ACC 릴리오, IGN 릴리오, 스타터) */
static void outputs_of(IgnState s, uint8_t *acc, uint8_t *ign, uint8_t *str)
{
    *acc = (uint8_t)(s >= IGN_ACC);
    *ign = (uint8_t)(s == IGN_ON || s == IGN_CRANK || s == IGN_RUN);
    *str = (uint8_t)(s == IGN_CRANK);
}

static IgnState ign_step(IgnState s, IgnEvent e)
{
    /* 전이 테이블 (간단버전) */
    switch (s) {
    case IGN_OFF:   return (e == EV_KEY_ACC) ? IGN_ACC : s;
    case IGN_ACC:
        if (e == EV_KEY_ON)  { return IGN_ON; }
        if (e == EV_KEY_OFF) { return IGN_OFF; }
        break;
    case IGN_ON:
        if (e == EV_START_BTN) { return IGN_CRANK; }
        if (e == EV_KEY_ACC)   { return IGN_ACC; }
        break;
    case IGN_CRANK:
        if (e == EV_ENGINE_STARTED) { return IGN_RUN; }
        if (e == EV_KEY_ACC)        { return IGN_ACC; }
        break;
    case IGN_RUN:
        if (e == EV_ENGINE_STALLED) { return IGN_ON; }
        if (e == EV_KEY_ACC)        { return IGN_ACC; }
        if (e == EV_KEY_OFF)        { return IGN_OFF; }
        break;
    default: break;                                            /* [SRS_SM_010] */
    }
    return s;
}

int main(void)
{
    IgnCtx ctx = { IGN_OFF };
    uint8_t acc, ign, str;

    MT_SECTION("정상 시동 시퀀스");
    ctx.state = ign_step(ctx.state, EV_KEY_ACC);
    ctx.state = ign_step(ctx.state, EV_KEY_ON);
    MT_CHECK(ctx.state == IGN_ON, "TC_SM_001 OFF->ACC->ON");
    ctx.state = ign_step(ctx.state, EV_START_BTN);
    outputs_of(ctx.state, &acc, &ign, &str);
    MT_CHECK(ctx.state == IGN_CRANK && str == 1u, "TC_SM_002 CRANK: 스타터 ON");
    ctx.state = ign_step(ctx.state, EV_ENGINE_STARTED);
    outputs_of(ctx.state, &acc, &ign, &str);
    MT_CHECK(ctx.state == IGN_RUN && ign == 1u && str == 0u,
             "TC_SM_003 RUN: 스타터 OFF, IGN 유지");

    MT_SECTION("예외 이벤트 / 무효 전이");
    ctx.state = IGN_OFF;
    MT_CHECK(ign_step(ctx.state, EV_START_BTN) == IGN_OFF,
             "TC_SM_010 OFF 에서 시동버튼은 무시");
    ctx.state = IGN_RUN;
    MT_CHECK(ign_step(ctx.state, EV_ENGINE_STALLED) == IGN_ON,
             "TC_SM_020 RUN 중 엔진 정지 -> ON 복귀(재시동 가능 상태)");

    MT_SUMMARY();
}
