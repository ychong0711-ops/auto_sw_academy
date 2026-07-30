/*
 * uds_server.c - UDS 서비스 서버 (간소화 Dcm)
 * NRC 우선순위(일반 원칙): 길이(0x13) -> 세션(0x7F) -> 보안(0x33) -> 파라미터(0x31)
 * (ISO 14229-1 부록의 서비스별 상세 우선순위는 이보다 복잡. 문서 참고)
 */
#include "uds_server.h"
#include "Uds_Data.h"   /* generated: DID 상수 + 정적 데이터 (tools/gen_cfg.py) */

/* ------------------------------------------------------------------ */
/*  차량 정적 데이터 → generated/Uds_Data.c (단일 소스: config/ecu.json)   */
/* ------------------------------------------------------------------ */

static uint32 s_lcg = 0x12345678u;   /* 시드 생성용 PRNG (데모 전용!) */

/* 학습용 seed->key: 실제 ECU 는 이 함수를 절대 공개하지 않는다 */
uint16 UdsDemo_CalcKey(uint16 seed)
{
    uint16 x = (uint16)(seed ^ 0x5A3Cu);
    return (uint16)((x << 3) | (x >> 13));   /* 3비트 회전 */
}

/* ------------------------------------------------------------------ */
/*  헬퍼 함수들                                                             */
/* ------------------------------------------------------------------ */
static void send_neg(UdsServer* s, uint8 sid, uint8 nrc)
{
    uint8 r[3] = { 0x7Fu, sid, nrc };
    s->sendCb(s->sendCtx, r, 3u);
}

static uint16 prng_next(void)
{
    s_lcg = (s_lcg * 1664525u) + 1013904223u;
    return (uint16)(s_lcg >> 16u);
}

static uint8 session_bit(uint8 sesNum)
{
    if (sesNum == 1u) { return UDS_SES_DEFAULT; }
    if (sesNum == 2u) { return UDS_SES_PROGRAMMING; }
    return UDS_SES_EXTENDED;
}

/* ------------------------------------------------------------------ */
/*  서비스 핸들러 (DSP)                                                  */
/* ------------------------------------------------------------------ */
static uint8 svc10_session(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    uint8 sub = (uint8)(req[1] & 0x7Fu);
    if ((sub < 1u) || (sub > 3u)) { send_neg(s, 0x10u, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED); return 0u; }
    s->session = sub;
    s->s3Ticks = (uint32)UDS_S3_TICKS;
    s->unlocked = 0u;              /* 세션이 바뀌면 보안 상태는 리셋된다 (중요!) */
    s->secAttempts = 0u;
    resp[0] = 0x50u; resp[1] = sub;
    return 2u;
}

static uint8 svc11_reset(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    uint8 sub = (uint8)(req[1] & 0x7Fu);
    if (sub != 1u) { send_neg(s, 0x11u, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED); return 0u; }  /* hardReset*/
    s->resetPending = 1u;
    resp[0] = 0x51u; resp[1] = sub;
    return 2u;
}

static uint8 svc14_clear_dtc(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    if ((req[1] == 0xFFu) && (req[2] == 0xFFu) && (req[3] == 0xFFu)) {
        s->dtcCount = 0u;                              /* 모든 그룹 클리어 */
        resp[0] = 0x54u;
        return 1u;
    }
    send_neg(s, 0x14u, UDS_NRC_REQUEST_OUT_OF_RANGE);  /* 부분 그룹은 미지원(간소화) */
    return 0u;
}

