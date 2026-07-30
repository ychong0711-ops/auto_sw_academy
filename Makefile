# Auto SW Academy - 전체 빌드 (gcc / g++ 만 있으면 됩니다, GNU Make 3.81+)
SHELL   := /bin/bash
CC      ?= gcc
CXX     ?= g++
CFLAGS  := -std=c99 -Wall -Wextra -O0 -g -MMD -MP \
           -Icommon \
           -Iex2_mini_autosar/src \
           -Iex3_comm_diag/src \
           -Iex4_safety_security/src \
           -Igenerated
CXXFLAGS := -std=c++11 -Wall -Wextra -O0 -g -MMD -MP \
            -static-libgcc -static-libstdc++ \
            -Icommon \
            -Iex2_mini_autosar/src \
            -Iex3_comm_diag/src \
            -Iex4_safety_security/src \
            -Igenerated
BUILD   := build
PYTHON  ?= python3   # <-- supply PYTHON=python env var on Windows
.DEFAULT_GOAL := all

# ---- 설정 생성 (미니 툴 체인): config/ecu.json -> generated/ ----
GEN_OUTS := generated/Cfg_Ids.h generated/Com_Cfg.c generated/CanIf_Cfg.c \
            generated/PduR_Cfg.c generated/Uds_Data.h generated/Uds_Data.c \
            generated/VehicleNetwork.dbc generated/SIGNAL_DOC.md

generated/.stamp: config/ecu.json tools/gen_cfg.py
	$(PYTHON) tools/gen_cfg.py config/ecu.json generated/
	@touch $@

.PHONY: gen
gen: generated/.stamp

