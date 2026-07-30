/*
 * Dio.c - MCAL Dio 드라이버 구현 (간소화)
 * Dio063: 쓰기는 ODR 레지스터에, 읽기는 IDR 레지스터로부터.
 * 채널 = 물리 핀 번호 (간소화: channel id == pin 번호)
 */
#include "Dio.h"
#include "SimMcu.h"

void Dio_Init(void) { /* 실제: Port 드라이버가 핀 방향을 설정. 여기선 생략 */ }

/* [SRS_DIO_010] 채널에 STD_HIGH/STD_LOW 출력 */
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level)
{
    if (ChannelId >= 8u) { return; }                     /* DET 에러로 처리하는 것이 정석 */
    if (Level == STD_HIGH) { SimMcu_GpioOdr |=  (uint8)(1u << ChannelId); }
    else                   { SimMcu_GpioOdr &= ~(uint8)(1u << ChannelId); }
}

/* [SRS_DIO_020] 채널의 물리 레벨 판독 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId)
{
    if (ChannelId >= 8u) { return STD_LOW; }
    return (Dio_LevelType)((SimMcu_GpioIdr >> ChannelId) & 1u);
}

/* [SRS_DIO_030] 출력 토글 (Dio_FlipChannel, AUTOSAR 실존 API) */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId)
{
    Dio_LevelType lv;
    if (ChannelId >= 8u) { return STD_LOW; }
    SimMcu_GpioOdr ^= (uint8)(1u << ChannelId);
    lv = (Dio_LevelType)((SimMcu_GpioOdr >> ChannelId) & 1u);
    return lv;
}
