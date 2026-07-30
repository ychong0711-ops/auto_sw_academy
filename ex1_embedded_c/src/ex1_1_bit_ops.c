/*
 * [EX1-1] 비트 연산 - 임베디드 C의 알파벳
 *
 * 학습 포인트
 *  - 레지스터는 "비트 필드의 집합"이다. 하드웨어 매뉴얼(RM)의 비트 정의를
 *    C 코드로 옮기는 것이 임베디드 개발의 출발점.
 *  - 부호 없는 타입(uint8_t 등)만 사용: 부호 있는 시프트는 구현 정의(implementation-defined)
 *  - MISRA-C:2012 Rule 10.x: 비트 연산은 unsigned 에서만.
 *
 * 요구사항 추적: [SRS_BIT_010] 지정 비트를 set/clear/toggle 할 수 있어야 한다
 *               [SRS_BIT_020] 특정 비트 구간(필드)의 값을 읽고 쓸 수 있어야 한다
 */
#include <stdint.h>
#include "mini_test.h"

#define BIT(n)            ((uint32_t)1u << (n))
#define SET_BIT(r, n)     ((r) |=  BIT(n))
#define CLR_BIT(r, n)     ((r) &= ~BIT(n))
#define TGL_BIT(r, n)     ((r) ^=  BIT(n))
#define GET_BIT(r, n)     (((r) >> (n)) & 1u)

/* [SRS_BIT_020] n비트 필드 마스크 + 필드 읽기/쓰기 */
#define FIELD_MASK(pos, width)  ((((uint32_t)1u << (width)) - 1u) << (pos))
static uint32_t field_get(uint32_t reg, uint8_t pos, uint8_t width)
{
    return (reg & FIELD_MASK(pos, width)) >> pos;
}
static uint32_t field_set(uint32_t reg, uint8_t pos, uint8_t width, uint32_t val)
{
    reg &= ~FIELD_MASK(pos, width);
    reg |= (val << (pos)) & FIELD_MASK(pos, width);
    return reg;
}

static uint8_t count_ones(uint32_t v)   /* Kernighan 방식 */
{
    uint8_t cnt = 0u;
    while (v != 0u) { v &= (v - 1u); cnt++; }
    return cnt;
}

int main(void)
{
    MT_SECTION("비트 set/clear/toggle");

    /* 가상의 타이머 제어 레지스터(TIMx_CR1)라고 생각하세요 */
    uint32_t CR1 = 0u;

    SET_BIT(CR1, 0);                 /* CEN: Counter Enable */
    MT_CHECK(GET_BIT(CR1, 0) == 1u, "TC_BIT_010_01 CEN 비트 set");

    CLR_BIT(CR1, 0);
    MT_CHECK(CR1 == 0u, "TC_BIT_010_02 CEN 비트 clear");

    TGL_BIT(CR1, 7);
    MT_CHECK(GET_BIT(CR1, 7) == 1u, "TC_BIT_010_03 비트7 toggle");

    MT_SECTION("비트 필드(구간) 읽기/쓰기");

    /* RCC 레지스터에서 흔히 보는 "APB1 프리스케러 [12:10]" 같은 필드 */
    uint32_t CFGR = 0u;
    CFGR = field_set(CFGR, 10, 3, 0b101u);          /* 3비트 필드에 5 기록 */
    MT_CHECK(field_get(CFGR, 10, 3) == 5u, "TC_BIT_020_01 필드 쓰고 읽기");
    CFGR = field_set(CFGR, 10, 3, 0u);
    MT_CHECK(CFGR == 0u, "TC_BIT_020_02 필드 클리어 시 다른 비트 무결");

    MT_SECTION("유틸");
    MT_CHECK(count_ones(0xF0F0u) == 8u, "TC_BIT_030_01 popcount(0xF0F0)=8");

    MT_SUMMARY();
}