static uint8 svc19_read_dtc(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    uint8 sub = (uint8)(req[1] & 0x7Fu);

    resp[0] = 0x59u; resp[1] = sub;
    if (sub == 0x01u) {                                /* reportNumberOfDTCByStatusMask */
        uint8 mask = req[2], cnt = 0u;
        for (uint8 i = 0u; i < s->dtcCount; i++) {
            if ((s->dtc[i].status & mask) != 0u) { cnt++; }
        }
        resp[2] = 0xFFu; resp[3] = 0x01u;              /* 가용마스크 + 형식(I SO14229) */
        resp[4] = 0u;    resp[5] = cnt;
        return 6u;
    }
    if ((sub == 0x02u) || (sub == 0x0Au)) {            /* ByStatusMask / SupportedDTC */
        uint8 mask = (sub == 0x02u) ? req[2] : 0xFFu;
        uint8 w = 2u;
        resp[w++] = 0xFFu;                             /* DTCStatusAvailabilityMask */
        for (uint8 i = 0u; i < s->dtcCount; i++) {
            if ((s->dtc[i].status & mask) != 0u) {
                resp[w++] = s->dtc[i].d0; resp[w++] = s->dtc[i].d1;
                resp[w++] = s->dtc[i].d2; resp[w++] = s->dtc[i].status;
            }
        }
        return w;
    }
    send_neg(s, 0x19u, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    return 0u;
}

/* DID 읽기 데이터 제공자: 성공 시 데이터 길이(>0), 실패 시 0 */
static uint8 did_read(const UdsServer* s, uint16 did, uint8* dst)
{
    uint8 i;
    switch (did) {
    case UDS_DID_VIN:          for (i = 0u; i < 17u; i++) { dst[i] = Uds_VINBytes[i]; } return 17u;
    case UDS_DID_ECUSERIAL:    for (i = 0u; i < 4u;  i++) { dst[i] = Uds_EcuSerialBytes[i]; } return 4u;
    case UDS_DID_ACTIVESESSION: dst[0] = s->session; return 1u;                  /* ActiveSession */
    case UDS_DID_VEHICLESPEED:  dst[0] = (uint8)(s->vehicleSpeedRaw >> 8u);      /* 차량 속도 */
                                dst[1] = (uint8)(s->vehicleSpeedRaw & 0xFFu);      return 2u;
    default: break;
    }
    return 0u;
}

static uint8 svc22_read_did(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    uint8 w = 1u;
    resp[0] = 0x62u;
    for (uint16 off = 1u; (off + 1u) < len; off += 2u) {
        uint16 did = (uint16)(((uint16)req[off] << 8u) | req[off + 1u]);
        uint8 n = did_read(s, did, &resp[w + 2u]);
        if (n == 0u) { send_neg(s, 0x22u, UDS_NRC_REQUEST_OUT_OF_RANGE); return 0u; }
        resp[w++] = req[off]; resp[w++] = req[off + 1u];
        w = (uint8)(w + n);
    }
    return w;
}

static uint8 svc27_security(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    uint8 sub = (uint8)(req[1] & 0x7Fu);

    if (s->delayTicks > 0u) {
        send_neg(s, 0x27u, UDS_NRC_TIME_DELAY_NOT_EXPIRED);
        return 0u;
    }
    if (sub == 0x01u) {                                /* requestSeed (level 1) */
        uint16 seed = (s->unlocked != 0u) ? 0u : prng_next();   /* ISO 규정: 잠금해제 상태면 seed=0 */
        s->seed = seed;
        resp[0] = 0x67u; resp[1] = sub;
        resp[2] = (uint8)(seed >> 8u); resp[3] = (uint8)(seed & 0xFFu);
        return 4u;
    }
    if (sub == 0x02u) {                                /* sendKey */
        if (len < 4u) { send_neg(s, 0x27u, UDS_NRC_INCORRECT_LENGTH); return 0u; }
        if (s->secAttempts >= 3u) {
            send_neg(s, 0x27u, UDS_NRC_EXCEEDED_ATTEMPTS);
            return 0u;
        }
        uint16 key = (uint16)(((uint16)req[2] << 8u) | req[3]);
        if (key == UdsDemo_CalcKey(s->seed)) {
            s->unlocked = 1u; s->secAttempts = 0u;
            resp[0] = 0x67u; resp[1] = sub;
            return 2u;
        }
        s->secAttempts++;
        if (s->secAttempts >= 3u) {
            s->delayTicks = (uint32)UDS_SEC_DELAY_TICKS;   /* 재시도 지연 시작 */
            send_neg(s, 0x27u, UDS_NRC_EXCEEDED_ATTEMPTS);
        } else {
            send_neg(s, 0x27u, UDS_NRC_INVALID_KEY);
        }
        return 0u;
    }
    send_neg(s, 0x27u, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    return 0u;
}

static uint8 svc2e_write_did(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    uint16 did = (uint16)(((uint16)req[1] << 8u) | req[2]);
    if (did != UDS_DID_VEHICLESPEED) { send_neg(s, 0x2Eu, UDS_NRC_REQUEST_OUT_OF_RANGE); return 0u; }
    s->vehicleSpeedRaw = (uint16)(((uint16)req[3] << 8u) | req[4]);
    resp[0] = 0x6Eu; resp[1] = req[1]; resp[2] = req[2];
    return 3u;
}

static uint8 svc31_routine(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    uint8  sub = (uint8)(req[1] & 0x7Fu);
    uint16 rid = (uint16)(((uint16)req[2] << 8u) | req[3]);

    if (rid != 0x0203u) { send_neg(s, 0x31u, UDS_NRC_REQUEST_OUT_OF_RANGE); return 0u; }
    if ((sub < 1u) || (sub > 3u)) { send_neg(s, 0x31u, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED); return 0u; }
    if ((sub == 0x01u) && (s->vehicleSpeedRaw != 0u)) {
        /* [SRS_UDS_031] 자기진단은 차량 정지 상태에서만 (조건 불만족 NRC) */
        send_neg(s, 0x31u, UDS_NRC_CONDITIONS_NOT_CORRECT);
        return 0u;
    }
    resp[0] = 0x71u; resp[1] = sub; resp[2] = req[2]; resp[3] = req[3]; resp[4] = 0x00u;
    return 5u;
}

static uint8 svc3e_tester_present(UdsServer* s, const uint8* req, uint16 len, uint8* resp)
{
    (void)len;
    if ((req[1] & 0x7Fu) != 0u) { send_neg(s, 0x3Eu, UDS_NRC_SUBFUNCTION_NOT_SUPPORTED); return 0u; }
    s->s3Ticks = (uint32)UDS_S3_TICKS;               /* 세션 유지 */
    resp[0] = 0x7Eu; resp[1] = 0x00u;
    return 2u;
}

/* ------------------------------------------------------------------ */
/*  서비스 테이블 (DSD)                                                 */
/* ------------------------------------------------------------------ */
typedef uint8 (*UdsSvcHandler)(UdsServer*, const uint8*, uint16, uint8*);

typedef struct {
    uint8  sid;
    uint8  sessions;      /* 비트마스크(UDS_SES_*) */
    uint8  needSecurity;
    uint16 minLen;
    uint8  hasSubFunc;    /* 긍정응답 억제 비트 유무 */
    UdsSvcHandler handler;
} UdsSvcEntry;

static const UdsSvcEntry SVC_TABLE[] = {
  /* sid   sessions                                   sec  minLen sub  handler */
  { 0x10u, UDS_SES_DEFAULT|UDS_SES_PROGRAMMING|UDS_SES_EXTENDED, 0u, 2u, 1u, svc10_session },
  { 0x11u, UDS_SES_DEFAULT|UDS_SES_PROGRAMMING|UDS_SES_EXTENDED, 0u, 2u, 1u, svc11_reset   },
  { 0x14u,                  UDS_SES_PROGRAMMING|UDS_SES_EXTENDED, 1u, 4u, 0u, svc14_clear_dtc },
  { 0x19u, UDS_SES_DEFAULT|UDS_SES_PROGRAMMING|UDS_SES_EXTENDED, 0u, 3u, 1u, svc19_read_dtc },
  { 0x22u, UDS_SES_DEFAULT|UDS_SES_PROGRAMMING|UDS_SES_EXTENDED, 0u, 3u, 0u, svc22_read_did },
  { 0x27u,                  UDS_SES_PROGRAMMING|UDS_SES_EXTENDED, 0u, 2u, 1u, svc27_security },
  { 0x2Eu,                                   UDS_SES_EXTENDED, 1u, 5u, 0u, svc2e_write_did },
  { 0x31u,                                   UDS_SES_EXTENDED, 1u, 4u, 1u, svc31_routine   },
  { 0x3Eu, UDS_SES_DEFAULT|UDS_SES_PROGRAMMING|UDS_SES_EXTENDED, 0u, 2u, 1u, svc3e_tester_present },
};

/* ------------------------------------------------------------------ */
/*  공개 API                                                            */
/* ------------------------------------------------------------------ */
void Uds_Init(UdsServer* s, Uds_SendRawCb sendCb, void* sendCtx)
{
    s->session = 1u; s->s3Ticks = 0u;
    s->unlocked = 0u; s->secAttempts = 0u; s->delayTicks = 0u; s->seed = 0u;
    s->resetPending = 0u; s->vehicleSpeedRaw = 0u;
    s->dtcCount = 0u; s->sendCb = sendCb; s->sendCtx = sendCtx;

    /* 예제 DTC 2건 적재 (Dem 이 하던 일): P0301(점화실화), U0100(통신끊김) */
    s->dtc[0].d0 = 0x03u; s->dtc[0].d1 = 0x01u; s->dtc[0].d2 = 0x00u; s->dtc[0].status = 0x2Cu;
    s->dtc[1].d0 = 0xC1u; s->dtc[1].d1 = 0x00u; s->dtc[1].d2 = 0x00u; s->dtc[1].status = 0x09u;
    s->dtcCount = 2u;
}

/* [SRS_UDS_010] */
void Uds_OnRequest(UdsServer* s, const uint8* req, uint16 len)
{
    uint8 resp[UDS_RESP_MAX];
    uint8 rlen;

    if ((req == NULL) || (len == 0u)) { return; }

    for (uint16 i = 0u; i < (uint16)(sizeof(SVC_TABLE)/sizeof(SVC_TABLE[0])); i++) {
        if (SVC_TABLE[i].sid == req[0]) {
            /* 1) 메시지 길이 */
            if (len < SVC_TABLE[i].minLen) { send_neg(s, req[0], UDS_NRC_INCORRECT_LENGTH); return; }
            /* 2) 세션 */
            if ((SVC_TABLE[i].sessions & session_bit(s->session)) == 0u) {
                send_neg(s, req[0], UDS_NRC_SESSION_NOT_SUPPORTED); return;
            }
            /* 3) 보안 */
            if ((SVC_TABLE[i].needSecurity != 0u) && (s->unlocked == 0u)) {
                send_neg(s, req[0], UDS_NRC_SECURITY_ACCESS_DENIED); return;
            }
            /* 4) 서비스별 상세 파라미터 */
            rlen = SVC_TABLE[i].handler(s, req, len, resp);
            /* 5) 긍정응답 억제 비트 */
            if ((rlen > 0u) && (SVC_TABLE[i].hasSubFunc != 0u) && ((req[1] & 0x80u) != 0u)) {
                rlen = 0u;
            }
            if (rlen > 0u) { s->sendCb(s->sendCtx, resp, rlen); }
            return;
        }
    }
    send_neg(s, req[0], UDS_NRC_SERVICE_NOT_SUPPORTED);
}

void Uds_Tick10ms(UdsServer* s)
{
    if (s->delayTicks > 0u) { s->delayTicks--; }
    if (s->session != 1u) {                 /* 비디폴트 세션만 S3 감시 */
        if (s->s3Ticks > 0u) { s->s3Ticks--; }
        if (s->s3Ticks == 0u) {
            s->session = 1u;                /* S3 타임아웃 -> 디폴트 세션 + 보안 잠금 */
            s->unlocked = 0u;
        }
    }
}
