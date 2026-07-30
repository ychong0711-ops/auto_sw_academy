#ifndef UDS_SERVER_H
#define UDS_SERVER_H
/*
 * uds_server.h - ISO 14229-1 (UDS) 서버 간소화 구현
 * AUTOSAR 에서는 Dcm 모듈(DSL+DSD+DSP)이 이 일을 한다.
 *  - DSL: 세션/타이밍 관리(S3, 세션 전환 시 보안 재잠금)
 *  - DSD: 서비스 디스패치 + NRC 우선순위
 *  - DSP: 개별 서비스/DID/루틴 처리
 * 지원 서비스: 0x10, 0x11, 0x14, 0x19, 0x22, 0x27, 0x2E, 0x31, 0x3E
 */
#include "Std_Types.h"

/* 세션 */
#define UDS_SES_DEFAULT      0x01u
#define UDS_SES_PROGRAMMING  0x02u
#define UDS_SES_EXTENDED     0x04u

/* 자주 쓰는 NRC */
#define UDS_NRC_SERVICE_NOT_SUPPORTED        0x11u
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED    0x12u
#define UDS_NRC_INCORRECT_LENGTH             0x13u
#define UDS_NRC_CONDITIONS_NOT_CORRECT       0x22u
#define UDS_NRC_REQUEST_OUT_OF_RANGE         0x31u
#define UDS_NRC_SECURITY_ACCESS_DENIED       0x33u
#define UDS_NRC_INVALID_KEY                  0x35u
#define UDS_NRC_EXCEEDED_ATTEMPTS            0x36u
#define UDS_NRC_TIME_DELAY_NOT_EXPIRED       0x37u
#define UDS_NRC_SESSION_NOT_SUPPORTED        0x7Fu

#define UDS_DTC_MAX        4u
#define UDS_RESP_MAX       64u
#define UDS_S3_TICKS       500u     /* S3 세션 타임아웃 5s (10ms tick 기준) */
#define UDS_SEC_DELAY_TICKS 1000u   /* 보안 재시도 지연 10s */

/* 트랜스포트(실제: PduR<-CanTp)로 긍정/부정 응답을 볼내는 콜백 */
typedef void (*Uds_SendRawCb)(void* ctx, const uint8* resp, uint16 len);

typedef struct {
    uint8  d0, d1, d2;     /* 3바이트 DTC (2B base + 1B failure type) */
    uint8  status;
} Uds_Dtc;

typedef struct {
    /* DSL 상태 */
    uint8  session;            /* UDS_SES_* 의 비트값이 아닌 세션 번호: 1/2/3 */
    uint32 s3Ticks;
    uint8  unlocked;
    uint8  secAttempts;
    uint32 delayTicks;
    uint16 seed;
    uint8  resetPending;
    /* 예제 차량 데이터 */
    uint16 vehicleSpeedRaw;    /* DID 0x1234: 주행 속도 raw (쓰기 가능 = 조작 시연용) */
    Uds_Dtc dtc[UDS_DTC_MAX];
    uint8  dtcCount;
    /* 출력 */
    Uds_SendRawCb sendCb;
    void* sendCtx;
} UdsServer;

void Uds_Init(UdsServer* s, Uds_SendRawCb sendCb, void* sendCtx);

/* [SRS_UDS_010] 진단 요청 처리 (ISO-TP 완성 메시지를 그대로 전달) */
void Uds_OnRequest(UdsServer* s, const uint8* req, uint16 len);

/* 10ms 주기: S3 타임아웃, 보안 지연 타이머 */
void Uds_Tick10ms(UdsServer* s);

/* =================================================================== */
/* 테스트/검증 편의: 시드->키 알고리즘 (보안설계 "나쁜 예"이므로 학습용)   */
/* 실무에선 임의 복잡 함수 + ECU 고유 비밀키. 절대 재사용 금지!           */
uint16 UdsDemo_CalcKey(uint16 seed);
/* =================================================================== */

#endif /* UDS_SERVER_H */
