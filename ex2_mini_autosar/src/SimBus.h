#ifndef SIMBUS_H
#define SIMBUS_H
/*
 * SimBus.h - 가상 CAN 버스 (테스터/타 ECU 역할)
 * Can 드라이버의 하위 경계. 보드 위 CAN 트랜시버+버스+다른 노드를 흉낸다.
 */
#include "Std_Types.h"

/* 테스트(의사 테스터)가 ECU 의 송신 프레임을 관찰하기 위한 콜백 */
typedef void (*SimBusSniffer)(uint16 id, uint8 dlc, const uint8* data);

void SimBus_SetSniffer(SimBusSniffer cb);

/* ECU -> 버스 (Can.c 가 호출) */
void SimBus_TransmitFromEcu(uint16 id, uint8 dlc, const uint8* data);

/* 버스 -> ECU (테스트가 호출: 다른 ECU/진단기가 별낸 프레임) */
void SimBus_InjectRx(uint16 id, uint8 dlc, const uint8* data);

#endif /* SIMBUS_H */
