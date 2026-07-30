#ifndef ADC_H
#define ADC_H
/*
 * Adc.h - MCAL Adc 드라이버 (AUTOSAR SWS_AdcDriver 간소화판)
 */
#include "Std_Types.h"

typedef uint16 Adc_ValueGroupType;

/* ==== Adc_Cfg.h 에 해당 ==== */
#define ADC_CH_BATTERY   0u   /* 배터리 전압 분배 입력 */
#define ADC_CH_AMBIENT   1u   /* 조도 센서            */

void           Adc_Init(void);
/* [SRS_ADC_010] 그룹 변환 결과를 raw(12bit)로 반환 */
Std_ReturnType Adc_ReadGroup(uint8 Group, Adc_ValueGroupType* DataBufferPtr);

#endif /* ADC_H */
