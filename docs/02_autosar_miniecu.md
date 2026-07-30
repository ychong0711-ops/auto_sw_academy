# 02. AUTOSAR 아키텍처 — 미니 ECU (MCAL/BSW/RTE/SWC)

실제 AUTOSAR Classic 은 설정 툴(EB tresos, Vector DaVinci)이 ARXML 에서 수십만 줄을 생성합니다.
여기서는 그 **뼈대 구조와 책임 분리**를 ~800줄로 재현했습니다. 코드 생성기가 하는 일을 손으로 합니다.

## 계층도 (본 저장소의 파일과 1:1 대응)

```
┌─ Application ─────────────────────────────────────────────────
│  SensorSwc.c        LightCtrlSwc.c            ← 비즈니스 로직 (핀/CAN ID를 모름)
├─ RTE (자동 생성 영역) ─────────────────────────────────────────
│  Rte.c  — 포트 버퍼, Rte_Call_*(IoHwAb 호출), Com 데이터 매핑    ← "SWC 는 RTE 만 만난다"
├─ BSW 서비스/추상화 ───────────────────────────────────────────
│  Com.c      — 신호 ↔ I-PDU 패킹/주기 송신                        (EX3 신호 코덱의 조상)
│  PduR.c     — PDU 라우팅 테이블 (누구→누구)
│  CanIf.c    — PDU ID ↔ CAN ID 변환, RX 파일터링
│  IoHwAb.c   — 물리 핀 의미화 ("배터리 전압 읽기" 추상화)
├─ MCAL ────────────────────────────────────────────────────────
│  Dio.c  Adc.c  Can.c                                ← SimMcu 위에서만 동작
├─ 하드웨어(시뮬레이션) ───────────────────────────────────────
└─ SimMcu.c (레지스터)  SimBus.c (CAN 버스/다른 노드)
```

규칙 하나만 기억하세요: **화살표는 아래로만, 그리고 인접 계층만.**
`SensorSwc.c` 가 `#include "Adc.h"` 하는 순간 아키텍처 파괴 → 이식성 사망.

## 데이터 흐름 추적 (테스트가 증명하는 경로)

### TX (센서 → 버스) — `TC_EX2_001`
```
SimMcu_InjectAdc(2000)                      "배터리 2000 raw"
→ Adc_ReadGroup → IoHwAb_BatteryVoltage_Get
→ Rte_Call_BatteryAdc_Read → SensorSwc → Rte_Write_BatteryVoltage
→ Rte_ComTxSync → Com_SendSignal(패킹: bit0..11)
→ Com_MainFunctionTx(주기) → PduR_ComTransmit
→ CanIf_Transmit(0→CAN 0x100) → Can_Write → SimBus
기대 프레임: [0x100] D0 37 ..   (테스트가 이 바이트를 assert)
```

### RX (버스 → 출력 핀) — `TC_EX2_002`
```
SimBus_InjectRx(0x200, {0x02,0x5A})          "BCM 이 강제 소등 명령"
→ Can_RxInterrupt → CanIf_RxIndication(필터링→PDU ID)
→ PduR_CanIfRxIndication → Com_RxIndication(버퍼 저장)
→ 다음 10ms tick: Rte_ComRxSync → Com_ReceiveSignal
→ LightCtrlSwc: 오버라이드 우선 적용(키 0x5A 검증)
→ Rte_Call_Headlight_Set → IoHwAb → Dio_WriteChannel
→ SimMcu_GpioOdr 비트 변화 (테스트가 핀을 assert)
```

## 설정(Cfg) 기반 설계 — AUTOSAR 의 진짜 힘

`CanIf.c` 의 `s_txCfg/s_rxCfg`, `PduR.c` 의 라우팅 테이블, `Com.c` 의 `s_sigCfg` 가
실제 프로젝트의 `*_Cfg.c`(설정 툴 생성물)에 해당합니다.

> **새 신호 추가 = 코드 수정이 아니라 테이블 행 추가.** 이 느낌이 핵심.

### 실습 과제 (난이도 ↑)
1. **EngineRpm 프레임 추가**: CAN 0x101, rpm 16bit(Intel), 주기 송신. 고칠 곳: `Com.c` 시그널 표 + `CanIf.c` TX 표 + `PduR` 라우팅 (코드 로직은 0줄 수정이어야 함)
2. **RX 채널 추가**: CAN 0x201 CruiseSet 신호 8bit 수신 → RTE 까지 전달
3. **IoHwAb 채널 추가**: "후진 스위치" 핀 2번을 Dio 입력으로 노출
4. **타겟 이식 시뮬레이션**: `SimMcu.*` 를 실제 MCU 의 ODR/IDR 레지스터 주소 정의로 갈아끼운다고 가정하고, 고쳐야 하는 코드 범위를 적어 보기 (힌트: MCAL 외 0줄)

### 배경 지식 체크
- 왜 RX 통지 함수는 아래→위로 `Xxx_RxIndication` 이름일까? (하위가 제어권을 쥐고 "통지"하기 때문)
- 왜 TX 는 상위 모듈 이름이 붙을까? (`PduR_ComTransmit` = "Com 이 쓰는 Transmit")
- PDU 라우터가 있어서 Dcm(진단) 과 Com(신호) 이 같은 CAN 인터페이스를 공유할 수 있다
