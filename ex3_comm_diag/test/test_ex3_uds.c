/*
 * [EX3-3] UDS (ISO 14229-1) 서버 테스트
 * 실무 배경: 진단기(CANoe Diva 등)가 ECU 를 시험하는 시나리오를 C 로 재현.
 * 요구사항 추적: [SRS_UDS_010] 서비스 디스패치, [SRS_UDS_020] 세션 관리,
 *               [SRS_UDS_030] SecurityAccess (seed & key + 지연),
 *               [SRS_UDS_040] DTC 읽기/삭제, [SRS_UDS_050] 긍정응답 억제
 */
#include "mini_test.h"
#include <string.h>
#include "uds_server.h"

static UdsServer s_srv;
static uint8  s_resp[UDS_RESP_MAX];
static uint16 s_respLen;

static void capture(void* ctx, const uint8* d, uint16 n)
{
    (void)ctx; for (uint16 i = 0u; i < n; i++) { s_resp[i] = d[i]; } s_respLen = n;
}

/* 요청 1건 볼내고 응답 캡처 */
static uint16 req(const uint8* r, uint16 n)
{
    s_respLen = 0u;
    Uds_OnRequest(&s_srv, r, n);
    return s_respLen;
}

static void ticks(uint32 n) { for (uint32 i = 0u; i < n; i++) { Uds_Tick10ms(&s_srv); } }

