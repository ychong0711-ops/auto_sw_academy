# 07. "툴 체인 경험"을 효율적으로 만드는 방법 (공짜 → 저비용 → 공식 경로)

> 대상: AUTOSAR 툴 체인 경험이 없는 주니어.
> 핵심 착각 금지: "Vector/DaVinci 정품 라이선스가 있어야 툴 경험이 쌓인다"는 틀렸다.
> 면접에서 검증되는 것은 **툴이 하는 일을 이해하고 재현/응용할 수 있는가**다.
> 그 기준에서 효율 순으로 정렬했다.

---

## 0. "툴 체인 경험"의 실체 3가지

공고의 "AUTOSAR 툴 경험"은 실제로 세 덩어리다:

| 체험 유형 | 대표 툴 | 면접에서 묻는 것 |
|---|---|---|
| ① **설정(Configuration)** | EB tresos Studio, Vector DaVinci Configurator | ARXML이 뭔가, 파라미터 트리, generate 를 누르면 무슨 파일이 나는지, 왜 설정과 코드를 분리하는가 |
| ② **측정/분석** | CANoe, CANalyzer (BUSMASTER/SavvyCAN 유사군) | Trace 읽기, DBC 디코딩, 재생(replay), 나머지 버스 시뮬레이션(restbus) 개념 |
| ③ **타겟 통합** | NXP/Infineon/ST 의 AUTOSAR MCAL·RTD, RTOS | 설정→생성→컴파일→보드 동작 한 루프, MCAL 플러그인 개념 |

전략: ①은 공식 평가판, ②은 공짜 대체재, ③은 약 10만 원 보드로 해결한다.

---

## 경로 A — EB tresos Studio 평가판 (공짜, 지금 시작) ★가성비 최상

