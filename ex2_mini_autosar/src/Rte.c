/*
 * Rte.c - RTE 구현 ("생성된 코드"라고 생각하세요)
 * 주의: 실제 RTE 는 implicit/explicit 접근, 인터런어블 변수, 큐잉 등을 다루지만
 *       여기서는 개념 이해에 필요한 단순 버퍼 방식(IRead/IWrite)만 구현.
 */
#include "Rte.h"
#include "Com.h"
#include "IoHwAb.h"

/* RTE 데이터 저장소 (Sender-Receiver 포트의 실체 — 생성 코드에서는 여기에 해당) */
typedef struct {
    uint16 batteryVoltage;
    uint16 ambientLight;
    uint8  lightSwitch;
    uint8  headlightCmd;
    uint8  overrideMode;   /* 유효하지 않으면 overrideKey!=0x5A 로 판정 */
    uint8  overrideKey;
} Rte_Store;

static Rte_Store s_rte;

void Rte_Init(void)
{
    s_rte.batteryVoltage = 0u;
    s_rte.ambientLight   = 0x0FFFu;   /* "밝음" 으로 초기화 */
    s_rte.lightSwitch    = 0u;
    s_rte.headlightCmd   = 0u;
    s_rte.overrideMode   = 0u;
    s_rte.overrideKey    = 0u;
}

/* ---------------- SensorSwc ---------------- */
Std_ReturnType Rte_Call_BatteryAdc_Read(uint16* raw12) { return IoHwAb_BatteryVoltage_Get(raw12); }
Std_ReturnType Rte_Call_AmbientAdc_Read(uint16* raw12) { return IoHwAb_AmbientLight_Get(raw12); }
uint8          Rte_Call_LightSwitch_Get(void)          { return IoHwAb_LightSwitch_Get(); }
void Rte_Write_BatteryVoltage(uint16 raw12) { s_rte.batteryVoltage = raw12; }
void Rte_Write_AmbientLight(uint16 raw12)   { s_rte.ambientLight   = raw12; }
void Rte_Write_LightSwitch(uint8 level)     { s_rte.lightSwitch    = level; }

/* ---------------- LightCtrlSwc ---------------- */
uint16 Rte_Read_AmbientLight(void) { return s_rte.ambientLight; }
uint8  Rte_Read_LightSwitch(void)  { return s_rte.lightSwitch; }
uint8  Rte_Read_OverrideMode(void) { return s_rte.overrideMode; }
uint8  Rte_Read_OverrideKey(void)  { return s_rte.overrideKey; }
uint8  Rte_Read_HeadlightCmd(void) { return s_rte.headlightCmd; }
void   Rte_Write_HeadlightCmd(uint8 cmd) { s_rte.headlightCmd = cmd; }
void   Rte_Call_Headlight_Set(uint8 level) { IoHwAb_Headlight_Set(level); }

/* ------------- Com <-> RTE 데이터 매핑 (생성 영역) ------------- */
void Rte_ComRxSync(void)
{
    uint32 v;
    if (Com_ReceiveSignal(COM_SIG_OverrideMode, &v) == E_OK) { s_rte.overrideMode = (uint8)v; }
    if (Com_ReceiveSignal(COM_SIG_OverrideKey,  &v) == E_OK) { s_rte.overrideKey  = (uint8)v; }
    /* [학습포인트] 첫 수신 전에는 이전 값(before-first-reception) 사용 = init value 유지 */
}

void Rte_ComTxSync(void)
{
    Com_SendSignal(COM_SIG_BatteryVoltage, (uint32)s_rte.batteryVoltage);
    Com_SendSignal(COM_SIG_LightSwitch,    (uint32)s_rte.lightSwitch);
    Com_SendSignal(COM_SIG_HeadlightCmd,   (uint32)s_rte.headlightCmd);
}
