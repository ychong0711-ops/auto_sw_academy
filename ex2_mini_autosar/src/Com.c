/*
 * Com.c - AUTOSAR COM 구현 + 신호 레이아웃 "설정"
 *
 * 아래 시그널 테이블이 실제로는 DBC/ARXML 에서 생성되는 설정(Cfg) 데이터.
 * 여기서의 바이트 오더는 Intel(little-endian) — AUTOSAR Com 의 기본.
 * (Motorola/big-endian 패킹은 EX3 can_signal.c 에서 다룹니다)
 *
 *  ┌ VehicleState (CAN 0x100, 100ms 주기) ───────────────────────────┐
 *  │ bit  0..11 : BatteryVoltage (raw 12bit)                         │
 *  │ bit 12     : LightSwitch                                        │
 *  │ bit 13     : HeadlightCmd                                       │
 *  └──────────────────────────────────────────────────────────────────┘
 *  ┌ LightOverride (CAN 0x200, BCM->본 ECU, 이벤트) ─────────────────┐
 *  │ byte 0 : OverrideMode (0=Auto, 1=ForceOn, 2=ForceOff)           │
 *  │ byte 1 : OverrideKey  (0x5A = 유효성 인증 난수)                  │
 *  └──────────────────────────────────────────────────────────────────┘
 */
#include "Com.h"
#include "PduR.h"        /* Com_MainFunctionTx -> PduR_ComTransmit */
#include "Cfg_Types.h"   /* extern Com_SignalConfig (generated/Com_Cfg.c) */

/* ---------- I-PDU 상태 (런타임: 생성 대상 아님) ---------- */
typedef struct {
    uint8  data[8];
    uint8  dlc;
    uint32 periodTicks;   /* ComTxModeTimePeriod */
    uint32 timer;
} Com_TxPduDyn;

typedef struct {
    uint8  data[8];
    uint8  dlc;
    uint8  received;      /* 첫 수신 여부 */
} Com_RxPduDyn;

/* ---------- 시그널 설정 테이블 → generated/Com_Cfg.c (tools/gen_cfg.py) 로 이동 ---------- */
static Com_TxPduDyn s_txPdu[1];
static Com_RxPduDyn s_rxPdu[1];

void Com_Init(void)
{
    s_txPdu[0].dlc = 8u; s_txPdu[0].periodTicks = 1u; s_txPdu[0].timer = 0u;
    for (uint8 i = 0u; i < 8u; i++) { s_txPdu[0].data[i] = 0u; s_rxPdu[0].data[i] = 0u; }
    s_rxPdu[0].dlc = 2u; s_rxPdu[0].received = 0u;
}

/* ---------- little-endian 비트 패킹 유틸 ---------- */
static uint32 unpack_le(const uint8* d, uint16 start, uint8 len)
{
    uint32 v = 0u;
    for (uint8 b = 0u; b < len; b++) {
        uint16 pos = (uint16)(start + b);
        uint8 bit = (uint8)((d[pos >> 3u] >> (pos & 7u)) & 1u);
        v |= ((uint32)bit << b);
    }
    return v;
}

static void pack_le(uint8* d, uint16 start, uint8 len, uint32 v)
{
    for (uint8 b = 0u; b < len; b++) {
        uint16 pos = (uint16)(start + b);
        uint8  msk = (uint8)(1u << (pos & 7u));
        if (((v >> b) & 1u) != 0u) { d[pos >> 3u] |=  msk; }
        else                       { d[pos >> 3u] &= (uint8)(~msk); }
    }
}

/* [SRS_COM_010] */
void Com_SendSignal(Com_SignalIdType SignalId, uint32 SignalData)
{
    for (uint16 i = 0u; i < Com_SignalConfig_Size; i++) {
        if ((Com_SignalConfig[i].id == SignalId) && (Com_SignalConfig[i].isTx != 0u)) {
            pack_le(s_txPdu[Com_SignalConfig[i].pduIdx].data, Com_SignalConfig[i].startBit,
                    Com_SignalConfig[i].length, SignalData);
            return;
        }
    }
}

/* [SRS_COM_020] */
Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, uint32* SignalDataPtr)
{
    if (SignalDataPtr == NULL) { return E_NOT_OK; }
    for (uint16 i = 0u; i < Com_SignalConfig_Size; i++) {
        if ((Com_SignalConfig[i].id == SignalId) && (Com_SignalConfig[i].isTx == 0u)) {
            if (s_rxPdu[Com_SignalConfig[i].pduIdx].received == 0u) { return E_NOT_OK; }
            *SignalDataPtr = unpack_le(s_rxPdu[Com_SignalConfig[i].pduIdx].data,
                                       Com_SignalConfig[i].startBit, Com_SignalConfig[i].length);
            return E_OK;
        }
    }
    return E_NOT_OK;
}

/* [SRS_COM_030] ComTxMode=PERIODIC 인 I-PDU 주기 전송 */
void Com_MainFunctionTx(void)
{
    for (uint8 p = 0u; p < 1u; p++) {
        s_txPdu[p].timer++;
        if (s_txPdu[p].timer >= s_txPdu[p].periodTicks) {
            PduInfoType info;
            s_txPdu[p].timer = 0u;
            info.SduDataPtr = s_txPdu[p].data;
            info.SduLength  = s_txPdu[p].dlc;
            (void)PduR_ComTransmit(COM_TXPDU_VehicleState, &info);
        }
    }
}

/* [SRS_COM_040] */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    if ((RxPduId == COM_RXPDU_LightOverride) && (PduInfoPtr != NULL)) {
        uint8 n = (PduInfoPtr->SduLength < 8u) ? (uint8)PduInfoPtr->SduLength : 8u;
        for (uint8 i = 0u; i < n; i++) { s_rxPdu[0].data[i] = PduInfoPtr->SduDataPtr[i]; }
        s_rxPdu[0].received = 1u;
    }
}
