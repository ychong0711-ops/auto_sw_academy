#ifndef SECOC_H
#define SECOC_H
/*
 * secoc.h - AUTOSAR SecOC(Secure Onboard Communication) 간소화 시뮬레이션
 *
 * 목적: CAN 프레임의 "위조 방지(무결성/인증) + 재전송 공격(replay) 방지".
 * Secured I-PDU 구성:
 *   [ Authentic I-PDU (원본 데이터) | 신선도(Freshness) 절단 | MAC(CMAC) 절단 ]
 * 인증자 계산 입력: DataToAuthenticator = PDU ID(2B BE) || 원본 || 전체 신선도(4B BE)
 *
 * 실제 AUTOSAR SecOC 와의 차이(학습용 간소화):
 *  - Freshness = 메시지 카운터만 사용 (Reset Flag/Resync 메시지는 생략)
 *  - MAC 입력/절단 길이는 프로젝트별 설정. 여기선 3B/3B 로 고정.
 */
#include "Std_Types.h"

#define SECOC_FRESH_TX_LEN  3u    /* 전송되는 신선도 절단 길이 */
#define SECOC_MAC_TX_LEN    3u    /* 전송되는 MAC 절단 길이 */
#define SECOC_OVERHEAD      (SECOC_FRESH_TX_LEN + SECOC_MAC_TX_LEN)

typedef struct {
    uint16 pduId;
    uint8  authLen;               /* 원본 PDU 길이 (=2 이면 최종 프레임 8바이트) */
    uint8  key[16];
    uint32 txCounter;             /* 신선도 소스: 송신 카운터 */
} SecOC_TxCtx;

typedef struct {
    uint16 pduId;
    uint8  authLen;
    uint8  key[16];
    uint32 lastAcceptedFresh;
    uint8  initialized;
    uint32 verifyFailCount;       /* MAC 불일치 누적 -> 보안 침입 대응 로직의 입력 */
    uint32 replayFailCount;       /* 구형/중복 신선도 누적 */
} SecOC_RxCtx;

void SecOC_InitTx(SecOC_TxCtx* ctx, uint16 pduId, uint8 authLen, const uint8 key[16]);
void SecOC_InitRx(SecOC_RxCtx* ctx, uint16 pduId, uint8 authLen, const uint8 key[16]);

/* [SRS_SECOC_010] 인증 PDU 를 보호하여 secured 프레임 생성 (길이: authLen+6) */
Std_ReturnType SecOC_Secure(SecOC_TxCtx* ctx, const uint8* auth, uint8* secured);

/* [SRS_SECOC_020] secured 프레임 검증. OK 시 auth 에 원본 복원. 실패 원인은 카운터로 */
Std_ReturnType SecOC_Verify(SecOC_RxCtx* ctx, const uint8* secured, uint8* auth);

#endif /* SECOC_H */
