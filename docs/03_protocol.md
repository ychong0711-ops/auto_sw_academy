# 03. 자동차 통신/진단 프로토콜

## 3-1. CAN 신호 코덱 (`can_signal.c`)

CAN 프레임은 8바이트의 "비트 배열". DBC 파일은 그 안에서 **어떤 신호가 몇 번 비트부터 몇 비트인지**를 정의합니다.

| 개념 | Intel(little) | Motorola(big) |
|---|---|---|
| start bit 의 의미 | LSB 의 위치 | MSB 의 위치 |
| 다음 비트 | start+1, +2, ... | 바이트 내에서 아래로, 경계에서 다음 바이트 MSB 로 (`pos%8==0 ? pos+15 : pos-1`) |
| 예 (12bit, 0xABC) | start 0 → `BC 0A` | start 7 → `AB C0` |

물리량 변환까지가 한 세트: `phys = raw × factor + offset`
(예: 차속 factor 0.5, offset -40 → raw 200 = 60.0 kph)

**연습**: DBC 에서 흔한 "모토로라 start bit=23, len=16" 을 손으로 비트계산하고 테스트로 확인하세요.

## 3-2. ISO-TP (`isotp.c`, ISO 15765-2)

8바이트 CAN 위에서 최대 4095바이트 진단 메시지를 실어 나르는 프로토콜.

| PCI (첫 니블) | 이름 | 역할 |
|---|---|---|
| 0x0N | SF 단일 프레임 | N=길이(≤7), 이것만으로 완결 |
| 0x1N | FF 첫 프레임 | 전체 길이 선언(12bit) + 데이터 6B |
| 0x3N | FC 흐름제어 | 수신측이: 0=계속(CTS) 1=대기(WT) 2=포기(OVFL), BS/STmin 지정 |
| 0x2N | CF 연속 프레임 | N=시퀀스 번호(0..15) + 데이터 7B |

```
송신측 ──FF(길이=30)──▶ 수신측
송신측 ◀──FC(CTS, STmin=5)──        "5ms 간격으로 볼네"
송신측 ──CF(SN=1)──(5ms)──CF(SN=2)──(5ms)──CF(SN=3)──CF(SN=4)──▶ 완성
```

본 구현이 다루는 방어 로직(테스트가 존재함):
- CF 시퀀스 불일치 → abort + 카운터 (TC_TP_030)
- FC Overflow → 송신 abort (TC_TP_031)
- STmin 페이싱 (TC_TP_040), N_Bs/N_Cr 타임아웃

**연습**: FC 의 BS(Block Size) 가 0이 아닐 때 BS 개 전송 후 FC 를 다시 기다리는 로직을 추가해 보세요.

## 3-3. UDS (`uds_server.c`, ISO 14229-1)

UDS = ECU 와 진단기의 "대화 규약". AUTOSAR 의 Dcm 이 담당합니다.

### 지원 서비스 (본 구현)
| SID | 서비스 | 여기서의 역할 |
|---|---|---|
| 0x10 | DiagnosticSessionControl | default(1)/programming(2)/extended(3) |
| 0x11 | ECUReset | hardReset 후 플래그 |
| 0x14 | ClearDiagnosticInformation | DTC 전체 삭제 (확장+보안 필요) |
| 0x19 | ReadDTCInformation | 상태 마스크별/전체 DTC 보고 |
| 0x22 | ReadDataByIdentifier | VIN(0xF190), 세션(0xF186), 속도(0x1234)… |
| 0x27 | SecurityAccess | seed&key, 3회 실패→지연(0x36/0x37) |
| 0x2E | WriteDataByIdentifier | 확장+보안 세션 한정 |
| 0x31 | RoutineControl | 자기진단(차량 정지 조건 NRC 0x22!) |
| 0x3E | TesterPresent | S3 세션 유지 |

### NRC 치트시트 (면접 단골)
| NRC | 의미 | 예시 상황 |
|---|---|---|
| 0x11 | serviceNotSupported | 없는 SID |
| 0x12 | subFunctionNotSupported | 없는 서브펑션 |
| 0x13 | incorrectMessageLength | 길이 부족 |
| 0x22 | conditionsNotCorrect | 주행 중에 자기진단 요청 |
| 0x31 | requestOutOfRange | 없는 DID |
| 0x33 | securityAccessDenied | 잠금에서 쓰기 시도 |
| 0x35 | invalidKey | 키 틀림 |
| 0x36 | exceededAttempts | 3회 실패 |
| 0x37 | requiredTimeDelayNotExpired | 벌칙 시간 내 재시도 |
| 0x7F | serviceNotSupportedInActiveSession | 디폴트에서 ClearDTC |

### 알아야 할 미묘한 규정 (구현/테스트에 반영됨)
- **긍정응답 억제 비트**: 서브펑션 최상위 비트가 1이면 긍정응답 생략 → `10 83` 은 무응답 (TC_UDS_020)
- **requestSeed in unlocked state** → seed = 0x0000 반환 (TC_UDS_030)
- **세션 전환 시 보안 상태 리셋**, **S3 타임아웃 시 디폴트 복귀** (TC_UDS_060)
- 보안 지연(10s) > S3(5s) 이면 **TesterPresent 로 세션을 유지하며 기다려야** 한다 — TC_UDS_031 의 주석 참조

**연습**:
1. ResponsePending(NRC 0x78) 패턴 추가: 루틴 처리가 오래 걸릴 때 `7F 31 78` 로 먼저 응답
2. 새 DID 추가: 0xF187 (SW 부품번호, 읽기 전용)
3. 진단기(유저 클라이언트) 클래스 작성: capstone 의 `tester_request` 를 재사용해 캘리브레이션 스크립트 흉내
