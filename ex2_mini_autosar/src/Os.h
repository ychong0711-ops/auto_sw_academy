#ifndef OS_H
#define OS_H
/*
 * Os.h - 초소형 주기 스케줄러 (AUTOSAR OS 의 ScheduleTable/Alarm 역할)
 * 실제로는 ScheduleTable 이 BSW MainFunction 과 SWC 러너블을 주기 구동한다.
 */
#include "Std_Types.h"

void Os_Init(void);
void Os_Tick10ms(void);      /* 10ms 기본 태스크 1회 실행 */
void Os_RunTicks(uint32 n);  /* n 번 반복 (테스트용 시간 전진) */

#endif /* OS_H */
