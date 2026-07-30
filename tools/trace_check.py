#!/usr/bin/env python3
"""trace_check.py — Dual-engine DBC decode verification.

Usage:
    python3 tools/trace_check.py <capstone_exe> <dbc_file> <project_root>

Replicates 'make trace-check' logic:
  1. Run capstone_demo --trace to generate bus trace
  2. Decode trace with cantools engine → decoded_cantools.log
  3. Decode trace with stdlib engine   → decoded_stdlib.log
  4. Compare both outputs (skip header row)
  5. Verify expected signal values are decoded

Exit code 0 = all checks pass, 1 = any check fails.
"""

import subprocess
import sys
import os


def main() -> None:
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <capstone_exe> <dbc_file> <project_root>", file=sys.stderr)
        sys.exit(1)

    capstone_exe = sys.argv[1]
    dbc_file = sys.argv[2]
    project_root = sys.argv[3]
    logs_dir = os.path.join(project_root, "logs")
    trace_log = os.path.join(logs_dir, "capstone_trace.log")
    decoded_cantools = os.path.join(logs_dir, "decoded_cantools.log")
    decoded_stdlib = os.path.join(logs_dir, "decoded_stdlib.log")
    decode_script = os.path.join(project_root, "tools", "decode_trace.py")

    os.makedirs(logs_dir, exist_ok=True)

    # Step 1: Run capstone_demo in trace mode
    print(f"[trace-check] Running: {capstone_exe} --trace {trace_log}")
    result = subprocess.run([capstone_exe, "--trace", trace_log],
                            capture_output=True, text=True)
    with open(os.path.join(logs_dir, "capstone_console.log"), "w") as f:
        f.write(result.stdout)
    if result.returncode != 0:
        print(f"[trace-check] capstone_demo failed (exit {result.returncode})", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        sys.exit(1)

    # Step 2: Decode with cantools engine
    with open(decoded_cantools, "w") as f:
        subprocess.run([sys.executable, decode_script, dbc_file, trace_log],
                       stdout=f, check=True)
    print(f"[trace-check] cantools decode → {decoded_cantools}")

    # Step 3: Decode with stdlib engine
    with open(decoded_stdlib, "w") as f:
        subprocess.run([sys.executable, decode_script, "--stdlib", dbc_file, trace_log],
                       stdout=f, check=True)
    print(f"[trace-check] stdlib decode    → {decoded_stdlib}")

    # Step 4: Compare outputs (skip header line)
    with open(decoded_cantools) as f:
        c_lines = f.readlines()
    with open(decoded_stdlib) as f:
        s_lines = f.readlines()

    if c_lines[1:] == s_lines[1:]:
        print("[trace-check] cantools vs stdlib: output match ✔")
    else:
        print("[trace-check] MISMATCH: cantools vs stdlib outputs differ ✘", file=sys.stderr)
        for i, (cl, sl) in enumerate(zip(c_lines[1:], s_lines[1:]), start=2):
            if cl != sl:
                print(f"  Line {i}: cantools={cl.rstrip()}  stdlib={sl.rstrip()}", file=sys.stderr)
        sys.exit(1)

    # Step 5: Verify expected signal values
    content = "".join(s_lines)
    checks = [
        ("BatteryVoltage = 12.8 V", "Expected signal BatteryVoltage=12.8V"),
        ("VehicleState ", "Message aggregation (VehicleState)"),
    ]
    for expected, label in checks:
        if expected in content:
            print(f"[trace-check] {label} ✔")
        else:
            print(f"[trace-check] {label} ✘ — not found in decode output", file=sys.stderr)
            sys.exit(1)

    print("[trace-check] ALL CHECKS PASSED ✔")


if __name__ == "__main__":
    main()
