/*
 * Adc.c - MCAL Adc 드라이버 구현 (간소화)
 * 실제 타겟에서는 ADC_CR2 의 SWSTART 비트로 변환을 시작하고
 * EOC 플래그/DMA 로 결과를 수집한다. 여기선 SimMcu 가 결과를 즉시 제공.
 */
#include "Adc.h"
#include "SimMcu.h"

void Adc_Init(void) { /* 실제: 클럭 인에이블, 샘플타임 설정 등 */ }

/* [SRS_ADC_010] */
Std_ReturnType Adc_ReadGroup(uint8 Group, Adc_ValueGroupType* DataBufferPtr)
{
    if ((Group >= SIMMCU_ADC_CH_MAX) || (DataBufferPtr == NULL)) { return E_NOT_OK; }
    /* 실제로는: 변환 트리거 -> (ISR/DMA) -> 상태버퍼에서 읽기 */
    *DataBufferPtr = (Adc_ValueGroupType)SimMcu_ReadAdcRaw(Group);
    return E_OK;
}
