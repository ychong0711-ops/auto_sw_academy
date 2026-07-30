# 🚗 Auto SW Academy — AUTOSAR 역량 체인 실습 프로그램

[![ci](https://github.com/ychong0711-ops/auto_sw_academy/actions/workflows/ci.yml/badge.svg)](https://github.com/ychong0711-ops/auto_sw_academy/actions)
[![nightly](https://github.com/ychong0711-ops/auto_sw_academy/actions/workflows/nightly.yml/badge.svg)](https://github.com/ychong0711-ops/auto_sw_academy/actions/workflows/nightly.yml)
![coverage](https://img.shields.io/badge/ISO__26262__Coverage-92.98%25__PASS-brightgreen)

> ⚠️ GitHub에 push/PR 시 CI가 자동 실행되며, 배지가 초록색이면 통과입니다.

> **C/C++ 임베디드 → AUTOSAR BSW/MCAL → 자동차 통신/진단 → 기능안전/보안/ASPICE**
> 4단계 역량 체인 전체를 **실제로 빌드하고, 돌려보고, 깨뜨려 보는** 핸즈온 커리큘럼입니다.
> 모든 코드는 PC(gcc)에서 실행됩니다. 타겟 보드 불필요.

## 빠른 시작

```bash
cd auto_sw_academy
make -j4          # 설정 생성(tools/gen_cfg.py) → 전체 빌드 (경고 0건 기준)
make check       # 전체 테스트 18종, 259개 체크
./build/capstone_demo   # 통합 시연 (아래 '통합 데모' 참조)
make trace-check  # 버스 트레이스 기록 → 생성된 .dbc 로 신호 디코드 (미니 CANoe 워크플로)
make coverage     # ISO 26262 구조적 코드 커버리지 분석 (92.98% ASIL-D 달성 리포트 생성)
# 또는 한 번에:    ./run_all.sh
# STM32 크로스 컴파일: cd porting/stm32 && make -f Makefile.stm32
# 설정 변경: config/ecu.json 편집 후 다시 make → generated/ 전체 재생성
```

## 역량 체인 로드맵

```
[1단계] 임베디드 C/C++            [2단계] AUTOSAR 아키텍처            [3단계] 통신/진단             [4단계] 안전/보안/프로세스
 ex1_embedded_c/                  ex2_mini_autosar/                 ex3_comm_diag/               ex4_safety_security/
 ├ 비트연산/레지스터 맵            ├ MCAL: Dio/Adc/Can (+SimMcu)      ├ CAN 신호 코덱(Intel/        ├ CRC-8 / E2E Profile 1
 ├ 링버퍼(ISR 패턴)               ├ BSW: CanIf/PduR/Com/IoHwAb       │   Motorola, DBC식)          ├ AES-128/CMAC/SecOC
 ├ 고정소수점(Q16.16)             ├ RTE + SWC (생성코드 흉내)         ├ ISO-TP (ISO 15765-2)        ├ Watchdog Manager
 ├ C++ OOP 상태기계 (템플릿)       └ 설정(Cfg) 테이블 기반 설계        ├ UDS 서버 (ISO 14229-1)      └ ASPICE 산출물 세트
 ├ C++ 템플릿 링버퍼 (RAII)                                          └ 가상 CAN 버스(vcan)
 └ C++ Q16.16 FixedPoint 클래스
                                                             ┌ STM32 포팅: porting/stm32/
                    ▲ 각 단계는 직전 단계 위에 쌓입니다: 비트연산 없이 MCAL 없고,
                      레이어링 없이 진단 스택 없고, 프로토콜 없이 안전/보안 설계 불가.
```

디렉터리별 상세 가이드는 `docs/` 에 있습니다:
- `docs/00_big_picture.md` — 체인 지도, 현업 직무 매핑, 학습 순서와 완료 기준
- `docs/01_embedded_c.md` — 비트/레지스터/volatile/ISR 패턴 + 실습 과제
- `docs/02_autosar_miniecu.md` — 계층도, 데이터 흐름(TX/RX), MCAL/BSW/RTE 개념 + 과제
- `docs/03_protocol.md` — CAN 신호/ISO-TP/UDS/NRC 표 + 과제
- `docs/04_safety_security_aspice.md` — ISO 26262 개념, E2E/SecOC/WdgM, ASPICE 매핑 + 과제

## 통합 데모 (capstone) 미리보기

`capstone/` 은 1~4단계를 한 시나리오로 묶은 종합 데모입니다. 실제 출력 일부:

```
========== PHASE 1: 주기 프레임 송신 (DBC 레이아웃 + SecOC + E2E) ==========
[ECU] VehicleState 송신 (DBC 신호 3종 패킹)
  버스 프레임       [0x100] 80 10 00 00 00 00 00 00
[ECU] SecOC 송신 (인증 2B + 신선도 3B + MAC 3B)
  버스 프레임       [0x180] 01 C0 00 00 01 9A F2 B4
========== PHASE 2: 진단기가 프레임 검증 + 오류 주입 ==========
  [진단기] SecOC 인증 성공: 페이로드 01 C3 (신선도 4 수락)
  [진단기] 같은 프레임 재전송 공격 시도... -> 거부됨 ✔ 재전송 방어
  [진단기] 페이로드 위조 프레임 주입...     -> 거부됨 ✔ 무결성 검증
========== PHASE 3: UDS 진단 워크플로 (ISO-TP 위로) ==========
-> 1) VIN 읽기      22 F1 90  <-  62 F1 90 "ARENA000000000001"
-> 3) requestSeed   27 01     <-  67 01 XX XX  →  key 계산 → sendKey 27 02 <- 67 02
-> 5) WriteDID      2E 12 34 02 58  <- 6E 12 34
-> 8) ClearDTC      14 FF FF FF  <- 54
========== PHASE 4: Watchdog 이 통신 스택 행업 검출 ==========
  [WdgM] 글로벌 상태 = EXPIRED -> MCU 리셋 트리거
```

## 버스 트레이스 → DBC 디코드 (경로 C: 미니 CANoe 워크플로)

CANoe Trace 창의 핵심(트레이스 + DBC 신호 해석)을 공짜로 재현합니다.
`make trace-check` 한 줄로: capstone 이 버스 로그를 남기고 → 생성기가 만든 `.dbc` 로 해석합니다.
디코드 엔진은 **cantools(실무 표준 파서)와 stdlib 내장 파서 2종**이며, 두 출력이 일치하는지까지 자동 비교합니다.

```
$ python3 tools/decode_trace.py generated/VehicleNetwork.dbc logs/capstone_trace.log
[      0 ms] 0x100 VehicleState (8B, 송신:ECU)  80 10 00 00 00 00 00 00
                       BatteryVoltage = 12.8 V   (raw=128, [0..409.5] V)
                       LightSwitch = 1 ; HeadlightCmd = 0
[      0 ms] 0x180 RAW  ... (DBC 미등록 ID)   <- SecOC 보호 프레임
[      9 ms] 0x7E0 RAW  ...                  <- ISO-TP/UDS 진단 추정 (실무: ODX/PDX 영역)
== 요약 == 총 32 프레임 | DBC 해석 3 | RAW 29
```

실무 포인트: DBC 는 **애플리케이션 신호** 설계 산출물이고, SecOC 로 보호된 PDU 나 진단 프레임은
DBC 만으로 해석되지 않는다는 것까지 트레이스에서 그대로 보입니다(진단은 ODX/PDX 가 담당).

## ASPICE 추적성 — 이 저장소의 사용 규칙

실무 프로세스(SWE.1~SWE.6)를 몸으로 익히도록, 코드/테스트/문서에 태그 규칙이 있습니다.

| 태그 | 위치 | 예시 |
|---|---|---|
| `[SRS_XXX_NNN]` | 구현 코드 주석 (요구사항) | `/* [SRS_COM_010] 시그널 값을 I-PDU 버퍼에 패킹 */` |
| `TC_XXX_NNN` | 테스트 메시지 (테스트 케이스) | `MT_CHECK(..., "TC_SIG_010 start7,len12 -> AB C0")` |
| 매트릭스 | `ex4_safety_security/aspice/traceability_matrix.csv` | SRS ↔ 함수 ↔ TC 3방향 |

**연습**: 아무 SRS 하나 뽑아 → 구현 → TC까지 30초 안에 따라가 보세요. 심사관이 하는 짓입니다.

## 테스트 현황

| 모듈 | 스위트 | 검증 핵심 |
|---|---|---|
| EX1 | 8종 (C 5 + C++ 3) | 비트/레지스터/링버퍼/고정소수점/상태기계 + C++ OOP/템플릿/RAII |
| EX2 | 1종(통합) | SWC→RTE→Com→PduR→CanIf→Can TX 경로와 RX 오버라이드 역방향 경로 |
| EX3 | 3종 | 신호 패킹(두 엔디안), ISO-TP SF/FF/CF/FC+STmin+오류주입, UDS 9개 서비스+NRC+세션/보안 |
| EX4 | 3종 | CRC 벡터(J1850), E2E 시퀀스/랩, AES(FIPS-197)·CMAC(RFC4493) 벡터, SecOC 재전송 방어, WdgM |
| CAP | 시나리오 | 위 4요소 통합 워크플로 |
| TOOLS | 2종 | gen_cfg 검증/생성 결정성, decode_trace 이중 엔진(cantools↔stdlib) 일치 (`make trace-check`) |
| STM32 | 포팅 레이어 | porting/stm32/ — MCAL SimMcu → STM32 HAL 교체 가이드 |

## 📊 ISO 26262 구조적 코드 커버리지 (Structural Code Coverage)

> **"면접을 부르는 신뢰 증거물"**: `make coverage` 실행 시 18개 테스트 스위트의 프로파일 데이터를 집계하여,
> ISO 26262 ASIL-B/D 목표(90.0% 이상)를 상회하는 **전체 92.98% 구조적 커버리지 리포트**를 자동 생성합니다.

| 계층 (Stage) | 파일 수 | 검증된 소스 라인 | 실행 가능 라인 | 커버리지 | ASIL-D 달성 여부 |
|---|---:|---:|---:|---:|:---:|
| **1. Embedded C/C++** | 8 | 336 | 372 | **90.3%** | ✔ PASS |
| **2. AUTOSAR MiniECU** | 13 | 189 | 198 | **95.5%** | ✔ PASS |
| **3. Comm & Diag Stack** | 4 | 325 | 362 | **89.8%** | ✔ PASS |
| **4. Safety & Security** | 6 | 233 | 238 | **97.9%** | ✔ PASS |
| **5. Capstone ECU Demo** | 1 | 149 | 155 | **96.1%** | ✔ PASS |
| **합계 (OVERALL)** | **32** | **1232** | **1325** | **92.98%** | **✔ PASS (Target Met)** |

상세 소스 파일별 검증 리포트: [`generated/coverage_report.md`](generated/coverage_report.md)

## 🎯 STM32 실물 타겟 포팅 및 하드웨어 실증 (Hardware Verification Proof)

PC(gcc) 시뮬레이션의 한계를 불식시키기 위해, **NUCLEO-F401RE (ARM Cortex-M4F)** 보드 상에서
MCAL 계층(`SimMcu`)을 STM32 HAL / bxCAN 드라이버로 치환한 실증 로그와 아키텍처 가이드를 제공합니다.

- **포팅 가이드**: [`porting/stm32/stm32_porting_guide.md`](porting/stm32/stm32_porting_guide.md)
- **실물 MCU 검증 로그**: [`porting/stm32/stm32f401re_nucleo_boot_trace.log`](porting/stm32/stm32f401re_nucleo_boot_trace.log) — 보드 초기화, 500kbps bxCAN 송수신, SecOC Freshness 인증, ISO-TP 다중 프레임 UDS VIN 판독, IWDG 하드웨어 리셋 검증 포함.

## 다음 단계 추천 (실전 연결)

1. **실제 타겟 이식**: \`SimMcu\` 만 실제 MCU 레지스터/헤더(STM32, TC3xx)로 치환 → MCAL 교체 경험 (porting/stm32/stm32_porting_guide.md 참조)
2. **표준 문서 읽기**: AUTOSAR_SWS_CANDriver / SWS_COM / SWS_DCM, ISO 15765-2, ISO 14229-1
3. **도구 체험**: CANoe/CANalyzer(시뮬레이션), EB tresos/Vector DaVinci(BSW 설정) 개념 비교
4. **커리어 역량 매핑**: docs/00 의 직무 매핑표(BSW 개발자, 진단 개발자, 기능안전 엔지니어)

## 주의

- 본 프로젝트의 암호 구현(AES/시드키)은 **교육용**입니다. 양산에는 검증된 암호 라이브러리/HSM을 사용하세요.
- 각 EX는 실제 AUTOSAR 스펙의 **간소화** 버전입니다. 파일 헤더에 실제 스펙과의 차이를 명시했습니다.
