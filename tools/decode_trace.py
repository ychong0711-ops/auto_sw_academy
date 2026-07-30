#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
decode_trace.py - 버스 트레이스를 DBC 로 해석하는 미니 CANoe Trace Window

[경로 C] 툴 체인 경험의 마지막 고리:
  config/ecu.json --(gen_cfg.py)--> generated/VehicleNetwork.dbc
  capstone_demo --trace ----------> logs/capstone_trace.log
  decode_trace.py ----------------> 사람이 읽는 신호 값 (물리량)

사용법:
  python3 tools/decode_trace.py generated/VehicleNetwork.dbc logs/capstone_trace.log
  python3 tools/decode_trace.py --stdlib generated/VehicleNetwork.dbc logs/capstone_trace.log

엔진 2종:
  * 기본   : cantools (pip install cantools) 를 DBC *파서* 로 사용
  * --stdlib: 외부 패키지 없이 이 파일 내장 미니 파서 사용 (stdlib only)
두 엔진은 동일한 렌더링 경로를 타므로 출력이 동일해야 한다(교차 검증 포인트).

트레이스 형식 (vcan.c VCan_TraceOpen() 이 기록):
  # 주석
  <ms 십진> <ID 16진> <DLC 십진> <DATA hex (2*DLC자)>
"""
import argparse
import re
import sys
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# 공통 데이터 모델 (두 파서 엔진이 이 구조로 변환된다)
# ---------------------------------------------------------------------------
@dataclass
class Signal:
    name: str
    start: int          # DBC 비트 번호 (Motorola 는 sawtooth 체계의 MSB 위치)
    length: int
    byte_order: str     # 'intel' | 'motorola'
    factor: float = 1.0
    offset: float = 0.0
    vmin: float = 0.0
    vmax: float = 0.0
    unit: str = ""
    is_signed: bool = False


@dataclass
class Message:
    can_id: int
    name: str
    dlc: int
    sender: str
    signals: list = field(default_factory=list)


class DbcError(Exception):
    pass


# ---------------------------------------------------------------------------
# 엔진 1: cantools 를 파서로 사용 (신설된 의존성, 있을 때만)
# ---------------------------------------------------------------------------
def load_dbc_cantools(path):
    import cantools  # 지연 임포트: --stdlib 경로에서는 필요 없음
    dbc = cantools.database.load_file(path)
    out = {}
    for m in dbc.messages:
        msg = Message(m.frame_id, m.name, m.length, ",".join(m.senders) or "?")
        for s in m.signals:
            # cantools 는 big_endian 의 start 를 DBC 파일의 sawtooth 번호 그대로 유지한다
            msg.signals.append(Signal(
                name=s.name,
                start=s.start,
                length=s.length,
                byte_order="motorola" if s.byte_order == "big_endian" else "intel",
                factor=float(s.scale), offset=float(s.offset),
                vmin=float(s.minimum) if s.minimum is not None else 0.0,
                vmax=float(s.maximum) if s.maximum is not None else 0.0,
                unit=s.unit or "",
                is_signed=s.is_signed,
            ))
        out[msg.can_id] = msg
    return out


# ---------------------------------------------------------------------------
# 엔진 2: stdlib 미니 DBC 파서 (우리 생성기가 만드는 DBC 부분집합 + 주석)
#   BO_ <id> <name>: <dlc> <sender>
#    SG_ <name> : <start>|<len>@<order><sign> (<factor>,<offset>) [<min>|<max>] "<unit>" <rx>
# ---------------------------------------------------------------------------
_RE_BO = re.compile(r"^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+)")
_RE_SG = re.compile(
    r'^\s*SG_\s+(\w+)\s*:\s*(\d+)\|(\d+)@(\d)([+-])\s*'
    r'\(\s*([-\d.eE]+)\s*,\s*([-\d.eE]+)\s*\)\s*'
    r'\[\s*([-\d.eE]+)\s*\|\s*([-\d.eE]+)\s*\]\s*'
    r'"([^"]*)"\s*(.*)$'
)


def load_dbc_stdlib(path):
    out = {}
    cur = None
    with open(path, encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            m = _RE_BO.match(line)
            if m:
                cur = Message(int(m.group(1)), m.group(2), int(m.group(3)), m.group(4))
                if cur.can_id in out:
                    raise DbcError(f"{path}:{ln}: ID 0x{cur.can_id:X} 중복 정의")
                out[cur.can_id] = cur
                continue
            s = _RE_SG.match(line)
            if s:
                if cur is None:
                    raise DbcError(f"{path}:{ln}: BO_ 없이 SG_ 가 나옴")
                cur.signals.append(Signal(
                    name=s.group(1), start=int(s.group(2)), length=int(s.group(3)),
                    byte_order="intel" if s.group(4) == "1" else "motorola",  # DBC 규약: @1=Intel, @0=Motorola
                    factor=float(s.group(6)), offset=float(s.group(7)),
                    vmin=float(s.group(8)), vmax=float(s.group(9)),
                    unit=s.group(10), is_signed=(s.group(5) == "-"),
                ))
    if not out:
        raise DbcError(f"{path}: BO_ 메시지를 하나도 찾지 못함")
    return out


# ---------------------------------------------------------------------------
# 비트 코덱 — ex3_comm_diag/src/can_signal.c 와 정확히 같은 규칙
#   Motorola: 첫 비트(MSB) 는 start, 다음 위치 = pos%8==0 ? pos+15 : pos-1
#   Intel   : LSB 부터 start, 이후 +1, +2, ...
# ---------------------------------------------------------------------------
def _bit_positions(sig, dlc):
    pos = sig.start
    out = []
    for _ in range(sig.length):
        if pos >= dlc * 8:
            raise DbcError(f"신호 {sig.name}: 비트 {pos} 가 DLC({dlc}) 범위 밖")
        out.append(pos)
        pos = pos + 1 if sig.byte_order == "intel" else (pos + 15 if pos % 8 == 0 else pos - 1)
    return out


def extract_raw(data, sig):
    raw = 0
    for i, pos in enumerate(_bit_positions(sig, len(data))):
        bit = (data[pos >> 3] >> (pos & 7)) & 1
        if sig.byte_order == "intel":
            raw |= bit << i
        else:
            raw |= bit << (sig.length - 1 - i)
    if sig.is_signed and raw & (1 << (sig.length - 1)):
        raw -= 1 << sig.length
    return raw


def validate_db(db):
    """gen_cfg.py 와 동일한 안전 검사: 모든 신호 비트가 프레임 안에 있어야 함"""
    for msg in db.values():
        for sig in msg.signals:
            _bit_positions(sig, msg.dlc)


# ---------------------------------------------------------------------------
# 렌더링 (두 엔진 공용 — 출력 동일성의 근거)
# ---------------------------------------------------------------------------
def _fmt_num(x):
    return f"{x:.6g}"


def render_frame(db, can_id, data):
    """한 프레임의 해석 결과 문자열 리스트를 반환"""
    msg = db.get(can_id)
    hexs = " ".join(f"{b:02X}" for b in data)
    if msg is None:
        hint = ""
        if 0x7E0 <= can_id <= 0x7EF:
            hint = "  <- ISO-TP/UDS 진단 추정 (실무: DBC 가 아니라 ODX/PDX 로 정의)"
        return [f"RAW  {hexs}   (DBC 미등록 ID){hint}"]
    if len(data) != msg.dlc:
        return [f"{msg.name}: DLC 불일치 (기대 {msg.dlc}, 실제 {len(data)})  RAW {hexs}"]
    lines = [f"{msg.name} ({msg.dlc}B, 송신:{msg.sender})  {hexs}"]
    for sig in msg.signals:
        raw = extract_raw(data, sig)
        phys = raw * sig.factor + sig.offset
        unit = f" {sig.unit}" if sig.unit else ""
        lines.append(f"    {sig.name} = {_fmt_num(phys)}{unit}   (raw={raw}, [{_fmt_num(sig.vmin)}..{_fmt_num(sig.vmax)}]{unit})")
    return lines


def decode_trace(db, trace_path, out=sys.stdout):
    n_total = n_decoded = 0
    counts = {}
    with open(trace_path, encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            tok = line.split()
            if len(tok) != 4:
                print(f"[decode_trace][경고] {trace_path}:{ln}: 형식 오류, 건너뜀: {line}", file=sys.stderr)
                continue
            try:
                ms = int(tok[0], 10)
                can_id = int(tok[1], 16)
                dlc = int(tok[2], 10)
                data = bytes.fromhex(tok[3])
            except ValueError:
                print(f"[decode_trace][경고] {trace_path}:{ln}: 숫자 해석 실패, 건너뜀: {line}", file=sys.stderr)
                continue
            if len(data) != dlc:
                print(f"[decode_trace][경고] {trace_path}:{ln}: DLC/데이터 길이 불일치, 건너뜀: {line}", file=sys.stderr)
                continue
            msg = db.get(can_id)
            name = msg.name if msg else f"0x{can_id:03X}"
            counts[name] = counts.get(name, 0) + 1
            n_total += 1
            n_decoded += 1 if msg else 0
            head = f"[{ms:>7} ms] 0x{can_id:03X} "
            lines = render_frame(db, can_id, data)
            lines[0] = head + lines[0]
            lines[1:] = [" " * len(head) + s for s in lines[1:]]
            print("\n".join(lines), file=out)
    print("", file=out)
    print(f"== 요약 == 총 {n_total} 프레임 | DBC 해석 {n_decoded} | RAW {n_total - n_decoded}", file=out)
    for name, cnt in sorted(counts.items()):
        print(f"   {name:<24} x{cnt}", file=out)
    return 0


def main():
    ap = argparse.ArgumentParser(description="버스 트레이스를 DBC 로 해석 (미니 CANoe Trace)")
    ap.add_argument("dbc", help="DBC 파일 (예: generated/VehicleNetwork.dbc)")
    ap.add_argument("trace", help="트레이스 파일 (예: logs/capstone_trace.log)")
    ap.add_argument("--stdlib", action="store_true",
                    help="cantools 대신 내장 stdlib 파서 사용 (패키지 설치 불가 환경용)")
    args = ap.parse_args()

    try:
        if args.stdlib:
            db = load_dbc_stdlib(args.dbc)
            engine = "stdlib 내장 파서"
        else:
            try:
                db = load_dbc_cantools(args.dbc)
                engine = "cantools"
            except ImportError:
                print("[decode_trace] cantools 미설치 -> stdlib 내장 파서로 자동 전환 "
                      "(pip install cantools 가능, --stdlib 로 강제 가능)", file=sys.stderr)
                db = load_dbc_stdlib(args.dbc)
                engine = "stdlib 내장 파서(자동 전환)"
        validate_db(db)
    except (OSError, DbcError) as e:
        print(f"[decode_trace][오류] DBC 로딩 실패: {e}", file=sys.stderr)
        return 1

    print(f"[decode_trace] DBC={args.dbc} ({len(db)} 메시지, 엔진: {engine})")
    try:
        return decode_trace(db, args.trace)
    except OSError as e:
        print(f"[decode_trace][오류] 트레이스 읽기 실패: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
