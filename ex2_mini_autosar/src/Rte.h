#ifndef RTE_H
#define RTE_H
/*
 * Rte.h - RTE (간소화 "자동 생성" 코드)
 * 실제 RTE 는 ARXML 에서 생성된 수만 줄의 코드. SWC 는 오직 이 API 로만
 * 세상과 통신한다 (MCAL/BSW 직접 호출 금지 → 이식성 확보).
 *
 * 여기서는 Sender-Receiver 포트를 단순 버퍼 + Rte_Read/Rte_Write 로,
 * Client-Server(IoHwAb 호출)를 Rte_Call_* 로 흉낸다.
 */
#include "Std_Types.h"

void Rte_Init(void);

/* ---- SensorSwc 포트 ---- */
/* Client-Server: 하드웨어 입력 (IoHwAb 를 RTE 통해 호출) */
Std_ReturnType Rte_Call_BatteryAdc_Read(uint16* raw12);
Std_ReturnType Rte_Call_AmbientAdc_Read(uint16* raw12);
uint8          Rte_Call_LightSwitch_Get(void);
/* Sender-Receiver: 타이어 데이터 송신(쓰기) 포트 */
void Rte_Write_BatteryVoltage(uint16 raw12);
void Rte_Write_AmbientLight(uint16 raw12);
void Rte_Write_LightSwitch(uint8 level);

/* ---- LightCtrlSwc 포트 ---- */
uint16 Rte_Read_AmbientLight(void);
uint8  Rte_Read_LightSwitch(void);
uint8  Rte_Read_OverrideMode(void);
uint8  Rte_Read_OverrideKey(void);
uint8  Rte_Read_HeadlightCmd(void);
void   Rte_Write_HeadlightCmd(uint8 cmd);
void   Rte_Call_Headlight_Set(uint8 level);

/* ---- RTE 태스크 내 Com 과의 데이터 매핑 (실제로는 생성됨) ---- */
void Rte_ComRxSync(void);   /* Com 수신 시그널 -> RTE 버퍼 */
void Rte_ComTxSync(void);   /* RTE 버퍼 -> Com 송신 시그널 */

#endif /* RTE_H */
