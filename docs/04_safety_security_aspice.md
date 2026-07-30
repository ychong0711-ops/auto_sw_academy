# 04. 기능안전 · 보안 · ASPICE

마지막 단계는 "동작하는 코드"를 "**틀려도 감지되고, 공격받아도 버티고, 증명 가능한** 코드"로 만드는 일입니다.

## 4-1. 세 가지 축의 차이부터

| 축 | 무엇으로부터 지키나 | 대표 기술/표준 | 본 저장소 |
|---|---|---|---|
| 기능안전(Safety) | **무작위 고장**(비트 반전, 프레임 유실, 프로그램 행) | ISO 26262, E2E 보호, Watchdog | `crc8.c`, `e2e_p01.c`, `wdg.c` |
| 보안(Security) | **악의적 공격**(위조, 재전송, 스니핑) | SecOC, 암호화, Secure Boot/Flash | `aes_mini.c`, `cmac.c`, `secoc.c` |
| 프로세스(ASPICE) | **사람의 실수**(추적 누락, 검증 누락) | Automotive SPICE SWE.1~6 | `ex4/aspice/` 산출물 세트 |

## 4-2. E2E Profile 1 — "신호가 무사히 왔나?" (안전)

AUTOSAR 의 E2E transformer 는 SWC 가 주고받는 데이터(특히 ASIL 등급 신호: 조향각, 브레이크 위치)에
제어 필드를 덧붙입니다:

```
byte0 하위니블: Alive Counter (0..14 순환)  → 프레임 유실/중복/순서이상 감지
byte1         : CRC-8 (DataID + 데이터)     → 데이터 오염·잘못된 발신자 감지
```

검증 결과는 단순 OK/FAIL 이 아니라 **상태값 분류**입니다: `INITIAL / OK / OKSOMELOST / REPEATED / WRONGSEQUENCE / ERROR`.
이유: 한 프레임 유실은 CAN 에서 흔한 일이므로 "허용 범위(MaxDeltaCounter) 내 유실"은 긴급 대응이 아니라 품질 지표로 다룹니다.

오류주입 테스트(FI-02)가 핵심: `틀린 데이터가 흘러도 안전 상태로 수렴하는가` 가 기능안전의 질문입니다.

> 참고: 현행 AUTOSAR 는 Profile 2/4/5/6/7/11/22 도 정의합니다(더 강한 CRC, 긴 데이터 필드 등). 본 구현의 CRC(SAE J1850)는 학습용 1바이트 버전입니다.

## 4-3. SecOC — "이 프레임, 진짜 그 ECU 가 볼낸 건가?" (보안)

CAN 은 인증이 없습니다(누구나 임의 ID 로 송신 가능). SecOC 가 추가하는 것:

```
[ 원본 PDU | 신선도(카운터 절단) | MAC = CMAC(PDU ID + 원본 + 신선도) ]
```

- **CMAC(AES-128)**: 키를 모르는 공격자는 MAC 위조 불가 → 무결성/인증
- **신선도(단조 카운터)**: 도둑이 녹음한 프레임 재전송(replay)한 조향 명령 → 거부
- 실 구현 포인트: 절단 길이(대역 vs 안전마진 트레이드오프), 신선도 동기화 전략, 검증 실패 카운터→침입 대응 로직

테스트 `test_ex4_secoc.c` 의 시나리오 = 공격 플레이북: 재전송(거부), 페이로드 위조(거부), 키 모름(거부).

> E2E 와의 차이 — 둘은 **상호 보완**:
> E2E 는 계산이 빠르고 "우연한 오류"를, SecOC 는 계산이 무겁고 "고의적 조작"을 방어합니다.
> 한 시그널에 둘 다 쓰는 구성도 실제로 존재합니다. (capstone 에서 나란히 시연)

## 4-4. Watchdog Manager — "프로그램이 계획대로 도는가"

- 각 Supervised Entity(러너블/메인함수)가 **체크포인트** 로 생존 보고
- 마감(Deadline) 초과 → Local EXPIRED → Global EXPIRED → **MCU 리셋 트리거**
- ISO 26262 의 프로그램 흐름 감시(Program flow monitoring) + 타임아웃 메커니즘

capstone PHASE 4 에서 통신 러너블을 60ms 멈춰 EXPIRED 를 유발합니다.

## 4-5. ASPICE — 코딩 이후가 진짜 업무

`ex4_safety_security/aspice/` 를 여세요:

| 파일 | ASPICE 관점 |
|---|---|
| `SWE_overview.md` | SWE.1~6 과 이 저장소 산출물의 매핑 — "프로세스가 코드에서 어떻게 보이나" |
| `traceability_matrix.csv` | SRS ↔ 함수 ↔ TC 3방향 추적 (CL2 심사 단골 지적사항) |
| `code_review_checklist.md` | 리뷰 "한 증거"를 남기는 도구 |
| `verification_report.md` | 검증 보고서 템플릿 + 오류주입 로그 형식 |

### 실습 과제
1. **SRS 추가 사이클**: `SRS_DCM_060: ResponsePending 지원` 요구사항을 새로 만들고 → UDS 구현 → TC 추가 → 매트릭스 갱신 → 검증 보고서 기재 까지 한 세트로
2. **리뷰 역할놀이**: 짝과 서로의 EX3 코드를 체크리스트로 리뷰하고 지적 3건 기록
3. **프로파일 확장**: E2E Profile 1 을 CRC-16 기반으로 확장해 Profile 2 느낌 재현

### 커리어 메모
- BSW 개발자: ②③ 이 일상, ④ 의 E2E/SecOC/WdgM 은 "왜 쓰는지"를 알면 차별화
- 진단 개발자: ③ 이 주력 + CANoe/DiVa 와 DIDS/ODX 데이터 모델 경험
- 기능안전 엔지니어: ④ + ISO 26262 파트(특히 Part 6: 소프트웨어) 문서를 읽고 본 저장소 메커니즘과 매핑해 보기
