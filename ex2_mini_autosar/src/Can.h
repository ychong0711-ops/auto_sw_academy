#ifndef CAN_H
#define CAN_H
/*
 * Can.h - MCAL Can 드라이버 (AUTOSAR SWS_CanDriver 간소화판)
 * 상위(CanIf)는 CAN 컨트롤러의 존재를 모른 채 HTH 로 송신한다.
 */
#include "Std_Types.h"

typedef uint16 Can_IdType;

typedef struct {
    Can_IdType    id;       /* 송신할 CAN 식별자 (11bit 표준) */
    uint8         length;   /* DLC 0..8 */
    const uint8*  sdu;      /* 데이터 */
} Can_PduType;

typedef uint8 Can_HwHandleType;   /* HOH(Hardware Object Handle) */

void Can_Init(void);
/* [SRS_CAN_010] 송신 처리(Hardware Object 에 PDU 적재) */
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType* PduInfo);

/* ==== 아래는 "CAN 컨트롤러 난입 인터럽트"에 해당. SimBus 가 호출한다 ==== */
void Can_RxInterrupt(Can_IdType CanId, uint8 Dlc, const uint8* Data);

#endif /* CAN_H */
