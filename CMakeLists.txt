# CMakeLists.txt
#
# Auto SW Academy — CMake 기반 빌드 (GNU Makefile 과 1:1 대응)
#
#   cmake -S . -B build-cmake
#   cmake --build build-cmake -j
#   (cd build-cmake && ctest --output-on-failure)   # 모든 테스트 스위트
#   cmake --build build-cmake --target trace-check  # DBC 트레이스 디코드 검증
#
# Makefile 과 동일하게 config/ecu.json 으로부터 generated/ 산출물을 생성한다.

cmake_minimum_required(VERSION 3.16)
project(auto_sw_academy C CXX)

# Python 의존성 (tools/gen_cfg.py, tools/decode_trace.py)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ---- 언어/경고 설정 ----
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -O0 -g)

# 모든 타겟이 공유하는 include 경로 (Makefile 의 -I 목록과 동일)
include_directories(
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/ex2_mini_autosar/src
    ${CMAKE_SOURCE_DIR}/ex3_comm_diag/src
    ${CMAKE_SOURCE_DIR}/ex4_safety_security/src
    ${CMAKE_BINARY_DIR}/generated
)

# 생성 산출물은 빌드 트리 하위 generated/ 에 둔다 (소스 트리 오염 방지).
set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})
set(GEN_OUTS
    ${GEN_DIR}/Cfg_Ids.h
    ${GEN_DIR}/Com_Cfg.c
    ${GEN_DIR}/CanIf_Cfg.c
    ${GEN_DIR}/PduR_Cfg.c
    ${GEN_DIR}/Uds_Data.h
    ${GEN_DIR}/Uds_Data.c
    ${GEN_DIR}/VehicleNetwork.dbc
    ${GEN_DIR}/SIGNAL_DOC.md
)

# 설정(단일 소스) -> C/DBC/문서 생성. Makefile 의 generated/.stamp 규칙과 동일.
add_custom_command(
    OUTPUT ${GEN_OUTS}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_cfg.py
            ${CMAKE_SOURCE_DIR}/config/ecu.json ${GEN_DIR}
    DEPENDS ${CMAKE_SOURCE_DIR}/config/ecu.json
            ${CMAKE_SOURCE_DIR}/tools/gen_cfg.py
    COMMENT "AUTOSAR 구성 생성 (config/ecu.json -> generated/)"
    VERBATIM
)
add_custom_target(gen ALL DEPENDS ${GEN_OUTS})

# 생성된 소스는 디스크에 실재하지 않으므로, CMake 에 "생성됨"을 알려준다.
set_source_files_properties(
    ${GEN_DIR}/Com_Cfg.c ${GEN_DIR}/CanIf_Cfg.c
    ${GEN_DIR}/PduR_Cfg.c ${GEN_DIR}/Uds_Data.c
    PROPERTIES GENERATED TRUE
)

# 모든 실행파일은 build/ 하위에 모은다 (Makefile 의 build/ 와 동일 위치).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build)

