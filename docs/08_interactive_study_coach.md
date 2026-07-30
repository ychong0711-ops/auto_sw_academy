# 08. 대화형 학습 코치 (`tools/study_trainer.py`) 활용 가이드

## 1. 개요

> **4주 마스터 로드맵 + ASPICE 30초 추적성 탐험 + 오류 주입 실증 + 6대 졸업 자가 진단 퀴즈**를
> 터미널에서 대화형 CLI 프로그램으로 실천할 수 있는 통합 교육 코치입니다.

### 실행 방법

```bash
# 1) 대화형 메뉴 모드 시작 (가장 추천)
make study

# 2) CLI 직접 실행
python3 tools/study_trainer.py                  # 대화형 메뉴
python3 tools/study_trainer.py --week 2         # 2주차 AUTOSAR MiniECU 가이드 및 단위 테스트 라이브 실행
python3 tools/study_trainer.py --trace SRS_COM_010  # ASPICE 요구사항 3방향 라이브 추적
python3 tools/study_trainer.py --lab 1          # SecOC Replay Attack 차단 방어 실증 랩
python3 tools/study_trainer.py --quiz           # 6대 핵심 졸업 자가 진단 퀴즈 (채점 모드)
python3 tools/study_trainer.py --demo-all       # 전체 학습법 기능 자동화 시연 (비대화형/CI 용도)
```

---

## 2. 5대 핵심 모듈 기능

### ① [1번 메뉴] 4주 실전 학습 로드맵 모드 (`--week 1~4`)
- 1~4주차별 핵심 학습 목표, 읽어야 할 필독 소스 파일 목록, 면접 핵심 체크리스트를 표시합니다.
- 해당 주차에 대응하는 개별 바이너리(예: `build/ex2_signal_flow`, `build/ex2_com` 등)를 순차 실행하여 **검증 결과를 실시간으로 확인**합니다.

### ② [2번 메뉴] ASPICE SWE.4 3방향 추적성 탐험기 (`--trace <ID>`)
- `ex4_safety_security/aspice/traceability_matrix.csv`를 기반으로 `SRS_ID` 또는 `TC_ID`를 검색합니다.
- 1초 안에 다음 3방향 증거를 묶어 출력합니다:
  1. **요구사항 (SRS)**: ASPICE 태그 및 설명
  2. **구현 소스 증거**: 해당 `[SRS_XXX_NNN]` 주석이 위치한 소스 코드 및 라인 번호
  3. **검증 테스트 증거**: 해당 테스트 케이스(`TC_XXX_NNN`)가 위치한 테스트 소스 코드 및 검증 결과

### ③ [3번 메뉴] 의도적 오류 주입 및 방어 실증 랩 (`--lab 1~4`)
- 차량 SW 안전/보안의 본질인 방어 기제를 시연합니다:
  - **Lab 1**: SecOC Replay Attack (재전송 공격) 차단 실증 (`build/ex4_secoc`)
  - **Lab 2**: E2E Profile 1 데이터 오염 (CRC 불일치) 감지 실증 (`build/ex4_crc_e2e`)
  - **Lab 3**: WdgM 하드웨어 Watchdog 통신 행업 리셋 실증 (`build/ex4_wdg`)
  - **Lab 4**: UDS SecurityAccess Seed-Key 3회 실패 누적 방어 실증 (`build/ex3_uds`)

### ④ [4번 메뉴] 6대 핵심 졸업 자가 진단 퀴즈 (`--quiz`)
- 면접 및 현업 필수 역량 6제(AUTOSAR 계층 분리, Intel vs Motorola 엔디안, UDS Unlocked Seed, ISO-TP STmin 페이싱, E2E vs SecOC 이중화, MCAL STM32 이식성)를 대화형 객관식으로 출제합니다.
- 선택한 답에 대해 즉시 정답 및 상세 해설을 제공하며 최종 스코어 80% 이상 시 졸업 자격(PASS)을 판정합니다.

### ⑤ [5번 메뉴] 전체 시스템 검증 현황 조회
- `make coverage`를 즉시 호출하여 전체 소스코드 92.98% 구조적 커버리지 달성 여부와 테스트 18종 PASS 상태를 집계합니다.
