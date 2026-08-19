# 면접 대비 자료 (Auto SW Academy)

## 예상 면접 질문 + 모범 답변

### 1. AUTOSAR 계층을 나눈 이유는 무엇인가요?
**답변 포인트:** 하드웨어 독립성, SWC 재사용, BSW 교체 용이성, 계층별 테스트 용이성
**답변:** "이 프로젝트에서 AUTOSAR MCAL/BSW/RTE/SWC 4계층을 나눈 이유는 각 계층이 명확한 책임을 가지도록 하기 위함입니다. MCAL은 하드웨어 독립적인 드라이버를 제공하고, BSW는 통신 스택(RTE, Com, CanIf 등)을 담당하며, RTE는 SWC와 통신을 추상화하고, SWC는 애플리케이션 로직을 구현합니다. 이러한 분리는 각 계층을 독립적으로 테스트하고, 하드웨어 변경 시 MCAL만 교체할 수 있게 합니다. 예를 들어, SimMcu를 STM32 HAL로 교체할 때 MCAL만 변경하면 됩니다."

### 2. E2E Profile 1이 뭔가요?
**답변 포인트:** CRC-8 + 카운터, ISO 26262 대응, 통신 오류 탐지
**답변:** "E2E Profile 1은 AUTOSAR의 오류 감지 프로파일로, CRC-8 계산과 8비트 카운터를 사용하여 데이터 무결성을 보장합니다. 각 프레임에 카운터를 포함시켜 순서 및 누락 오류를 탐지하며, ASIL D 요구사항을 충족합니다. 이 프로젝트에서는 `e2e_p01.c/h`로 구현했습니다."

### 3. SecOC 재전송 방어 방식은?
**답변 포인트:** 인증, 신선도, MAC, 재전송 탐지, 거부
**답변:** "SecOC는 SecOC 프레임에 인증(AES-CMAC), 신선도(3바이트), MAC(3바이트)을 포함시킵니다. 수신 측은 MAC을 검증하고, 신선도 값을 확인하며, 동일한 프레임 재전송을 거부합니다. 구현은 `secoc.c/h`에 있으며, `cmac.c`를 사용하여 AES-CMAC을 계산합니다."

### 4. UDS 0x27 보안 접근 구현 방식은?
**답변 포인트:** 시드-키 챌린지-응답, AES 암호화, 세션/보안
**답변:** "UDS 0x27는 시드-키 챌린지-응답 프로토콜을 사용합니다. 진단기는 0x27 0x01로 시드 요청, ECU는 16바이트 시드 생성, 진단기는 AES-CMAC으로 키 계산, ECU는 0x27 0x02로 키 전송. `uds_server.c/h`에 구현되었으며, `aes_mini.c`를 사용합니다."

### 5. WdgM이 통신 스택 행업 검출 방식은?
**답변 포인트:** 워치독 타이머, 통신 스택 상태, MCU 리셋
**답변:** "WdgM(Watchdog Manager)은 통신 스택의 상태를 모니터링합니다. 각 SWC가 주기적으로 '하트비트'를 전송하지 않으면, WdgM이 OSEK OS의 워치독 타이머를 트리거하여 MCU를 리셋합니다. 이를 통해 통신 스택이 행업된 것을 감지합니다. `wdg.c/h`에 구현되었습니다."

### 6. DBC 디코드 이중 검증 방식은?
**답변 포인트:** cantools + stdlib, 비교, 신뢰성
**답변:** "버스 트레이스 디코드는 두 가지 엔진으로 검증됩니다: cantools(업계 표준)와 stdlib 내장 파서. `tools/decode_trace.py`가 두 엔진으로 동일한 트레이스를 디코드하고, 출력을 비교하여 일치 여부를 확인합니다. 불일치 시 빌드가 실패하여 데이터 신뢰성을 보장합니다."

### 7. SimMcu를 STM32로 포팅한 방식은?
**답변 포인트:** MCAL 교체, HAL 매핑, 포팅 가이드
**답변:** "포팅은 `porting/stm32/` 디렉토리에서 이루어집니다. SimMcu의 가상 MCU 레지스터를 STM32 HAL 함수 호출로 매핑하고, `SimMcu_stm32.c/h`로 구현했습니다. MCAL만 변경하여 전체 SW 스택을 유지합니다. `porting/stm32/stm32_porting_guide.md`에 상세히 설명되어 있습니다."

### 8. 생성 코드 설계 방식은?
**답변 포인트:** 결정성, 재현성, 템플릿, gen_cfg.py
**답변:** "생성 코드는 `tools/gen_cfg.py`로 생성됩니다. 템플릿 기반 설계로, 동일한 ECU 설정에서 매번 동일한 `Com_Cfg.c`, `CanIf_Cfg.c`, `PduR_Cfg.c`를 생성합니다. 이는 수동 오류 가능성을 제거하고, 구성 변경 시 전체 스택을 재생성할 수 있게 합니다."

### 9. CI 파이프라인이 3-OS를 지원하는 방식은?
**답변 포인트:** GitHub Actions, Windows/Ubuntu/macOS, Python 호환
**답변:** "CI는 GitHub Actions로 3개 OS(Windows, Ubuntu, macOS)에서 실행됩니다. 각 OS는 Python, C, C++ 컴파일러를 설치하고, `make check`를 실행하여 18개 테스트 스위트를 검증합니다. macOS에서는 PEP 668을 위해 `--break-system-packages`를 사용하고, Windows에서는 UTF-8 인코딩을 보장합니다."

