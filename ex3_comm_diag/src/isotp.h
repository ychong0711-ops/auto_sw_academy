#ifndef ISOTP_H
#define ISOTP_H
/*
 * isotp.h - ISO 15765-2 (ISO-TP) 간소화 구현
 * 역할: 8바이트 CAN 프레임 위에서 최대 4095 바이트 메시지를 분할/재조립.
 * 프레임 종류 (첫 바이트 상위 니블 = PCI 타입):
 *   SF(0x0N): 단일 프레임, N=길이(1..7)
 *   FF(0x1N): 첫 프레임,  N+다음바이트 = 전체 길이(12bit), 데이터 6바이트
 *   FC(0x3N): 흐름제어  N: 0=CTS,1=Wait,2=Overflow / 이후 BS, STmin
 *   CF(0x2N): 연속 프레임, N=시퀀스번호(0..15 롤오버)
 * 타이머: N_As/N_Ar/N_Bs/N_Cs/N_Cr 중 핵심(N_Bs, N_Cr)만 구현 (나머지는 과제).
 */
#include "Std_Types.h"

#define ISOTP_MAX_BUF   255u     /* 수용 가능한 최대 메시지 길이 */
#define ISOTP_N_BS_MS   1000u    /* FC 대기 타임아웃 */
#define ISOTP_N_CR_MS   1000u    /* CF 대기 타임아웃 */
#define ISOTP_WFT_MAX   3u       /* FC.Wait 연속 허용 횟수 */

typedef void (*IsoTp_RxMsgCb)(void* ctx, const uint8* data, uint16 len);
typedef void (*IsoTp_TxDoneCb)(void* ctx, uint8 success);

typedef enum { ISOTP_TX_IDLE = 0, ISOTP_TX_WAIT_FC, ISOTP_TX_SEND_CF } IsoTpTxState;
typedef enum { ISOTP_RX_IDLE = 0, ISOTP_RX_WAIT_CF } IsoTpRxState;

typedef struct IsoTp_s {
    /* ---- 설정 ---- */
    uint16 rxId;                 /* 이 링크가 수신할 CAN ID */
    uint16 txId;                 /* 송신 CAN ID             */
    int    node;                 /* vcan 노드               */
    uint8  rxStminMs;            /* 상대 CF 송신 시 요구할 간격 */
    /* ---- TX 상태 ---- */
    IsoTpTxState txState;
    uint8  txBuf[ISOTP_MAX_BUF];
    uint16 txLen, txOff;
    uint8  txSn;
    uint8  txStminMs;            /* 피어가 지정한 분리 시간 */
    uint8  txStminLeft;
    uint8  wftCnt;
    uint16 txNbsLeft;
    /* ---- RX 상태 ---- */
    IsoTpRxState rxState;
    uint8  rxBuf[ISOTP_MAX_BUF];
    uint16 rxTotal, rxOff;
    uint8  rxSn;
    uint16 rxNcrLeft;
    /* ---- 콜백/통계 ---- */
    IsoTp_RxMsgCb  rxCb;      void* rxCtx;
    IsoTp_TxDoneCb txDoneCb;  void* txCtx;
    uint32 errRxSn;              /* 시퀀스번호 불일치로 abort */
    uint32 errRxTimeout;
    uint32 errRxOverflow;        /* 버퍼 초과로 OVFL 송신 */
    uint32 errTxAbort;
} IsoTp;

void  IsoTp_Init(IsoTp* l, uint16 rxId, uint16 txId, int node);
void  IsoTp_SetRxCb(IsoTp* l, IsoTp_RxMsgCb cb, void* ctx);
void  IsoTp_SetTxDoneCb(IsoTp* l, IsoTp_TxDoneCb cb, void* ctx);

/* [SRS_TP_010] 메시지 송신 시작 (비동기: 완료는 txDoneCb) */
uint8 IsoTp_Send(IsoTp* l, const uint8* data, uint16 len);

/* vcan 콜백에서 호출: CAN 프레임 수신 처리 */
void  IsoTp_OnCanFrame(IsoTp* l, uint16 id, uint8 dlc, const uint8* data);

/* 1ms 주기 호출 (STmin, 타임아웃 처리) */
void  IsoTp_MainFunction(IsoTp* l);

#endif /* ISOTP_H */
