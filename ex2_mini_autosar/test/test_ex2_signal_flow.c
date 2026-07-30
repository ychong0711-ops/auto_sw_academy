/*
 * [EX2 통합 테스트] 미니 AUTOSAR 신호 흐름 검증
 *
 * 검증 경로(TX):  센서 주입 -> Adc/Dio -> IoHwAb -> RTE -> SWC 로직
 *                 -> Com(패킹) -> PduR -> CanIf -> Can -> 버스 프레임 관찰
 * 검증 경로(RX):  버스 주입(BCM 오버라이드) -> Can ISR -> CanIf -> PduR
 *                 -> Com(언패킹) -> RTE -> SWC 로직 -> Dio 출력 관찰
 *
 * 요구사항 추적: [SRS_EX2_001] TX 프레임에 시그널이 설정 레이아웃대로 실려야 한다
 *               [SRS_EX2_002] 유효한 RX 오버라이드는 자동 로직보다 우선한다
 */
#include "mini_test.h"
#include "SimMcu.h"
#include "Dio.h"
#include "Adc.h"
#include "Can.h"
#include "SimBus.h"
#include "Com.h"
#include "IoHwAb.h"
#include "Rte.h"
#include "Os.h"

/* ---- 테스트 스파이: ECU 가 버스에 쏘는 프레임을 관찰 ---- */
static uint16 s_lastId;
static uint8  s_lastDlc;
static uint8  s_lastData[8];
static uint32 s_frameCnt;

static void spy_on_tx(uint16 id, uint8 dlc, const uint8* data)
{
    s_lastId = id; s_lastDlc = dlc;
    for (uint8 i = 0u; i < dlc && i < 8u; i++) { s_lastData[i] = data[i]; }
    s_frameCnt++;
}

static void boot_ecu(void)
{
    SimMcu_Init();
    Dio_Init();
    Adc_Init();
    Can_Init();
    Com_Init();
    IoHwAb_Init();
    Rte_Init();
    Os_Init();
    s_frameCnt = 0u;
    SimBus_SetSniffer(spy_on_tx);
}

int main(void)
{
    boot_ecu();

    MT_SECTION("TC_EX2_001: 센서->네트워크 TX 경로");
    /* 물리 세계: 배터리 ADC 2000, 어두운 밤(0x50), 라이트 스위치 눌림 */
    SimMcu_InjectAdc(ADC_CH_BATTERY, 2000u);
    SimMcu_InjectAdc(ADC_CH_AMBIENT, 0x0050u);
    SimMcu_InjectPin(DIO_CH_LIGHT_SWITCH, STD_HIGH);
    Os_RunTicks(3u);

    MT_CHECK(s_frameCnt >= 1u, "VehicleState 주기 프레임 발생");
    MT_CHECK(s_lastId == 0x100u, "CAN ID = 0x100 (VehicleState)");
    /* 기대 프레임: 2000=0x7D0 -> byte0 0xD0, byte1 = 0x07|sw(0x10)|cmd(0x20)=0x37 */
    MT_CHECK(s_lastData[0] == 0xD0u, "BatteryVoltage raw 하위바이트=0xD0");
    MT_CHECK(s_lastData[1] == 0x37u, "Volt상위+스위치+명령 비트=0x37");
    MT_CHECK(SimMcu_GetOutputPin(DIO_CH_HEADLIGHT) == 1u, "전조등 물리 출력 ON");

    MT_SECTION("TC_EX2_002: 네트워크 RX 오버라이드 경로");
    {
        uint8 ovrOff[2] = { 0x02u, 0x5Au };   /* 강제 소등 + 유효키 */
        SimBus_InjectRx(0x200u, 2u, ovrOff);  /* 인터럽트 즉시 반영 -> 다음 tick 에 로직 적용 */
        Os_RunTicks(2u);
    }
    MT_CHECK(SimMcu_GetOutputPin(DIO_CH_HEADLIGHT) == 0u, "오버라이드로 출력 강제 OFF");
    MT_CHECK((s_lastData[1] & 0x20u) == 0u, "TX 프레임 HeadlightCmd 비트도 0");

    MT_SECTION("TC_EX2_003: 오버라이드 해제 -> 자동 로직 복귀");
    {
        uint8 ovrAuto[2] = { 0x00u, 0x5Au };
        SimBus_InjectRx(0x200u, 2u, ovrAuto);
    }
    SimMcu_InjectAdc(ADC_CH_AMBIENT, 0x0300u);  /* 밝은 낮 */
    SimMcu_InjectPin(DIO_CH_LIGHT_SWITCH, STD_LOW);
    Os_RunTicks(2u);
    MT_CHECK(SimMcu_GetOutputPin(DIO_CH_HEADLIGHT) == 0u, "자동 로직: 낮+스위치OFF -> 소등");
    MT_CHECK(s_lastData[1] == 0x07u, "byte1 = 전압상위만 (0x07)");

    MT_SUMMARY();
}
