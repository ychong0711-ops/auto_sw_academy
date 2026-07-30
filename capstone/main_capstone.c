/*
 * [CAPSTONE 통합 데모] 역량 체인 종합 시연
 * 한 대의 가상 ECU(+진단기) 안에서 4단계 체인이 전부 동작한다:
 *
 *   [C/임베디드]  비트/레지스터 조작으로 신호 생성
 *   [AUTOSAR 적층] (EX2 의 라이트 체인)  …여기선 통신구간을 통합 시연
 *   [통신/진단]    가상 CAN 버스 + ISO-TP + UDS 진단 세션
 *   [안전/보안/ASPICE] E2E P01 (안전), SecOC (보안), WdgM (감시), 추적성 로그
 *
 * 시나리오:
 *   PHASE 1: ECU 가 주기 프레임 송신 (보호 2종: E2E 프레임 + SecOC 프레임)
 *   PHASE 2: 진단기가 프레임 인증/검증 + 오류주입(위조/재전송) 시연
 *   PHASE 3: UDS 진단 워크플로 (세션->보안->쓰기/읽기->DTC->리셋)
 *   PHASE 4: Watchdog 이 통신 스택 행업을 검출
 */
#include <stdio.h>
#include <string.h>

#include "vcan.h"
#include "can_signal.h"
#include "isotp.h"
#include "uds_server.h"
#include "e2e_p01.h"
#include "secoc.h"
#include "wdg.h"

/* ---------------- 주소/식별자 ---------------- */
#define CANID_T2E        0x7E0u   /* 진단 요청  */
#define CANID_E2T        0x7E8u   /* 진단 응답  */
#define CANID_VEHSTATE   0x100u   /* 애플리케이션 프레임 — generated/VehicleNetwork.dbc 와 동일 레이아웃 */
#define CANID_VSTATE_SEC 0x180u   /* SecOC 보호 프레임 (8B) — 실무에선 동일 ID 위 MAC 편승,
                                     여기선 트레이스 가독성을 위해 별도 ID 로 구분한다 */
#define CANID_SAFETY     0x400u   /* E2E 보호 프레임 (6B)  */

/* DBC 신호 위치 (generated/VehicleNetwork.dbc, gen_cfg.py 가 검증한 값) */
#define SIG_BATT_START   0u
#define SIG_BATT_LEN     12u
#define SIG_LTSW_START   12u
#define SIG_HLCMD_START  13u

#define SE_COMM_STACK    10u      /* WdgM 감시 대상: 통신 러너블 */
#define SE_DIAG          11u      /* WdgM 감시 대상: 진단 처리  */

static IsoTp      s_ecu, s_tester;
static int        s_nodeEcu, s_nodeTester;
static UdsServer  s_uds;
static SecOC_TxCtx s_secTx; static SecOC_RxCtx s_secRx;
static E2E_P01Config s_e2eCfg; static E2E_P01State s_e2eTx, s_e2eRx;
static uint32     g_ms;

/* 진단기 수신 버퍼 */
static uint8  s_diagResp[UDS_RESP_MAX]; static uint16 s_diagRespLen;

/* ---------------- 출력 유틸 ---------------- */
static void hexprint(const char* tag, uint16 id, const uint8* d, uint16 n)
{
    printf("  %-22s [0x%03X] ", tag, id);
    for (uint16 i = 0u; i < n; i++) { printf("%02X ", d[i]); }
    printf("\n");
}

/* ---------------- ECU 측: UDS 송신 연결 ---------------- */
static void ecu_send_diag(void* ctx, const uint8* resp, uint16 len)
{
    (void)ctx;
    hexprint("ECU->진단기 응답", CANID_E2T, resp, len);
    WdgM_Checkpoint(SE_DIAG);
    (void)IsoTp_Send(&s_ecu, resp, len);
}

/* ---------------- vcan 콜백 ---------------- */
static void ecu_can_rx(int node, uint16 id, uint8 dlc, const uint8* data)
{
    (void)node;
    IsoTp_OnCanFrame(&s_ecu, id, dlc, data);     /* ISO-TP 로 위임 */
}
static void tester_can_rx(int node, uint16 id, uint8 dlc, const uint8* data)
{
    (void)node;
    IsoTp_OnCanFrame(&s_tester, id, dlc, data);
}

