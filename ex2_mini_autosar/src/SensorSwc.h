#ifndef SENSORSWC_H
#define SENSORSWC_H
/*
 * SensorSwc - Application 계층 SWC #1 (센서 수집 담당)
 * 규칙: RTE_API 로만 통신. #include "Adc.h"/"SimMcu.h" 같은 직접 접근은 금지!
 */
void SensorSwc_Runnable(void);
#endif
