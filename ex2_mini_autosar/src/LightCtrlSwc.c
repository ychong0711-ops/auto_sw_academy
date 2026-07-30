/*
 * LightCtrlSwc.c - [SRS_APP_020] 전조등 제어
 * 로직:
 *   1) 유효한 외부 오버라이드(mode 1/2, key==0x5A)가 있으면 최우선 적용
 *   2) 아니면 자동 로직: 라이트 스위치 ON 이거나 주변이 어두우면 점등
 * 산출물: RTE 포트(네트워크 송신용 HeadlightCmd) + 실제 출력(Headlight Set)
 */
#include "Rte.h"

#define OVERRIDE_KEY_VALID   0x5Au
#define AMBIENT_DARK_RAW     0x0100u   /* 조도 센서 raw 기준: 이보다 작으면 어두움 */

void LightCtrlSwc_Runnable(void)
{
    uint8  lightSw    = Rte_Read_LightSwitch();
    uint16 ambient    = Rte_Read_AmbientLight();
    uint8  ovrMode    = Rte_Read_OverrideMode();
    uint8  ovrKey     = Rte_Read_OverrideKey();

    uint8 cmd;

    if (ovrKey == OVERRIDE_KEY_VALID) {
        if      (ovrMode == 1u) { cmd = 1u; }
        else if (ovrMode == 2u) { cmd = 0u; }
        else    { cmd = (uint8)((lightSw != 0u) || (ambient < AMBIENT_DARK_RAW)); }
    } else {
        cmd = (uint8)((lightSw != 0u) || (ambient < AMBIENT_DARK_RAW));
    }

    Rte_Write_HeadlightCmd(cmd);   /* 네트워크(BCM 게이트웨이 등)로 알리기 */
    Rte_Call_Headlight_Set(cmd);   /* 내 ECU 의 전조등 릴리오 구동        */
}
