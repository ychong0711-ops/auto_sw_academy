#ifndef VCAN_H
#define VCAN_H
/*
 * vcan.h - 가상 CAN 버스 (큐 기반, 지연 모델링)
 * 송신은 큐에 적재되고, VCan_Pump() 가 전달하므로 "비동기 버스"를 흉낸다.
 * -> ISO-TP 의 흐름제어/STmin 같은 진짜 프로토콜 동작을 시험할 수 있다.
 */
#include "Std_Types.h"

#define VCAN_MAX_NODES  4u
#define VCAN_QUEUE      32u

typedef void (*VCanRxCb)(int node, uint16 id, uint8 dlc, const uint8* data);

int    VCan_Attach(VCanRxCb cb);           /* returns node id (>=0), -1 fail */
void   VCan_SetCb(int node, VCanRxCb cb);  /* 콜백 교체(시험용) */
uint8  VCan_Send(int from, uint16 id, uint8 dlc, const uint8* data);
void   VCan_Pump(void);                    /* 큐의 모든 프레임 전달 */
uint32 VCan_Pending(void);

/* [SRS_MON_010] 버스 모니터 트레이스 (CANoe 의 Trace Window 에 해당)
 * 열어두면 송신되는 모든 프레임이 한 줄씩 기록된다:
 *   <ms> <ID(16진)> <DLC> <DATA(hex 2*DLC자)>
 * -> tools/decode_trace.py 가 이 파일을 .dbc 로 해석한다. */
uint8  VCan_TraceOpen(const char* path);   /* E_OK/E_NOT_OK */
void   VCan_TraceClose(void);
void   VCan_TraceSetNow(uint32 ms);        /* 타임스탬프 기준 시각 */

#endif /* VCAN_H */
