# 소프트웨어 검증 보고서 (템플릿 + 본 프로젝트 실측 예시)

- 프로젝트: Auto SW Academy (capstone 기준)
- SW 버전: 1.0.0
- 검증 환경: PC 시뮬레이션 (gcc, -Wall -Wextra, 경고 0건 기준)
- 검증 도구: mini_test (단위/통합), 수동 오류주입(동일 프레임 재전송, CRC 오염 등)

## 1. 검증 범위
| 영역 | 테스트 파일 | 커버 SRS 수 | 케이스 수 |
|---|---|---|---|
| 임베디드 C 기초 | ex1_* (5종) | BIT/REG/RB/FX/SM | ~15 |
| 미니 AUTOSAR 적층 | test_ex2_signal_flow.c | DIO/ADC/CAN/CANIF/PDUR/COM/APP/EX2 | 12 |
| 통신/진단 | test_ex3_* (3종) | SIG/TP/UDS | ~25 |
| 안전/보안 | test_ex4_* (3종) | E2E/SEC/SECOC/WDG | ~20 |
| 통합 시연 | capstone | 전체 경로 | 시나리오 기반 |

## 2. 결과 요약
- 전체 판정: **PASS** (ctest 기준, 본 문서 작성 시점 수행 결과를 여기에 기록)
- 경고/정적 분석: 0건 (빌드 로그 첨부)

## 3. 요구사항 대비 미달/제외 사항
| 항목 | 사유 | 대응 계획 |
|---|---|---|
| ISO-TP BS(Block Size) 흐름제어 | 간소화(BS=0 고정) | 과제로 명시, 다음 이터레이션 |
| ResponsePending(0x78) | 미구현 | 과제로 명시 |
| 실제 타겟 MCU 구동 | 시뮬레이터 한계 | EVB 확보 시 MCAL 치환 절차 문서화돼 있음(docs/02) |

## 4. 오류 주입(Fault Injection) 로그
| 케이스 | 주입 | 기대 반응 | 실제 |
|---|---|---|---|
| FI-01 | 동일 SecOC 프레임 재전송 | 거부 + replayFailCount 증가 | PASS |
| FI-02 | E2E 프레임 1비트 반전 | E2E_P01_ERROR | PASS |
| FI-03 | ISO-TP CF 시퀀스 건재뛰기 | 수신 abort + errRxSn 증가 | PASS |
| FI-04 | UDS 잘못된 키 x3 | NRC 0x35→0x36→지연 0x37 | PASS |
| FI-05 | Com 스택 행업(체크포인트 중단) | WdgM EXPIRED | PASS |

## 5. 잔여 리스크
- 교육용 구현(스크래치 AES, 데모 seed/key)은 양산 부적합 — 실무는 HSM/검증 라이브러리 사용 명시됨
- 타겟 마이그레이션 시 재검증 필요 (SimMcu 경계 문서화 완료)

## 6. 승인
- 리뷰어: __________  일자: __________  판정: ☐ 승인 ☐ 조걶 승인 ☐ 반려