/* ISO-TP 완성 메시지 -> UDS */
static void ecu_msg_rx(void* ctx, const uint8* d, uint16 n)
{
    (void)ctx;
    hexprint("진단기->ECU 요청", CANID_T2E, d, n);
    Uds_OnRequest(&s_uds, d, n);
}
static void tester_msg_rx(void* ctx, const uint8* d, uint16 n)
{
    (void)ctx;
    for (uint16 i = 0u; i < n; i++) { s_diagResp[i] = d[i]; }
    s_diagRespLen = n;
}

/* ---------------- 시간 진행 (버스 펌프 + ISO-TP + WdgM 메인) ---------------- */
static void pump_ms(uint32 n, uint8 feedWdg)
{
    for (uint32 i = 0u; i < n; i++) {
        g_ms++;
        VCan_TraceSetNow(g_ms);
        VCan_Pump();
        IsoTp_MainFunction(&s_ecu);
        IsoTp_MainFunction(&s_tester);
        if (feedWdg != 0u) { WdgM_Checkpoint(SE_COMM_STACK); }
        WdgM_MainFunction(1u);
    }
}

/* 진단기 요청 1건 (간이 클라이언트) */
static void tester_request(const char* desc, const uint8* req, uint16 len)
{
    s_diagRespLen = 0u;
    printf("-> %s\n", desc);
    (void)IsoTp_Send(&s_tester, req, len);
    for (int t = 0; (t < 200) && (s_diagRespLen == 0u); t++) { pump_ms(1u, 1u); }
    printf("\n");
}

/* ---------------- PHASE 유틸 ---------------- */
static void log_phase(const char* s) { printf("\n========== %s ==========\n", s); }

