# CMakeLists.txt

# Auto SW Academy — CMake 기반 빌드 (Makefile 호환)
# Makefile을 감싸는 방식: make gen, make check, make trace-check, make all

cmake_minimum_required(VERSION 3.16)
project(auto_sw_academy C CXX)

# Python 의존성 (tools/gen_cfg.py, tools/decode_trace.py)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# 빌드 타입 설정
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 경고 설정
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O0 -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O0 -g")

# 빌드 디렉토리
set(BUILD_DIR ${CMAKE_BINARY_DIR}/build)
file(MAKE_DIRECTORY ${BUILD_DIR})

# ---- EX1: 임베디드 C/C++ ----
file(GLOB EX1_SRC "ex1_embedded_c/src/*.c")
file(GLOB EX1_CPP "ex1_embedded_c/src/*.cpp")

# C 타겟
foreach(src ${EX1_SRC})
    get_filename_component(target_name ${src} NAME_WE)
    add_executable(${target_name} ${src})
    target_include_directories(${target_name} PRIVATE common generated)
    set_target_properties(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
endforeach()

# C++ 타겟
foreach(src ${EX1_CPP})
    get_filename_component(target_name ${src} NAME_WE)
    add_executable(${target_name} ${src})
    target_include_directories(${target_name} PRIVATE common generated)
    set_target_properties(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
endforeach()

# ---- EX2: 미니 AUTOSAR ----
# 생성된 구성(Cfg) 먼저 생성
add_custom_command(
    OUTPUT generated/Com_Cfg.c generated/CanIf_Cfg.c generated/PduR_Cfg.c
    COMMAND ${Python3_EXECUTABLE} tools/gen_cfg.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "AUTOSAR 구성 생성"
)

file(GLOB EX2_SRC "ex2_mini_autosar/src/*.c")
file(GLOB EX2_TEST "ex2_mini_autosar/test/*.c")

# EX2 테스트 타겟
add_executable(ex2_signal_flow ex2_mini_autosar/test/test_ex2_signal_flow.c ${EX2_SRC} generated/Com_Cfg.c generated/CanIf_Cfg.c generated/PduR_Cfg.c)
target_include_directories(ex2_signal_flow PRIVATE common generated ex2_mini_autosar/src)
set_target_properties(ex2_signal_flow PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

add_executable(ex2_com ex2_mini_autosar/test/test_ex2_com.c ${EX2_SRC} generated/Com_Cfg.c generated/CanIf_Cfg.c generated/PduR_Cfg.c)
target_include_directories(ex2_com PRIVATE common generated ex2_mini_autosar/src)
set_target_properties(ex2_com PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

add_executable(ex2_simbus ex2_mini_autosar/test/test_ex2_simbus.c ${EX2_SRC} generated/Com_Cfg.c generated/CanIf_Cfg.c generated/PduR_Cfg.c)
target_include_directories(ex2_simbus PRIVATE common generated ex2_mini_autosar/src)
set_target_properties(ex2_simbus PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

# ---- EX3: 통신/진단 ----
file(GLOB EX3_SRC "ex3_comm_diag/src/*.c")
file(GLOB EX3_TEST "ex3_comm_diag/test/*.c")

add_executable(ex3_can_signal ex3_comm_diag/test/test_ex3_can_signal.c ex3_comm_diag/src/can_signal.c)
target_include_directories(ex3_can_signal PRIVATE ex3_comm_diag/src)
set_target_properties(ex3_can_signal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

add_executable(ex3_isotp ex3_comm_diag/test/test_ex3_isotp.c ex3_comm_diag/src/vcan.c ex3_comm_diag/src/isotp.c)
target_include_directories(ex3_isotp PRIVATE ex3_comm_diag/src)
set_target_properties(ex3_isotp PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

add_executable(ex3_uds ex3_comm_diag/test/test_ex3_uds.c ex3_comm_diag/src/uds_server.c generated/Uds_Data.c generated/Uds_Data.h)
target_include_directories(ex3_uds PRIVATE ex3_comm_diag/src generated)
set_target_properties(ex3_uds PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

# ---- EX4: 안전/보안 ----
file(GLOB EX4_SRC "ex4_safety_security/src/*.c")
file(GLOB EX4_TEST "ex4_safety_security/test/*.c")

add_executable(ex4_crc_e2e ex4_safety_security/test/test_ex4_crc_e2e.c ex4_safety_security/src/crc8.c ex4_safety_security/src/e2e_p01.c)
target_include_directories(ex4_crc_e2e PRIVATE ex4_safety_security/src)
set_target_properties(ex4_crc_e2e PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

add_executable(ex4_secoc ex4_safety_security/test/test_ex4_secoc.c ex4_safety_security/src/aes_mini.c ex4_safety_security/src/cmac.c ex4_safety_security/src/secoc.c)
target_include_directories(ex4_secoc PRIVATE ex4_safety_security/src)
set_target_properties(ex4_secoc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

add_executable(ex4_wdg ex4_safety_security/test/test_ex4_wdg.c ex4_safety_security/src/wdg.c)
target_include_directories(ex4_wdg PRIVATE ex4_safety_security/src)
set_target_properties(ex4_wdg PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

# ---- CAPSTONE ----
file(GLOB CAPSTONE_SRC "capstone/*.c")
add_executable(capstone_demo capstone/main_capstone.c ${EX3_SRC} ${EX4_SRC} generated/Uds_Data.c generated/Uds_Data.h)
target_include_directories(capstone_demo PRIVATE capstone generated)
set_target_properties(capstone_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})

# ---- 도구 ----
add_custom_target(gen
    COMMAND ${Python3_EXECUTABLE} tools/gen_cfg.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "AUTOSAR 구성 생성"
)

add_custom_target(check
    COMMAND ${Python3_EXECUTABLE} -m pytest tools/ -v
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS gen
    COMMENT "모든 테스트 실행"
)

add_custom_target(trace-check
    COMMAND ${Python3_EXECUTABLE} tools/decode_trace.py generated/VehicleNetwork.dbc logs/capstone_trace.log
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS gen
    COMMENT "버스 트레이스 디코드 검증"
)

# 편의상 Makefile 호환 타겟
add_custom_target(all DEPENDS ex2_signal_flow ex2_com ex2_simbus ex3_can_signal ex3_isotp ex3_uds ex4_crc_e2e ex4_secoc ex4_wdg capstone_demo)

# 설치
install(TARGETS ex2_signal_flow ex2_com ex2_simbus ex3_can_signal ex3_isotp ex3_uds ex4_crc_e2e ex4_secoc ex4_wdg capstone_demo
        RUNTIME DESTINATION bin)
