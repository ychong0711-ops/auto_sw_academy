#ifndef CANIF_H
#define CANIF_H
/*
 * CanIf.h - CAN Interface (AUTOSAR SWS_CANInterface 간소화판)
 * 역할: "PDU ID <-> CAN ID/드라이버" 변환. 상위(PduR)는 CAN ID 를 모른다.
 */
#include "ComStack_Types.h"
#include "Cfg_Ids.h"   /* 생성된 PDU ID 심볼 */

/* ==== CanIf_Cfg.h 에 해당 (설정 툴 생성 영역) — ID 정의는 generated/Cfg_Ids.h ==== */

/* [SRS_CANIF_010] 상위(PduR)가 PDU ID 만으로 송신 요청 */
Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);

/* [SRS_CANIF_020] Can 드라이버로부터의 수신 통지 -> 상위 라우팅 (PduR) */
void CanIf_RxIndication(uint16 CanId, uint8 CanDlc, const uint8* CanSduPtr);

#endif /* CANIF_H */