int main(int argc, char** argv)
{
    const uint8 key[16] = {0x10,0x22,0x34,0x46,0x58,0x6A,0x7C,0x8E,
                           0xA0,0xB2,0xC4,0xD6,0xE8,0xFA,0x0C,0x1E};
    const char* tracePath = 0;

    /* 사용법: capstone_demo [--trace <파일>]  — 버스 트레이스를 .dbc 로 나중에 해석 가능 */
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--trace") == 0) { tracePath = argv[i + 1]; }
    }

    printf("============================================================\n");
    printf("  CAPSTONE: AUTOSAR 체인 통합 데모 (가상 ECU + 진단기)\n");
    printf("============================================================\n");
    if ((tracePath != 0) && (VCan_TraceOpen(tracePath) == E_OK)) {
        printf("[트레이스] 버스 모니터 기록 중 -> %s\n", tracePath);
    } else if (tracePath != 0) {
        printf("[트레이스] 파일을 열 수 없음: %s (계속 진행)\n", tracePath);
        tracePath = 0;
    }

    /* ---- 부팅 ---- */
    s_nodeEcu    = VCan_Attach(ecu_can_rx);
    s_nodeTester = VCan_Attach(tester_can_rx);
    IsoTp_Init(&s_ecu,    CANID_T2E, CANID_E2T, s_nodeEcu);
    IsoTp_Init(&s_tester, CANID_E2T, CANID_T2E, s_nodeTester);
    IsoTp_SetRxCb(&s_ecu, ecu_msg_rx, 0);
    IsoTp_SetRxCb(&s_tester, tester_msg_rx, 0);
    Uds_Init(&s_uds, ecu_send_diag, 0);
    SecOC_InitTx(&s_secTx, CANID_VSTATE_SEC, 2u, key);
    SecOC_InitRx(&s_secRx, CANID_VSTATE_SEC, 2u, key);
    s_e2eCfg.dataId = 0x0123u; s_e2eCfg.maxDeltaCounter = 2u;
    s_e2eTx.txCounter = 0u; s_e2eRx.rxWaitForFirst = 1u; s_e2eRx.rxLastCounter = 0u;
    WdgM_Init();
    (void)WdgM_RegisterEntity(SE_COMM_STACK, 50u);
    (void)WdgM_RegisterEntity(SE_DIAG, 50u);
    WdgM_Checkpoint(SE_COMM_STACK); WdgM_Checkpoint(SE_DIAG);
    printf("[부팅] ECU 초기화 완료: CanIf/PduR/IsoTp, Dcm(UDS), SecOC, E2E, WdgM\n");

    /* ================= PHASE 1 ================= */
    log_phase("PHASE 1: 주기 프레임 송신 (DBC 레이아웃 + SecOC + E2E)");
    for (uint8 i = 0u; i < 3u; i++) {
        /* (a) 애플리케이션 프레임: VehicleNetwork.dbc 레이아웃대로 신호 패킹 (Com 역할)
         *     BatteryVoltage=12.8V(raw 128), LightSwitch=1, HeadlightCmd=i&1 */
        uint8 app[8] = {0};
        (void)CanSig_InsertRaw(app, 8u, SIG_BATT_START, SIG_BATT_LEN, CAN_SIG_INTEL, 128u);
        (void)CanSig_InsertRaw(app, 8u, SIG_LTSW_START, 1u, CAN_SIG_INTEL, 1u);
        (void)CanSig_InsertRaw(app, 8u, SIG_HLCMD_START, 1u, CAN_SIG_INTEL, (uint32)(i & 1u));
        printf("[ECU] VehicleState 송신 (DBC 신호 3종 패킹)\n");
        hexprint("버스 프레임", CANID_VEHSTATE, app, 8u);
        (void)VCan_Send(s_nodeEcu, CANID_VEHSTATE, 8u, app);

        /* (b) SecOC 프레임: 차량 상태 2B 인증 페이로드 (무결성/신선도 보호) */
        uint8 auth[2] = { 0x01u, (uint8)(0xC0u + i) };   /* e.g., 속도 raw */
        uint8 secured[8];
        (void)SecOC_Secure(&s_secTx, auth, secured);
        printf("[ECU] SecOC 송신 (인증 2B + 신선도 3B + MAC 3B)\n");
        hexprint("버스 프레임", CANID_VSTATE_SEC, secured, 8u);
        (void)VCan_Send(s_nodeEcu, CANID_VSTATE_SEC, 8u, secured);

        /* (c) E2E 프레임: 스티어링 각도 raw 4B + P01 제어 2B */
        uint8 e2eFrm[6] = {0,0, 0x12u,0x34u, 0x00u,(uint8)(0x10u + i)};
        E2E_P01Protect(&s_e2eCfg, &s_e2eTx, e2eFrm, 6u);
        (void)VCan_Send(s_nodeEcu, CANID_SAFETY, 6u, e2eFrm);

        pump_ms(2u, 1u);
    }

    /* ================= PHASE 2 ================= */
    log_phase("PHASE 2: 진단기가 프레임 검증 + 오류 주입");
    {
        uint8 secured[8], out[2];
        uint8 auth[2] = { 0x01u, 0xC3u };
        (void)SecOC_Secure(&s_secTx, auth, secured);
        if (SecOC_Verify(&s_secRx, secured, out) == E_OK) {
            printf("  [진단기] SecOC 인증 성공: 페이로드 %02X %02X (신선도 %u 수락)\n",
                   out[0], out[1], (unsigned)s_secRx.lastAcceptedFresh);
        }
        printf("  [진단기] 같은 프레임 재전송 공격 시도...\n");
        if (SecOC_Verify(&s_secRx, secured, out) != E_OK) {
            printf("  [진단기] -> 거부됨 (replay 누적=%u)  ✔ 재전송 방어\n",
                   (unsigned)s_secRx.replayFailCount);
        }
        /* 새 신선도의 정상 프레임을 만들고 페이로드만 변조(=MAC 이 안 맞는 위조) */
        uint8 forged[8];
        uint8 auth2[2] = { 0x01u, 0xC4u };
        (void)SecOC_Secure(&s_secTx, auth2, forged);
        forged[0] ^= 0x80u;
        printf("  [진단기] 페이로드 위조 프레임 주입...\n");
        if (SecOC_Verify(&s_secRx, forged, out) != E_OK) {
            printf("  [진단기] -> 거부됨 (위조 검증 실패 누적=%u)  ✔ 무결성 검증\n",
                   (unsigned)s_secRx.verifyFailCount);
        }
        /* E2E 정상 1건 + 오염 1건 */
        uint8 e2eFrm[6] = {0,0, 0x12u,0x34u, 0x00u,0x13u};
        E2E_P01Protect(&s_e2eCfg, &s_e2eTx, e2eFrm, 6u);
        printf("  [진단기] E2E 체크: %s\n",
               E2E_P01Check(&s_e2eCfg, &s_e2eRx, e2eFrm, 6u) == E2E_P01_INITIAL ? "INITIAL/OK ✔" : "이상 ✘");
        e2eFrm[4] ^= 0x01u;
        printf("  [진단기] E2E 오염 프레임 체크: %s ✔ (안전 매커니즘 검출)\n",
               E2E_P01Check(&s_e2eCfg, &s_e2eRx, e2eFrm, 6u) == E2E_P01_ERROR ? "ERROR" : "감지 실패");
    }

    /* ================= PHASE 3 ================= */
    log_phase("PHASE 3: UDS 진단 워크플로 (ISO-TP 위로)");
    {
        uint8 r_vin[3]    = {0x22u, 0xF1u, 0x90u};
        uint8 r_ext[2]    = {0x10u, 0x03u};
        uint8 r_seed[2]   = {0x27u, 0x01u};
        uint8 r_key[4]    = {0x27u, 0x02u, 0u, 0u};
        uint8 r_write[5]  = {0x2Eu, 0x12u, 0x34u, 0x00u, 0x00u};
        uint8 r_read[3]   = {0x22u, 0x12u, 0x34u};
        uint8 r_dtc[3]    = {0x19u, 0x02u, 0xFFu};
        uint8 r_clear[4]  = {0x14u, 0xFFu, 0xFFu, 0xFFu};
        uint8 r_reset[2]  = {0x11u, 0x01u};

        tester_request("1) VIN 읽기 (default session)", r_vin, 3u);
        if (s_diagRespLen == 20u) {
            printf("   VIN = %.17s\n\n", &s_diagResp[3]);
        }
        tester_request("2) 확장 세션 진입", r_ext, 2u);
        tester_request("3) SecurityAccess: requestSeed", r_seed, 2u);

        if (s_diagRespLen == 4u && s_diagResp[0] == 0x67u) {
            uint16 seed = (uint16)(((uint16)s_diagResp[2] << 8u) | s_diagResp[3]);
            uint16 key16 = UdsDemo_CalcKey(seed);
            r_key[2] = (uint8)(key16 >> 8u); r_key[3] = (uint8)(key16 & 0xFFu);
            printf("   [진단기] seed=0x%04X -> key=0x%04X 계산 (알고리즘 비밀 보유 가정)\n\n",
                   seed, key16);
        }
        tester_request("4) SecurityAccess: sendKey", r_key, 4u);

        r_write[3] = 0x02u; r_write[4] = 0x58u;      /* 차량 속도 raw = 600 쓰기 */
        tester_request("5) WriteDID 0x1234 = 600 (ECU 매개변수 변경)", r_write, 5u);
        tester_request("6) Read DID 0x1234 (변경 확인)", r_read, 3u);
        if (s_diagRespLen == 5u) {
            printf("   현재 값 = %u\n\n", ((unsigned)s_diagResp[3] << 8u) | s_diagResp[4]);
        }
        tester_request("7) DTC 판독 (reportDTCByStatusMask)", r_dtc, 3u);
        tester_request("8) DTC 전체 삭제", r_clear, 4u);
        tester_request("9) ECU hard reset", r_reset, 2u);
        printf("   resetPending=%u (실무: BswM 이 리셋 수행)\n", s_uds.resetPending);
    }

    /* ================= PHASE 4 ================= */
    log_phase("PHASE 4: Watchdog 이 통신 스택 행업 검출");
    printf("  [상황] 통신 러너블이 60ms 동안 생존 보고를 멈춤(행업)  \n");
    pump_ms(60u, 0u);   /* 50ms 마감 초과 */
    printf("  [WdgM] 글로벌 상태 = %s (위반 누적=%u)\n",
           (WdgM_GetGlobalStatus() == WDGM_EXPIRED) ? "EXPIRED -> MCU 리셋 트리거" : "OK",
           (unsigned)WdgM_GetViolationCount());

    printf("\n[종료] 전 체인(임베디드 C → AUTOSAR → 통신/진단 → 안전/보안/프로세스) 시연 완료\n");
    printf("       상세 학습 경로: README.md, docs/*.md, ASPICE 산출물: ex4/aspice/\n");
    if (tracePath != 0) {
        VCan_TraceClose();
        printf("       트레이스 해석: python3 tools/decode_trace.py generated/VehicleNetwork.dbc %s\n",
               tracePath);
    }
    return 0;
}
