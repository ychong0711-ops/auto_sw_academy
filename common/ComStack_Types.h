#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H
/*
 * ComStack_Types.h (AUTOSAR 통신 스택 공용 타입 - 간소화판)
 * Com / PduR / CanIf / Dcm 이 주고받는 PDU를 표현하는 공통 타입.
 */
#include "Std_Types.h"

typedef uint16 PduIdType;
typedef uint16 PduLengthType;

typedef struct {
    uint8*        SduDataPtr;   /* PDU 데이터 버퍼 포인터 */
    PduLengthType SduLength;    /* 유효 길이 (바이트)      */
} PduInfoType;

#endif /* COMSTACK_TYPES_H */
