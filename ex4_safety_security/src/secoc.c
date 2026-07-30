/*
 * secoc.c - SecOC 간소화 구현 (CMAC 기반 인증 + 신선도)
 */
#include "secoc.h"
#include "cmac.h"

static void put_be16(uint8* d, uint16 v) { d[0] = (uint8)(v >> 8u); d[1] = (uint8)(v & 0xFFu); }
static void put_be32(uint8* d, uint32 v)
{
    d[0] = (uint8)(v >> 24u); d[1] = (uint8)(v >> 16u);
    d[2] = (uint8)(v >> 8u);  d[3] = (uint8)(v & 0xFFu);
}
static void put_be24(uint8* d, uint32 v)
{
    d[0] = (uint8)((v >> 16u) & 0xFFu); d[1] = (uint8)((v >> 8u) & 0xFFu); d[2] = (uint8)(v & 0xFFu);
}
static uint32 get_be24(const uint8* d)
{
    return ((uint32)d[0] << 16u) | ((uint32)d[1] << 8u) | (uint32)d[2];
}

static void build_auth_input(uint16 pduId, const uint8* auth, uint8 authLen,
                             uint32 freshFull, uint8* buf /*2+authLen+4*/)
{
    put_be16(buf, pduId);
    for (uint8 i = 0u; i < authLen; i++) { buf[2u + i] = auth[i]; }
    put_be32(&buf[2u + authLen], freshFull);
}

void SecOC_InitTx(SecOC_TxCtx* ctx, uint16 pduId, uint8 authLen, const uint8 key[16])
{
    ctx->pduId = pduId; ctx->authLen = authLen; ctx->txCounter = 0u;
    for (uint8 i = 0u; i < 16u; i++) { ctx->key[i] = key[i]; }
}

void SecOC_InitRx(SecOC_RxCtx* ctx, uint16 pduId, uint8 authLen, const uint8 key[16])
{
    ctx->pduId = pduId; ctx->authLen = authLen;
    ctx->lastAcceptedFresh = 0u; ctx->initialized = 0u;
    ctx->verifyFailCount = 0u; ctx->replayFailCount = 0u;
    for (uint8 i = 0u; i < 16u; i++) { ctx->key[i] = key[i]; }
}

/* [SRS_SECOC_010] */
Std_ReturnType SecOC_Secure(SecOC_TxCtx* ctx, const uint8* auth, uint8* secured)
{
    uint8 input[16];
    uint8 mac[16];

    if ((ctx->authLen > 2u) || (ctx->authLen == 0u)) { return E_NOT_OK; }   /* 데모: 프레임 8B 한정 */

    ctx->txCounter++;                               /* 신선도: 단조 증가 카운터 */
    for (uint8 i = 0u; i < ctx->authLen; i++) { secured[i] = auth[i]; }
    put_be24(&secured[ctx->authLen], ctx->txCounter & 0xFFFFFFu);

    build_auth_input(ctx->pduId, auth, ctx->authLen, ctx->txCounter, input);
    if (Cmac_Calculate(ctx->key, input, (uint32)(2u + ctx->authLen + 4u), mac) != E_OK) {
        return E_NOT_OK;
    }
    for (uint8 i = 0u; i < SECOC_MAC_TX_LEN; i++) {  /* MAC 상위 3바이트만 전송(절단) */
        secured[ctx->authLen + SECOC_FRESH_TX_LEN + i] = mac[i];
    }
    return E_OK;
}

/* [SRS_SECOC_020] */
Std_ReturnType SecOC_Verify(SecOC_RxCtx* ctx, const uint8* secured, uint8* auth)
{
    uint8 input[16];
    uint8 mac[16];
    uint32 freshLow = get_be24(&secured[ctx->authLen]);
    uint32 candidate;
    uint8 macOk = 1u;

    /* 신선도 복원: 상위 바이트는 로컬 최신값 기준, 뒤처진 절단값이면 다음 랩 */
    if (ctx->initialized == 0u) {
        candidate = freshLow;
    } else {
        candidate = (ctx->lastAcceptedFresh & 0xFF000000u) | freshLow;
        if (candidate <= ctx->lastAcceptedFresh) { candidate += 0x01000000u; }
    }

    build_auth_input(ctx->pduId, secured, ctx->authLen, candidate, input);
    (void)Cmac_Calculate(ctx->key, input, (uint32)(2u + ctx->authLen + 4u), mac);

    for (uint8 i = 0u; i < SECOC_MAC_TX_LEN; i++) {
        if (secured[ctx->authLen + SECOC_FRESH_TX_LEN + i] != mac[i]) { macOk = 0u; }
    }

    if (macOk == 0u) {
        /* 같은 절단 신선도 재사용(재전송 공격)과 그냥 위조를 구분해 집계 */
        if ((ctx->initialized != 0u) && (freshLow == (ctx->lastAcceptedFresh & 0xFFFFFFu))) {
            ctx->replayFailCount++;
        } else {
            ctx->verifyFailCount++;
        }
        return E_NOT_OK;
    }

    ctx->lastAcceptedFresh = candidate;
    ctx->initialized = 1u;
    for (uint8 i = 0u; i < ctx->authLen; i++) { auth[i] = secured[i]; }
    return E_OK;
}