Elektrobit 가 EB tresos Studio 평가판을 공식 제공한다:
"Download the EB tresos evaluation version for free and immediately start configuring software compliant to the AUTOSAR standard" [1](https://www.elektrobit.com/products/ecu/eb-tresos/evaluation-package/) [2](https://www.elektrobit.com/products/ecu/eb-tresos/studio/demonstrator/)

**이것으로 단련할 워크플로:**

1. 설치 후 샘플 프로젝트(데모) 열기 — Importer/Exporter, System Description Editor 구경 [2](https://www.elektrobit.com/products/ecu/eb-tresos/studio/demonstrator/)
2. BSW 모듈(Com, PduR, CanIf, Dcm...) 설정 트리를 본 저장소 ex2 의 Cfg 테이블과 **1:1 대조** — "우리가 손으로 쓴 테이블 = 툴이 생성할 데이터"임이 쭏감된다
3. 파라미터 하나 바꿔 코드 재생성 → diff 확인 → **"설정 기반 개발"을 몸으로 확증** (생성 코드를 손으로 고치지 않는 문화의 이유까지 설명 가능)

> 면접 문장: "EB tresos 평가판으로 파라미터→검증→코드 생성 워크플로를 수행했고, 같은 설정 데이터를 제 미니 ECU 의 Cfg 테이블로 직접 구현해 두 방식을 대조 학습했습니다."

## 경로 B — "나만의 툴 체인" 만들기 (공짜, 면접 특효) ✅ 이 저장소에 구현됨

본 저장소에는 이미 구현되어 있다 (유니 전부 표준 라이브러리):

```
config/ecu.json            ← 단일 소스 설정 (실무의 ARXML 역할)
tools/gen_cfg.py           ← 검증(validate) + 코드 생성기
└─> generated/
    ├─ Cfg_Ids.h           ID 매크로 단일 소스 (모든 모듈 헤더가 공유)
    ├─ Com_Cfg.c           Com 신호 설정 테이블   → ex2 Com 이 extern 으로 소비
    ├─ CanIf_Cfg.c         CanIf PDU 매핑        → ex2 CanIf 이 소비
    ├─ PduR_Cfg.c          라우팅 테이블          → ex2 PduR 이 소비
    ├─ Uds_Data.h/c        DID 카탈로그 + VIN/시리얼 정적 데이터 → ex3 UDS 가 소비
    ├─ VehicleNetwork.dbc  CANdb 포맷 (cantools/SavvyCAN/CANoe 에서 디코딩 가능)
    └─ SIGNAL_DOC.md       신호 문서 자동 생성
```

**실전 감각 포인트**: `make` 하면 생성이 자동 선행되고, ex2/ex3 모듈은 더 이상 자기 파일 안에
설정 테이블을 갖지 않는다 — `config/ecu.json` 만 바꾸고 `make` 하면 전 스택이 재구성된다.
(이전에는 테이블이 각 모듈 .c 안에 있었다 → 진화한 구조 그대로가 "리팩터링 경험" 스토리)

**검증(validation) 단계도 재현됐다** — 설정이 틀리면 생성 전에 멈춘다 (실 출력):

```
$ python3 tools/gen_cfg.py /tmp/ecu_broken.json /tmp/gen_broken
[gen_cfg][오류] VehicleState.BatteryVoltage: bit 64 가 DLC(8) 밖 (start=60, len=12)
[gen_cfg] 검증 실패 — 생성 중단 (툴의 validation 단계)   exit=1
```

**신호 추가 데모** (설정에 `GearPos`(start16, 3bit) 추가 후 재생성):

```
[gen_cfg] 검증 통과 + 생성 완료
generated/Cfg_Ids.h:  #define COM_SIG_GearPos (1u)
generated/VehicleNetwork.dbc: SG_ GearPos : 16|3@1+ (1,0) [0|7] "" BCM
```

이것이 강력한 이유: Vector/EB 툴이 하는 일(모델→검증→코드생성)의 **축소 재현**이자,
"왜 ARXML 이 있는지, 생성 코드를 왜 손대지 않는지"를 답변으로 설명할 수 있게 해준다.
실험 과제: EngineRpm 프레임(CAN 0x101)을 config 에만 추가해 재생성 → 바뀐 파일 목록을 diff로 확인해 보기.

## 경로 C — CAN 분석 툴 워크플로 (✅ 저장소에 구현 완료 + 공짜 확장)

CANoe/CANalyzer 의 기술은 "트레이스 읽기/신호 디코딩/재생/자동화"다.

**이 저장소 안 구현 (설치물 0건, PC 만으로 재현)**:

- `ex3_comm_diag/src/vcan.c` 의 버스 모니터 훅 — `VCan_TraceOpen()` 이 모든 송신 프레임을 타임스탬프와 함께 기록 (CANoe 로거 역할)
- `./build/capstone_demo --trace logs/capstone_trace.log` — 데모 실행 = 로그 수집
- `tools/decode_trace.py` — 경로 B 가 생성한 `generated/VehicleNetwork.dbc` 로 트레이스 신호 해석 (Trace 창 역할).
  **엔진 2종**: cantools(실무 표준) / stdlib 내장 파서(무설치 환경용 자동 전환, `--stdlib` 로 강제)
- `make trace-check` — 전 과정 자동 검증: 두 엔진 출력 일치 diff + 기대 신호 값(12.8 V) 디코드 + 메시지 집계
- CI(`.github/workflows/ci.yml`)에도 동일 단계 포함 — clone 만으로 재현 가능

실제 출력:

```
[      0 ms] 0x100 VehicleState (8B, 송신:ECU)  80 10 00 00 00 00 00 00
                       BatteryVoltage = 12.8 V   (raw=128, [0..409.5] V)
[      9 ms] 0x7E0 RAW  02 10 03   (DBC 미등록 ID)  <- ISO-TP/UDS 진단 추정
```

부가 효과(실제 일어난 일): 디코더를 붙이자마자 생성기의 바이트오더 기재가 cantools 규약(`@0`=Motorola / `@1`=Intel)과
어긋남이 두 엔진 교차검증으로 즉시 드러났고, cantools 의 겹침 검사가 독립 린터 역할을 했다.
**"내가 만든 툴 출력을 표준 툴로 린트한다"** — 이것이 툴 체인 엔지니어의 일상이다.

**SocketCAN 으로 확장 (선택, 실물 버스까지 가려면)**:

| 일 | CANoe | 공짜 실행 |
|---|---|---|
| 프레임 수신/디코딩 | Trace + DBC | Linux **SocketCAN(vcan)** + `candump` + `python-can`, `cantools decode` (경로 B 의 .dbc 재사용!) |
| GUI 분석 | CANalyzer 창들 | **SavvyCAN**(오픈소스) — DBC 불러오기, 신호 그래프, 재생 |
| 실물 버스 접속 | Vector VN16xx (수백만 원) | **CANable/USB2CAN 동글(약 $30~60, slcan)** |
| 측정 자동화 | CAPL | Python 스크립트(cantools) — "CAPL 대신 Python" 은 실무에서도 통용 |

본 저장소의 vcan 개념이 Linux 의 가상 CAN(vcan)과 이름까지 일치 → capstone 데모를 SocketCAN 위에 올리면 도구 흐름이 그대로 이어진다.

## 경로 D — 실물 AUTOSAR MCAL 보드 (약 10만 원, 결정타)

**NXP S32K144EVB + S32K1 RTD(Real-Time Drivers)**: NXP 공식 안내상 RTD 는 제품 구매에 포함되며 추가 라이선스 비용이 없고, **EB tresos Studio(AUTOSAR) 및 S32CT(비-AUTOSAR) 두 설정기를 모두 지원** [3](https://www.nxp.com/design/design-center/software/automotive-software-and-tools/real-time-drivers-rtd:AUTOMOTIVE-RTD). S32K1 제품 페이지도 "Production-grade S32 SDK: SPICE Level 3, MISRA tested / NXP AUTOSAR MCAL(ISO 26262, QM compliant)" 명시 [4](https://www.nxp.com/products/S32K1).

- 저장소의 `SimMcu` 를 RTD MCAL 호출로 교체 = **면접에서 "설정→생성→타겟 빌드→동작" 한 루프 완성**
- 커뮤니티 가이드 존재: S32K1 RTD + EB tresos 평가판 설치부터 프로젝트까지의 오픈 부트캠프 [5](https://github.com/renatosoriano/AUTOSAR-MCAL-Embedded-Upskilling-Bootcamp), MCAL 생성 코드 배치 샘플 [6](https://github.com/marshallma21/AUTOSAR_SampleProject_S32K144)
- 한국 구매 약 8~13만 원 대(S32K144EVB-Q100). 예산이 없으면 A~C 만으로도 면접 문장은 완성됨

## 경로 E — 정식 툴 접촉 (선택, 상황에 따라)

- **Vector**: 공식 제품안내서에 "A demo version is available on the internet for CANoe DE" 명시 [7](https://cdn.vector.com/cms/content/products/canoe/canoe/docs/Product%20Informations/CANoe_ProductInformation_EN.pdf). 취득 경로는 시기에 따라 바뀌므로 Vector 코리아 영업/교육 문의 + 학교 라이선스 경유가 현실적(커뮤니티 증언: 대학 라이선스/영업 요청으로 취득 사례 [8](https://www.reddit.com/r/CarHacking/comments/j2hr8z/vector_canoe_demo_version/))
- **Vector Korea / ETAS Korea 정규 교육**: AUTOSAR·CANoe 단기 과정(유료). 취업 직전 "정식 툴 체험" 용도로 효율적
- **국비/기업 연계 과정**: 자동차 SW 인력양성 과정은 라이선스 툴 실습이 포함되는 경우가 많음 — 지원 전 커리큘럼에 Vector/EB 명시 여부 확인

## 현실 주석

**입사 후 OJT 가 정석인 부분이 있다.** 상용 툴은 라이선스가 수천만 원 단위라 회사들도 주니어에게 "툴 장인"을 기대하지 않는다.
공고의 툴 키워드는 "처음 들어보는 사람"을 거르는 필터에 가깝다. 목표는 숙련이 아니라 **개념 이해 + 재현 증거 + 한 번 만져봄** 의 3단 증거이고, 그것은 위 A~D 로 완성된다.

---

## 4주 실행 플랜과 산출물

| 주 | 행동 | GitHub/이력서에 남는 것 |
|---|---|---|
| 1 | 경로 B: gen_cfg.py + DBC export, README 에 before/after diff 캡처 | "설정 생성 툴 직접 제작" |
| 2 | 경로 C: ✅ 저장소 내 구현 완료(vcan 트레이스 + decode_trace 이중 엔진 + trace-check). 확장: SocketCAN/SavvyCAN 캡처 | "CAN 계측/디코딩 자동화(Python)" |
| 3 | 경로 A: EB tresos 평가판 설치, 모듈 설정 ↔ 본 저장소 Cfg 대조 노트 | 대조표(블로그/문서) 1부 |
| 4 | (선택) 경로 D: S32K144EVB + RTD, SimMcu 치환 또는 병행 브랜치 | "AUTOSAR MCAL 타겟 빌드 완료" 로그/사진 |

### 완성 후 이력서 문장 (예)

> - AUTOSAR 설정 워크플로(모델→검증→코드 생성)를 Python 생성기로 축소 재현: YAML 설정에서 Com/CanIf/PduR Cfg 코드와 DBC 를 동시 생성 — EB tresos Studio 평가판과 대조 검증
> - CANoe 대체 계측 파이프라인 직접 구현: 가상 버스 트레이스 로깅 훅(C) + 자체 생성 DBC 로 신호 디코딩하는 Python 툴 — cantools/stdlib 이중 엔진 교차검증 및 GitHub Actions CI 통합
> - NXP S32K144EVB + RTD(AUTOSAR MCAL, ISO 26262/QM) 타겟 빌드 및 SimMcu 계층 치환 이식 (진행 시)

[1]: EB tresos 평가 패키지 https://www.elektrobit.com/products/ecu/eb-tresos/evaluation-package/
[2]: EB tresos Studio demonstrator https://www.elektrobit.com/products/ecu/eb-tresos/studio/demonstrator/
[3]: NXP Real-Time Drivers — 추가 라이선스비 없음, EB tresos 지원 https://www.nxp.com/design/design-center/software/automotive-software-and-tools/real-time-drivers-rtd:AUTOMOTIVE-RTD
[4]: NXP S32K1 제품 페이지 https://www.nxp.com/products/S32K1
[5]: 커뮤니티 AUTOSAR MCAL 부트캠프(S32K1 RTD + EB tresos) https://github.com/renatosoriano/AUTOSAR-MCAL-Embedded-Upskilling-Bootcamp
[6]: S32K144 MCAL 샘플 프로젝트 https://github.com/marshallma21/AUTOSAR_SampleProject_S32K144
[7]: CANoe 제품 안내서(데모 버전 언급) https://cdn.vector.com/cms/content/products/canoe/canoe/docs/Product%20Informations/CANoe_ProductInformation_EN.pdf
[8]: CANoe 데모 취득 커뮤니티 경험담 https://www.reddit.com/r/CarHacking/comments/j2hr8z/vector_canoe_demo_version/
