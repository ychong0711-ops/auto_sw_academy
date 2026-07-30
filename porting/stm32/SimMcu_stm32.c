/*
 * SimMcu_stm32.c — STM32 MCAL 대체 구현
 *
 * PC 시뮬레이션(SimMcu.c)을 STM32 HAL 기반 실제 하드웨어 드라이버로 교체.
 *
 * 주요 차이점:
 *   - GPIO: 실제 포트 C 레지스터(ODR/IDR)를 읽고 씀
 *   - ADC: HAL_ADC_GetValue()로 실제 ADC 채널변환
 *   - CAN: HAL_CAN_AddTxMessage()로 실제 CAN 버스 전송
 *   - SimMcu_InjectPin/InjectAdc: 테스트 용도로만 동작
 *
 * CubeMX 핀 할당 (권장):
 *   PC0 → Headlight 출력 (LED)
 *   PC1 → LightSwitch 입력 (버튼)
 *   PA0 → 배터리 전압 ADC (ADC1_IN0)
 *   PA1 → 조도 센서 ADC (ADC1_IN1)
 *   PB8 → CAN1_RX
 *   PB9 → CAN1_TX
 *
 * 참조: porting/stm32/stm32_porting_guide.md
 */

#include "SimMcu_stm32.h"
#include <string.h>

/* ====================================================================== */
/* ADC 채널 시뮬레이션 버퍼 (테스트 주입용)                               */
/* ====================================================================== */

/* 실제 ADC 값은 이 배열과 HAL_ADC_GetValue()를 OR로 결합.
   주입된 값이 있으면 우선, 없으면 하드웨어에서 읽음. */
static uint16_t s_injectedAdc[SIMMCU_ADC_CH_MAX];
static uint8_t  s_injectedValid[SIMMCU_ADC_CH_MAX];

/* PC 시뮬레이션에서 제공하는 상수 — 헤더 include 필요시 동일한 값 사용 */
#ifndef SIMMCU_ADC_CH_MAX
#define SIMMCU_ADC_CH_MAX  8u
#endif

/* ====================================================================== */
/* HAL 핸들 (CubeMX가 main.h에 extern 선언)                              */
/* ====================================================================== */

extern ADC_HandleTypeDef hadc1;
extern CAN_HandleTypeDef hcan1;

/* ====================================================================== */
/* 구현                                                                   */
/* ====================================================================== */

void SimMcu_Init(void)
{
    /* 주변장치 초기화는 MX_*_Init()에서 이미 수행됨 (CubeMX 생성 main.c) */

    /* ADC 주입 버퍼 초기화 */
    memset(s_injectedAdc, 0, sizeof(s_injectedAdc));
    memset(s_injectedValid, 0, sizeof(s_injectedValid));

    /* GPIO 초기 상태: Headlight OFF */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
}

/* ====================================================================== */
/* GPIO — 실제 포트 C 레지스터 접근                                       */
/* ====================================================================== */

/* SimMcu_InjectPin: 테스트/주입 용도. IDR은 하드웨어 전용이므로
   실제 구동 시에는 효과 없음. 디버그 메시지만 출력. */
void SimMcu_InjectPin(uint8_t pin, uint8_t level)
{
    if (pin >= 8u) return;  /* PC 시뮬레이션과 동일 범위 (pin 0-7) */

    if (level != 0u) {
        HAL_GPIO_WritePin(GPIOC, (uint16_t)(1u << pin), GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOC, (uint16_t)(1u << pin), GPIO_PIN_RESET);
    }
}

/* SimMcu_GetOutputPin: ODR에서 핀 상태 읽기 */
uint8_t SimMcu_GetOutputPin(uint8_t pin)
{
    if (pin >= 8u) return 0u;
    return (uint8_t)((GPIOC->ODR >> pin) & 1u);
}

/* ====================================================================== */
/* ADC — 실제 하드웨어 채널 변환                                         */
/* ====================================================================== */

/* SimMcu_InjectAdc: 특정 채널에 가상 값 주입 (테스트 용도).
   실제 ADC 변환보다 우선함. */
void SimMcu_InjectAdc(uint8_t ch, uint16_t raw12)
{
    if (ch >= SIMMCU_ADC_CH_MAX) return;
    s_injectedAdc[ch]    = raw12 & 0x0FFFu;
    s_injectedValid[ch]  = 1u;
}

/* SimMcu_ReadAdcRaw: 실제 ADC 변환 결과 반환 (12비트 마스킹).
   주입된 값이 있으면 주입값 우선. */
uint16_t SimMcu_ReadAdcRaw(uint8_t ch)
{
    if (ch >= SIMMCU_ADC_CH_MAX) return 0u;

    /* 주입된 값이 있으면 우선 반환 (테스트/디버그) */
    if (s_injectedValid[ch]) {
        return s_injectedAdc[ch];
    }

    /* 실제 ADC 변환 수행 */
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        return HAL_ADC_GetValue(&hadc1) & 0x0FFFu;
    }
    return 0u;
}

/* ====================================================================== */
/* CAN — See Can.c for the AUTOSAR CAN driver layer                        */
/* ====================================================================== */

/*
 * Can_Write()는 Can.c(수정 없음)에 구현되어 있으며,
 * 내부적으로 SimBus_TransmitFromEcu()를 호출합니다.
 * STM32 포팅 시 Can.c의 SimBus_TransmitFromEcu 호출을
 * HAL_CAN_AddTxMessage()로 변경해야 합니다.
 *
 * Can.c 수정 예:
 *
 *   // 기존 (PC 시뮬레이션):
 *   // SimBus_TransmitFromEcu(msgId, dlc, data);
 *
 *   // STM32 실제:
 *   CAN_TxHeaderTypeDef header;
 *   header.StdId = msgId;
 *   header.ExtId = 0;
 *   header.IDE   = CAN_ID_STD;
 *   header.RTR   = CAN_RTR_DATA;
 *   header.DLC   = dlc;
 *   header.TransmitGlobalIntent = CAN_TX_PRIORITY_HIGH;
 *   uint32_t mailbox;
 *   HAL_CAN_AddTxMessage(&hcan1, &header, (uint8_t*)data, &mailbox);
 *
 * CAN 수신 (CanIf_RxIndication 호출 체인):
 *
 *   void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
 *       CAN_RxHeaderTypeDef header;
 *       uint8_t data[8];
 *       HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, data);
 *       CanIf_RxIndication(header.StdId, header.DLC, data);
 *   }
 */

/* ====================================================================== */
/* 디버그: UART printf 지원 (ITM / UART)                                  */
/* ====================================================================== */

/*
 * printf를 UART로 출력하려면 _write() stub 필요:
 *
 *   // stm32/Core/Src/main.c 에 추가
 *   extern UART_HandleTypeDef huart2;
 *
 *   int _write(int file, char *ptr, int len) {
 *       HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
 *       return len;
 *   }
 *
 * CubeMX: USART2 Asynchronous, PA2=TX, PA3=RX
 * PC: PuTTY or screen (115200-8N1)
 */