### 10. ASPICE 추적성 방식은?
**답변 포인트:** SWE 요구사항, 함수, TC, 매트릭스
**답변:** "ASPICE 추적성은 `ex4_safety_security/aspice/traceability_matrix.csv`로 관리됩니다. 각 SRS 요구사항(SRS_XXX_XXX)은 구현 코드(C 파일 주석)와 테스트 케이스(TC_XXX_XXX)에 매핑됩니다. 이를 통해 요구사항에서 구현, 검증까지 전 과정을 추적할 수 있습니다."

## 면접 팁

### 1. 프로젝트 구조 설명
```
[1단계] 임베디드 C/C++ → [2단계] AUTOSAR MCAL/BSW/RTE/SWC → [3단계] CAN/ISO-TP/UDS → [4단계] SecOC/E2E/WdgM
```

### 2. 기술 스택 강조
- **언어:** C99, C++11, Python3
- **표준:** AUTOSAR R19-03, ISO 26262, ISO 15765-2, ISO 14229-1
- **도구:** Git, GitHub Actions, CMake, Makefile, pytest, cantools
- **환경:** Windows, Ubuntu, macOS (PEP 668, UTF-8)

### 3. 프로젝트 차별성 강조
- 일반적인 임베디드 프로젝트와 달리 **완전한 AUTOSAR 스택 구현**
- **실무 표준** ISO 프로토콜(CAN, ISO-TP, UDS) 구현
- **기능안전** SecOC, E2E, WdgM으로 ISO 26262 대응
- **DevOps** 3-OS CI 파이프라인, DBC 디코드 이중 검증

### 4. 학습 과정 강조
- **단계별 학습:** 임베디드 C → AUTOSAR → 통신 → 안전
- **실무 중심:** 이론이 아닌 실제 코드 구현
- **프로젝트 기반:** 각 단계는 이전 단계 위에 쌓임

### 5. 커리어 목표 연결
- **목표:** AUTOSAR MCAL/BSW/RTE/SWC 개발자
- **독일 시장:** Tier-1(Bosch, Continental, ZF) 타겟
- **자격증:** ISO 26262, AUTOSAR, UDS 진단

## 예상 후속 질문

### "이 프로젝트의 한계는 무엇인가요?"
**답변:** "이 프로젝트는 AUTOSAR 스펙의 간소화 버전입니다. 실제 AUTOSAR OS(RTE + OS + Com + CanIf + PduR)는 SimMcu로 시뮬레이션되며, 실제 AUTOSAR OS는 구현되지 않았습니다. 또한, 암호 구현(AES/시드키)은 교육용이며, 양산에는 검증된 라이브러리를 사용합니다."

### "실제 AUTOSAR 프로젝트와 어떻게 다른가요?"
**답변:** "실제 AUTOSAR 프로젝트는 ECU에서 실행되며, R19-03 스펙을 완전히 구현하고, RTE + OS + MCAL + BSW + SWC를 포함합니다. 이 프로젝트는 PC에서 실행되는 교육용 간소화 버전으로, 실제 AUTOSAR OS는 SimMcu로 시뮬레이션됩니다."

### "포팅 경험이 있나요?"
**답변:** "예, `porting/stm32/`에서 SimMcu를 STM32 HAL로 포팅한 경험이 있습니다. MCAL만 변경하여 전체 SW 스택을 유지하는 방법을 구현했습니다. `porting/stm32/stm32_porting_guide.md`에 상세히 설명되어 있습니다."

## 면접 준비 체크리스트

- [ ] 프로젝트 구조(4계층) 설명
- [ ] AUTOSAR 계층별 역할 설명
- [ ] ISO 프로토콜(CAN, ISO-TP, UDS) 구현 방식 설명
- [ ] 기능안전(SecOC, E2E, WdgM) 구현 방식 설명
- [ ] DBC 디코드 이중 검증 방식 설명
- [ ] 포팅 경험(SimMcu → STM32) 설명
- [ ] 생성 코드 설계 방식 설명
- [ ] CI 파이프라인(3-OS) 설명
- [ ] ASPICE 추적성 설명
- [ ] 프로젝트 한계점 설명

## 면접 후속 조치

### 1. 면접 후 감사 이메일
```
제목: Auto SW Academy 지원 관련 문의

[회사명] 면접 감사합니다. AUTOSAR 역량 체인 프로젝트에 대한 깊은 관심을 주셔서 감사합니다.

주요 기술 스택:
- AUTOSAR MCAL/BSW/RTE/SWC 4계층 구현
- ISO 프로토콜(CAN, ISO-TP, UDS) 표준 준수
- 기능안전(SecOC, E2E, WdgM) 구현
- 3-OS CI 파이프라인, DBC 디코드 이중 검증

추가 자료:
- 상세한 프로젝트 구조: docs/00_big_picture.md
- 기술 스택: README.md (영문화 예정)
- 포팅 가이드: porting/stm32/stm32_porting_guide.md

후속 조치:
- [ ] 코드 리뷰 일정 협의
- [ ] 추가 기술 과제 논의

감사합니다.
[이름]
```

### 2. 면접 후 블로그 작성
- 경험, 배운 점, 프로젝트 차별성 강조
- LinkedIn에 게시하여 네트워킹 활용

### 3. 면접 피드백 수집
- 강점/약점, 개선점, 다음 단계 논의
- 다음 지원 시 반영

## 마무리 말

이 프로젝트는 **독일 자동차 시장**에 특화된 AUTOSAR 포트폴리오로, 일반적인 임베디드 프로젝트와 차별화됩니다. 면접 시 **실무 경험**과 **독일 시장 적합성**을 강조하세요.

**핵심 메시지:** "저는 실제 AUTOSAR 스택을 구현한 개발자로, Tier-1에 입사해서 1년차에 할 일을 미리 해본 경험을 가지고 있습니다."

이 자료가 면접 성공에 도움이 되길 바랍니다! 🚀
