/*
 * wdg.c - 감시 상태기계: OK --(마감 초과)--> EXPIRED (자동 복구 없음, 리셋 필요)
 * 실제 WdgM 이 하는 일의 최소 골격: alive 모니터링 + 글로벌 상태 집계.
 */
#include "wdg.h"

typedef struct {
    uint8  seId;
    uint16 deadlineMs;
    uint16 sinceLastCp;
    uint8  registered;
} WdgM_Se;

static WdgM_Se  s_se[WDGM_MAX_SE];
static uint32   s_violations;
static WdgM_Status s_global;

void WdgM_Init(void)
{
    for (uint8 i = 0u; i < WDGM_MAX_SE; i++) { s_se[i].registered = 0u; }
    s_violations = 0u;
    s_global = WDGM_OK;
}

Std_ReturnType WdgM_RegisterEntity(uint8 seId, uint16 deadlineMs)
{
    for (uint8 i = 0u; i < WDGM_MAX_SE; i++) {
        if (s_se[i].registered == 0u) {
            s_se[i].seId = seId; s_se[i].deadlineMs = deadlineMs;
            s_se[i].sinceLastCp = 0u; s_se[i].registered = 1u;
            return E_OK;
        }
    }
    return E_NOT_OK;
}

static WdgM_Se* find(uint8 seId)
{
    for (uint8 i = 0u; i < WDGM_MAX_SE; i++) {
        if ((s_se[i].registered != 0u) && (s_se[i].seId == seId)) { return &s_se[i]; }
    }
    return 0;
}

void WdgM_Checkpoint(uint8 seId)
{
    WdgM_Se* se = find(seId);
    if (se != 0) { se->sinceLastCp = 0u; }   /* EXPIRED 후에는 리셋 전까지 자동 복구 안 함 */
}

void WdgM_MainFunction(uint16 elapsedMs)
{
    uint8 anyExpired = 0u;
    for (uint8 i = 0u; i < WDGM_MAX_SE; i++) {
        if (s_se[i].registered == 0u) { continue; }
        s_se[i].sinceLastCp = (uint16)(s_se[i].sinceLastCp + elapsedMs);
        if (s_se[i].sinceLastCp >= s_se[i].deadlineMs) { anyExpired = 1u; }
    }
    if (anyExpired != 0u) { s_violations++; s_global = WDGM_EXPIRED; }
}

WdgM_Status WdgM_GetLocalStatus(uint8 seId)
{
    const WdgM_Se* se = find(seId);
    if (se == 0) { return WDGM_STOPPED; }
    return (se->sinceLastCp >= se->deadlineMs) ? WDGM_EXPIRED : WDGM_OK;
}

WdgM_Status WdgM_GetGlobalStatus(void) { return s_global; }
uint32      WdgM_GetViolationCount(void) { return s_violations; }

void WdgM_Reset(void)
{
    for (uint8 i = 0u; i < WDGM_MAX_SE; i++) { s_se[i].sinceLastCp = 0u; }
    s_global = WDGM_OK;
}
