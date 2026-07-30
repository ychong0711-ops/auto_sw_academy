cmake_minimum_required(VERSION 3.16)
project(auto_sw_academy LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 공통 include 경로 ----
list(APPEND CMAKE_C_FLAGS "-Wall -Wextra -O0 -g")
list(APPEND CMAKE_CXX_FLAGS "-Wall -Wextra -O0 -g -static-libgcc -static-libstdc++")
include_directories(
  common
  ex2_mini_autosar/src
  ex3_comm_diag/src
  ex4_safety_security/src
  generated
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# ---- 설정 생성기 (config/ecu.json -> generated/) ----
set(GEN_SCRIPT "${CMAKE_SOURCE_DIR}/tools/gen_cfg.py")
set(GEN_INPUT  "${CMAKE_SOURCE_DIR}/config/ecu.json")
set(GEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(GEN_OUTPUTS
  ${GEN_OUTPUT_DIR}/Cfg_Ids.h
  ${GEN_OUTPUT_DIR}/Com_Cfg.c
  ${GEN_OUTPUT_DIR}/CanIf_Cfg.c
  ${GEN_OUTPUT_DIR}/PduR_Cfg.c
  ${GEN_OUTPUT_DIR}/Uds_Data.h
  ${GEN_OUTPUT_DIR}/Uds_Data.c
  ${GEN_OUTPUT_DIR}/VehicleNetwork.dbc
  ${GEN_OUTPUT_DIR}/SIGNAL_DOC.md
)

add_custom_command(
  OUTPUT ${GEN_OUTPUTS}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_OUTPUT_DIR}
  COMMAND python3 ${GEN_SCRIPT} ${GEN_INPUT} ${GEN_OUTPUT_DIR}
  DEPENDS ${GEN_SCRIPT} ${GEN_INPUT}
  COMMENT "Generating configuration from config/ecu.json → generated/"
)

# ---- EX1: 단일 파일 연습 ----
set(EX1_SRCS
  ex1_embedded_c/src/ex1_1_bit_ops.c
  ex1_embedded_c/src/ex1_2_register_map.c
  ex1_embedded_c/src/ex1_3_ring_buffer.c
  ex1_embedded_c/src/ex1_4_fixed_point.c
  ex1_embedded_c/src/ex1_5_state_machine.c
)

foreach(src ${EX1_SRCS})
  get_filename_component(name ${src} NAME_WLE)
  add_executable(${name} ${src})
endforeach()

# ---- EX1: C++ 단일 파일 연습 ----
set(EX1_CPP_SRCS
  ex1_embedded_c/src/ex1_6_cpp_state_machine.cpp
  ex1_embedded_c/src/ex1_7_cpp_template_buffer.cpp
  ex1_embedded_c/src/ex1_8_cpp_fixed_point.cpp
)

foreach(src ${EX1_CPP_SRCS})
  get_filename_component(name ${src} NAME_WLE)
  add_executable(${name} ${src})
endforeach()

# ---- EX2: 미니 AUTOSAR (생성된 Cfg 포함) ----
file(GLOB EX2_SRC ex2_mini_autosar/src/*.c)

add_executable(ex2_signal_flow
  ex2_mini_autosar/test/test_ex2_signal_flow.c
  ${EX2_SRC}
  ${GEN_OUTPUT_DIR}/Com_Cfg.c
  ${GEN_OUTPUT_DIR}/CanIf_Cfg.c
  ${GEN_OUTPUT_DIR}/PduR_Cfg.c
)
add_dependencies(ex2_signal_flow gen_outputs)

add_executable(ex2_com
  ex2_mini_autosar/test/test_ex2_com.c
  ${EX2_SRC}
  ${GEN_OUTPUT_DIR}/Com_Cfg.c
  ${GEN_OUTPUT_DIR}/CanIf_Cfg.c
  ${GEN_OUTPUT_DIR}/PduR_Cfg.c
)
add_dependencies(ex2_com gen_outputs)

add_executable(ex2_simmcu
  ex2_mini_autosar/test/test_ex2_simmcu.c
  ex2_mini_autosar/src/SimMcu.c
)

add_executable(ex2_simbus
  ex2_mini_autosar/test/test_ex2_simbus.c
  ${EX2_SRC}
  ${GEN_OUTPUT_DIR}/Com_Cfg.c
  ${GEN_OUTPUT_DIR}/CanIf_Cfg.c
  ${GEN_OUTPUT_DIR}/PduR_Cfg.c
)
add_dependencies(ex2_simbus gen_outputs)

# ---- EX3: 통신/진단 ----
add_executable(ex3_can_signal
  ex3_comm_diag/test/test_ex3_can_signal.c
  ex3_comm_diag/src/can_signal.c
)

add_executable(ex3_isotp
  ex3_comm_diag/test/test_ex3_isotp.c
  ex3_comm_diag/src/vcan.c
  ex3_comm_diag/src/isotp.c
)

add_executable(ex3_uds
  ex3_comm_diag/test/test_ex3_uds.c
  ex3_comm_diag/src/uds_server.c
  ${GEN_OUTPUT_DIR}/Uds_Data.c
  ${GEN_OUTPUT_DIR}/Uds_Data.h
)
add_dependencies(ex3_uds gen_outputs)

# ---- EX4: 안전/보안 ----
add_executable(ex4_crc_e2e
  ex4_safety_security/test/test_ex4_crc_e2e.c
  ex4_safety_security/src/crc8.c
  ex4_safety_security/src/e2e_p01.c
)

add_executable(ex4_secoc
  ex4_safety_security/test/test_ex4_secoc.c
  ex4_safety_security/src/aes_mini.c
  ex4_safety_security/src/cmac.c
  ex4_safety_security/src/secoc.c
)

add_executable(ex4_wdg
  ex4_safety_security/test/test_ex4_wdg.c
  ex4_safety_security/src/wdg.c
)

# ---- CAPSTONE ----
file(GLOB EX3_SRC ex3_comm_diag/src/*.c)
file(GLOB EX4_SRC ex4_safety_security/src/*.c)

add_executable(capstone_demo
  capstone/main_capstone.c
  ${EX3_SRC}
  ${EX4_SRC}
  ${GEN_OUTPUT_DIR}/Uds_Data.c
  ${GEN_OUTPUT_DIR}/Uds_Data.h
)
add_dependencies(capstone_demo gen_outputs)

# ---- 사용자 편의 타겟 ----
add_custom_target(gen_outputs DEPENDS ${GEN_OUTPUTS})

add_custom_target(check
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  COMMENT "Running all test suites (ctest)"
)

add_custom_target(trace-check
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/decode_trace.py
    ${GEN_OUTPUT_DIR}/VehicleNetwork.dbc
    ${CMAKE_SOURCE_DIR}/logs/capstone_trace.log
  COMMENT "DBC trace decode verification"
  DEPENDS capstone_demo
)

# ---- 테스트 등록 (ctest용) ----
enable_testing()

foreach(t
  ex1_1_bit_ops ex1_2_register_map ex1_3_ring_buffer ex1_4_fixed_point ex1_5_state_machine
  ex1_6_cpp_state_machine ex1_7_cpp_template_buffer ex1_8_cpp_fixed_point
  ex2_signal_flow ex2_com ex2_simmcu ex2_simbus
  ex3_can_signal ex3_isotp ex3_uds
  ex4_crc_e2e ex4_secoc ex4_wdg
)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
