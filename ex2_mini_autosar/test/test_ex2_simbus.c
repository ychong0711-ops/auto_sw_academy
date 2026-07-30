/*
 * [EX2 SimBus 단위 테스트] 가상 CAN 버스 스니퍼/송신 검증
 *
 * SimBus 는 CAN 트랜시버 + 버스 + 타 노드를 흉내내는 계층.
 * TransmitFromEcu → Sniffer 콜백 경로와 Sniffer NULL 안전성을 검증한다.
 *
 * 요구사항 추적: [SRS_SIMBUS_010] 송신 프레임 스니퍼 콜백 전달
 *               [SRS_SIMBUS_020] Sniffer NULL 안전성
 *               [SRS_SIMBUS_030] 다중 송신 시 매번 스니퍼 호출
 */
#include "mini_test.h"
#include "SimBus.h"

/* ---- 테스트 스파이 ---- */
static uint16 s_spyId;
static uint8  s_spyDlc;
static uint8  s_spyData[8];
static int    s_spyCallCnt;

static void spy_reset(void) {
    s_spyId = 0u; s_spyDlc = 0u; s_spyCallCnt = 0;
    for (uint8 i = 0u; i < 8u; i++) { s_spyData[i] = 0u; }
}

static void spy_cb(uint16 id, uint8 dlc, const uint8* data) {
    s_spyId = id; s_spyDlc = dlc;
    for (uint8 i = 0u; i < dlc && i < 8u; i++) { s_spyData[i] = data[i]; }
    s_spyCallCnt++;
}

/* TC_SIMBUS_040 용 대체 sniffer */
static int s_altCalled = 0;
static void alt_cb(uint16 id, uint8 dlc, const uint8* data) {
    (void)id; (void)dlc; (void)data;
    s_altCalled = 1;
}

int main(void)
{
    spy_reset();

    MT_SECTION("TC_SIMBUS_010: Sniffer 등록 후 Transmit -> 콜백 호출");
    {
        SimBus_SetSniffer(spy_cb);

        uint8 txData[] = { 0x12u, 0x34u, 0x56u, 0x78u };
        SimBus_TransmitFromEcu(0x100u, 4u, txData);

        MT_CHECK(s_spyCallCnt == 1, "스니퍼 1회 호출");
        MT_CHECK(s_spyId == 0x100u, "CAN ID = 0x100");
        MT_CHECK(s_spyDlc == 4u, "DLC = 4");
        MT_CHECK(s_spyData[0] == 0x12u, "Data[0] = 0x12");
        MT_CHECK(s_spyData[3] == 0x78u, "Data[3] = 0x78");
    }

    MT_SECTION("TC_SIMBUS_020: 다중 송신 -> 호출 횟수 누적");
    {
        spy_reset();
        SimBus_SetSniffer(spy_cb);

        uint8 d1[] = { 0x01u };
        uint8 d2[] = { 0x02u };
        uint8 d3[] = { 0x03u };
        SimBus_TransmitFromEcu(0x100u, 1u, d1);
        SimBus_TransmitFromEcu(0x200u, 1u, d2);
        SimBus_TransmitFromEcu(0x300u, 1u, d3);

        MT_CHECK(s_spyCallCnt == 3, "3회 송신 -> 3회 호출");
        MT_CHECK(s_spyId == 0x300u, "마지막 ID = 0x300");
        MT_CHECK(s_spyData[0] == 0x03u, "마지막 data = 0x03");
    }

    MT_SECTION("TC_SIMBUS_030: Sniffer NULL 등록 -> 안전성");
    {
        spy_reset();
        SimBus_SetSniffer(spy_cb);
        uint8 d[] = { 0xAAu };
        SimBus_TransmitFromEcu(0x100u, 1u, d);
        MT_CHECK(s_spyCallCnt == 1, "등록 후 정상 호출");

        SimBus_SetSniffer(0);  /* NULL 등록 */
        SimBus_TransmitFromEcu(0x100u, 1u, d);
        MT_CHECK(s_spyCallCnt == 1, "NULL sniffer: 증가 없음 (크래시 방지)");
    }

    MT_SECTION("TC_SIMBUS_040: Sniffer 교체");
    {
        spy_reset();
        s_altCalled = 0;

        SimBus_SetSniffer(alt_cb);
        uint8 d[] = { 0xBBu };
        SimBus_TransmitFromEcu(0x100u, 1u, d);
        MT_CHECK(s_altCalled != 0, "교체된 sniffer 호출됨");
        MT_CHECK(s_spyCallCnt == 0, "이전 sniffer 호출 안 됨");
    }

    MT_SECTION("TC_SIMBUS_050: TransmitFromEcu 원본 데이터 무결성");
    {
        spy_reset();
        SimBus_SetSniffer(spy_cb);

        uint8 srcData[8] = { 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u };
        SimBus_TransmitFromEcu(0x100u, 8u, srcData);

        MT_CHECK(s_spyDlc == 8u, "DLC = 8");
        for (uint8 i = 0u; i < 8u; i++) {
            if (s_spyData[i] != srcData[i]) {
                MT_CHECK(0, "모든 데이터 바이트 일치");
                break;
            }
        }
    }

    MT_SUMMARY();
}
