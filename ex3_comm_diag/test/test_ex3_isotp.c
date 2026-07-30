/*
 * [EX3-2] ISO-TP (ISO 15765-2) 상태기계 테스트
 * 구성: ECU 링크(0x7E0 수신/0x7E8 송신) <-vcan-> 테스터
 *   - 정상 경로: 테스터도 IsoTp 링크
 *   - 위반 경로: 테스터는 수동 프레임 조작 노드
 * 요구사항 추적: [SRS_TP_010] 송신, [SRS_TP_020] 다중 프레임 수신,
 *               [SRS_TP_030] 시퀀스 불일치 abort, [SRS_TP_040] STmin/STmin pacing
 */
#include "mini_test.h"
#include "vcan.h"
#include "isotp.h"

#define ID_T2E 0x7E0u    /* 테스터->ECU */
#define ID_E2T 0x7E8u    /* ECU->테스터 */

static IsoTp  s_ecu;            /* 우리가 검증하는 대상 */
static IsoTp  s_tester;         /* 정상 경로용 피어 */
static int    s_nodeEcu, s_nodeTester;
static int    s_testerIsIsoTp = 0;
static uint32 g_cycle;

/* 수신 캡처 */
static uint8  s_rxBuf[256]; static uint16 s_rxLen; static uint32 s_rxCnt;
static void ecu_rx(void* ctx, const uint8* d, uint16 n)
{
    (void)ctx; for (uint16 i = 0u; i < n; i++) { s_rxBuf[i] = d[i]; }
    s_rxLen = n; s_rxCnt++;
}
static uint8 s_txDoneVal; static uint32 s_txDoneCnt;
static void ecu_txdone(void* ctx, uint8 ok) { (void)ctx; s_txDoneVal = ok; s_txDoneCnt++; }

/* 테스터 수동 캡처 (ECU 가 볼내는 프레임 관찰: CF 시점 기록용) */
static uint8  s_lastEcuFrame[8]; static uint8 s_lastEcuDlc;
static uint32 s_cfCycle[8];      static uint8 s_cfCnt;
static void tester_capture(int node, uint16 id, uint8 dlc, const uint8* data)
{
    (void)node; (void)id;
    s_lastEcuDlc = dlc;
    for (uint8 i = 0u; i < dlc; i++) { s_lastEcuFrame[i] = data[i]; }
    if ((data[0] >> 4u) == 0x2u && s_cfCnt < 8u) { s_cfCycle[s_cfCnt++] = g_cycle; }
}

static void ecu_cb(int node, uint16 id, uint8 dlc, const uint8* data)
{ (void)node; IsoTp_OnCanFrame(&s_ecu, id, dlc, data); }
static void tester_cb_isotp(int node, uint16 id, uint8 dlc, const uint8* data)
{ (void)node; IsoTp_OnCanFrame(&s_tester, id, dlc, data); }

static void step(void)
{
    g_cycle++;
    VCan_Pump();
    IsoTp_MainFunction(&s_ecu);
    if (s_testerIsIsoTp) { IsoTp_MainFunction(&s_tester); }
}
static void run(int n) { for (int i = 0; i < n; i++) { step(); } }

static void setup(void)
{
    s_nodeEcu    = VCan_Attach(ecu_cb);
    s_nodeTester = VCan_Attach(tester_capture);
    IsoTp_Init(&s_ecu, ID_T2E, ID_E2T, s_nodeEcu);
    IsoTp_SetRxCb(&s_ecu, ecu_rx, 0);
    IsoTp_SetTxDoneCb(&s_ecu, ecu_txdone, 0);
    IsoTp_Init(&s_tester, ID_E2T, ID_T2E, s_nodeTester);
    s_rxLen = 0u; s_rxCnt = 0u; s_txDoneCnt = 0u; s_txDoneVal = 0xFFu;
    s_cfCnt = 0u; g_cycle = 0u; s_testerIsIsoTp = 0;
}

