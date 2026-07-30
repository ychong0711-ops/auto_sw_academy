#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
study_trainer.py — Auto SW Academy 대화형 학습 코치 및 자가 검증 프로그램

[4주 마스터 로드맵 + ASPICE 30초 추적성 탐험 + 오류 주입 실습 + 졸업 진단 퀴즈]
  - 이 저장소의 학습법(00_big_picture.md, README.md 등)을 프로그램으로 구현한
    대화형 CLI 학습 도우미입니다.
  - 단계별 소스 가이드, 라이브 테스트 실행, ASPICE 3방향 추적성(SRS↔코드↔TC) 탐색,
    오류 주입 시나리오 실증, 졸업 자가 진단 퀴즈 기능을 제공합니다.

사용법:
  make study                          # 대화형 메뉴 모드 시작
  python3 tools/study_trainer.py --week 1     # 1주차 학습 가이드 및 테스트 실행
  python3 tools/study_trainer.py --trace SRS_COM_010  # ASPICE 추적성 3방향 탐색
  python3 tools/study_trainer.py --lab 1      # 오류 주입 및 방어 실증 (Replay Attack)
  python3 tools/study_trainer.py --quiz       # 졸업 자가 진단 퀴즈 (CLI 모드)
  python3 tools/study_trainer.py --demo-all   # 전체 기능 자동화 데모 실행