int main(void)
{
    Uds_Init(&s_srv, capture, 0);

    MT_SECTION("TC_UDS_010: 기본 서비스 / NRC 디스패치");
    {
        uint8 r1[3] = { 0x22u, 0xF1u, 0x90u };
        MT_CHECK(req(r1, 3u) == 20u && s_resp[0] == 0x62u && s_resp[3] == 'A',
                 "ReadDataByIdentifier F190 (VIN 17B)");
        uint8 r2[5] = { 0x2Eu, 0x12u, 0x34u, 0x00u, 0x00u };
        MT_CHECK(req(r2, 5u) == 3u && s_resp[0] == 0x7Fu && s_resp[2] == 0x7Fu,
                 "디폴트 세션에서 WriteDID -> NRC 7F");
        uint8 r3[1] = { 0x99u };
        MT_CHECK(req(r3, 1u) == 3u && s_resp[2] == 0x11u, "미지원 SID -> NRC 11");
        uint8 r4[1] = { 0x10u };
        MT_CHECK(req(r4, 1u) == 3u && s_resp[2] == 0x13u, "길이 부족 -> NRC 13");
    }

    MT_SECTION("TC_UDS_020: 세션 전환 + 억제 비트");
    {
        uint8 r1[2] = { 0x10u, 0x03u };
        MT_CHECK(req(r1, 2u) == 2u && s_resp[0] == 0x50u && s_resp[1] == 0x03u,
                 "확장 세션 진입");
        uint8 r2[2] = { 0x10u, 0x83u };   /* suppressPosRspMsgIndicationBit */
        MT_CHECK(req(r2, 2u) == 0u, "긍정응답 억제 비트 -> 무응답");
        uint8 r3[2] = { 0x3Eu, 0x80u };
        MT_CHECK(req(r3, 2u) == 0u, "TesterPresent 억제 -> 무응답");
        uint8 r4[2] = { 0x10u, 0x05u };
        MT_CHECK(req(r4, 2u) == 3u && s_resp[2] == 0x12u, "미지원 세션번호 -> NRC 12");
    }

    MT_SECTION("TC_UDS_030: SecurityAccess seed & key");
    {
        uint8 w1[5] = { 0x2Eu, 0x12u, 0x34u, 0x01u, 0x2Cu };
        MT_CHECK(req(w1, 5u) == 3u && s_resp[2] == 0x33u, "잠금 상태 Write -> NRC 33");

        uint8 seed_req[2] = { 0x27u, 0x01u };
        MT_CHECK(req(seed_req, 2u) == 4u && s_resp[0] == 0x67u, "requestSeed -> 67 01 + seed");
        uint16 seed = (uint16)(((uint16)s_resp[2] << 8u) | s_resp[3]);
        MT_CHECK(seed != 0u, "잠금 상태에서는 seed != 0 (ISO 규정)");

        uint16 key = UdsDemo_CalcKey(seed);
        uint8 kreq[4] = { 0x27u, 0x02u, (uint8)(key >> 8u), (uint8)(key & 0xFFu) };
        MT_CHECK(req(kreq, 4u) == 2u && s_resp[0] == 0x67u && s_resp[1] == 0x02u,
                 "정확한 키 -> 잠금 해제");

        MT_CHECK(req(seed_req, 2u) == 4u && s_resp[2] == 0u && s_resp[3] == 0u,
                 "해제 상태에서 requestSeed -> seed=0000");

        MT_CHECK(req(w1, 5u) == 3u && s_resp[0] == 0x6Eu, "잠금 해제 후 Write 성공");
        MT_CHECK(s_srv.vehicleSpeedRaw == 0x012Cu, "DID 0x1234 값 반영");
        uint8 rr[3] = { 0x22u, 0x12u, 0x34u };
        MT_CHECK(req(rr, 3u) == 5u && s_resp[3] == 0x01u && s_resp[4] == 0x2Cu,
                 "Read back 확인");
    }

    MT_SECTION("TC_UDS_031: 잘못된 키 3회 -> 0x36 -> 0x37 지연");
    Uds_Init(&s_srv, capture, 0);                       /* 상태 초기화 */
    {
        uint8 ext[2] = { 0x10u, 0x03u }; (void)req(ext, 2u);
        uint8 seed_req[2] = { 0x27u, 0x01u };
        (void)req(seed_req, 2u);                        /* seed 발급 */
        uint8 bad[4] = { 0x27u, 0x02u, 0xDEu, 0xADu };  /* 틀린 키 */
        MT_CHECK(req(bad, 4u) == 3u && s_resp[2] == 0x35u, "1차 실패 -> NRC 35");
        MT_CHECK(req(bad, 4u) == 3u && s_resp[2] == 0x35u, "2차 실패 -> NRC 35");
        MT_CHECK(req(bad, 4u) == 3u && s_resp[2] == 0x36u, "3차 실패 -> NRC 36");
        MT_CHECK(req(seed_req, 2u) == 3u && s_resp[2] == 0x37u, "지연 중 -> NRC 37");
        /* 주의: 보안 지연(10s)이 S3(5s)보다 길다 → 지연이 끝날 때쯤이면 세션도 만료.
           실제 진단기는 지연 중에도 TesterPresent 로 세션을 유지한다! */
        for (int i = 0; i < 5; i++) {                          /* S3 유지하며 지연 경과 */
            uint8 tp[2] = { 0x3Eu, 0x80u }; (void)req(tp, 2u);
            ticks(UDS_SEC_DELAY_TICKS / 4u);
        }
        ticks(10u);
        MT_CHECK(req(seed_req, 2u) == 4u && s_resp[0] == 0x67u, "지연 만료 후 재시도 가능");
    }

    MT_SECTION("TC_UDS_040: DTC 읽기/삭제");
    Uds_Init(&s_srv, capture, 0);
    {
        uint8 n1[3] = { 0x19u, 0x02u, 0x2Cu };
        uint16 n = req(n1, 3u);   /* 두 DTC 모두 status&0x2C != 0 */
        MT_CHECK(n == 3u + 8u && s_resp[0] == 0x59u && s_resp[1] == 0x02u,
                 "reportDTCByStatusMask: 2건 보고");
        MT_CHECK(s_resp[3] == 0x03u && s_resp[4] == 0x01u && s_resp[6] == 0x2Cu,
                 "P0301 DTC 바이트 + 상태");
        /* 0x14 는 확장+보안 필요 */
        uint8 ext[2] = { 0x10u, 0x03u }; (void)req(ext, 2u);
        uint8 clr[4] = { 0x14u, 0xFFu, 0xFFu, 0xFFu };
        MT_CHECK(req(clr, 4u) == 3u && s_resp[2] == 0x33u, "보안 전 ClearDTC -> NRC 33");
        uint8 seed_req[2] = { 0x27u, 0x01u }; (void)req(seed_req, 2u);
        uint16 seed = (uint16)(((uint16)s_resp[2] << 8u) | s_resp[3]);
        uint16 key = UdsDemo_CalcKey(seed);
        uint8 kreq[4] = { 0x27u, 0x02u, (uint8)(key >> 8u), (uint8)(key & 0xFFu) };
        (void)req(kreq, 4u);
        MT_CHECK(req(clr, 4u) == 1u && s_resp[0] == 0x54u, "ClearDTC 성공(54)");
        uint8 n2[3] = { 0x19u, 0x01u, 0xFFu };
        MT_CHECK(req(n2, 3u) == 6u && s_resp[5] == 0u, "삭제 후 개수 0");
    }

    MT_SECTION("TC_UDS_050: 루틴 제어 (차량 정지 조건)");
    Uds_Init(&s_srv, capture, 0);
    {
        uint8 ext[2] = { 0x10u, 0x03u }; (void)req(ext, 2u);
        uint8 seed_req[2] = { 0x27u, 0x01u }; (void)req(seed_req, 2u);
        uint16 key = UdsDemo_CalcKey((uint16)(((uint16)s_resp[2] << 8u) | s_resp[3]));
        uint8 kreq[4] = { 0x27u, 0x02u, (uint8)(key >> 8u), (uint8)(key & 0xFFu) };
        (void)req(kreq, 4u);
        uint8 start_routine[4] = { 0x31u, 0x01u, 0x02u, 0x03u };
        s_srv.vehicleSpeedRaw = 100u;                 /* 주행 중 */
        MT_CHECK(req(start_routine, 4u) == 3u && s_resp[2] == 0x22u,
                 "주행 중 자기진단 -> NRC 22 조건 불만족");
        s_srv.vehicleSpeedRaw = 0u;
        MT_CHECK(req(start_routine, 4u) == 5u && s_resp[0] == 0x71u,
                 "정지 후 자기진단 시작 성공");
    }

    MT_SECTION("TC_UDS_060: S3 세션 타임아웃");
    Uds_Init(&s_srv, capture, 0);
    {
        uint8 ext[2] = { 0x10u, 0x03u }; (void)req(ext, 2u);
        uint8 rds[3] = { 0x22u, 0xF1u, 0x86u };       /* ActiveDiagnosticSession */
        ticks(UDS_S3_TICKS - 1u);
        MT_CHECK(req(rds, 3u) == 4u && s_resp[3] == 0x03u, "S3 전: 여전히 확장 세션");
        ticks(2u);
        MT_CHECK(req(rds, 3u) == 4u && s_resp[3] == 0x01u, "S3 경과: 디폴트로 복귀");
    }

    MT_SECTION("TC_UDS_070: ECU 리셋");
    Uds_Init(&s_srv, capture, 0);
    {
        uint8 r[2] = { 0x11u, 0x01u };
        MT_CHECK(req(r, 2u) == 2u && s_resp[0] == 0x51u, "hardReset 긍정응답");
        MT_CHECK(s_srv.resetPending == 1u, "응답 후 리셋 플래그 확인");
    }

    MT_SUMMARY();
}