int main(void)
{
    setup();

    MT_SECTION("TC_TP_001: 단일 프레임(SF)");
    VCan_SetCb(s_nodeTester, tester_cb_isotp);      /* 상대를 정상 IsoTp 로 */
    s_testerIsIsoTp = 1;
    IsoTp_SetRxCb(&s_tester, ecu_rx, 0);            /* 테스터 수신도 같은 캡처 */
    {
        const uint8 msg[5] = { 'H', 'E', 'L', 'L', 'O' };
        MT_CHECK(IsoTp_Send(&s_ecu, msg, 5u) == E_OK, "SF 송신 수락");
        run(5);
        MT_CHECK(s_rxCnt == 1u && s_rxLen == 5u && s_rxBuf[0] == 'H', "테스터가 5B 수신");
    }

    MT_SECTION("TC_TP_002: 길이 7 경계(최대 SF)");
    {
        const uint8 msg[7] = { 1, 2, 3, 4, 5, 6, 7 };
        s_rxCnt = 0u;
        (void)IsoTp_Send(&s_ecu, msg, 7u);
        run(5);
        MT_CHECK(s_rxCnt == 1u && s_rxLen == 7u && s_rxBuf[6] == 7u, "7B 도 SF 로 전달");
    }

    MT_SECTION("TC_TP_003: 다중 프레임(30B) — FF+FC+CF");
    {
        uint8 msg[30]; for (uint8 i = 0u; i < 30u; i++) { msg[i] = i; }
        s_rxCnt = 0u; s_txDoneCnt = 0u;
        IsoTp_SetTxDoneCb(&s_tester, ecu_txdone, 0);
        (void)IsoTp_Send(&s_tester, msg, 30u);       /* 테스터->ECU 방향 */
        run(40);
        int ok = (s_rxCnt == 1u) && (s_rxLen == 30u);
        for (uint8 i = 0u; ok && i < 30u; i++) { if (s_rxBuf[i] != i) { ok = 0; } }
        MT_CHECK(ok, "ECU 가 30B 정확히 재조립");
        MT_CHECK(s_txDoneCnt == 1u && s_txDoneVal == 1u, "송신 완료 콜백(ok)");
    }

    MT_SECTION("TC_TP_030: CF 시퀀스번호 불일치 -> abort 후 복구");
    VCan_SetCb(s_nodeTester, tester_capture);       /* 상대를 수동 조작기로 */
    s_testerIsIsoTp = 0;
    s_rxCnt = 0u;
    {
        uint8 ff[8] = { 0x10u, 20u, 1, 2, 3, 4, 5, 6 };  /* 길이 20 선언 */
        uint8 cf_bad[8] = { 0x23u, 9, 9, 9, 9, 9, 9, 9 };/* SN=3 (기대값 1) */
        (void)VCan_Send(s_nodeTester, ID_T2E, 8u, ff);
        run(3);
        MT_CHECK(s_ecu.rxState == ISOTP_RX_WAIT_CF, "FC 후 CF 대기 상태");
        (void)VCan_Send(s_nodeTester, ID_T2E, 8u, cf_bad);
        run(3);
        MT_CHECK(s_ecu.errRxSn == 1u, "시퀀스 불일치 감지");
        MT_CHECK(s_ecu.rxState == ISOTP_RX_IDLE && s_rxCnt == 0u, "수신 abort, 조립 폐기");
        (void)VCan_Send(s_nodeTester, ID_T2E, 8u, ff);   /* 재시도는 받아들여야 함 */
        run(3);
        MT_CHECK(s_ecu.rxState == ISOTP_RX_WAIT_CF, "새 FF 로 복구 가능");
    }

    MT_SECTION("TC_TP_031: FC Overflow -> 송신 abort");
    {
        uint8 msg[20]; for (uint8 i = 0u; i < 20u; i++) { msg[i] = i; }
        uint8 fc_ovfl[3] = { 0x32u, 0u, 0u };
        s_txDoneCnt = 0u;
        (void)IsoTp_Send(&s_ecu, msg, 20u);          /* FF 송신, FC 대기 진입 */
        run(3);
        MT_CHECK(s_ecu.txState == ISOTP_TX_WAIT_FC, "FC 대기 상태");
        (void)VCan_Send(s_nodeTester, ID_T2E, 3u, fc_ovfl);
        run(3);
        MT_CHECK(s_ecu.errTxAbort == 1u && s_txDoneVal == 0u, "OVFL 에 abort + 실패 콜백");
    }

    MT_SECTION("TC_TP_040: STmin 에 따른 CF 간격(sending pacing)");
    {
        uint8 msg[16]; for (uint8 i = 0u; i < 16u; i++) { msg[i] = i; }
        uint8 fc_cts[3] = { 0x30u, 0x00u, 0x05u };   /* CTS, BS=0, STmin=5ms */
        s_cfCnt = 0u;
        (void)IsoTp_Send(&s_ecu, msg, 16u);          /* FF */
        run(2);
        (void)VCan_Send(s_nodeTester, ID_T2E, 3u, fc_cts);
        run(12);
        MT_CHECK(s_cfCnt >= 2u, "CF 2개 관찰됨");
        MT_CHECK(s_cfCnt >= 2u && (s_cfCycle[1] - s_cfCycle[0]) >= 5u,
                 "CF 간격 >= STmin(5ms)");
    }

    MT_SUMMARY();
}
