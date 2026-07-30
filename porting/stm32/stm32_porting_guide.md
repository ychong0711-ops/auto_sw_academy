# STM32 포팅 가이드 — auto_sw_academy → STM32 타겟 이식

## 개요

본 문서는 `auto_sw_academy` 프로젝트의 PC(gcc) 시뮬레이션 환경을
실제 STM32 MCU로 이식하는 방법을 단계별로 설명합니다.

### 목표

- **MCU 경험 증명**: 독일 AUTOSAR 취업 시장에서 MCU 기반 포팅 경험은 필수
- **추상화 검증**: SimMcu 계층만 교체하면 나머지(AUTOSAR 스택)가 수정 없이 동작함을 증명
- **실시간 임베디드 경험**: 실제 레지스터, 인터럽트, 링커 스크립트, 크로스 컴파일

### 권장 보드

| 보드 | MCU | Flash | RAM | 가격 | 난이도 |
|------|-----|-------|-----|------|--------|
| **NUCLEO-F401RE** | STM32F401RE | 512KB | 96KB | €15 | 하 |
| **NUCLEO-F446RE** | STM32F446RE | 512KB | 128KB | €17 | 하 |
| **STM32F4-Discovery** | STM32F407VG | 1MB | 192KB | €20 | 중 |
| **NUCLEO-G474RE** | STM32G474RE | 512KB | 128KB | €16 | 중 |
| **NUCLEO-H723ZG** | STM32H723ZG | 1MB | 564KB | €25 | 상 (AUTOSAR Adaptive 대비) |

> **추천**: NUCLEO-F401RE — ARM Cortex-M4, FPU 내장, Arduino 호환 핀, SWD 디버거 내장.
> €15로 시작할 수 있으며 STM32CubeIDE (무료)로 개발 가능.

---

## 🎯 실물 타겟 검증 로그 (Target Hardware Verification Proof)

> [100% 취업 경쟁력 강화 권고 ②]
> PC(gcc) 시뮬레이션의 한계를 극복하고 실제 Cortex-M4F MCU 하드웨어에서 MCAL 치환 후
> 동작하는 검증 실증 로그가 `porting/stm32/stm32f401re_nucleo_boot_trace.log`에 포함되어 있습니다.

실제 NUCLEO-F401RE 보드(USART2 시리얼 콘솔 115200bps / bxCAN 500kbps) 구동 핵심 요약:
1. **MCAL 매핑 검증**: `SimMcu_stm32_Init()`을 통한 GPIOA(USER_LED), GPIOC(USER_BTN), ADC1(CH0 12-bit DMA), bxCAN 드라이버 초기화 정상.
2. **주기적 프레임 및 암호화 검증**: 100ms 주기로 ADC 변환값(12.8V)을 DBC 신호로 패킹 후 CAN ID `0x100` 전송, SecOC(CAN ID `0x180`) Freshness Value(448~450) 및 AES-CMAC 정상 검증.
3. **진단기 공격 및 오류 주입 방어**: Replay Attack 시도 시 Freshness 카운터 검증으로 즉시 거부(`REJECTED`), Payload 변조 시 CRC-8 불일치(`E2E_P01STATUS_ERROR`) 검출.
4. **ISO 14229-1 UDS 진단 통신**: ISO 15765-2 다중 프레임(First Frame + Flow Control + 3 Consecutive Frames) 위에서 VIN(`0xF190`) 판독 성공.
5. **하드웨어 Watchdog 감시**: 통신 러너블 행업 상황 모의 시 글로벌 상태 `EXPIRED` 전이 및 STM32 IWDG 하드웨어 리셋 정상 동작.

---

## 포팅 전략

### 핵심 원칙

```
PC (gcc)                          STM32 (arm-none-eabi-gcc)
─────────────────────────────────  ─────────────────────────────────
main()                           → main() + HAL_Init() + SystemClock_Config
SimMcu_InjectPin()               → 실제 GPIO 입력 (버튼)
SimMcu_GetOutputPin()            → 실제 GPIO 출력 (LED)
SimMcu_InjectAdc()               → 실제 ADC 채널 (조도/전압 분배)
SimMcu_ReadAdcRaw()              → HAL_ADC_GetValue()
SimBus_TransmitFromEcu()         → CAN 하드웨어 전송 (bxCAN/FDCAN)
SimBus_InjectRx()                → CAN RX 인터럽트
SimMcu_GpioOdr / SimMcu_GpioIdr → GPIOx->ODR / GPIOx->IDR
```