# EX2 BSW 전체 소스
file(GLOB EX2_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/ex2_mini_autosar/src/*.c")
set(EX2_GEN_CFG ${GEN_DIR}/Com_Cfg.c ${GEN_DIR}/CanIf_Cfg.c ${GEN_DIR}/PduR_Cfg.c)

# CTest 등록용: 타겟 이름 = 실행파일 이름, 작업 디렉터리는 소스 루트.
enable_testing()
function(add_named_test target)
    add_test(NAME ${target} COMMAND ${target} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# ---- EX1: 임베디드 C/C++ (단일 파일) ----
foreach(src
    ex1_embedded_c/src/ex1_1_bit_ops.c
    ex1_embedded_c/src/ex1_2_register_map.c
    ex1_embedded_c/src/ex1_3_ring_buffer.c
    ex1_embedded_c/src/ex1_4_fixed_point.c
    ex1_embedded_c/src/ex1_5_state_machine.c
    ex1_embedded_c/src/ex1_6_cpp_state_machine.cpp
    ex1_embedded_c/src/ex1_7_cpp_template_buffer.cpp
    ex1_embedded_c/src/ex1_8_cpp_fixed_point.cpp)
    get_filename_component(tname ${src} NAME_WE)
    add_executable(${tname} ${src})
    add_named_test(${tname})
endforeach()

# ---- EX2: 미니 AUTOSAR ----
add_executable(ex2_signal_flow ex2_mini_autosar/test/test_ex2_signal_flow.c ${EX2_SRC} ${EX2_GEN_CFG})
add_named_test(ex2_signal_flow)

add_executable(ex2_com ex2_mini_autosar/test/test_ex2_com.c ${EX2_SRC} ${EX2_GEN_CFG})
add_named_test(ex2_com)

add_executable(ex2_simmcu ex2_mini_autosar/test/test_ex2_simmcu.c ex2_mini_autosar/src/SimMcu.c)
add_named_test(ex2_simmcu)

add_executable(ex2_simbus ex2_mini_autosar/test/test_ex2_simbus.c ${EX2_SRC} ${EX2_GEN_CFG})
add_named_test(ex2_simbus)

# ---- EX3: 통신/진단 ----
add_executable(ex3_can_signal ex3_comm_diag/test/test_ex3_can_signal.c ex3_comm_diag/src/can_signal.c)
add_named_test(ex3_can_signal)

add_executable(ex3_isotp ex3_comm_diag/test/test_ex3_isotp.c
               ex3_comm_diag/src/vcan.c ex3_comm_diag/src/isotp.c)
add_named_test(ex3_isotp)

add_executable(ex3_uds ex3_comm_diag/test/test_ex3_uds.c
               ex3_comm_diag/src/uds_server.c ${GEN_DIR}/Uds_Data.c)
add_named_test(ex3_uds)

# ---- EX4: 안전/보안 ----
add_executable(ex4_crc_e2e ex4_safety_security/test/test_ex4_crc_e2e.c
               ex4_safety_security/src/crc8.c ex4_safety_security/src/e2e_p01.c)
add_named_test(ex4_crc_e2e)

add_executable(ex4_secoc ex4_safety_security/test/test_ex4_secoc.c
               ex4_safety_security/src/aes_mini.c ex4_safety_security/src/cmac.c ex4_safety_security/src/secoc.c)
add_named_test(ex4_secoc)

add_executable(ex4_wdg ex4_safety_security/test/test_ex4_wdg.c ex4_safety_security/src/wdg.c)
add_named_test(ex4_wdg)

# ---- CAPSTONE ----
file(GLOB EX3_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/ex3_comm_diag/src/*.c")
file(GLOB EX4_SRC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/ex4_safety_security/src/*.c")
add_executable(capstone_demo capstone/main_capstone.c ${EX3_SRC} ${EX4_SRC}
               ${GEN_DIR}/Uds_Data.c)

# ---- check: CTest 로 전체 18 스위트 실행 (Makefile check 대응) ----
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ex1_1_bit_ops ex1_2_register_map ex1_3_ring_buffer ex1_4_fixed_point
            ex1_5_state_machine ex1_6_cpp_state_machine ex1_7_cpp_template_buffer
            ex1_8_cpp_fixed_point
            ex2_signal_flow ex2_com ex2_simmcu ex2_simbus
            ex3_can_signal ex3_isotp ex3_uds
            ex4_crc_e2e ex4_secoc ex4_wdg
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "전체 테스트 스위트 실행 (CTest)")

# ---- trace-check: capstone 트레이스 -> DBC 디코드 (cantools / stdlib 이중 엔진) ----
set(LOG_DIR ${CMAKE_BINARY_DIR}/logs)
add_custom_target(trace-check
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LOG_DIR}
    COMMAND $<TARGET_FILE:capstone_demo> --trace ${LOG_DIR}/capstone_trace.log
            > ${LOG_DIR}/capstone_console.log
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/decode_trace.py
            ${GEN_DIR}/VehicleNetwork.dbc ${LOG_DIR}/capstone_trace.log
    DEPENDS capstone_demo gen
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "버스 트레이스 -> DBC 디코드 검증")

# ---- 설치 (Makefile 호환) ----
install(TARGETS
        ex1_1_bit_ops ex1_2_register_map ex1_3_ring_buffer ex1_4_fixed_point ex1_5_state_machine
        ex1_6_cpp_state_machine ex1_7_cpp_template_buffer ex1_8_cpp_fixed_point
        ex2_signal_flow ex2_com ex2_simmcu ex2_simbus
        ex3_can_signal ex3_isotp ex3_uds
        ex4_crc_e2e ex4_secoc ex4_wdg capstone_demo
        RUNTIME DESTINATION bin)
