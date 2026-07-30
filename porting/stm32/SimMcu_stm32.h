/*
 * SimMcu_stm32.h — STM32 MCAL 교체용 헤더
 *
 * 역할:
 *   PC 시뮬레이션의 SimMcu.h를 실제 STM32 HAL로 대체.
 *   이 헤더를 include하면 MCAL(Dio, Adc, Can)이 실제 하드웨어 레지스터에
 *   접근하게 된다. Application/SWC/BSW는 0줄 수정.
 *
 * 사용법:
 *   #include "SimMcu_stm32.h"   // STM32 HAL 레지스터 사용
 *   // #include "SimMcu.h"      // PC 시뮬레이션 (원본)
 *
 * 타겟: STM32F401RE (NUCLEO-F401RE)
 *   GPIO 포트 할당:
 *     PC0 — Headlight 출력 (LED)
 *     PC1 — LightSwitch 입력 (버튼)
 *     PA0 — 배터리 전압 센서 (ADC1_IN0)
 *     PA1 — 조도 센서 (ADC1_IN1)
 *   CAN:
 *     PB8 — CAN1_RX
 *     PB9 — CAN1_TX
 *     (외부 TJA1050 트랜시버 필요)
 *
 * 이식 참고 (stm32_porting_guide.md 참조):
 *   1. STM32CubeMX로 핀 할당
 *   2. stm32f4xx_hal_conf.h: HAL_GPIO, HAL_ADC, HAL_CAN, HAL_UART 활성화
 *   3. main.c에서 MX_*_Init() 호출 후 SimMcu_Init()
 *   4. SysTick_Handler()에서 1ms 틱 → Os_Tick10ms() 카운트
 */

#ifndef SIMMCU_STM32_H
#define SIMMCU_STM32_H

#include <stdint.h>
#include "stm32f4xx_hal.h"   /* STM32 HAL — CubeMX 생성 */

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================== */
/* GPIO 레지스터 — PC 시뮬레이션 변수를 실제 하드웨어 레지스터로 교체      */
/* ====================================================================== */

/* PC 시뮬레이션 (SimMcu.h):
 *   extern volatile uint8_t SimMcu_GpioOdr;  // 가상 ODR
 *   extern volatile uint8_t SimMcu_GpioIdr;  // 가상 IDR
 *
 * STM32 실제:
 *   ODR: 포트 C의 출력 데이터 레지스터
 *   IDR: 포트 C의 입력 데이터 레지스터
 *
 * 포트 할당 (NUCLEO-F401RE 기준):
 *   PC0 — Headlight (출력, LED)
 *   PC1 — LightSwitch (입력, 버튼)
 */

/* 주의: STM32 ODR/IDR은 16비트, PC 시뮬레이션은 8비트.
   IoHwAb와 Dio는 uint8_t로 통일되어 있으므로,
   하위 8비트만 사용. 포트 C의 pin 0~7로 제한됨. */
#define SimMcu_GpioOdr  (*(volatile uint8_t *)(&(GPIOC->ODR)))
#define SimMcu_GpioIdr  (*(volatile uint8_t *)(&(GPIOC->IDR)))

/* ====================================================================== */
/* 시뮬레이션 전용 API — STM32에서는 테스트/진단 용도로 한정               */
/* ====================================================================== */

/* SimMcu_InjectPin: STM32에서 IDR은 하드웨어가 결정하므로
   이 함수는 테스트 용도로만 제한. 실제 운용 시에는 호출 금지.
   (안전: extern 선언만 유지, 내용은 빈 함수 또는 에러) */
void     SimMcu_Init(void);
void     SimMcu_InjectPin(uint8_t pin, uint8_t level);
uint8_t  SimMcu_GetOutputPin(uint8_t pin);

/* InjectAdc → STM32에서는 테스트 용도로만 사용.
   실 ADC 값이 필요하면 HAL_ADC_GetValue() 직접 호출 */
void     SimMcu_InjectAdc(uint8_t ch, uint16_t raw12);
uint16_t SimMcu_ReadAdcRaw(uint8_t ch);

/* ====================================================================== */
/* 실제 HAL 래퍼 — SimMcu_ReadAdcRaw의 STM32 구현을 위한 외부 참조       */
/* ====================================================================== */

/* STM32CubeMX가 생성하는 ADC 핸들 (main.h에 extern 선언 필요):
 *   extern ADC_HandleTypeDef hadc1;
 *
 * 사용 예 (SimMcu_stm32.c):
 *   uint16_t SimMcu_ReadAdcRaw(uint8_t ch) {
 *       HAL_ADC_Start(&hadc1);
 *       HAL_ADC_PollForConversion(&hadc1, 10);
 *       return HAL_ADC_GetValue(&hadc1) & 0x0FFF;  // 12-bit masking
 *   }
 */

/* ====================================================================== */
/* CAN 레지스터 — Can.c와 Can_RxInterrupt를 STM32 HAL로 연결              */
/* ====================================================================== */

/* PC 시뮬레이션:
 *   SimBus_TransmitFromEcu() — CAN 버스로 송신
 *   SimBus_InjectRx() — CAN 수신 인터럽트 시뮬레이션
 *
 * STM32 실제:
 *   Can_Write() → HAL_CAN_AddTxMessage()  (하드웨어 CAN TX)
 *   HAL_CAN_RxFifo0MsgPendingCallback() → CanIf_RxIndication() (HW RX ISR)
 *
 * CAN 핀 할당:
 *   PB8 = CAN1_RX
 *   PB9 = CAN1_TX
 *
 * 간소화: NUCLEO-F401RE는 내부 CAN 트랜시버가 없음.
 *   옵션 A: 외부 TJA1050 모듈 (€3) 연결
 *   옵션 B: CAN Loopback Mode — 하드웨어 없이 자체 검증
 *     CAN 필터를 Loopback으로 설정:
 *       CAN_FilterTypeDef filter;
 *       filter.FilterMode = CAN_FILTERMODE_IDMASK;
 *       filter.FilterScale = CAN_FILTERSCALE_32BIT;
 *       filter.FilterFIFOAssignment = CAN_RX_FIFO0;
 *       filter.FilterActivation = CAN_FILTER_ENABLE;
 *       filter.SlaveStartFilterBank = 14;
 *       HAL_CAN_ConfigFilter(&hcan1, &filter);
 *       HAL_CAN_Start(&hcan1);
 *       HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
 */

/* CAN 컨트롤러 초기화 (CubeMX 생성 핸들) */
extern CAN_HandleTypeDef hcan1;

/* AUTOSAR Can 드라이버 — Can.c (수정 없음)에서 사용 */
#define CAN_HTH_COUNT  1u

/*  =================================================================== */
/*  타이머 — SysTick 기반 틱                                         */
/* =================================================================== */

/* PC 시뮬레이션: Os_Tick10ms() — 수동 호출
 * STM32:         SysTick_Handler() — 1ms 인터럽트
 *
 * main_stm32.c 구현 예:
 *   volatile uint32_t g_sysTick1ms = 0;
 *
 *   void SysTick_Handler(void) {
 *       HAL_IncTick();
 *       g_sysTick1ms++;
 *   }
 *
 *   while (1) {  // 메인 루프
 *       uint32_t now = g_sysTick1ms;
 *       static uint32_t last = 0;
 *       if (now - last >= 10) {  // 10ms 경과
 *           last = now;
 *           Os_Tick10ms();       // AUTOSAR 주기 처리
 *       }
 *   }
 */

#ifdef __cplusplus
}
#endif

#endif /* SIMMCU_STM32_H */
