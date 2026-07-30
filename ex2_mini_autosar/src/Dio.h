#ifndef DIO_H
#define DIO_H
/*
 * Dio.h - MCAL Dio 드라이버 (AUTOSAR SWS_DioDriver 간소화판)
 * DioDriver_002: 채널 단위 읽기/쓰기를 제공한다.
 */
#include "Std_Types.h"

typedef uint8 Dio_ChannelType;
typedef uint8 Dio_LevelType;

/* ==== Dio_Cfg.h 에 해당 (일반적으로 설정 툴이 생성) ==== */
#define DIO_CH_HEADLIGHT     ((Dio_ChannelType)0u)  /* 출력: 전조등 릴리오     */
#define DIO_CH_LIGHT_SWITCH  ((Dio_ChannelType)1u)  /* 입력: 라이트 스위치     */

void          Dio_Init(void);
void          Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level); /* [SRS_DIO_010] */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId);                       /* [SRS_DIO_020] */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId);                       /* [SRS_DIO_030] */

#endif /* DIO_H */
