/*
 * SensorSwc.c - [SRS_APP_010] 10ms 마다 센서 원시값을 RTE 포트로 발행
 * 이 SWC 의 "코드"는 MCAL 의 존재를 모른다 → 다른 ECU 로 옮겨도 컴파일됨.
 */
#include "Rte.h"

void SensorSwc_Runnable(void)
{
    uint16 v;

    if (Rte_Call_BatteryAdc_Read(&v) == E_OK) { Rte_Write_BatteryVoltage(v); }
    if (Rte_Call_AmbientAdc_Read(&v) == E_OK) { Rte_Write_AmbientLight(v); }
    Rte_Write_LightSwitch(Rte_Call_LightSwitch_Get());
}