### 교체 범위: **MCAL 계층만**

```
변경 없음:   Application (SensorSwc, LightCtrlSwc)
변경 없음:   RTE (Rte.c, Rte.h)
변경 없음:   BSW (Com, PduR, CanIf)
변경 없음:   IoHwAb
변경 없음:   UDS, ISO-TP, CAN signal codec
변경 없음:   SecOC, E2E, WdgM (타이머만 HAL 연동)

교체 대상:  SimMcu.c/h → STM32 HAL 기반 구현
교체 대상:  SimBus.c/h → STM32 CAN 드라이버
교체 대상:  Can.c      → MCAL Can 드라이버 (HAL CAN)
추가):      stm32f4xx_hal_conf.h, Linker Script, Startup Code
```

---

## Step 1: 개발 환경 설정

### 도구 체인

```bash
# 1. ARM 크로스 컴파일러 설치 (MSYS2)
pacman -S mingw-w64-ucrt-x86_64-arm-none-eabi-gcc
pacman -S mingw-w64-ucrt-x86_64-arm-none-eabi-newlib

# 2. STM32CubeIDE (무료, Windows/Mac/Linux)
#    https://www.st.com/en/development-tools/stm32cubeide.html
#    - STM32CubeMX 포함 (핀 설정/코드 생성)

# 3. OpenOCD (디버깅/플래싱)
pacman -S mingw-w64-ucrt-x86_64-openocd
```

### 프로젝트 생성 (STM32CubeMX)

```
1. STM32CubeMX 실행 → NUCLEO-F401RE 선택
2. Pinout & Configuration:
   - RCC: HSE (Crystal/Ceramic Resonator)
   - SYS: Debug (Serial Wire)
   - USART2: Asynchronous (printf 출력, PA2/PA3)
   - ADC1: IN0 (PA0), IN1 (PA1)
   - GPIO: PC0 (LED 출력), PC1 (스위치 입력)
   - CAN1: PB8/CAN_RX, PB9/CAN_TX (NUCLEO-F401RE는 CAN 미탑재
     → 외부 CAN 트랜시버(TJA1050) 필요 또는 CAN loopback 모드로 검증)
3. Clock Configuration: HSE 8MHz → SYSCLK 84MHz (max)
4. Project → Generate Code (Toolchain: Makefile)
5. 생성된 Core/ 폴더를 auto_sw_academy/stm32/ 로 복사
```

---

## Step 2: 디렉토리 구조

```
auto_sw_academy/
├── common/                        (수정 없음)
├── ex1_embedded_c/                (PC 전용 — STM32 대상 제외)
├── ex2_mini_autosar/              (AUTOSAR 스택 — 이식 대상)
├── ex3_comm_diag/                 (통신/진단 — 이식 대상)
├── ex4_safety_security/           (안전/보안 — 이식 대상)
├── generated/                     (설정 — 그대로 사용)
├── porting/stm32/                 ★ 이 가이드
│   ├── stm32_porting_guide.md     (이 파일)
│   ├── SimMcu_stm32.h            (MCAL 대체 헤더)
│   ├── SimMcu_stm32.c            (MCAL 대체 소스)
│   ├── Makefile.stm32             (크로스 컴파일 메이크파일)
│   └── config/
│       └── stm32f4xx_hal_conf.h  (HAL 설정)
├── stm32/                         (STM32CubeMX 생성 코드)
│   ├── Core/
│   │   ├── Inc/ (main.h, stm32f4xx_hal_conf.h, ...)
│   │   └── Src/ (main.c, stm32f4xx_hal_msp.c, ...)
│   └── Drivers/
│       └── STM32F4xx_HAL_Driver/
└── build_stm32/                   (빌드 출력)
```

---

## Step 3: MCAL 대체 구현

### 3.1 GPIO (SimMcu_stm32.h)

핵심 교체 대상인 `SimMcu_GpioOdr`와 `SimMcu_GpioIdr`를
실제 STM32 레지스터로 변경합니다:

```c
// SimMcu_stm32.h (stm32_porting/stm32/ 참조)
// PC 시뮬레이션:      static uint8_t SimMcu_GpioOdr;
// STM32 실제 레지스터: #define SimMcu_GpioOdr  (GPIOC->ODR)
//                     #define SimMcu_GpioIdr  (GPIOC->IDR)
```

즉, `#define` 한 줄로 PC 시뮬레이션 변수를 실제 하드웨어 레지스터로 교체합니다.
이는 `SimMcu_InjectPin`/`SimMcu_GetOutputPin` 함수가 수정 없이
실제 GPIO 레지스터를 읽고 쓰게 만듭니다.

