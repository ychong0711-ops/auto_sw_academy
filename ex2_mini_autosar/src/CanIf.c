/*
 * CanIf.c - CAN Interface 구현 (설정 데이터는 generated/CanIf_Cfg.c 에 자동 생성됨)
 * "코드는 재사용, 매핑만 설정으로" 가 AUTOSAR BSW 의 핵심 철학.
 */
#include "CanIf.h"
#include "Can.h"
#include "PduR.h"        /* 상향 라우팅: CanIf -> PduR (CanIf 보고 경유지, PduR 이 상위) */
#include "Cfg_Types.h"   /* extern CanIf_TxPduConfig/CanIf_RxPduConfig */

/* [SRS_CANIF_010] */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    if (PduInfoPtr == NULL) { return E_NOT_OK; }
    for (uint16 i = 0u; i < CanIf_TxPduConfig_Size; i++) {
        if (CanIf_TxPduConfig[i].pduId == TxPduId) {
            Can_PduType canPdu;
            canPdu.id     = (Can_IdType)CanIf_TxPduConfig[i].canId;
            canPdu.length = (uint8)PduInfoPtr->SduLength;
            canPdu.sdu    = PduInfoPtr->SduDataPtr;
            return Can_Write(CanIf_TxPduConfig[i].hth, &canPdu);
        }
    }
    return E_NOT_OK;
}

/* [SRS_CANIF_020] 소프트웨어 필터링: 수신 프레임의 CAN ID -> PDU ID 해석 후 상향 통지 */
void CanIf_RxIndication(uint16 CanId, uint8 CanDlc, const uint8* CanSduPtr)
{
    for (uint16 i = 0u; i < CanIf_RxPduConfig_Size; i++) {
        if (CanIf_RxPduConfig[i].canId == CanId) {
            PduInfoType info;
            info.SduDataPtr = (uint8*)CanSduPtr;   /* const 해제: 상위는 읽기만 함 */
            info.SduLength  = CanDlc;
            PduR_CanIfRxIndication(CanIf_RxPduConfig[i].rxPduId, &info);
            return;
        }
    }
    /* 필터 미통과 프레임은 조용히 폐기 (실제 차량 버스에서 대부분의 프레임이 여기 해당) */
}
