#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
report_coverage.py - ISO 26262 / ASPICE 대응 구조적 코드 커버리지(Line Coverage) 리포트 생성기

[100% 취업 경쟁력 강화 권고 ①]
  - gcc/g++ 의 '--coverage' (-fprofile-arcs -ftest-coverage) 옵션으로 컴파일된
    18개 테스트 스위트의 실행 프로파일(.gcda/.gcno)을 기반으로 전체 소스코드의
    Line Coverage 를 수집·통계화한다.
  - ISO 26262 ASIL-B/D 권장 기준인 90% 이상을 자동으로 검증하며,
    현업 독일 Tier-1 / 툴 벤더 제출용 검증 리포트(generated/coverage_report.md)를 생성한다.

사용법:
  make coverage
  # 또는 수동 실행:
  python3 tools/report_coverage.py [--out generated/coverage_report.md]
"""

import argparse
import glob
import os
import re
import subprocess
import sys


def parse_gcov_file(gcov_path, cov_map):
    """단일 .gcov 파일을 파싱하여 cov_map[filename][lineno] = count 를 업데이트한다."""
    try:
        with open(gcov_path, "r", encoding="utf-8", errors="ignore") as f:
            src_name = None
            for line in f:
                line_str = line.strip()
                if line_str.startswith("-:") and "Source:" in line_str:
                    src_name = line_str.split("Source:", 1)[1].strip()
                    # 백슬래시를 슬래시로 정규화
                    src_name = src_name.replace("\\", "/")
                    if src_name not in cov_map:
                        cov_map[src_name] = {}
                elif src_name:
                    parts = line_str.split(":", 2)
                    if len(parts) >= 3:
                        cnt_str = parts[0].strip()
                        try:
                            lineno = int(parts[1].strip())
                        except ValueError:
                            continue
                        if cnt_str == "-" or lineno == 0:
                            continue
                        if cnt_str == "#####":
                            cnt = 0
                        else:
                            try:
                                cnt = int(cnt_str.rstrip("*"))
                            except ValueError:
                                cnt = 1
                        if lineno not in cov_map[src_name]:
                            cov_map[src_name][lineno] = 0
                        cov_map[src_name][lineno] += cnt
    except Exception as e:
        print(f"[warning] gcov file read error: {gcov_path}: {e}", file=sys.stderr)


def collect_coverage(build_dir="build"):
    """build 디렉토리 내 모든 .gcda 파일에 대해 gcov를 수행하고 누적 커버리지를 반환한다."""
    gcda_files = glob.glob(os.path.join(build_dir, "*.gcda"))
    if not gcda_files:
        print(f"[report_coverage] Error: No .gcda files found in '{build_dir}'.", file=sys.stderr)
        print("                  Please run 'make coverage' or compile tests with '--coverage' first.", file=sys.stderr)
        return None

    cov_map = {}
    for gcda in gcda_files:
        # gcov 실행 (출력 숨김)
        subprocess.run(
            ["gcov", "--object-directory", build_dir, gcda],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        # 생성된 .gcov 파싱 후 삭제
        gcov_files = glob.glob("*.gcov")
        for gf in gcov_files:
            parse_gcov_file(gf, cov_map)
            try:
                os.remove(gf)
            except OSError:
                pass

    return cov_map


def categorize_file(filepath):
    """소스 파일 경로에 따라 단계(Stage)를 분류한다."""
    if "/test/" in filepath:
        return None  # 테스트 소스는 대상에서 제외 (실제 구현 소스만 측정)
    if "ex1_embedded_c/" in filepath:
        return "1. Embedded C/C++"
    if "ex2_mini_autosar/" in filepath:
        return "2. AUTOSAR MiniECU"
    if "ex3_comm_diag/" in filepath:
        return "3. Comm & Diag Stack"
    if "ex4_safety_security/" in filepath:
        return "4. Safety & Security"
    if "capstone/" in filepath:
        return "5. Capstone ECU Demo"
    return None


def generate_reports(cov_map, out_path="generated/coverage_report.md", target_pct=90.0):
    """콘솔 ASCII 테이블과 Markdown 리포트를 함께 생성한다."""
    grouped = {
        "1. Embedded C/C++": [],
        "2. AUTOSAR MiniECU": [],
        "3. Comm & Diag Stack": [],
        "4. Safety & Security": [],
        "5. Capstone ECU Demo": [],
    }

    total_exec_lines = 0
    total_cov_lines = 0

    for filepath, lines in sorted(cov_map.items()):
        category = categorize_file(filepath)
        if not category:
            continue
        exec_count = len(lines)
        cov_count = sum(1 for cnt in lines.values() if cnt > 0)
        pct = (cov_count / exec_count * 100.0) if exec_count > 0 else 100.0
        grouped[category].append((filepath, cov_count, exec_count, pct))
        total_exec_lines += exec_count
        total_cov_lines += cov_count

    overall_pct = (total_cov_lines / total_exec_lines * 100.0) if total_exec_lines > 0 else 100.0

    # ----- 1) 콘솔 출력 -----
    print()
    print("==================================================================================")
    print("           ISO 26262 / ASPICE Structural Code Coverage (Line Coverage)            ")
    print("==================================================================================")
    print(f" {'Module / Stage':<22} | {'Source File':<32} | {'Covered / Total':<15} | {'Coverage':<8}")
    print("-----------------------+----------------------------------+-----------------+----------")

    for stage, items in grouped.items():
        if not items:
            continue
        stage_cov = sum(x[1] for x in items)
        stage_total = sum(x[2] for x in items)
        stage_pct = (stage_cov / stage_total * 100.0) if stage_total > 0 else 100.0
        for i, (fp, cov, tot, pct) in enumerate(items):
            stage_str = stage if i == 0 else ""
            fname = os.path.basename(fp)
            print(f" {stage_str:<22} | {fname:<32} | {cov:>5} / {tot:<7} | {pct:>6.1f}%")
        print("-----------------------+----------------------------------+-----------------+----------")

    total_files = sum(len(v) for v in grouped.values())
    status_icon = "✔ PASS (ASIL Target Met)" if overall_pct >= target_pct else "✘ FAIL"
    print(f" {'OVERALL TOTAL':<22} | {f'All {total_files} Implementation Sources':<32} | {total_cov_lines:>5} / {total_exec_lines:<7} | {overall_pct:>6.2f}%")
    print("==================================================================================")
    print(f" Target (ISO 26262 ASIL-B/D Goal): >= {target_pct:.1f}% --> Status: {status_icon}")
    print("==================================================================================")
    print()

    # ----- 2) Markdown 리포트 생성 -----
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as md:
        md.write("# 📊 ISO 26262 / ASPICE Structural Code Coverage Report\n\n")
        md.write("> **프로젝트**: Auto SW Academy (`auto_sw_academy`)\n")
        md.write("> **검증 대상**: 18개 테스트 스위트 (EX1~EX4 + Capstone 통합 데모)\n")
        md.write(f"> **목표 커버리지**: ISO 26262 ASIL-B/D 권장 기준 (**{target_pct:.1f}% 이상**)\n")
        md.write(f"> **달성 결과**: **{overall_pct:.2f}% ({total_cov_lines} / {total_exec_lines} lines)** — **{status_icon}**\n\n")

        md.write("## 1. 계층별 커버리지 요약 (Summary by Layer)\n\n")
        md.write("| Stage / Module | Source Files | Covered Lines | Total Executable Lines | Line Coverage |\n")
        md.write("|---|---|---:|---:|---:|\n")
        for stage, items in grouped.items():
            if not items:
                continue
            stage_cov = sum(x[1] for x in items)
            stage_total = sum(x[2] for x in items)
            stage_pct = (stage_cov / stage_total * 100.0) if stage_total > 0 else 100.0
            md.write(f"| **{stage}** | {len(items)} | {stage_cov} | {stage_total} | **{stage_pct:.1f}%** |\n")
        md.write(f"| **TOTAL** | **{sum(len(v) for v in grouped.values())}** | **{total_cov_lines}** | **{total_exec_lines}** | **{overall_pct:.2f}%** |\n\n")

        md.write("## 2. 모듈별 상세 커버리지 (Detailed Source Coverage)\n\n")
        md.write("| Module / Layer | Source File Path | Covered / Executable | Coverage (%) | Status |\n")
        md.write("|---|---|---:|---:|:---:|\n")
        for stage, items in grouped.items():
            for fp, cov, tot, pct in items:
                status = "✔ PASS" if pct >= 80.0 else "⚠️ REVIEW"
                md.write(f"| {stage} | `{fp}` | {cov} / {tot} | **{pct:.1f}%** | {status} |\n")
        md.write("\n---\n")
        md.write("*Generated by `tools/report_coverage.py` automatically after test execution.*\n")

    print(f"[report_coverage] Saved markdown verification report to '{out_path}'.")
    return overall_pct >= target_pct


def main():
    parser = argparse.ArgumentParser(description="Generate structural code coverage report.")
    parser.add_argument("--out", default="generated/coverage_report.md", help="Output Markdown file path")
    parser.add_argument("--target", type=float, default=90.0, help="Target minimum coverage percentage (default: 90.0)")
    args = parser.parse_args()

    cov_map = collect_coverage("build")
    if cov_map is None:
        sys.exit(1)

    success = generate_reports(cov_map, out_path=args.out, target_pct=args.target)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
