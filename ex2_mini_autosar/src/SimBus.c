/*
 * SimBus.c - 가상 CAN 버스 구현
 */
#include "SimBus.h"
#include "Can.h"   /* 버스 -> ECU 전달은 CAN 컨트롤러 수신 인터럽트로 진입 */

static SimBusSniffer s_sniffer = 0;

void SimBus_SetSniffer(SimBusSniffer cb) { s_sniffer = cb; }

void SimBus_TransmitFromEcu(uint16 id, uint8 dlc, const uint8* data)
{
    if (s_sniffer != 0) { s_sniffer(id, dlc, data); }
}

void SimBus_InjectRx(uint16 id, uint8 dlc, const uint8* data)
{
    /* 실제 차량에서는 CAN 컨트롤러의 RX FIFO 에 프레임이 도착해 인터럽트 발생 */
    Can_RxInterrupt(id, dlc, data);
}