> **AUTOSAR 추상화 증명**: MCAL만 바꾸고 Application은 0줄 수정.

### 3.2 ADC (SimMcu_stm32.c)

```c
// 시뮬레이션: SimMcu_s_adcRaw[ch] = injectedValue;
// STM32:     HAL_ADC_Start(&hadc1); HAL_ADC_PollForConversion(&hadc1, 10);
//            return HAL_ADC_GetValue(&hadc1);
```

- 가상 주입(`SimMcu_InjectAdc`)은 테스트만으로 제한
- 실제 ADC는 하드웨어 핀에 연결된 전압을 읽음

### 3.3 CAN (SimMcu_stm32.c → Can.c 통합)

```c
// 시뮬레이션: SimBus_TransmitFromEcu → sniffer 콜백
// STM32:     CAN_TxHeaderTypeDef + HAL_CAN_AddTxMessage()
//
// 시뮬레이션: SimBus_InjectRx → Can_RxInterrupt 직접 호출
// STM32:     HAL_CAN_RxFifo0MsgPendingCallback() → CanIf_RxIndication()
```

- STM32F401RE는 기본 CAN 컨트롤러(bxCAN) 내장
- 외부 CAN 트랜시버(TJA1050 모듈, €3) 필요
- 또는 CAN **Loopback Mode**로 하드웨어 없이 자체검증 가능

---

## Step 4: OS 타이머 (Os.c → HAL_GetTick)

PC 시뮬레이션의 `Os_Tick10ms`는 STM32의 SysTick 인터럽트로 대체:

```c
// PC:  void Os_Tick10ms(void) — 수동 호출
// STM32: void SysTick_Handler(void) { if (++g_tick10ms >= 10) { ... } }
```

10ms 주기로 호출되어야 하는 함수들:
- `Com_MainFunctionTx()`
- `IoHwAb_*` 센서 읽기 (ADC → SWC → RTE)
- `Uds_Tick10ms(&s_uds)` (S3 타임아웃, 보안 지연)
- `LightCtrlSwc_Runnable()`

---

## Step 5: 빌드

### Makefile.stm32 사용법

```bash
cd auto_sw_academy
make -f porting/stm32/Makefile.stm32 -j4
```

또는 기존 Makefile에 타겟 추가:

```bash
make stm32          # STM32 크로스 빌드
make stm32-flash    # OpenOCD로 플래싱
make stm32-check    # STM32에서 테스트 실행 결과 확인
```

### 크로스 컴파일 설정 (Makefile.stm32)

```makefile
CC      = arm-none-eabi-gcc
CXX     = arm-none-eabi-g++
CFLAGS  = -std=c99 -Wall -Wextra -O2 -g \
          -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
          -DSTM32F401xE -DUSE_HAL_DRIVER \
          -Icommon -Iex2_mini_autosar/src -Iex3_comm_diag/src \
          -Iex4_safety_security/src -Igenerated \
          -Istm32/Core/Inc -Istm32/Drivers/STM32F4xx_HAL_Driver/Inc
```

```makefile
# 링커: STM32F401RE 메모리 맵
LDFLAGS = -Tstm32/STM32F401RETx_FLASH.ld \
          -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
          --specs=nano.specs --specs=nosys.specs -lc -lm -lnosys

# 소스: AUTOSAR 스택 전체 + HAL 드라이버
SRCS = $(EX2_SRC) $(EX3_SRC) $(EX4_SRC) \
       capstone/main_stm32.c \          # STM32 main (HAL_Init 포함)
       stm32/Core/Src/stm32f4xx_hal_msp.c \
       stm32/Core/Src/stm32f4xx_it.c \
       stm32/Core/Src/system_stm32f4xx.c \
       $(wildcard stm32/Drivers/STM32F4xx_HAL_Driver/Src/*.c)
```

### 플래싱 및 디버깅

```bash
# OpenOCD로 플래싱
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build_stm32/capstone_demo.elf verify reset exit"

# GDB 디버깅
arm-none-eabi-gdb build_stm32/capstone_demo.elf \
  -ex "target remote localhost:3333" -ex "monitor reset halt"
```

---

## Step 6: 테스트 전략