EX1 := ex1_1_bit_ops ex1_2_register_map ex1_3_ring_buffer ex1_4_fixed_point ex1_5_state_machine
EX1_CPP := ex1_6_cpp_state_machine ex1_7_cpp_template_buffer ex1_8_cpp_fixed_point
EX2_SRC := $(wildcard ex2_mini_autosar/src/*.c)
EX3_SRC := ex3_comm_diag/src/vcan.c ex3_comm_diag/src/can_signal.c ex3_comm_diag/src/isotp.c ex3_comm_diag/src/uds_server.c
EX4_SRC := $(wildcard ex4_safety_security/src/*.c)

EX2_TESTS := ex2_signal_flow ex2_com ex2_simmcu ex2_simbus

TARGETS := \
  $(BUILD)/ex1_1_bit_ops $(BUILD)/ex1_2_register_map $(BUILD)/ex1_3_ring_buffer \
  $(BUILD)/ex1_4_fixed_point $(BUILD)/ex1_5_state_machine \
  $(addprefix $(BUILD)/,$(EX1_CPP)) \
  $(addprefix $(BUILD)/,$(EX2_TESTS)) \
  $(BUILD)/ex3_can_signal $(BUILD)/ex3_isotp $(BUILD)/ex3_uds \
  $(BUILD)/ex4_crc_e2e $(BUILD)/ex4_secoc $(BUILD)/ex4_wdg \
  $(BUILD)/capstone_demo

all: $(TARGETS)

$(BUILD):
	mkdir -p $(BUILD)

# ---- EX1: 단일 파일 연습 (C) ----
$(BUILD)/ex1_%: ex1_embedded_c/src/ex1_%.c | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@

# ---- EX1: 단일 파일 연습 (C++) ----
# ex1_6 이상 (C++11) — 확장자가 .cpp인 파일은 g++로 컴파일
$(BUILD)/ex1_6_cpp_state_machine: ex1_embedded_c/src/ex1_6_cpp_state_machine.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD)/ex1_7_cpp_template_buffer: ex1_embedded_c/src/ex1_7_cpp_template_buffer.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD)/ex1_8_cpp_fixed_point: ex1_embedded_c/src/ex1_8_cpp_fixed_point.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@

# ---- EX2: 미니 AUTOSAR (생성된 Cfg 포함) ----
EX2_GEN_CFG := generated/Com_Cfg.c generated/CanIf_Cfg.c generated/PduR_Cfg.c

$(BUILD)/ex2_signal_flow: ex2_mini_autosar/test/test_ex2_signal_flow.c $(EX2_SRC) $(EX2_GEN_CFG) generated/.stamp | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

# EX2 개별 계층 단위 테스트
$(BUILD)/ex2_com: ex2_mini_autosar/test/test_ex2_com.c $(EX2_SRC) $(EX2_GEN_CFG) generated/.stamp | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/ex2_simmcu: ex2_mini_autosar/test/test_ex2_simmcu.c ex2_mini_autosar/src/SimMcu.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/ex2_simbus: ex2_mini_autosar/test/test_ex2_simbus.c $(EX2_SRC) $(EX2_GEN_CFG) generated/.stamp | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

# ---- EX3: 통신/진단 ----
$(BUILD)/ex3_can_signal: ex3_comm_diag/test/test_ex3_can_signal.c ex3_comm_diag/src/can_signal.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/ex3_isotp: ex3_comm_diag/test/test_ex3_isotp.c ex3_comm_diag/src/vcan.c ex3_comm_diag/src/isotp.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/ex3_uds: ex3_comm_diag/test/test_ex3_uds.c ex3_comm_diag/src/uds_server.c \
                  generated/Uds_Data.c generated/Uds_Data.h generated/.stamp | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

# ---- EX4: 안전/보안 ----
$(BUILD)/ex4_crc_e2e: ex4_safety_security/test/test_ex4_crc_e2e.c ex4_safety_security/src/crc8.c ex4_safety_security/src/e2e_p01.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/ex4_secoc: ex4_safety_security/test/test_ex4_secoc.c ex4_safety_security/src/aes_mini.c ex4_safety_security/src/cmac.c ex4_safety_security/src/secoc.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/ex4_wdg: ex4_safety_security/test/test_ex4_wdg.c ex4_safety_security/src/wdg.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

# ---- CAPSTONE ----
$(BUILD)/capstone_demo: capstone/main_capstone.c $(EX3_SRC) $(EX4_SRC) \
                        generated/Uds_Data.c generated/Uds_Data.h generated/.stamp | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@

# ---- 정적 분석 (cppcheck) ----
ANALYZE_SRC := $(wildcard ex1_embedded_c/src/*.c ex2_mini_autosar/src/*.c \
                         ex3_comm_diag/src/*.c ex4_safety_security/src/*.c \
                         capstone/*.c)

analyze: $(GEN_OUTS)
	@echo "=== cppcheck (전체 $((words $(ANALYZE_SRC)))개 소스) ==="; \
	cppcheck --enable=all --inconclusive --std=c99 \
	  --suppress=missingIncludeSystem \
	  --suppress=unmatchedSuppression \
	  --suppress=normalCheckLevelMaxBranches \
	  --suppress=checkersReport \
	  --suppress=purgefiableCode \
	  --suppress=unusedFunction \
	  --suppress=staticFunction \
	  --suppress=funcArgNamesDifferentUnnamed \
	  --suppress=constParameterPointer \
	  -Icommon -Iex2_mini_autosar/src -Iex3_comm_diag/src \
	  -Iex4_safety_security/src -Igenerated \
	  --error-exitcode=1 \
	  $(ANALYZE_SRC) 2>&1 && \
	echo "[analyze] cppcheck: 모든 경고/에러 0건 ✔"

# ---- 엄격 빌드 (-Werror) ----
strict: CFLAGS += -Werror
strict: all
	@echo "[strict] -Werror 빌드 성공 ✔"

# ---- 코드 포맷팅 ----
format:
	@if which clang-format > /dev/null 2>&1; then \
	  find . \( -name '*.c' -o -name '*.h' \) \
	    ! -path './generated/*' ! -path './build/*' \
	    -exec clang-format -i {} +; \
	  echo "[format] clang-format 적용 완료 ✔"; \
	else echo "[format] clang-format 미설치, 건너뜀"; fi

# ---- 실행 ----
check: all
	@fail=0; \
	for t in $(TARGETS); do \
	  echo "================ $$t ================"; \
	  $$t || fail=1; \
	done; \
	if [ $$fail -eq 0 ]; then echo; echo "ALL SUITES PASS"; else echo; echo "SOME SUITES FAILED"; exit 1; fi

clean:
	rm -rf $(BUILD) logs

# ---- 경로 C: 버스 트레이스 + DBC 디코드 (미니 CANoe 워크플로) ----
# 1) capstone 을 버스 모니터 모드로 실행 -> 2) 생성된 .dbc 로 신호 해석
TRACE := logs/capstone_trace.log

$(TRACE): $(BUILD)/capstone_demo
	@mkdir -p logs
	./$(BUILD)/capstone_demo --trace $(TRACE) > logs/capstone_console.log
	@echo "트레이스/콘솔 로그 저장: $(TRACE), logs/capstone_console.log"

trace: $(TRACE)
	$(PYTHON) tools/decode_trace.py generated/VehicleNetwork.dbc $(TRACE)

# 자동 검증: 두 디코드 엔진(cantools / stdlib) 출력이 같고, 기대 신호 값이 읽히는지 확인
trace-check: $(BUILD)/capstone_demo
	@mkdir -p logs
	./	$(BUILD)/capstone_demo --trace $(TRACE) > logs/capstone_console.log
	$(PYTHON) tools/decode_trace.py          generated/VehicleNetwork.dbc $(TRACE) > logs/decoded_cantools.log
	$(PYTHON) tools/decode_trace.py --stdlib generated/VehicleNetwork.dbc $(TRACE) > logs/decoded_stdlib.log
	@if diff <(tail -n +2 logs/decoded_cantools.log) <(tail -n +2 logs/decoded_stdlib.log) > /dev/null; then \
	  echo "[trace-check] cantools vs stdlib 디코드 출력 일치 ✔"; \
	else echo "[trace-check] 두 엔진 출력 불일치 ✘"; exit 1; fi
	@grep "BatteryVoltage = 12.8 V" logs/decoded_stdlib.log > /dev/null 2>&1 \
	  && echo "[trace-check] 기대 신호 값(BatteryVoltage=12.8V) 디코드 ✔" \
	  || { echo "[trace-check] 기대 신호 디코드 실패 ✘"; exit 1; }
	@grep "VehicleState  *x3" logs/decoded_stdlib.log > /dev/null 2>&1 \
	  && echo "[trace-check] 메시지 집계(VehicleState x3) ✔" \
	  || { echo "[trace-check] 메시지 집계 실패 ✘"; exit 1; }

# ---- 헤더 의존성 자동 추적 (.d 파일, -MMD 로 생성됨) ----
-include $(wildcard $(BUILD)/*.d)

.PHONY: all check clean trace trace-check
