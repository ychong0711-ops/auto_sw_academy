#ifndef IOHWAB_H
#define IOHWAB_H
/*
 * IoHwAb.h - I/O Hardware Abstraction (간소화판)
 * 역할: SWC 가 "어떤 핀/채널에 물리적으로 연결됐는지" 모르게 만드는 층.
 *       핀 배치가 바뀌어도(하드웨어 개정) 바뀌는 건 이 파일뿐.
 */
#include "Std_Types.h"

void           IoHwAb_Init(void);

/* [SRS_IO_010] 물리 의미 단위 인터페이스 (채널 번호는 숨김) */
Std_ReturnType IoHwAb_BatteryVoltage_Get(uint16* raw12);
Std_ReturnType IoHwAb_AmbientLight_Get(uint16* raw12);
uint8          IoHwAb_LightSwitch_Get(void);
void           IoHwAb_Headlight_Set(uint8 level);

#endif /* IOHWAB_H */
