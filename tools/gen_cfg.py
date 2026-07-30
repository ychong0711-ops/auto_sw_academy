#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_cfg.py - Auto SW Academy 미니 설정 생성 툴
실무의 Vector DaVinci / EB tresos 가 하는 일의 축소판:
    설정(JSON)  ->  검증(validate)  ->  C 코드 생성 + DBC + 문서
입력:  config/ecu.json
출력:  generated/Cfg_Ids.h   : ID 매크로 (모든 심볼의 단일 소스)
       generated/Com_Cfg.c   : Com 신호 설정 테이블
       generated/CanIf_Cfg.c : CAN If PDU 매핑
       generated/PduR_Cfg.c  : 라우팅 테이블
       generated/Uds_Data.h/c: UDS DID 카탈로그 + 정적 데이터
       generated/VehicleNetwork.dbc : CANdb 포맷
       generated/SIGNAL_DOC.md      : 신호 문서
실패 시: 오류 목록 출력 후 종료코드 1 (툴의 검증 단계 재현)
"""
import json
import os
import sys

def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)

# ---------------------------------------------------------------- 검증
def bit_positions(start, length, order):
    pos = start
    out = []
    for _ in range(length):
        out.append(pos)
        if order == "little":
            pos += 1
        else:  # big(Motorola): 경계(pos%8==0)에서 +15, 아니면 -1
            pos = pos + 15 if pos % 8 == 0 else pos - 1
    return out

def validate(cfg, errors, warns):
    can_ids = {}
    for fr in cfg["can_frames"]:
        name = fr["name"]
        if not (1 <= fr["dlc"] <= 8):
            errors.append(f"{name}: DLC {fr['dlc']} — classic CAN 은 1..8")
        if fr["direction"] == "TX" and fr.get("period_ms", 0) <= 0:
            warns.append(f"{name}: TX frame 에 period_ms 없음 — 이벤트 송신 가정")
        if fr["can_id"] in can_ids:
            errors.append(f"CAN ID {fr['can_id']} 중복: {can_ids[fr['can_id']]} / {name}")
        can_ids[fr["can_id"]] = name

        used = {}
        for sig in fr["signals"]:
            s = sig["name"]
            if not (1 <= sig["length"] <= 32):
                errors.append(f"{name}.{s}: length {sig['length']} — 1..32 범위 밖")
                continue
            for p in bit_positions(sig["start_bit"], sig["length"], sig.get("byte_order", "little")):
                if p >= fr["dlc"] * 8:
                    errors.append(f"{name}.{s}: bit {p} 가 DLC({fr['dlc']}) 밖 (start={sig['start_bit']}, len={sig['length']})")
                elif p in used:
                    errors.append(f"{name}: 신호 겹침 — {used[p]} / {s} (bit {p})")
                used[p] = s

    seen_did = {}
    for d in cfg["uds"]["dids"]:
        if d["id"] in seen_did:
            errors.append(f"DID {d['id']} 중복")
        seen_did[d["id"]] = d["name"]
        if d["kind"] == "static":
            if d.get("value_kind") == "ascii" and not d.get("value"):
                errors.append(f"{d['name']}: static value 누락")
            if d["name"] == "VIN" and len(d.get("value", "")) != 17:
                errors.append(f"VIN 길이 {len(d.get('value',''))} — 반드시 17")
        else:
            if not (1 <= d.get("length", 0) <= 4):
                errors.append(f"{d['name']}: dynamic DID 길이 {d.get('length')} — 1..4")
        if d["access"] != "read" and d["kind"] == "static":
            warns.append(f"{d['name']}: static DID 쓰기 요청 — 보안 정책 확인 필요")

# ---------------------------------------------------------------- 생성
GEN_TAG = "/* 자동 생성 by tools/gen_cfg.py — 직접 수정 금지. 변경은 config/ecu.json 후 재생성 */\n"

def ids_header(cfg):
    tx = [f for f in cfg["can_frames"] if f["direction"] == "TX"]
    rx = [f for f in cfg["can_frames"] if f["direction"] == "RX"]
    s = [GEN_TAG, "#ifndef CFG_IDS_H\n#define CFG_IDS_H", "#include \"ComStack_Types.h\"", ""]
    s.append("/* ---- CanIf PDU IDs ---- */")
    for i, f in enumerate(tx):
        s.append(f"#define CANIF_TXPDU_{f['macro']}   ((PduIdType){i}u)")
    for i, f in enumerate(rx):
        s.append(f"#define CANIF_RXPDU_{f['macro']}   ((PduIdType){i}u)")
    s.append("\n/* ---- PduR 경로 상의 Com 측 PDU IDs ---- */")
    for i, f in enumerate(tx):
        s.append(f"#define PDUR_COM_TXPDU_{f['macro']}  ((PduIdType){i}u)")
    for i, f in enumerate(rx):
        s.append(f"#define PDUR_COM_RXPDU_{f['macro']}  ((PduIdType){i}u)")
    s.append("\n/* ---- Com 모듈이 쓰는 PDU ID 별칭 ---- */")
    for i, f in enumerate(tx):
        s.append(f"#define COM_TXPDU_{f['name']}  PDUR_COM_TXPDU_{f['macro']}")
    for i, f in enumerate(rx):
        s.append(f"#define COM_RXPDU_{f['name']}  PDUR_COM_RXPDU_{f['macro']}")
    s.append("\n/* ---- Com 신호 IDs (TX 신호 다음, RX 신호 순) ---- */")
    n = 0
    for f in tx + rx:
        for sig in f["signals"]:
            s.append(f"#define COM_SIG_{sig['name']}  ({n}u)")
            n += 1
    s.append("\n#endif /* CFG_IDS_H */")
    return "\n".join(s) + "\n"

def com_cfg(cfg):
    tx = [f for f in cfg["can_frames"] if f["direction"] == "TX"]
    rx = [f for f in cfg["can_frames"] if f["direction"] == "RX"]
    s = [GEN_TAG, "#include \"Cfg_Types.h\"", "#include \"Com.h\"   /* Com_SignalIdType, COM_SIG_* */", ""]
    s.append("/* [SRS_GEN_020] 신호 설정 테이블 (TX pduIdx=TX 배열 순, RX pduIdx=RX 배열 순) */")
    s.append("const Com_SignalCfg Com_SignalConfig[] = {")
    for f in tx:
        for sig in f["signals"]:
            s.append(f"    {{COM_SIG_{sig['name']}, 1u, 0u, {sig['start_bit']}u, {sig['length']}u}},  /* {f['name']}.{sig['name']} */")
    for f in rx:
        for sig in f["signals"]:
            s.append(f"    {{COM_SIG_{sig['name']}, 0u, 0u, {sig['start_bit']}u, {sig['length']}u}},  /* {f['name']}.{sig['name']} */")
    n = sum(len(f["signals"]) for f in tx + rx)
    s.append("};")
    s.append(f"const uint16 Com_SignalConfig_Size = {n}u;\n")
    return "\n".join(s)

def canif_cfg(cfg):
    tx = [f for f in cfg["can_frames"] if f["direction"] == "TX"]
    rx = [f for f in cfg["can_frames"] if f["direction"] == "RX"]
    s = [GEN_TAG, "#include \"Cfg_Types.h\"", "#include \"CanIf.h\"", ""]
    s.append("const CanIf_TxPduCfg CanIf_TxPduConfig[] = {")
    for f in tx:
        s.append(f"    {{CANIF_TXPDU_{f['macro']}, {f['can_id']}u, 0u}},  /* {f['name']} */")
    s.append("};")
    s.append(f"const uint16 CanIf_TxPduConfig_Size = {len(tx)}u;\n")
    s.append("const CanIf_RxPduCfg CanIf_RxPduConfig[] = {")
    for f in rx:
        s.append(f"    {{{f['can_id']}u, 0u, CANIF_RXPDU_{f['macro']}}},  /* {f['name']} */")
    s.append("};")
    s.append(f"const uint16 CanIf_RxPduConfig_Size = {len(rx)}u;\n")
    return "\n".join(s)

def pdur_cfg(cfg):
    tx = [f for f in cfg["can_frames"] if f["direction"] == "TX"]
    rx = [f for f in cfg["can_frames"] if f["direction"] == "RX"]
    s = [GEN_TAG, "#include \"Cfg_Types.h\"", "#include \"Com.h\"", "#include \"CanIf.h\"", ""]
    s.append("const PduR_TxRoute PduR_TxRoutes[] = {")
    for f in tx:
        s.append(f"    {{COM_TXPDU_{f['name']}, CANIF_TXPDU_{f['macro']}}},")
    s.append("};")
    s.append(f"const uint16 PduR_TxRoutes_Size = {len(tx)}u;\n")
    s.append("const PduR_RxRoute PduR_RxRoutes[] = {")
    for f in rx:
        s.append(f"    {{CANIF_RXPDU_{f['macro']}, COM_RXPDU_{f['name']}}},")
    s.append("};")
    s.append(f"const uint16 PduR_RxRoutes_Size = {len(rx)}u;\n")
    return "\n".join(s)

def uds_data_h(cfg):
    s = [GEN_TAG, "#ifndef UDS_DATA_H\n#define UDS_DATA_H", "#include \"Std_Types.h\"", ""]
    for d in cfg["uds"]["dids"]:
        s.append(f"#define UDS_DID_{d['name'].upper()}  {d['id']}u")
    s.append("")
    for d in cfg["uds"]["dids"]:
        if d["kind"] == "static" and d.get("value_kind") == "ascii":
            s.append(f"extern const uint8 Uds_{d['name']}Bytes[{len(d['value'])}u];")
        elif d["kind"] == "static":
            s.append(f"extern const uint8 Uds_{d['name']}Bytes[{d['length']}u];")
    s.append("\n#endif /* UDS_DATA_H */")
    return "\n".join(s) + "\n"

def uds_data_c(cfg):
    s = [GEN_TAG, "#include \"Uds_Data.h\"", ""]
    for d in cfg["uds"]["dids"]:
        if d["kind"] != "static":
            continue
        if d.get("value_kind") == "ascii":
            chars = ", ".join(f"'{c}'" for c in d["value"])
            s.append(f"const uint8 Uds_{d['name']}Bytes[{len(d['value'])}u] = {{ {chars} }};")
        else:
            s.append(f"const uint8 Uds_{d['name']}Bytes[{d['length']}u] = {{ {d['value']} }};")
    s.append("")
    return "\n".join(s)

def dbc(cfg):
    lines = ['VERSION ""', "", "NS_ :", "", "BS_:", "", "BU_: ECU BCM", ""]
    for fr in cfg["can_frames"]:
        sender = "ECU" if fr["direction"] == "TX" else "BCM"
        recv = "BCM" if fr["direction"] == "TX" else "ECU"
        lines.append(f"BO_ {int(fr['can_id'], 16)} {fr['name']}: {fr['dlc']} {sender}")
        for sig in fr["signals"]:
            order = "1" if sig.get("byte_order", "little") == "little" else "0"  # DBC 규약(Vector/cantools): @1=Intel(little), @0=Motorola(big)
            factor = sig.get("factor", 1)
            offset = sig.get("offset", 0)
            vmax = ((2 ** sig["length"]) - 1) * factor + offset
            unit = sig.get("unit", "")
            lines.append(f' SG_ {sig["name"]} : {sig["start_bit"]}|{sig["length"]}@{order}+ ({factor},{offset}) [0|{vmax}] "{unit}" {recv}')
        if fr.get("comment"):
            lines.append(f'CM_ BO_ {int(fr["can_id"], 16)} "{fr["comment"]}";')
        lines.append("")
    return "\n".join(lines)

def doc_md(cfg):
    l = ["# Signal & Config 문서 (자동 생성)", "", f"프로젝트: {cfg['meta']['project']}", ""]
    for fr in cfg["can_frames"]:
        l.append(f"## {fr['name']} (CAN {fr['can_id']}, {fr['direction']}, DLC {fr['dlc']}"
                 + (f", {fr['period_ms']}ms 주기" if fr.get('period_ms') else "") + ")")
        l.append("")
        l.append("| Signal | Start | Len | Order | Factor | Offset | Unit |")
        l.append("|---|---|---|---|---|---|---|")
        for sig in fr["signals"]:
            l.append(f"| {sig['name']} | {sig['start_bit']} | {sig['length']} | {sig.get('byte_order','little')} | "
                     f"{sig.get('factor',1)} | {sig.get('offset',0)} | {sig.get('unit','')} |")
        l.append("")
    l.append("## UDS DID 카탈로그")
    l.append("")
    l.append("| DID | Name | Access | Kind | Len |")
    l.append("|---|---|---|---|---|")
    for d in cfg["uds"]["dids"]:
        ln = len(d["value"]) if d["kind"] == "static" and d.get("value_kind") == "ascii" else d.get("length", "-")
        l.append(f"| {d['id']} | {d['name']} | {d['access']} | {d['kind']} | {ln} |")
    l.append("")
    return "\n".join(l)

# ---------------------------------------------------------------- 메인
def main():
    cfg_path = sys.argv[1] if len(sys.argv) > 1 else "config/ecu.json"
    out_dir = sys.argv[2] if len(sys.argv) > 2 else "generated"
    os.makedirs(out_dir, exist_ok=True)
    cfg = load(cfg_path)

    errors, warns = [], []
    validate(cfg, errors, warns)
    print(f"[gen_cfg] 입력: {cfg_path}")
    print(f"[gen_cfg] 프레임 {len(cfg['can_frames'])}개, 신호 "
          f"{sum(len(f['signals']) for f in cfg['can_frames'])}개, DID {len(cfg['uds']['dids'])}개")
    for w in warns:
        print(f"[gen_cfg][경고] {w}")
    if errors:
        for e in errors:
            print(f"[gen_cfg][오류] {e}")
        print("[gen_cfg] 검증 실패 — 생성 중단 (툴의 validation 단계)")
        return 1

    outputs = {
        "Cfg_Ids.h": ids_header(cfg),
        "Com_Cfg.c": com_cfg(cfg),
        "CanIf_Cfg.c": canif_cfg(cfg),
        "PduR_Cfg.c": pdur_cfg(cfg),
        "Uds_Data.h": uds_data_h(cfg),
        "Uds_Data.c": uds_data_c(cfg),
        "VehicleNetwork.dbc": dbc(cfg),
        "SIGNAL_DOC.md": doc_md(cfg),
    }
    for fn, content in outputs.items():
        with open(os.path.join(out_dir, fn), "w", encoding="utf-8") as f:
            f.write(content)
        print(f"[gen_cfg][생성] {out_dir}/{fn}")
    print("[gen_cfg] 검증 통과 + 생성 완료")
    return 0

if __name__ == "__main__":
    sys.exit(main())
