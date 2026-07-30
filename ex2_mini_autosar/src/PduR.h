#ifndef PDUR_H
#define PDUR_H
/*
 * PduR.h - PDU Router (AUTOSAR SWS_PDURouter 간소화판)
 * 역할: 상위 모듈(Com, Dcm...)과 하위 인터페이스(CanIf, LinIf...) 사이의
 *       PDU 라우팅. "누가 누구에게 전달하는가"를 설정 테이블로 기술.
 */
#include "ComStack_Types.h"
#include "Cfg_Ids.h"   /* 생성된 라우팅用 PDU ID 심볼 */

/* ===== PduR_Cfg.h 에 해당 — ID 정의는 generated/Cfg_Ids.h ===== */

/* TX: Com 이 호출하는 API (실제 네이밍 그대로: 송신 요청은 상위 모듈 이름이 붙음) */
Std_ReturnType PduR_ComTransmit(PduIdType ComTxPduId, const PduInfoType* PduInfoPtr);

/* RX: CanIf 가 호출하는 상향 통지 (실제 네이밍 그대로: 통지는 하위 모듈 이름이 붙음) */
void PduR_CanIfRxIndication(PduIdType CanIfRxPduId, const PduInfoType* PduInfoPtr);

#endif /* PDUR_H */
