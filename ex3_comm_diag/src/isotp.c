/*
 * isotp.c - ISO-TP 송수신 상태기계
 *
 *      송신측                        수신측
 *        | ----- FF(길이,데이터6B) ---> |
 *        | <---- FC(CTS,BS,STmin) ---- |
 *        | ----- CF(SN=1, 7B) -------> |
 *        | ...STmin 간격으로 CF ...   |
 *        | ----- CF(SN=N, 나머지) ---> | -> 완성되면 상위(UDS)로 전달
 */
#include "isotp.h"
#include "vcan.h"

static void bus_send(IsoTp* l, const uint8* frame, uint8 len)
{
    (void)VCan_Send(l->node, l->txId, len, frame);
}

static void send_fc(IsoTp* l, uint8 fs)
{
    uint8 fc[3] = { (uint8)(0x30u | (fs & 0x0Fu)), 0x00u /*BS=0 제한없음*/, l->rxStminMs };
    bus_send(l, fc, 3u);
}

static uint8 parse_stmin_ms(uint8 raw)
{
    if (raw <= 0x7Fu) { return raw ? raw : 1u; }      /* 0..127 ms (0도 1틱으로) */
    return 1u;                                         /* 0xF1..0xF9 = 100..900us -> 1ms 올림 */
}

static void tx_abort(IsoTp* l)
{
    l->txState = ISOTP_TX_IDLE;
    l->errTxAbort++;
    if (l->txDoneCb != 0) { l->txDoneCb(l->txCtx, 0u); }
}

static void send_next_cf(IsoTp* l)
{
    uint8 frame[8];
    uint16 remain = (uint16)(l->txLen - l->txOff);
    uint8 n = (remain > 7u) ? 7u : (uint8)remain;

    frame[0] = (uint8)(0x20u | (l->txSn & 0x0Fu));
    for (uint8 i = 0u; i < n; i++) { frame[1u + i] = l->txBuf[l->txOff + i]; }
    l->txOff = (uint16)(l->txOff + n);
    l->txSn = (uint8)((l->txSn + 1u) & 0x0Fu);
    bus_send(l, frame, (uint8)(n + 1u));
    l->txStminLeft = l->txStminMs;

    if (l->txOff >= l->txLen) {
        l->txState = ISOTP_TX_IDLE;
        if (l->txDoneCb != 0) { l->txDoneCb(l->txCtx, 1u); }
    }
}

void IsoTp_Init(IsoTp* l, uint16 rxId, uint16 txId, int node)
{
    l->rxId = rxId; l->txId = txId; l->node = node; l->rxStminMs = 1u;
    l->txState = ISOTP_TX_IDLE; l->rxState = ISOTP_RX_IDLE;
    l->txLen = l->txOff = 0u; l->txSn = 0u;
    l->txStminMs = 1u; l->txStminLeft = 0u; l->wftCnt = 0u; l->txNbsLeft = 0u;
    l->rxTotal = l->rxOff = 0u; l->rxSn = 0u; l->rxNcrLeft = 0u;
    l->rxCb = 0; l->rxCtx = 0; l->txDoneCb = 0; l->txCtx = 0;
    l->errRxSn = l->errRxTimeout = l->errRxOverflow = l->errTxAbort = 0u;
}

void IsoTp_SetRxCb(IsoTp* l, IsoTp_RxMsgCb cb, void* ctx)     { l->rxCb = cb; l->rxCtx = ctx; }
void IsoTp_SetTxDoneCb(IsoTp* l, IsoTp_TxDoneCb cb, void* ctx){ l->txDoneCb = cb; l->txCtx = ctx; }

/* [SRS_TP_010] */
uint8 IsoTp_Send(IsoTp* l, const uint8* data, uint16 len)
{
    if ((l->txState != ISOTP_TX_IDLE) || (data == NULL) ||
        (len == 0u) || (len > ISOTP_MAX_BUF)) { return E_NOT_OK; }

    for (uint16 i = 0u; i < len; i++) { l->txBuf[i] = data[i]; }
    l->txLen = len; l->txOff = 0u;

    if (len <= 7u) {                               /* === Single Frame === */
        uint8 frame[8];
        frame[0] = (uint8)len;
        for (uint8 i = 0u; i < (uint8)len; i++) { frame[1u + i] = data[i]; }
        bus_send(l, frame, (uint8)(len + 1u));
        l->txState = ISOTP_TX_IDLE;
        if (l->txDoneCb != 0) { l->txDoneCb(l->txCtx, 1u); }
    } else {                                       /* === First Frame === */
        uint8 frame[8];
        frame[0] = (uint8)(0x10u | ((len >> 8u) & 0x0Fu));
        frame[1] = (uint8)(len & 0xFFu);
        for (uint8 i = 0u; i < 6u; i++) { frame[2u + i] = data[i]; }
        bus_send(l, frame, 8u);
        l->txOff = 6u; l->txSn = 1u;
        l->wftCnt = 0u;
        l->txNbsLeft = (uint16)ISOTP_N_BS_MS;
        l->txState = ISOTP_TX_WAIT_FC;
    }
    return E_OK;
}

