#ifndef WDG_H
#define WDG_H
/*
 * wdg.h - Watchdog Manager(WdgM) 간소화판
 * 역할: "프로그램이 계획대로 살아 실행되고 있는가" 를 감시한다 (ISO 26262 의
 *       프로그램 흐름 감시 / 타임아웃 감지). 실패 시 MCU 리셋 등 안전 반응 트리거.
 *   Supervised Entity(SE): 감시 대상(런어블/BSW 메인함수)
 *   Checkpoint           : 대상이 "살아있음"을 보고하는 호출
 *   Deadline             : 마지막 체크포인트 이후 허용 최대 시간
 */
#include "Std_Types.h"

#define WDGM_MAX_SE 4u

typedef enum { WDGM_OK = 0, WDGM_FAILED, WDGM_EXPIRED, WDGM_STOPPED } WdgM_Status;

void          WdgM_Init(void);
Std_ReturnType WdgM_RegisterEntity(uint8 seId, uint16 deadlineMs);   /* returns idx or E_NOT_OK */
void          WdgM_Checkpoint(uint8 seId);                           /* alive 보고 */
void          WdgM_MainFunction(uint16 elapsedMs);                   /* period 호출 */
WdgM_Status   WdgM_GetLocalStatus(uint8 seId);
WdgM_Status   WdgM_GetGlobalStatus(void);
uint32        WdgM_GetViolationCount(void);
void          WdgM_Reset(void);                                      /* 감시 상태 초기화 */

#endif /* WDG_H */
