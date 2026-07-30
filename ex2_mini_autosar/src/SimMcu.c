/*
 * SimMcu.c - 가상 MCU 구현부
 * [참고] 이 파일의 Inject 계열 API 는 "물리적 세계"를 흉낼 뿐,
 *        MCAL 드라이버(Dio/Adc)는 절대 이들을 호출하지 않는다 (계층 위반 금지!)
 */
#include "SimMcu.h"

volatile uint8 SimMcu_GpioOdr;   /* 출력 데이터 레지스터 */
volatile uint8 SimMcu_GpioIdr;   /* 입력 데이터 레지스터 */

static uint16 s_adc[SIMMCU_ADC_CH_MAX];

void SimMcu_Init(void)
{
    SimMcu_GpioOdr = 0u;
    SimMcu_GpioIdr = 0u;
    for (uint8 i = 0u; i < SIMMCU_ADC_CH_MAX; i++) { s_adc[i] = 0u; }
}

void SimMcu_InjectPin(uint8 pin, uint8 level)
{
    if (pin >= 8u) { return; }
    if (level != 0u) { SimMcu_GpioIdr |=  (uint8)(1u << pin); }
    else             { SimMcu_GpioIdr &= ~(uint8)(1u << pin); }
}

uint8 SimMcu_GetOutputPin(uint8 pin)
{
    if (pin >= 8u) { return 0u; }
    return (uint8)((SimMcu_GpioOdr >> pin) & 1u);
}

void SimMcu_InjectAdc(uint8 ch, uint16 raw12)
{
    if (ch < SIMMCU_ADC_CH_MAX) { s_adc[ch] = (uint16)(raw12 & 0x0FFFu); }
}

uint16 SimMcu_ReadAdcRaw(uint8 ch)
{
    if (ch >= SIMMCU_ADC_CH_MAX) { return 0u; }
    return s_adc[ch];
}
