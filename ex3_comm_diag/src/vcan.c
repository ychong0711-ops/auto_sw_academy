/*
 * vcan.c - 큐 기반 가상 CAN 버스: 브로드캐스트(자기 자신 제외) 의미론 재현
 */
#include <stdio.h>
#include "vcan.h"

typedef struct { int from; uint16 id; uint8 dlc; uint8 data[8]; } VCanFrame;

static VCanRxCb  s_cb[VCAN_MAX_NODES];
static uint8     s_used[VCAN_MAX_NODES];
static VCanFrame s_q[VCAN_QUEUE];
static uint8     s_head, s_tail;
static FILE*     s_trace;
static uint32    s_nowMs;

int VCan_Attach(VCanRxCb cb)
{
    for (int i = 0; i < (int)VCAN_MAX_NODES; i++) {
        if (s_used[i] == 0u) { s_used[i] = 1u; s_cb[i] = cb; return i; }
    }
    return -1;
}

void VCan_SetCb(int node, VCanRxCb cb)
{
    if ((node >= 0) && (node < (int)VCAN_MAX_NODES)) { s_cb[node] = cb; }
}

uint8 VCan_Send(int from, uint16 id, uint8 dlc, const uint8* data)
{
    uint8 next = (uint8)((s_head + 1u) % VCAN_QUEUE);
    if (next == s_tail) { return E_NOT_OK; }       /* bus overload -> 유실 */
    VCanFrame* f = &s_q[s_head];
    f->from = from; f->id = id;
    f->dlc = (dlc > 8u) ? 8u : dlc;
    for (uint8 i = 0u; i < f->dlc; i++) { f->data[i] = data[i]; }
    s_head = next;

    /* [SRS_MON_010] 송신 시점 그대로 트레이스 (실차 로거처럼 TX 기준) */
    if (s_trace != NULL) {
        fprintf(s_trace, "%u %03X %u ", (unsigned)s_nowMs, id, (unsigned)f->dlc);
        for (uint8 i = 0u; i < f->dlc; i++) { fprintf(s_trace, "%02X", f->data[i]); }
        fputc('\n', s_trace);
        fflush(s_trace);
    }
    return E_OK;
}

uint8 VCan_TraceOpen(const char* path)
{
    if (s_trace != NULL) { fclose(s_trace); s_trace = NULL; }
    s_trace = fopen(path, "w");
    if (s_trace == NULL) { return E_NOT_OK; }
    fprintf(s_trace, "# auto_sw_academy bus trace: <ms> <ID hex> <DLC> <DATA hex>\n");
    return E_OK;
}

void VCan_TraceClose(void)
{
    if (s_trace != NULL) { fclose(s_trace); s_trace = NULL; }
}

void VCan_TraceSetNow(uint32 ms) { s_nowMs = ms; }

void VCan_Pump(void)
{
    while (s_tail != s_head) {
        VCanFrame f = s_q[s_tail];   /* 복사: 콜백 내 재진입 송신 대비 */
        s_tail = (uint8)((s_tail + 1u) % VCAN_QUEUE);
        for (int n = 0; n < (int)VCAN_MAX_NODES; n++) {
            if ((s_used[n] != 0u) && (n != f.from) && (s_cb[n] != 0)) {
                s_cb[n](n, f.id, f.dlc, f.data);
            }
        }
    }
}

uint32 VCan_Pending(void)
{
    return (uint32)((s_head + VCAN_QUEUE - s_tail) % VCAN_QUEUE);
}
