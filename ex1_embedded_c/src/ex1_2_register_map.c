/*
 * [EX1-2] 메모리 맵 레지스터 접근 - volatile 과 구조체 레지스터 맵
 *
 * 실무 배경
 *  - MCU의 주변장치(GPIO/UART/ADC...)는 특정 주소의 레지스터로 제어한다.
 *  - C 구조체를 레지스터 배치에 맞춰 정의하고 그 주소에 캐스팅하는 것이 표준 기법.
 *    (실제헤더: stm32f4xx.h 의 GPIOA->ODR 등)
 *  - volatile 이 없으면 컴파일러가 "의미 없는" 읽기/쓰기를 최적화로 삭제한다.
 *
 * 이 실습에서는 실제 MCU 대신 정적 배열을 "주변장치 메모리"로 사용한다.
 *
 * 요구사항 추적: [SRS_REG_010] BSRR 을 이용해 ODR 비트를 원자적으로 set/reset
 *               [SRS_REG_020] IDR 에서 입력 핀 상태를 읽는다
 */
#include <stdint.h>
#include "mini_test.h"

/* ---- 실제 MCU 데이터시트의 레지스터 블록에 해당 ---- */
typedef struct {
    volatile uint32_t CRL;    /* offset 0x00 */
    volatile uint32_t CRH;    /* offset 0x04 */
    volatile uint32_t IDR;    /* offset 0x08 읽기 전용  */
    volatile uint32_t ODR;    /* offset 0x0C            */
    volatile uint32_t BSRR;   /* offset 0x10 쓰기 전용:  1쓰면 set/reset */
} GpioRegs_t;
/* 주의: volatile 은 "최적화 금지"일 뿐 스레드/ISR 안전(동기화)은 아니다 */

/* 시뮬레이션된 주변장치 메모리. 실제로는 링커가 0x40010800 같은 주소를 부여 */
static uint32_t s_gpio_mem[5];
#define GPIOA ((GpioRegs_t *)s_gpio_mem)

/* [SRS_REG_010] BSRR: 하위16비트=set, 상위16비트=reset. RMW 없이 원자적 제어 */
static void gpio_write_pin(GpioRegs_t *port, uint8_t pin, uint8_t level)
{
    uint32_t v = (level != 0u) ? (1u << pin) : (1u << (pin + 16u));
    port->BSRR = v;
    /* 시뮬레이션 한계 처리: 실제 MCU 에서는 "하드웨어"가 BSRR 쓰기 직후
       ODR 을 갱신한다. 평범한 RAM 모델이므로 그 하드웨어 동작을 여기서 재현.
       (set 비트는 OR 로, reset 비트는 AND 로 — 같은 쓰기에서 set 우선) */
    port->ODR = (port->ODR | (v & 0x0000FFFFu)) & ~(v >> 16u);
}

/* [SRS_REG_020] IDR 로부터 입력 읽기 (시뮬에서는 IDR 직접 조작이 외부 신호 주입) */
static uint8_t gpio_read_pin(const GpioRegs_t *port, uint8_t pin)
{
    return (uint8_t)((port->IDR >> pin) & 1u);
}

static void sim_exti_drive(uint8_t pin, uint8_t level)   /* 외부 세계 시뮬레이션 */
{
    if (level != 0u) { s_gpio_mem[2] |=  (1u << pin); }   /* IDR = s_gpio_mem[2] */
    else             { s_gpio_mem[2] &= ~(1u << pin); }
}

int main(void)
{
    MT_SECTION("레지스터 맵 기본 동작");
    for (uint8_t i = 0; i < 5u; i++) { s_gpio_mem[i] = 0u; }

    gpio_write_pin(GPIOA, 5u, 1u);                 /* PA5 = LED ON 이라고 가정 */
    MT_CHECK((GPIOA->ODR & (1u << 5u)) != 0u, "TC_REG_010_01 BSRR set -> ODR 반영");

    gpio_write_pin(GPIOA, 5u, 0u);
    MT_CHECK((GPIOA->ODR & (1u << 5u)) == 0u, "TC_REG_010_02 BSRR reset -> ODR 클리어");

    sim_exti_drive(0u, 1u);                        /* 외부 버튼 눌림 시뮬레이션 */
    MT_CHECK(gpio_read_pin(GPIOA, 0u) == 1u, "TC_REG_020_01 IDR 로 입력 읽기");

    MT_SECTION("RMW(read-modify-write)와의 비교");
    /* ODR |= mask 방식은 읽고-수정-쓰기 3단계라 ISR과 경쟁하면 비트가 유실될 수 있음.
       real MCU 의 BSRR 는 그래서 존재한다. 문서 docs/01 을 참고 */
    GPIOA->ODR = 0x00FFu;
    gpio_write_pin(GPIOA, 3u, 1u);                 /* 다른 비트 보존 확인 */
    MT_CHECK(GPIOA->ODR == (0x00FFu | (1u << 3u)), "TC_REG_010_03 BSRR 는 타 비트 보존");

    MT_SUMMARY();
}