void IsoTp_OnCanFrame(IsoTp* l, uint16 id, uint8 dlc, const uint8* data)
{
    if ((id != l->rxId) || (data == 0) || (dlc == 0u)) { return; }
    uint8 pci = (uint8)(data[0] >> 4u);
    uint16 i;

    if (pci == 0x0u) {                               /* ------- SF ------- */
        uint8 n = (uint8)(data[0] & 0x0Fu);
        if ((n > 0u) && (n <= 7u) && (dlc >= (uint8)(n + 1u))) {
            if (l->rxCb != 0) { l->rxCb(l->rxCtx, &data[1], n); }
        }
    } else if (pci == 0x1u) {                        /* ------- FF ------- */
        if (dlc < 2u) { return; }
        uint16 total = (uint16)(((uint16)(data[0] & 0x0Fu) << 8u) | data[1]);
        if ((total <= 7u) || (total > ISOTP_MAX_BUF)) {
            l->errRxOverflow++;
            send_fc(l, 0x2u);                        /* OVFL: 못 받겠다 */
            return;
        }
        l->rxTotal = total; l->rxOff = 6u; l->rxSn = 1u;
        for (i = 0u; i < 6u; i++) { l->rxBuf[i] = data[2u + i]; }
        send_fc(l, 0x0u);                            /* CTS */
        l->rxNcrLeft = (uint16)ISOTP_N_CR_MS;
        l->rxState = ISOTP_RX_WAIT_CF;
    } else if (pci == 0x2u) {                        /* ------- CF ------- */
        if (l->rxState != ISOTP_RX_WAIT_CF) { return; }
        uint8 sn = (uint8)(data[0] & 0x0Fu);
        if (sn != l->rxSn) {                         /* 시퀀스 불일치 -> abort */
            l->errRxSn++;
            l->rxState = ISOTP_RX_IDLE;
            return;
        }
        {
            uint16 remain = (uint16)(l->rxTotal - l->rxOff);
            uint8 n = (remain > 7u) ? 7u : (uint8)remain;
            for (i = 0u; i < n && (1u + i) < dlc; i++) { l->rxBuf[l->rxOff + i] = data[1u + i]; }
            l->rxOff = (uint16)(l->rxOff + n);
        }
        l->rxSn = (uint8)((l->rxSn + 1u) & 0x0Fu);
        l->rxNcrLeft = (uint16)ISOTP_N_CR_MS;
        if (l->rxOff >= l->rxTotal) {
            l->rxState = ISOTP_RX_IDLE;
            if (l->rxCb != 0) { l->rxCb(l->rxCtx, l->rxBuf, l->rxTotal); }
        }
    } else if (pci == 0x3u) {                        /* ------- FC ------- */
        if (l->txState != ISOTP_TX_WAIT_FC) { return; }
        uint8 fs = (uint8)(data[0] & 0x0Fu);
        if (fs == 0x0u) {                            /* CTS */
            l->txStminMs = parse_stmin_ms((dlc > 2u) ? data[2] : 0u);
            l->txStminLeft = 0u;                     /* 첫 CF 는 즉시 */
            l->txState = ISOTP_TX_SEND_CF;
        } else if (fs == 0x1u) {                     /* Wait */
            l->wftCnt++;
            l->txNbsLeft = (uint16)ISOTP_N_BS_MS;
            if (l->wftCnt > ISOTP_WFT_MAX) { tx_abort(l); }
        } else {                                     /* OVFL/기타 -> abort */
            tx_abort(l);
        }
    } else { /* 예약된 PCI 타입: 무시 */ }
}

void IsoTp_MainFunction(IsoTp* l)
{
    /* 송신: FC 대기 타임아웃(N_Bs) */
    if (l->txState == ISOTP_TX_WAIT_FC) {
        if (l->txNbsLeft > 0u) { l->txNbsLeft--; }
        if (l->txNbsLeft == 0u) { tx_abort(l); }
    }
    /* 송신: STmin 간격으로 CF 발사 */
    else if (l->txState == ISOTP_TX_SEND_CF) {
        if (l->txStminLeft > 0u) { l->txStminLeft--; }
        if (l->txStminLeft == 0u) { send_next_cf(l); }
    }
    /* 수신: CF 대기 타임아웃(N_Cr) */
    if (l->rxState == ISOTP_RX_WAIT_CF) {
        if (l->rxNcrLeft > 0u) { l->rxNcrLeft--; }
        if (l->rxNcrLeft == 0u) {
            l->rxState = ISOTP_RX_IDLE;
            l->errRxTimeout++;
        }
    }
}
