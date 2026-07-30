#ifndef COM_H
#define COM_H
/*
 * Com.h - AUTOSAR COM (SWS_COM 간소화판)
 * 역할: "시그널"(의미 있는 물리량/제어비트)을 "I-PDU"(CAN 프레임 페이로드)로
 *       패킹/언패킹. RTE/SWC 는 시그널만 보고, CAN 프레임 레이아웃은 모른다.
 */
#include "ComStack_Types.h"
#include "Cfg_Ids.h"   /* 생성된 ID 심볼 (tools/gen_cfg.py) — 신호 추가 시 여기만 갱신됨 */

typedef uint8 Com_SignalIdType;

/* ==== Com_Cfg.h 에 해당 (설정 툴 생성) — ID 정의는 모두 generated/Cfg_Ids.h 로 이동 ==== */

void           Com_Init(void);

/* [SRS_COM_010] 시그널 값을 I-PDU 버퍼에 패킹 */
void           Com_SendSignal(Com_SignalIdType SignalId, uint32 SignalData);

/* [SRS_COM_020] 수신된 I-PDU 로부터 시그널 값을 언패킹. 미수신 시 E_NOT_OK */
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint32* SignalDataPtr);

/* [SRS_COM_030] 주기 송신 처리 (Os 10ms 태스크에서 호출) */
void           Com_MainFunctionTx(void);

/* [SRS_COM_040] 하위(PduR)로부터의 수신 통지 */
void           Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);

#endif /* COM_H */
