#ifndef SIMMCU_H
#define SIMMCU_H
/*
 * SimMcu.h - 시뮬레이션된 MCU (실제 프로젝트의 MCU 볼드/헤더 파일 역할)
 * PC 에서 MCAL 을 검증하기 위한 "가상 실리콘".
 * MCAL 소스는 이 헤더를 통해 레지스터에 접근하므로, 실제 타겟 이식 시
 * SimMcu 만 진짜 레지스터 주소 정의로 교체하면 된다 (추상화의 힘).
 */
#include "Std_Types.h"

#define SIMMCU_ADC_CH_MAX  8u

/* Dio 가 접근하는 GPIO 레지스터 (port0 의 ODR/IDR) */
extern volatile uint8 SimMcu_GpioOdr;
extern volatile uint8 SimMcu_GpioIdr;

void   SimMcu_Init(void);

/* --- 테스트/시뮬레이션 전용 API (실제 MCU 에는 없는 부분) --- */
void   SimMcu_InjectPin(uint8 pin, uint8 level); /* 외부 스위치 신호 주입 */
uint8  SimMcu_GetOutputPin(uint8 pin);           /* 출력 핀 관찰          */
void   SimMcu_InjectAdc(uint8 ch, uint16 raw12); /* 센서 아날로그 값 주입 */
uint16 SimMcu_ReadAdcRaw(uint8 ch);              /* ADC 변환 결과 읽기    */

#endif /* SIMMCU_H */
