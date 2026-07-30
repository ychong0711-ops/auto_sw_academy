#ifndef E2E_P01_H
#define E2E_P01_H
/*
 * e2e_p01.h - AUTOSAR E2E Profile 1 간소화판 (기능안전 ISO 26262 의 통신 보호)
 *
 * 목적: SWC 간/ECU 간 신호가 "길을 잃고/복사되고/순서가 뒤바뀌고/깨져도" 감지.
 * 구성 (제어 필드 2바이트):
 *   byte0 하위니블: Alive Counter (0..14 순환)
 *   byte1         : CRC-8 over [DataID low, DataID high, data 전체(CRC바이트 제외)]
 *
 * 여기서 DataID 는 신호의 고유 식별자(설정값). 수신측은 같은 DataID 를 알아야 함.
 */
#include "Std_Types.h"
#include "crc8.h"

typedef enum {
    E2E_P01_OK = 0,          /* 정상 (카운터 +1) */
    E2E_P01_OKSOMELOST,      /* 정상이나 중간 프레임 유실 (허용 범위 내) */
    E2E_P01_REPEATED,        /* 같은 카운터 재수신 (정상 중복일 수 있음) */
    E2E_P01_WRONGSEQUENCE,   /* 유실 허용 초과/순서 이상 */
    E2E_P01_ERROR,           /* CRC 불일치 (데이터 오염) */
    E2E_P01_INITIAL          /* 첫 수신 정상 */
} E2E_P01Status;

typedef struct {
    uint16 dataId;           /* 신호 고유 ID */
    uint8  maxDeltaCounter;  /* 허용 최대 카운터 도약 */
} E2E_P01Config;

typedef struct {
    uint8 txCounter;         /* 송신 측 카운터 */
    uint8 rxLastCounter;
    uint8 rxWaitForFirst;    /* 첫 수신 전인가 */
} E2E_P01State;

#define E2E_CTRL_LEN 2u      /* 제어 필드 길이 */
#define E2E_COUNTER_MAX 14u

void E2E_P01Protect(const E2E_P01Config* cfg, E2E_P01State* st, uint8* data, uint8 len);
E2E_P01Status E2E_P01Check(const E2E_P01Config* cfg, E2E_P01State* st,
                           const uint8* data, uint8 len);

#endif /* E2E_P01_H */
