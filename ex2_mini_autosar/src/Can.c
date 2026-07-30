/*
 * Can.c - MCAL Can 드라이버 구현 (간소화)
 * Can036 등: 송신 모니터링, 컨트롤러 상태기계 등은 생략.
 * 하위 경계는 "버스" = SimBus.
 */
#include "Can.h"
#include "SimBus.h"
#include "CanIf.h"   /* Can_RxInterrupt -> CanIf_RxIndication (실제: Can283 상향 통지) */

void Can_Init(void) { /* 실제: baudrate, acceptance filter, 컨트롤러 STARTED 상태기계 */ }

/* [SRS_CAN_010] */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo)
{
    (void)Hth;  /* 간소화: mailbox 1개만 있다고 가정 */
    if ((PduInfo == NULL) || (PduInfo->length > 8u) || ((PduInfo->length > 0u) && (PduInfo->sdu == NULL))) {
        return E_NOT_OK;
    }
    SimBus_TransmitFromEcu(PduInfo->id, PduInfo->length, PduInfo->sdu);
    return E_OK;
}

/* CAN 수신 인터럽트 핸들러 역할 — 상향 통지 (CanIf_RxIndication) 이 핵심 */
void Can_RxInterrupt(Can_IdType CanId, uint8 Dlc, const uint8* Data)
{
    CanIf_RxIndication(CanId, Dlc, Data);
}
