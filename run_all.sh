#!/usr/bin/env bash
# 전체 빌드 + 전체 테스트 + 통합 데모 실행
set -euo pipefail
cd "$(dirname "$0")"
echo "==> 빌드"; make -j4
echo; echo "==> 전체 테스트"; make check
echo; echo "==> 통합 데모 (capstone) — 자세한 로그는 capstone 출력 참조"
./build/capstone_demo --trace logs/capstone_trace.log | tee capstone_output.txt
echo; echo "==> 트레이스 DBC 디코드 (미니 CANoe 워크플로)"
make trace-check