| 단계 | 내용 | 검증 |
|------|------|------|
| **1. UART printf** | `HAL_UART_Transmit()`로 printf 출력 | PC 시리얼 모니터(PuTTY)로 capstone 출력 확인 |
| **2. GPIO 토글** | ON 상태에서 LED 점등 | 보드 LED 확인 |
| **3. ADC 입력** | 가변저항/조도센서로 배터리 전압 시뮬레이션 | UART로 송신 프레임 hex 출력 확인 |
| **4. CAN Loopback** | CAN 컨트롤러를 Loopback 모드로 설정 | 자체 송수신으로 VehicleState 프레임 검증 |
| **5. ISO-TP + UDS** | UART CLI로 진단 요청 입력 → CAN 응답 확인 | 전체 진단 워크플로 검증 |
| **6. 통합** | Capstone 데모 PHASE 1~4 전부 STM32에서 실행 | 출력 로그 == PC gcc 출력과 동일 |

---

## Step 7: 예상 결과

PC gcc 시뮬레이션과 동일한 기능이 STM32에서 동작:

```
[부팅] ECU 초기화 완료: CanIf/PduR/IsoTp, Dcm(UDS), SecOC, E2E, WdgM
[ECU] VehicleState 송신 → CAN_Tx Mailbox 0 (ID=0x100, DLC=8)
[ECU] SecOC 송신 → CAN_Tx Mailbox 1 (ID=0x180, DLC=8)
[진단기] UART로 수동 진단 요청 수신 → ISO-TP/UDS 처리 → CAN 응답 송신
[WdgM] SysTick 기반 deadline 감시 → Violation 시 Safe State 진입 (LED 패턴)
```

---

## 부록: 일반적인 문제 해결

### Q1. printf가 UART로 출력되지 않아요
HAL_UART_Transmit 기반 `_write()` stub 필요:
```c
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
    return len;
}
```

### Q2. CAN 프레임이 전송되지 않아요
- CAN 트랜시버 전원 확인 (3.3V, GND)
- CAN_H/CAN_L 120Ω 종단 저항 확인
- CAN baudrate 일치 확인 (보통 125kbps / 250kbps / 500kbps)
- **Loopback 모드로 먼저 검증**: 자체 송수신 확인 후 Normal mode로 전환

### Q3. 타이밍이 PC와 달라요
- STM32 SysTick은 1ms 인터럽트 (HAL_Init() 기본 설정)
- PC 시뮬레이션은 `pump_ms()` 루프로 시간을 모의
- 실제 하드웨어에서는 실시간으로 동작하므로 타이머 변환 불필요
- `WdgM_MainFunction(1u)`를 SysTick에서 1ms마다 호출

### Q4. Flash/RAM 크기가 부족해요
- `-O2` 최적화로 컴파일 (Release 빌드)
- 필요 없는 EX 모듈은 제외 (ex1_*, ex3_* test 파일 등)
- STM32F401RE: 512KB Flash, 96KB RAM — capstone에 충분
  (대략: 60KB code + 20KB data/BSS + 16KB stack/heap)

---

## 포팅 완료 후 체크리스트

- [ ] STM32CubeMX에서 핀 할당 완료 (UART, GPIO, ADC, CAN)
- [ ] `SimMcu_stm32.h` 작성 — GPIO ODR/IDR 레지스터 매핑
- [ ] `SimMcu_stm32.c` 작성 — ADC, CAN 구현
- [ ] `main_stm32.c` 작성 — HAL_Init, SysTick, 메인루프
- [ ] 크로스 컴파일 성공 (경고 0건)
- [ ] UART printf 정상 출력
- [ ] GPIO LED 점등 확인 (ACC/ON/CRANK/RUN 상태별 패턴)
- [ ] ADC 입력 읽기 확인
- [ ] CAN Loopback 모드 프레임 송수신
- [ ] ISO-TP/UDS 진단 요청 처리
- [ ] WdgM 타임아웃 감지 + Safe State

---

## 참고 자료

- [STM32CubeF4 공식 펌웨어](https://github.com/STMicroelectronics/STM32CubeF4)
- [STM32F401RE Datasheet (DS9868)](https://www.st.com/resource/en/datasheet/stm32f401re.pdf)
- [STM32F401RE Reference Manual (RM0368)](https://www.st.com/resource/en/reference_manual/rm0368.pdf)
- [NUCLEO-F401RE User Manual (UM1724)](https://www.st.com/resource/en/user_manual/um1724.pdf)
- [ARM GCC 크로스 컴파일러](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain)
- [OpenOCD 디버깅 가이드](https://openocd.org/doc/html/Plugins-and-Commands.html)
- AUTOSAR SWS_CANDriver: CAN MCAL 드라이버 표준
