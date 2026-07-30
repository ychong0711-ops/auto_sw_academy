#ifndef STD_TYPES_H
#define STD_TYPES_H
/*
 * Std_Types.h  (AUTOSAR SWS_StdType 간소화판)
 * 실제 AUTOSAR 프로젝트의 모든 모듈이 공유하는 표준 타입 헤더.
 * [참고] 진짜 AUTOSAR에서는 Std_ReturnType, Std_VersionInfoType 등이 정의됨.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t   uint8;
typedef int8_t    sint8;
typedef uint16_t  uint16;
typedef int16_t   sint16;
typedef uint32_t  uint32;
typedef int32_t   sint32;
typedef uint64_t  uint64;
typedef int64_t   sint64;

typedef uint8_t   Std_ReturnType;
#define E_OK       ((Std_ReturnType)0u)
#define E_NOT_OK   ((Std_ReturnType)1u)

#define STD_HIGH   ((uint8)0x01u)
#define STD_LOW    ((uint8)0x00u)

#endif /* STD_TYPES_H */