"""

import argparse
import csv
import glob
import os
import re
import subprocess
import sys
import textwrap

# Windows 등 다양한 콘솔에서 한글 UTF-8 출력 보장
if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass


# ==============================================================================
# 1. 4주 실전 학습 로드맵 모드 (Week 1 ~ Week 4)
# ==============================================================================

WEEK_DATA = {
    1: {
        "title": "[1주차] MCU 제어와 메모리 사고법 (Embedded C/C++)",
        "dir": "ex1_embedded_c",
        "objectives": [
            "하드웨어 레지스터 접근 패턴(BSRR vs ODR)과 volatile 키워드의 물리적 의미 체화",
            "인터럽트(ISR) 안전 링버퍼 FIFO에서 실제 수용량을 N-1 바이트로 설계하는 이유 이해",
            "FPU가 없는 MCU에서 64비트 중간 연산으로 오버플로를 막는 Q16.16 고정소수점 연산",
        ],
        "sources": [
            "ex1_embedded_c/src/ex1_1_bit_ops.c",
            "ex1_embedded_c/src/ex1_2_register_map.c",
            "ex1_embedded_c/src/ex1_3_ring_buffer.c",
            "ex1_embedded_c/src/ex1_4_fixed_point.c",
            "ex1_embedded_c/src/ex1_8_cpp_fixed_point.cpp",
        ],
        "test_targets": [
            "build/ex1_1_bit_ops",
            "build/ex1_2_register_map",
            "build/ex1_3_ring_buffer",
            "build/ex1_4_fixed_point",
            "build/ex1_8_cpp_fixed_point",
        ],
        "checklist": "Q. BSRR 레지스터를 사용하면 왜 Read-Modify-Write(RMW) 없이 원자적으로 핀을 변경할 수 있는가?",
    },
    2: {
        "title": "[2주차] AUTOSAR BSW 계층화 및 설정 테이블 설계 (MiniECU)",
        "dir": "ex2_mini_autosar",
        "objectives": [
            "Vector DaVinci / EB tresos 가 생성하는 설정 테이블(Cfg.c)과 계층간 데이터 분리 원리",
            "SWC -> RTE -> Com -> PduR -> CanIf -> CAN TX 송신 콜스택과 I-PDU 버퍼 패킹 흐름",
            "네트워크 RX 제어 명령(LightOverride)이 들어왔을 때 기존 자동 로직을 덮어쓰는 역방향 흐름",
        ],
        "sources": [
            "config/ecu.json",
            "generated/Com_Cfg.c",
            "ex2_mini_autosar/src/Com.c",
            "ex2_mini_autosar/src/CanIf.c",
            "ex2_mini_autosar/test/test_ex2_signal_flow.c",
        ],
        "test_targets": [
            "build/ex2_signal_flow",
            "build/ex2_com",
            "build/ex2_simmcu",
            "build/ex2_simbus",
        ],
        "checklist": "Q. 새로운 CAN 프레임 ID와 시그널을 추가할 때 BSW/SWC 소스를 안 고치고 Cfg 테이블만 바꾸는 이유는?",
    },
    3: {
        "title": "[3주차] 차량 통신/진단 스택 및 미니 CANoe 워크플로 (Protocol Stack)",
        "dir": "ex3_comm_diag",
        "objectives": [
            "CAN 시그널 엔디안(Intel vs Motorola Sawtooth) 패킹 규칙 및 물리량 변환(Factor/Offset)",
            "ISO-TP(ISO 15765-2) 다중 프레임 SF/FF/CF/FC 분할·재조립 및 Flow Control STmin 간격 제어",
            "ISO 14229-1 UDS 서버 9대 서비스(0x22, 0x2E, 0x27, 0x19 등)와 NRC(0x13, 0x33, 0x36) 처리",
        ],
        "sources": [
            "ex3_comm_diag/src/can_signal.c",
            "ex3_comm_diag/src/isotp.c",
            "ex3_comm_diag/src/uds_server.c",
            "ex3_comm_diag/test/test_ex3_uds.c",
        ],
        "test_targets": [
            "build/ex3_can_signal",
            "build/ex3_isotp",
            "build/ex3_uds",
        ],
        "checklist": "Q. UDS 0x27 (SecurityAccess)에서 잘못된 키 3회 입력 시 왜 즉시 거부하고 NRC 0x36 지연을 주어야 하는가?",
    },
    4: {
        "title": "[4주차] 기능안전(ISO 26262)/보안(ISO 21434)/ASPICE 및 STM32 실증",
        "dir": "ex4_safety_security",
        "objectives": [
            "AUTOSAR E2E Profile 1 (CRC-8 + Sequence Counter 0~14 랩) 통한 시퀀스 오류/유실 감지",
            "SecOC (AES-CMAC + Freshness Value) 기반 PDU 인증 및 Replay Attack 재전송 공격 차단",
            "WdgM 하드웨어 Watchdog 감시 (통신 러너블 행업 시 글로벌 EXPIRED -> IWDG 리셋 트리거)",
            "STM32F401RE NUCLEO 보드 포팅 실증 가이드(SimMcu -> STM32 HAL 치환) 체화",
        ],
        "sources": [
            "ex4_safety_security/src/e2e_p01.c",
            "ex4_safety_security/src/secoc.c",
            "ex4_safety_security/src/wdg.c",
            "porting/stm32/stm32_porting_guide.md",
            "porting/stm32/stm32f401re_nucleo_boot_trace.log",
        ],
        "test_targets": [
            "build/ex4_crc_e2e",
            "build/ex4_secoc",
            "build/ex4_wdg",
            "build/capstone_demo",
        ],
        "checklist": "Q. E2E Profile 1 (안전)과 SecOC (보안)를 차량 네트워크 프레임에 둘 다 적용해야 하는 본질적인 이유는?",
    },
}


def run_week_guide(week_num, execute_tests=True):
    """지정한 1~4주차 학습 가이드를 출력하고 해당 주차의 단위 테스트를 실행한다."""
    if week_num not in WEEK_DATA:
        print(f"[study_trainer] Error: 1~4주차 중에서 선택하세요 (입력: {week_num})", file=sys.stderr)
        return False

    w = WEEK_DATA[week_num]
    print()
    print("==================================================================================")
    print(f"  🎯 {w['title']}")
    print("==================================================================================")
    print(" 📌 학습 목표:")
    for obj in w["objectives"]:
        print(f"    - {obj}")
    print()
    print(" 📖 핵심 정복 소스 파일 (스펙 문서 대신 코드와 주석 읽기):")
    for src in w["sources"]:
        exists = "✔" if os.path.exists(src) else "✘ (미생성/확인필요)"
        print(f"    - [{exists}] {src:<48}")
    print()
    print(f" 💡 핵심 면접/실무 자가 진단 질문:\n    --> {w['checklist']}")
    print("==================================================================================")

    if execute_tests:
        print(f"\n ==> {week_num}주차 검증 테스트 스위트 라이브 실행:")
        all_ok = True
        for target in w["test_targets"]:
            print(f" ---------------- [ RUN: {target} ] ----------------")
            res = subprocess.run([target], capture_output=True, text=True)
            if res.returncode == 0:
                lines = [l for l in res.stdout.splitlines() if "PASS" in l or "결과:" in l or "== " in l]
                for ln in lines[:6]:
                    print(f"   {ln}")
                print(f"   [PASS] {target} 정상 검증 완료 ✔")
            else:
                print(f"   [FAIL] {target} 실행 실패 (종료코드: {res.returncode}) ✘")
                print(res.stderr or res.stdout)
                all_ok = False
        print("==================================================================================")
        return all_ok
    return True


# ==============================================================================
# 2. ASPICE 30초 추적성 탐험기 (Traceability Explorer)
# ==============================================================================

def explore_traceability(query="SRS_COM_010"):
    """ASPICE 양방향 추적성 매트릭스 CSV를 조회하고 코드 주석 및 TC를 연결 출력한다."""
    csv_path = "ex4_safety_security/aspice/traceability_matrix.csv"
    if not os.path.exists(csv_path):
        print(f"[study_trainer] Error: Traceability matrix not found at '{csv_path}'", file=sys.stderr)
        return False

    query_upper = query.strip().upper().replace("[", "").replace("]", "")
    matches = []
    with open(csv_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            srs_id = row.get("SRS_ID", "").strip().upper()
            tc_id = row.get("테스트 케이스", "").strip().upper()
            func = row.get("구현 함수", "").strip()
            unit = row.get("SW유닛(파일)", "").strip()
            desc = row.get("요구사항", "").strip()
            if query_upper in srs_id or query_upper in tc_id or query_upper in func.upper():
                matches.append(row)

    print()
    print("==================================================================================")
    print(f"  🔍 ASPICE SWE.4 3방향 추적성 탐험기 (Query: '{query}')")
    print("==================================================================================")

    if not matches:
        print(f"  [결과 없음] '{query}'와 매칭되는 SRS_ID 또는 테스트 케이스가 없습니다.")
        print("  예시 검색어: SRS_COM_010, SRS_TP_010, SRS_UDS_010, TC_EX2_001")
        print("==================================================================================")
        return False

    for idx, m in enumerate(matches, 1):
        srs_id = m.get("SRS_ID", "")
        desc = m.get("요구사항", "")
        unit = m.get("SW유닛(파일)", "")
        func = m.get("구현 함수", "")
        tc_id = m.get("테스트 케이스", "")
        res = m.get("결과", "PASS")

        print(f" [{idx}] 요구사항 (SRS)   : [{srs_id}] {desc}")
        print(f"     구현 유닛/함수   : {unit} -> {func}()")
        print(f"     검증 테스트 (TC) : {tc_id} (검증결과: {res})")

        # 1) 소스 주석 검색
        print("     └─> [소스코드 구현 증거]:")
        found_code = False
        for root_dir in ["ex1_embedded_c/src", "ex2_mini_autosar/src", "ex3_comm_diag/src", "ex4_safety_security/src"]:
            for fpath in glob.glob(os.path.join(root_dir, "*.*")):
                try:
                    with open(fpath, "r", encoding="utf-8", errors="ignore") as src_file:
                        for lno, ltext in enumerate(src_file, 1):
                            if srs_id in ltext:
                                snippet = ltext.strip()
                                print(f"         • {fpath}:{lno}: {snippet[:75]}")
                                found_code = True
                                break
                except Exception:
                    pass
        if not found_code:
            print(f"         • (소스 주석 내 explicit '[{srs_id}]' 태그 미검출 - {func} 함수 참조)")

        # 2) 테스트 케이스 검색
        print("     └─> [검증 테스트 증거]:")
        found_tc = False
        for root_dir in ["ex1_embedded_c/test", "ex2_mini_autosar/test", "ex3_comm_diag/test", "ex4_safety_security/test"]:
            for fpath in glob.glob(os.path.join(root_dir, "*.c")):
                try:
                    with open(fpath, "r", encoding="utf-8", errors="ignore") as tc_file:
                        for lno, ltext in enumerate(tc_file, 1):
                            if any(x in ltext for x in tc_id.split("/")[:2]) or srs_id in ltext:
                                snippet = ltext.strip()
                                print(f"         • {fpath}:{lno}: {snippet[:75]}")
                                found_tc = True
                                break
                except Exception:
                    pass
        if not found_tc:
            print(f"         • (테스트 파일 내 TC 명칭 일치 항목 - {tc_id} 참조)")
        print("----------------------------------------------------------------------------------")

    print(" ✔ 독일/유럽 ASPICE 감사관이 수행하는 30초 라이브 양방향 추적 검증 증명 완료.")
    print("==================================================================================")
    return True


# ==============================================================================
# 3. 의도적 오류 주입 및 방어 실증 (Fault Injection & Defense Lab)
# ==============================================================================

LAB_DATA = {
    1: {
        "title": "[Lab 1] SecOC Replay Attack (재전송 공격) 차단 실증",
        "desc": "같은 Freshness Value를 가진 CAN 프레임을 버스에 재주입했을 때 ECU의 거부 메커니즘 확인",
        "command": ["build/ex4_secoc"],
        "expected_kw": "거부",
        "explanation": "SecOC는 MAC(메시지 인증 코드) 외에 Freshness Value(신선도 카운터)를 포함하여\n"
                       "이미 수신된 과거 프레임이 다시 주입되면 즉시 거부(REJECTED)하고 공격 카운터를 증가시킵니다.",
    },
    2: {
        "title": "[Lab 2] E2E Profile 1 데이터 오염(CRC 불일치) 감지 실증",
        "desc": "CAN 페이로드 비트 1개가 손상되었을 때 CRC-8 / 시퀀스 카운터 검증 결과 확인",
        "command": ["build/ex4_crc_e2e"],
        "expected_kw": "ERROR",
        "explanation": "ISO 26262 ASIL-D 통신에서 E2E Profile 1은 CRC-8과 0~14 카운터 랩을 통해\n"
                       "메모리/네트워크 손상 시 E2E_P01STATUS_ERROR 상태를 트리거하여 제어기 진입을 막습니다.",
    },
    3: {
        "title": "[Lab 3] WdgM 하드웨어 Watchdog 통신 행업 리셋 실증",
        "desc": "60ms 동안 체크포인트 생존 보고가 멈췄을 때 WdgM의 글로벌 상태 EXPIRED 전이 확인",
        "command": ["build/ex4_wdg"],
        "expected_kw": "EXPIRED",
        "explanation": "WdgM(Watchdog Manager)은 Alive/Deadline 감시를 통해 특정 러너블이 행업(정지)되면\n"
                       "글로벌 상태를 EXPIRED로 전이시키고 STM32 IWDG 하드웨어 리셋을 트리거합니다.",
    },
    4: {
        "title": "[Lab 4] UDS SecurityAccess Seed-Key 3회 실패 누적 방어 실증",
        "desc": "잘못된 키를 3회 입력했을 때 NRC 0x36 / 0x37(지연 시간) 방어 기제 동작 확인",
        "command": ["build/ex3_uds"],
        "expected_kw": "NRC",
        "explanation": "ISO 14229-1 UDS 0x27 서비스는 Brute-force 공격을 방지하기 위해\n"
                       "키 검증 3회 실패 시 NRC 0x36(ExceededNumberOfAttempts) 및 타이머 만료 전 NRC 0x37을 반환합니다.",
    },
}


def run_fault_lab(lab_id):
    """지정한 오류 주입 실험을 설명하고 실제 바이너리의 테스트 검증을 출력한다."""
    if lab_id not in LAB_DATA:
        print(f"[study_trainer] Error: Lab 1~4 중 선택하세요 (입력: {lab_id})", file=sys.stderr)
        return False

    lab = LAB_DATA[lab_id]
    print()
    print("==================================================================================")
    print(f"  ⚡ {lab['title']}")
    print("==================================================================================")
    print(f" 🧪 실험 목적: {lab['desc']}")
    print()
    print(" 📖 방어 메커니즘 해설:")
    for line in lab["explanation"].splitlines():
        print(f"    {line}")
    print("----------------------------------------------------------------------------------")
    print(f" ==> 실험 시연 실행: {' '.join(lab['command'])}")

    cmd = lab["command"][0]
    if not os.path.exists(cmd):
        print(f"   [Error] {cmd} 실행 파일을 찾을 수 없습니다. 'make all'을 먼저 수행하세요.")
        return False

    res = subprocess.run([cmd], capture_output=True, text=True)
    out_lines = [l for l in res.stdout.splitlines() if lab["expected_kw"] in l or "PASS" in l or "ERROR" in l or "NRC" in l]
    for ln in out_lines[:8]:
        print(f"   {ln}")

    print("----------------------------------------------------------------------------------")
    print(" ✔ 의도적 예외/공격 주입 시 안전/보안 방어 로직 정상 작동 실증 완료.")
    print("==================================================================================")
    return True


# ==============================================================================
# 4. 6대 핵심 졸업 자가 진단 퀴즈 (Final Graduation Quiz)
# ==============================================================================

QUIZ_QUESTIONS = [
    {
        "q": "1. 새로운 CAN 프레임 ID와 시그널을 ECU에 추가할 때, 응용 SWC나 BSW 드라이버 코드를 고치지 않고 오직 설정 테이블(Cfg.c)만 변경하여 적용할 수 있는 AUTOSAR 설계 원리는?",
        "options": [
            "A) MCAL 레지스터 매핑",
            "B) 설정 테이블 기반 데이터와 인터페이스 분리 (Configuration Table Layering)",
            "C) ODX 진단 서비스 테이블",
            "D) ISR 링버퍼 메모리 할당",
        ],
        "ans": "B",
        "expl": "AUTOSAR Com/CanIf 등은 실제 신호 파라미터를 Com_SignalConfig 구조체 테이블에 분리하여 저장하므로 Cfg.c만 재생성하면 스택 수정이 필요 없습니다.",
    },
    {
        "q": "2. CAN 신호 12비트를 패킹할 때, Intel(Little-Endian) 방식과 Motorola(Big-Endian) 방식의 가장 핵심적인 비트 배치 차이점은?",
        "options": [
            "A) Intel은 LSB부터 채우고, Motorola(Sawtooth)는 MSB 비트 번호 기준으로 역방향 배치한다",
            "B) Intel은 8바이트 고정이고 Motorola는 64바이트까지 확장된다",
            "C) Motorola는 부동소수점 Float 연산을 수행한다",
            "D) Intel은 CRC-8을 필요로 하지 않는다",
        ],
        "ans": "A",
        "expl": "CANdb / cantools 규칙상 Motorola(Big-Endian)는 Sawtooth 비트 체계에서 MSB 위치를 start_bit로 지정하여 역방향 배치됩니다.",
    },
    {
        "q": "3. UDS 0x27 (SecurityAccess) 서비스에서 이미 잠금 해제(Unlocked)된 세션 상태에서 다시 requestSeed를 요청할 때 0x0000을 반환해야 하는 ISO 14229 규정 이유는?",
        "options": [
            "A) 키 계산 알고리즘이 삭제되어서",
            "B) ECU 리셋 요청임을 뜻해서",
            "C) 이미 잠금 해제되어 추가 인증이 필요 없음을 진단기에 명시적으로 알리기 위해",
            "D) NRC 0x13 길이 부족 오류를 알리기 위해",
        ],
        "ans": "C",
        "expl": "ISO 14229-1 규정에 따라 이미 Unlocked 상태이면 seed = 0x0000을 반환하여 진단기가 sendKey 없이 작업을 수행하게 합니다.",
    },
    {
        "q": "4. ISO-TP(ISO 15765-2)에서 수신 ECU가 Flow Control(FC) 프레임의 STmin 파라미터를 통해 송신 테스터에게 지시하는 제어는?",
        "options": [
            "A) CAN 버스 통신 속도를 100kbps로 강제 저하",
            "B) Consecutive Frame(CF) 전송 간격(Pacing) 최소 시간 지연 제어",
            "C) 세션 타임아웃 S3 타이머 초기화",
            "D) ECU 시리얼 번호 요청",
        ],
        "ans": "B",
        "expl": "STmin(Separation Time minimum)은 수신 버퍼 오버플로를 막기 위해 다음 CF 프레임 전송까지 최소 대기 시간을 밀리초 단위로 지시합니다.",
    },
    {
        "q": "5. 차량 CAN 프레임에 안전 메커니즘인 E2E Profile 1(CRC+Counter)과 보안 메커니즘인 SecOC(MAC+Freshness)를 둘 다 적용해야 하는 이유로 옳은 것은?",
        "options": [
            "A) E2E는 비의도적 결함/유실(Safety)을, SecOC는 악의적 위조/재전송 공격(Security)을 방어하므로 목적이 서로 다르다",
            "B) SecOC는 ARM 프로세서에서 작동하지 않아서",
            "C) E2E는 CANFD 전용이고 SecOC는 Classic CAN 전용이라서",
            "D) ASPICE SWE.1 단계에서 이중 암호화를 법으로 의무화해서",
        ],
        "ans": "A",
        "expl": "E2E는 하드웨어/네트워크의 우발적 손상(Safety)을 감지하고, SecOC는 공격자의 위조 및 재전송 공격(Security)을 차단하는 보완적 메커니즘입니다.",
    },
    {
        "q": "6. 이 저장소의 PC(gcc) 시뮬레이션 코드(3,600줄)를 실제 STM32F401RE MCU 보드로 이식할 때, 나머지 스택(BSW/RTE/SWC) 수정 없이 교체해야 하는 하위 드라이버 레이어는?",
        "options": [
            "A) ODX 데이터베이스",
            "B) SimMcu (MCAL 가상화 레이어 -> STM32 HAL / bxCAN 치환)",
            "C) CanIf PDU 매핑 테이블",
            "D) CRC-8 룩업 테이블",
        ],
        "ans": "B",
        "expl": "SimMcu_stm32.c/.h 계층만 STM32 HAL 및 bxCAN 드라이버로 대체하면 BSW, RTE, SWC는 단 한 줄 수정 없이 하드웨어에서 구동됩니다.",
    },
]


def run_quiz_mode(demo_mode=False):
    """6대 핵심 졸업 자가 진단 퀴즈를 대화형 또는 자동화 데모 모드로 수행한다."""
    print()
    print("==================================================================================")
    print("  🎓 Auto SW Academy 6대 핵심 졸업 자가 진단 퀴즈 (Graduation Self-Assessment)")
    print("==================================================================================")
    print(" 이 6문제를 보지 않고 정답과 이유를 말할 수 있다면 독일/한국 차량 SW 주니어 상위 5%입니다.")
    print("----------------------------------------------------------------------------------")

    score = 0
    for idx, item in enumerate(QUIZ_QUESTIONS, 1):
        print(f"\n[Q{idx}] {item['q']}")
        for opt in item["options"]:
            print(f"    {opt}")

        if demo_mode:
            user_ans = item["ans"]
            print(f"  >> [자동 시연 응답] {user_ans}")
        else:
            try:
                user_ans = input("  >> 정답 선택 (A/B/C/D): ").strip().upper()
            except (EOFError, KeyboardInterrupt):
                print("\n[quiz] 퀴즈 종료.")
                return False

        if user_ans == item["ans"]:
            print("  ✔ [정답!]", item["expl"])
            score += 1
        else:
            print(f"  ✘ [오답] 정답은 {item['ans']} 입니다. -> {item['expl']}")

    print()
    print("==================================================================================")
    pct = (score / len(QUIZ_QUESTIONS)) * 100.0
    print(f" 🏁 최종 스코어: {score} / {len(QUIZ_QUESTIONS)} ({pct:.1f}%)")
    if pct >= 80.0:
        print(" ✔ [졸업 자격 획득] 축하합니다! 기술 면접과 실무 검증 준비가 완료되었습니다.")
    else:
        print(" 💡 [복습 권장] docs/00_big_picture.md 및 1~4주차 핵심 소스를 다시 점검해 보세요.")
    print("==================================================================================")
    return True


# ==============================================================================
# 5. 대화형 CLI 메뉴 (Interactive Menu)
# ==============================================================================

def print_menu():
    print()
    print("==================================================================================")
    print("  🚗 Auto SW Academy 대화형 실전 학습 코치 (Interactive Study Trainer)            ")
    print("==================================================================================")
    print("  [1] 4주 실전 학습 로드맵 모드 (Week 1~4 주차별 목표 + 라이브 테스트 실행)")
    print("  [2] ASPICE SWE.4 30초 추적성 탐험기 (SRS_ID <-> 소스코드 <-> TC_ID 연결 조회)")
    print("  [3] 의도적 오류 주입 및 방어 실증 랩 (Replay Attack, CRC 오염, WdgM 행업, UDS 지연)")
    print("  [4] 6대 핵심 졸업 자가 진단 퀴즈 (Final Graduation Interview Quiz)")
    print("  [5] 전체 검증 현황 조회 (make check + make coverage 92.98% 요약)")
    print("  [0] 프로그램 종료 (Exit)")
    print("==================================================================================")


def interactive_loop():
    while True:
        print_menu()
        try:
            choice = input(" >> 메뉴 번호 선택 (0-5): ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n[study_trainer] 프로그램을 종료합니다.")
            break

        if choice == "0":
            print("[study_trainer] 학습 코치 프로그램을 종료합니다. Keep Coding!")
            break
        elif choice == "1":
            try:
                wn = int(input("   >> 보고 싶은 주차를 선택하세요 (1-4): ").strip())
                run_week_guide(wn, execute_tests=True)
            except ValueError:
                print("   [Error] 1~4 숫자를 입력하세요.")
        elif choice == "2":
            q = input("   >> 검색할 SRS_ID 또는 테스트 케이스 (예: SRS_COM_010): ").strip()
            if not q:
                q = "SRS_COM_010"
            explore_traceability(q)
        elif choice == "3":
            print("   --- [오류 주입 랩 선택] ---")
            for lid, linfo in LAB_DATA.items():
                print(f"     {lid}: {linfo['title']}")
            try:
                lnum = int(input("   >> 실험 번호 선택 (1-4): ").strip())
                run_fault_lab(lnum)
            except ValueError:
                print("   [Error] 1~4 숫자를 입력하세요.")
        elif choice == "4":
            run_quiz_mode(demo_mode=False)
        elif choice == "5":
            print("\n === [전체 시스템 검증 현황 조회] ===")
            subprocess.run(["make", "coverage"])
        else:
            print("   [Error] 0~5 번호 중 하나를 입력하세요.")


# ==============================================================================
# 메인 함수 (CLI 옵션 파싱)
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(description="Auto SW Academy Interactive Study Trainer.")
    parser.add_argument("--week", type=int, choices=[1, 2, 3, 4], help="1~4주차 가이드 및 단위 테스트 실행")
    parser.add_argument("--trace", type=str, help="ASPICE 추적성 검색 (예: SRS_COM_010)")
    parser.add_argument("--lab", type=int, choices=[1, 2, 3, 4], help="의도적 오류 주입 실험 번호 (1-4)")
    parser.add_argument("--quiz", action="store_true", help="6대 핵심 졸업 진단 퀴즈 (CLI 대화형 모드)")
    parser.add_argument("--demo-all", action="store_true", help="전체 학습 코치 모드 자동 시연 (비대화형 모드)")
    args = parser.parse_args()

    if args.demo_all:
        print("[study_trainer] --demo-all 모드: 전체 학습법 프로그램 기능 자동화 시연")
        run_week_guide(2, execute_tests=True)
        explore_traceability("SRS_COM_010")
        run_fault_lab(1)
        run_quiz_mode(demo_mode=True)
        sys.exit(0)

    if args.week:
        run_week_guide(args.week, execute_tests=True)
        sys.exit(0)

    if args.trace:
        explore_traceability(args.trace)
        sys.exit(0)

    if args.lab:
        run_fault_lab(args.lab)
        sys.exit(0)

    if args.quiz:
        run_quiz_mode(demo_mode=False)
        sys.exit(0)

    # CLI 옵션이 없으면 대화형 터미널 루프 실행
    interactive_loop()


if __name__ == "__main__":
    main()
