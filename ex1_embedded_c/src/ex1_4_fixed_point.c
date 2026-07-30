/*
 * [EX1-4] 고정소수점(Q 포맷) - FPU 없는 ECU에서 실수 연산하기
 *
 * 실무 배경
 *  - 저가 MCU(Cortex-M0 등)에는 FPU 가 없다. float 는 소프트웨어 에뮬레이션 → 느리고 코드 증가.
 *  - AUTOSAR SWC 의 물리량은 대부분 "정수 raw 값 + factor/offset" 으로 다룬다
 *    (Com 신호의 ComSignal factor/offset 과 동일한 사고방식! → EX3 로 이어짐)
 *
 * 요구사항 추적: [SRS_FX_010] Q16.16 곱셈은 64비트 중간 결과 사용(오버플로 방지)
 *               [SRS_FX_020] 물리량 -> raw 변환 결과는 반올림
 */
#include <stdint.h>
#include "mini_test.h"

typedef int32_t q16_t;                    /* Q16.16 */
#define Q_SHIFT      16
#define Q_FROM_INT(x)   ((q16_t)((x) << Q_SHIFT))
#define Q_TO_INT(q)     ((int32_t)((q) >> Q_SHIFT))

static q16_t q_mul(q16_t a, q16_t b)      /* [SRS_FX_010] */
{
    int64_t t = ((int64_t)a * (int64_t)b) >> Q_SHIFT;
    /* 실무(MISRA)에서는 여기에 saturate 인터페이스를 둠 */
    return (q16_t)t;
}

static q16_t q_div(q16_t a, q16_t b)
{
    return (q16_t)(((int64_t)a << Q_SHIFT) / (int64_t)b);
}

/* 물리량 <-> raw (CAN 신호의 factor/offset 과 같은 개념) */
static uint16_t phys_to_raw(double phys, double factor, double offset)
{
    return (uint16_t)((phys - offset) / factor + 0.5);   /* [SRS_FX_020] 반올림 */
}
static double raw_to_phys(uint16_t raw, double factor, double offset)
{
    return (double)raw * factor + offset;
}

int main(void)
{
    MT_SECTION("Q16.16 연산");
    q16_t a = Q_FROM_INT(3) / 2;                       /* 1.5 */
    q16_t b = Q_FROM_INT(4);
    MT_CHECK(q_mul(a, b) == Q_FROM_INT(6), "TC_FX_010_01 1.5 * 4 = 6.0");
    MT_CHECK(q_div(b, Q_FROM_INT(2)) == Q_FROM_INT(2), "TC_FX_010_02 4 / 2 = 2");

    /* 오버플로 시연: (150<<16)*(150<<16) 은 int32 곱셈이면 깨진다(≈9.7e13).
       64비트 중간 결과로 계산 후 >>16 하면 Q 표현 범위(±32767) 안의 정답 22500.0 */
    q16_t big = Q_FROM_INT(150);
    MT_CHECK(q_mul(big, big) == (q16_t)(22500L << Q_SHIFT),
             "TC_FX_010_03 큰 수 곱셈에서 64비트 중간결과 필요");

    MT_SECTION("물리량 <-> raw (factor/offset)");
    /* 예: 차량 배터리 전압, factor=0.1 V/bit, offset=0, 12bit raw */
    uint16_t raw = phys_to_raw(12.7, 0.1, 0.0);
    MT_CHECK(raw == 127u, "TC_FX_020_01 12.7V -> raw 127");
    MT_CHECK(raw_to_phys(127u, 0.1, 0.0) > 12.6, "TC_FX_020_02 raw -> 12.7V 복원");

    MT_SUMMARY();
}
