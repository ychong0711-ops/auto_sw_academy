/*
 * PduR.c - PDU Router 구현 (라우팅 테이블은 generated/PduR_Cfg.c 자동 생성)
 */
#include "PduR.h"
#include "CanIf.h"
#include "Com.h"
#include "Cfg_Types.h"   /* extern PduR_TxRoutes/PduR_RxRoutes */

Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType* PduInfoPtr)
{
    for (uint16 i = 0u; i < PduR_TxRoutes_Size; i++) {
        if (PduR_TxRoutes[i].srcCom == ComTxPduId) {
            return CanIf_Transmit(PduR_TxRoutes[i].dstCanIf, PduInfoPtr);
        }
    }
    return E_NOT_OK;
}

void PduR_CanIfRxIndication(PduIdType CanIfRxPduId, const PduInfoType* PduInfoPtr)
{
    for (uint16 i = 0u; i < PduR_RxRoutes_Size; i++) {
        if (PduR_RxRoutes[i].srcCanIf == CanIfRxPduId) {
            Com_RxIndication(PduR_RxRoutes[i].dstCom, PduInfoPtr);
            return;
        }
    }
}
