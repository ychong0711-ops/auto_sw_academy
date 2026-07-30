/*
 * Os.c - 스케줄 순서가 곧 "데이터 흐름의 방향"
 *
 *   Rte_ComRxSync  : CAN 수신분을 RTE 로
 *   SensorSwc      : 하드웨어 -> RTE
 *   LightCtrlSwc   : RTE -> 제어 결정 -> RTE(+출력)
 *   Rte_ComTxSync  : RTE -> Com 송신 시그널
 *   Com_MainFunctionTx : Com -> PduR -> CanIf -> Can -> 버스
 */
#include "Os.h"
#include "Rte.h"
#include "Com.h"
#include "SensorSwc.h"
#include "LightCtrlSwc.h"

void Os_Init(void) { }

void Os_Tick10ms(void)
{
    Rte_ComRxSync();
    SensorSwc_Runnable();
    LightCtrlSwc_Runnable();
    Rte_ComTxSync();
    Com_MainFunctionTx();
}

void Os_RunTicks(uint32 n)
{
    for (uint32 i = 0u; i < n; i++) { Os_Tick10ms(); }
}
