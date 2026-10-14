#!/usr/bin/env python3
"""gate_check_composition5.py — offline acceptance gate for the composition-5
PS5 UE4SS runtime candidate (module_start wiring + rebuild).

Takes a packaging candidate build dir (same shape as
worker-composition-4-fix/prx-candidate-composition4-1/) and emits PASS/FAIL per
check with a JSON receipt. Fail-closed: exit 1 if anything fails.

Checks (see GATE-CHECKS.md):
  W1  chain symbols DEFINED in the combined object
  W2  module_start actually references the chain (relocations, disassembly evidence)
  W3  entry exports intact (module_start + module_stop, global/default)
  I1  import census unchanged vs composition-4 (unless justified in build dir)
  D1  determinism double-build byte-identical (compare-dir or determinism.json)
  F1  fail-open diagnostic markers present in the entry object
  S1  size sanity vs composition-4 baseline
  P1  sharpprospero packaging static checks all green + packager receipt fields

Negative control: run against worker-composition-4-fix/prx-candidate-composition4-1
— W2 must FAIL there (comp4 module_entry calls probe_lifecycle_start only).

Stdlib only. Read-only on the build dir.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------- constants

ROOT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
LLVM_DIR = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"

BASELINE_BARE_BYTES = 30771688  # composition-4 bare packager output
BASELINE_BARE_SHA256 = "9b3758171db6c91661ac2d579c34b00c850bc687c28a95799103f5f7cdbf961d"
DEFAULT_EXPECTED_IMPORTS = 196  # frozen 191 + 5 extended (package_prx.py:152-158)

SYM_MFA_ACTIVATE = "mfa_activate"
SYM_START_HELPER = "_ZN9modbridge6native28StartActivatedRuntimeWithAckE"
SYM_PREPARE = "_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE"
SYM_OWNER_START = "_ZN9modbridge6native12RuntimeOwner5startEv"
SYM_NATIVE_MOD_SOURCE = "_ZN2RC8Platform15NativeModSourceC"
SYM_STOP_HELPER = "_ZN9modbridge6native18StopRuntimeWithAckE"

# call-range relocation types on x86-64 (ELF64)
# Controller fix 2026-10-04: -fPIC external calls go through the GOT as
# GOTPCRELX (41) / REX_GOTPCRELX (42) / GOTPCREL (9); direct calls are
# PC32 (2) / PLT32 (4). The original set missed 41/9 and mislabeled 42.
CALL_RELOC_TYPES = {2: "R_X86_64_PC32", 4: "R_X86_64_PLT32",
                    9: "R_X86_64_GOTPCREL", 41: "R_X86_64_GOTPCRELX",
                    42: "R_X86_64_REX_GOTPCRELX"}
SYM_BRIDGE_START = "modbridge_runtime_start_with_activation"

STB_GLOBAL, STB_WEAK = 1, 2
STV_DEFAULT = 0
SHT_SYMTAB, SHT_RELA, SHT_NOBITS = 2, 4, 8

MARKERS_STATE = (b"module_start_entered", b"module_start_return")
CHANNEL_SYMBOL = "diagnostic_open_channel"

STATIC_CHECKS_PASS_STATUS = "STATIC_CHECKS_PASSED"
EXPECTED_STATIC_CHECK_NAMES = [
    "elf_magic_bare", "fself_magic_wrapped", "wrapped_embedded_elf_header_equal",
    "fself_payload_digest_matches_bare", "wrapped_segments_equal", "elf_module_type_flags",
    "elf64_x86_64_le", "pt_sce_module_unique", "sdk_stamp_0x11000000",
    "explicit_dt_init_dt_fini", "loader_owns_no_arrays", "init_fini_slots",
    "init_fini_slots_present", "section_.eh_frame_present", "section_.eh_frame_hdr_present",
    "section_.gcc_except_table_present", "imports_equal_extended_contract",
    "linker_provided_dynamic_not_imported", "imports_resolve_to_sdk_or_linker",
    "entry_alignment_evidence",
]

# ---------------------------------------------------------------- helpers


def tool(name: str) -> str | None:
    """Prefer the pinned LLVM toolchain, fall back to PATH."""
    cand = LLVM_DIR / name
    if cand.exists():
        return str(cand)
    for d in os.environ.get("PATH", "").split(os.pathsep):
        p = Path(d) / name
        if p.exists():
            return str(p)
    # macOS system fallbacks
    for sysname in ("/usr/bin/" + name,):
        if Path(sysname).exists():
            return sysname
    return None


def run(cmd: list[str], timeout: int = 120) -> tuple[int, str, str]:
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return p.returncode, p.stdout, p.stderr
    except (OSError, subprocess.TimeoutExpired) as e:
        return 127, "", str(e)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_elf64_symbols(path: Path) -> dict[str, dict] | None:
    """Pure-python ELF64 symtab parse. Returns {name: {value,size,bind,vis,shndx}}
    or None when the file has no usable section header table."""
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) < 64 or data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        return None
    (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    if e_shoff == 0 or e_shnum == 0 or e_shentsize < 64:
        return None
    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        if off + 64 > len(data):
            return None
        sh_name, sh_type = struct.unpack_from("<II", data, off)
        sh_offset, sh_size = struct.unpack_from("<QQ", data, off + 24)
        sh_link, sh_info = struct.unpack_from("<II", data, off + 40)
        sh_entsize = struct.unpack_from("<Q", data, off + 56)[0]
        sections.append(dict(type=sh_type, offset=sh_offset, size=sh_size,
                             link=sh_link, info=sh_info, entsize=sh_entsize))
    symtabs = [s for s in sections if s["type"] == SHT_SYMTAB]
    if not symtabs:
        return None
    out: dict[str, dict] = {}
    for st in symtabs:
        strsec = sections[st["link"]]
        strdata = data[strsec["offset"]:strsec["offset"] + strsec["size"]]
        n = st["size"] // 24
        for i in range(n):
            off = st["offset"] + i * 24
            st_name, st_info, st_other, st_shndx = struct.unpack_from("<IBBH", data, off)
            st_value, st_size = struct.unpack_from("<QQ", data, off + 8)
            if st_name >= len(strdata):
                continue
            end = strdata.find(b"\x00", st_name)
            name = strdata[st_name:end].decode("utf-8", "replace")
            if not name:
                continue
            rec = out.setdefault(name, dict(value=st_value, size=st_size,
                                            bind=st_info >> 4, vis=st_other & 3,
                                            shndx=st_shndx, defined=False))
            defined = st_shndx != 0
            rec["defined"] = rec["defined"] or defined
            if defined and rec["bind"] == 0:
                rec["bind"] = st_info >> 4
                rec["vis"] = st_other & 3
    return out or None


def symbols_of(path: Path) -> dict[str, dict]:
    """Symbols via pure-python parse, falling back to nm output."""
    syms = parse_elf64_symbols(path)
    if syms is not None:
        return syms
    nm = tool("llvm-nm") or tool("nm")
    if not nm:
        return {}
    rc, out, _ = run([nm, path.as_posix()])
    if rc != 0:
        return {}
    result: dict[str, dict] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            name = parts[-1]
            t = parts[-2]
            defined = t in ("T", "W", "D", "B", "R", "V")
            rec = result.setdefault(name, dict(value=0, size=0,
                                               bind=STB_GLOBAL if defined else 0,
                                               vis=STV_DEFAULT, shndx=0,
                                               defined=False))
            rec["defined"] = rec["defined"] or defined
    return result


def parse_elf64_rela(path: Path) -> list[dict] | None:
    """All .rela.text.* relocations: [{offset, sym_index, type, section}]."""
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) < 64 or data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        return None
    (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    if e_shoff == 0 or e_shnum == 0 or e_shentsize < 64:
        return None
    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        if off + 64 > len(data):
            return None
        sh_name, sh_type = struct.unpack_from("<II", data, off)
        sh_offset, sh_size = struct.unpack_from("<QQ", data, off + 24)
        sh_link, sh_info = struct.unpack_from("<II", data, off + 40)
        sections.append(dict(nameoff=sh_name, type=sh_type, offset=sh_offset,
                             size=sh_size, link=sh_link, info=sh_info))
    # section name string table
    strtab = b""
    if e_shentsize and len(sections) > 0:
        # best-effort: locate .shstrtab via header field e_shstrndx
        e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]
        if 0 < e_shstrndx < len(sections):
            s = sections[e_shstrndx]
            strtab = data[s["offset"]:s["offset"] + s["size"]]

    def sname(s):
        o = s["nameoff"]
        if o >= len(strtab):
            return ""
        return strtab[o:strtab.find(b"\x00", o)].decode("utf-8", "replace")

    symtabs = [s for s in sections if s["type"] == SHT_SYMTAB]
    if not symtabs:
        return None
    st = symtabs[0]
    strsec = sections[st["link"]]
    strdata = data[strsec["offset"]:strsec["offset"] + strsec["size"]]

    def symname(idx):
        off = st["offset"] + idx * 24
        if off + 24 > len(data):
            return ""
        (st_name,) = struct.unpack_from("<I", data, off)
        if st_name >= len(strdata):
            return ""
        return strdata[st_name:strdata.find(b"\x00", st_name)].decode("utf-8", "replace")

    relas = []
    for s in sections:
        if s["type"] != SHT_RELA:
            continue
        target = sname(s)
        if not target.startswith(".rela.text"):
            continue
        n = s["size"] // 24
        for i in range(n):
            off = s["offset"] + i * 24
            r_offset, r_info = struct.unpack_from("<QQ", data, off)
            relas.append(dict(section=target, offset=r_offset,
                              sym=symname(r_info >> 32), sym_index=r_info >> 32,
                              rtype=r_info & 0xFFFFFFFF))
    return relas


def disasm_entry(path: Path, symbol: str = "module_start") -> str:
    """Best-effort annotated disassembly of `symbol` from a relocatable object."""
    od = tool("llvm-objdump") or tool("objdump")
    if not od:
        return "(no objdump available)"
    rc, out, err = run([od, "-dr", "--disassemble-symbols=" + symbol, path.as_posix()])
    if rc != 0:
        rc, out, err = run([od, "-dr", path.as_posix()])
        if rc != 0:
            return "(objdump failed: %s)" % err.strip()[:200]
    lines = out.splitlines()
    keep, capture = [], False
    for ln in lines:
        if ln.startswith(symbol) or (">:" in ln and symbol in ln):
            capture = True
        elif capture and ln and not ln.startswith(" ") and not ln.startswith("\t") \
                and ("Disassembly" in ln or ln.endswith(">:")):
            break
        if capture:
            keep.append(ln)
    return "\n".join(keep) if keep else out[:4000]


def find_prxs(build_dir: Path) -> dict:
    """Locate bare + wrapped PRX artifacts."""
    found: dict = {}
    candidates = [
        ("bare", build_dir / "bare-original" / "modbridge_supplier_composition.prx"),
        ("wrapped", build_dir / "modbridge_supplier_composition.prx"),
        ("bare", build_dir / "bare-sdk" / "modbridge_supplier_composition.prx"),
    ]
    for kind, p in candidates:
        if p.exists() and kind not in found:
            found[kind] = p
    if not found:
        for p in sorted(build_dir.rglob("*.prx")):
            found.setdefault("wrapped", p)
            break
    return found


def packager_receipt(build_receipt: Path) -> dict | None:
    """Extract the Packager's stdout JSON (imports count, init/fini, bytes)."""
    try:
        doc = json.loads(build_receipt.read_text())
    except (OSError, ValueError):
        return None

    def scan(obj):
        if isinstance(obj, dict):
            for v in obj.values():
                r = scan(v)
                if r:
                    return r
        elif isinstance(obj, list):
            for v in obj:
                r = scan(v)
                if r:
                    return r
        elif isinstance(obj, str) and obj.lstrip().startswith("{"):
            try:
                d = json.loads(obj)
            except ValueError:
                return None
            if isinstance(d, dict) and "imports" in d and "output_sha256" in d:
                return d
        return None

    return scan(doc)


# ---------------------------------------------------------------- checks


class Report:
    def __init__(self):
        self.checks = []
        self.evidence = {}

    def add(self, cid, name, passed, details, info=None):
        self.checks.append(dict(id=cid, name=name,
                                status="PASS" if passed else "FAIL",
                                details=details, info=info or {}))
        print(f"[{'PASS' if passed else 'FAIL'}] {cid} {name} — {details}")
        return passed


def check_w1(build_dir: Path, rep: Report):
    combined = build_dir / "build" / "combined-supplier.o"
    if not combined.exists():
        objs = sorted((build_dir / "build").glob("*.o"), key=lambda p: p.stat().st_size) \
            if (build_dir / "build").exists() else []
        if not objs:
            return rep.add("W1", "chain-symbols-defined", False,
                           "no combined-supplier.o and no build/*.o found")
        combined = objs[-1]
    syms = symbols_of(combined)
    required = {
        "mfa_activate": SYM_MFA_ACTIVATE,
        "PrepareActivatedUE4SSRuntime": SYM_PREPARE,
        "RuntimeOwner::start": SYM_OWNER_START,
        "NativeModSource ctor": SYM_NATIVE_MOD_SOURCE,
    }
    missing, found = [], {}
    for label, needle in required.items():
        hit = [n for n, s in syms.items() if s["defined"] and (n == needle or n.startswith(needle))]
        if hit:
            found[label] = hit[:3]
        else:
            missing.append(label)
    info = {"object": str(combined), "defined": found,
            "StartActivatedRuntimeWithAck": sorted(
                n for n, s in syms.items() if s["defined"] and n.startswith(SYM_START_HELPER))[:2],
            "StopRuntimeWithAck": sorted(
                n for n, s in syms.items() if s["defined"] and n.startswith(SYM_STOP_HELPER))[:2]}
    return rep.add("W1", "chain-symbols-defined", not missing,
                   ("all 4 required chain symbols DEFINED in " + combined.name)
                   if not missing else "MISSING: " + ", ".join(missing), info)


def find_entry_objects(build_dir: Path) -> list[Path]:
    bld = build_dir / "build"
    if not bld.exists():
        return []
    entries = []
    for p in sorted(bld.glob("*.o")):
        if p.name == "combined-supplier.o":
            continue
        syms = symbols_of(p)
        ms = syms.get("module_start")
        if ms and ms["defined"]:
            entries.append(p)
    return entries


def check_w2(build_dir: Path, rep: Report):
    entries = find_entry_objects(build_dir)
    if not entries:
        return rep.add("W2", "module_start-references-chain", False,
                       "no per-unit object defining module_start found in build/")
    refs: set[str] = set()
    details = []
    for p in entries:
        relas = parse_elf64_rela(p)
        if relas is None:
            od = tool("llvm-objdump") or tool("objdump")
            rc, out, _ = run([od, "-r", p.as_posix()]) if od else (1, "", "")
            relas = []
            for ln in out.splitlines():
                parts = ln.split()
                if len(parts) >= 3 and parts[0].lower().startswith("0"):
                    relas.append(dict(sym=parts[-1], rtype=-1, offset=0, section="?"))
            details.append(f"{p.name}: relocations via objdump fallback ({len(relas)})")
        else:
            details.append(f"{p.name}: {len(relas)} .rela.text relocations")
        for r in relas:
            if r["rtype"] in CALL_RELOC_TYPES or r["rtype"] == -1:
                refs.add(r["sym"])
    has_mfa = any(n == SYM_MFA_ACTIVATE for n in refs)
    has_helper = any(n.startswith(SYM_START_HELPER) for n in refs)
    has_prepare = any(n.startswith(SYM_PREPARE) for n in refs)
    has_start = any(n == SYM_OWNER_START for n in refs)
    # bridge-mediated wiring (controller addition): module_start calls the
    # bridge function; the bridge TU (object defining it) references the chain
    # helper. Proven edge-by-edge with relocation evidence in both objects.
    has_bridge_call = any(n == SYM_BRIDGE_START for n in refs)
    bridge_chain = False
    bridge_obj = build_dir / "build" / "runtime_start_bridge.o"
    if has_bridge_call and bridge_obj.is_file():
        brelas = parse_elf64_rela(bridge_obj) or []
        brefs = {r["sym"] for r in brelas
                 if r["rtype"] in CALL_RELOC_TYPES or r["rtype"] == -1}
        bridge_chain = any(n.startswith(SYM_START_HELPER) for n in brefs)
    via_helper = has_mfa and has_helper
    via_direct = has_mfa and has_prepare and has_start
    via_bridge = has_mfa and has_bridge_call and bridge_chain
    wired = via_helper or via_direct or via_bridge
    variant = ("helper StartActivatedRuntimeWithAck" if via_helper
               else "direct PrepareActivatedUE4SSRuntime + RuntimeOwner::start" if via_direct
               else "bridge runtime_start_bridge.o -> StartActivatedRuntimeWithAck" if via_bridge
               else "-")
    missing = []
    if not has_mfa:
        missing.append("mfa_activate")
    if not (has_helper or (has_prepare and has_start)):
        missing.append("StartActivatedRuntimeWithAck OR {PrepareActivatedUE4SSRuntime + RuntimeOwner::start}")
    chain_refs = sorted(n for n in refs if n.startswith(
        (SYM_START_HELPER, SYM_PREPARE, SYM_OWNER_START)) or n == SYM_MFA_ACTIVATE)
    info = {"entry_objects": [str(p) for p in entries], "call_refs_chain": chain_refs,
            "via": variant,
            "missing": missing,
            "disassembly": {p.name: disasm_entry(p) for p in entries}}
    detail = f"wired via {variant}; entry={', '.join(p.name for p in entries)}" if wired \
        else f"NOT wired; module_start call refs lack: {', '.join(missing)}"
    return rep.add("W2", "module_start-references-chain", wired, detail, info)


def check_w3(build_dir: Path, rep: Report):
    entries = find_entry_objects(build_dir)
    if not entries:
        return rep.add("W3", "entry-exports-intact", False, "no entry object found")
    ok, info = True, {}
    for p in entries:
        syms = symbols_of(p)
        ms, mx = syms.get("module_start"), syms.get("module_stop")
        good = bool(ms and ms["defined"] and ms["bind"] == STB_GLOBAL and ms["vis"] == STV_DEFAULT) \
            and bool(mx and mx["defined"] and mx["bind"] == STB_GLOBAL and mx["vis"] == STV_DEFAULT)
        info[p.name] = {"module_start": {k: ms[k] for k in ("defined", "bind", "vis")} if ms else None,
                        "module_stop": {k: mx[k] for k in ("defined", "bind", "vis")} if mx else None}
        ok = ok and good
    return rep.add("W3", "entry-exports-intact", ok,
                   "module_start+module_stop global/default in " + ", ".join(p.name for p in entries)
                   if ok else "missing or non-exported module_start/module_stop", info)


def check_i1(build_dir: Path, rep: Report, expected_imports: int):
    receipt = packager_receipt(build_dir / "build-receipt.json")
    justified = (build_dir / "IMPORT-DELTA-JUSTIFIED.md").exists()
    if receipt is None:
        return rep.add("I1", "import-census-unchanged", False,
                       "no Packager receipt JSON found in build-receipt.json")
    imports = receipt.get("imports")
    delta = None if imports is None else imports - expected_imports
    ok = (imports == expected_imports) or (justified and delta and delta > 0)
    sc = build_dir / "verification-evidence" / "static-checks.json"
    contract_ok = None
    if sc.exists():
        try:
            doc = json.loads(sc.read_text())
            chk = doc.get("checks", {}).get("imports_equal_extended_contract", {})
            contract_ok = bool(chk.get("pass"))
            ok = ok and contract_ok
        except (OSError, ValueError):
            pass
    detail = f"imports={imports} expected={expected_imports} (delta {delta}); " \
             f"imports_equal_extended_contract={'PASS' if contract_ok else 'n/a'}" \
             + ("; justified by IMPORT-DELTA-JUSTIFIED.md" if justified and imports != expected_imports else "")
    return rep.add("I1", "import-census-unchanged", ok, detail,
                   {"imports": imports, "expected": expected_imports,
                    "functions": receipt.get("functions"), "justified": justified})


def check_d1(build_dir: Path, rep: Report, compare_dir: Path | None):
    prxs = find_prxs(build_dir)
    primary = prxs.get("bare") or prxs.get("wrapped")
    if primary is None:
        return rep.add("D1", "determinism-double-build", False, "no PRX artifact found")
    if compare_dir:
        other = find_prxs(compare_dir)
        second = other.get("bare") or other.get("wrapped")
        if second is None:
            return rep.add("D1", "determinism-double-build", False,
                           f"no PRX in compare dir {compare_dir}")
        h1, h2 = sha256_file(primary), sha256_file(second)
        ok = h1 == h2
        return rep.add("D1", "determinism-double-build", ok,
                       f"{'identical' if ok else 'DIFFERENT'} sha256 across two builds: {h1[:16]}… vs {h2[:16]}…",
                       {"sha1": h1, "sha2": h2})
    det = build_dir / "determinism.json"
    if det.exists():
        try:
            d = json.loads(det.read_text())
            h1, h2 = d.get("build1_sha256"), d.get("build2_sha256")
            ok = bool(h1 and h1 == h2)
            actual = sha256_file(primary)
            match = (h1 == actual) or (d.get("bare_sha256") == actual)
            return rep.add("D1", "determinism-double-build", ok and match,
                           f"receipt hashes {'equal' if ok else 'differ'}; "
                           f"artifact match={match}", d)
        except (OSError, ValueError):
            pass
    return rep.add("D1", "determinism-double-build", False,
                   "determinism not evidenced: pass --compare-dir with a second "
                   "candidate dir or provide determinism.json (build1_sha256==build2_sha256)")


def check_f1(build_dir: Path, rep: Report):
    entries = find_entry_objects(build_dir)
    if not entries:
        return rep.add("F1", "fail-open-diagnostic-markers", False, "no entry object found")
    ok, info = True, {}
    for p in entries:
        blob = p.read_bytes()
        have_state = [m.decode() for m in MARKERS_STATE if m in blob]
        relas = parse_elf64_rela(p) or []
        has_channel = any(r["sym"] == CHANNEL_SYMBOL for r in relas) or b"diagnostic" in blob
        good = len(have_state) == len(MARKERS_STATE) and has_channel
        info[p.name] = {"state_markers": have_state, "channel": has_channel}
        ok = ok and good
    return rep.add("F1", "fail-open-diagnostic-markers", ok,
                   "entry state markers + diagnostic channel present"
                   if ok else f"markers missing: {info}", info)


def check_s1(build_dir: Path, rep: Report, baseline_bytes: int, tolerance: float,
             max_bytes: int):
    prxs = find_prxs(build_dir)
    bare = prxs.get("bare")
    if bare is None:
        return rep.add("S1", "size-sanity", False, "no bare PRX found for size check")
    n = bare.stat().st_size
    h = sha256_file(bare)
    rel = abs(n - baseline_bytes) / baseline_bytes
    ok = rel <= tolerance and n <= max_bytes
    baseline_match = " (byte-identical to comp4 baseline sha)" if h == BASELINE_BARE_SHA256 else ""
    return rep.add("S1", "size-sanity", ok,
                   f"bare PRX {n:,} bytes vs baseline {baseline_bytes:,} "
                   f"({rel*100:.2f}% delta, cap {max_bytes:,}){baseline_match}",
                   {"bytes": n, "sha256": h, "baseline_bytes": baseline_bytes,
                    "delta_rel": rel, "baseline_sha256_match": h == BASELINE_BARE_SHA256})


def check_p1(build_dir: Path, rep: Report):
    sc = build_dir / "verification-evidence" / "static-checks.json"
    if not sc.exists():
        return rep.add("P1", "packager-static-checks", False,
                       "verification-evidence/static-checks.json missing")
    try:
        doc = json.loads(sc.read_text())
    except (OSError, ValueError) as e:
        return rep.add("P1", "packager-static-checks", False, f"unreadable: {e}")
    status = doc.get("status")
    checks = doc.get("checks", {})
    failing = [k for k, v in checks.items() if not v.get("pass")]
    unexpected_missing = [n for n in EXPECTED_STATIC_CHECK_NAMES if n not in checks]
    receipt = packager_receipt(build_dir / "build-receipt.json")
    fields_ok, field_info = True, {}
    if receipt:
        for key, want in (("explicit_init", "module_start"), ("explicit_fini", "module_stop"),
                          ("loader_array_ownership", False), ("runtime_or_hooks_executed", False)):
            got = receipt.get(key)
            field_info[key] = got
            fields_ok = fields_ok and got == want
    else:
        fields_ok = False
    ok = status == STATIC_CHECKS_PASS_STATUS and not failing and not unexpected_missing and fields_ok
    detail = f"status={status}, {len(checks)} checks, failing={failing or 'none'}, " \
             f"receipt fields={'ok' if fields_ok else field_info}"
    return rep.add("P1", "packager-static-checks", ok, detail,
                   {"status": status, "failing": failing,
                    "unexpected_missing": unexpected_missing, "fields": field_info})


# ---------------------------------------------------------------- main


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--build-dir", required=True, type=Path)
    ap.add_argument("--compare-dir", type=Path, default=None,
                    help="second candidate dir for the determinism double-build check")
    ap.add_argument("--baseline-bytes", type=int, default=BASELINE_BARE_BYTES)
    ap.add_argument("--size-tolerance", type=float, default=0.05)
    ap.add_argument("--max-size-bytes", type=int, default=64 * 1024 * 1024)
    ap.add_argument("--expected-imports", type=int, default=DEFAULT_EXPECTED_IMPORTS)
    ap.add_argument("--json", type=Path, default=None, help="write receipt JSON here")
    args = ap.parse_args(argv)

    build_dir = args.build_dir.resolve()
    if not build_dir.exists():
        print(f"ERROR: build dir does not exist: {build_dir}")
        return 2

    rep = Report()
    print(f"=== composition-5 gate: {build_dir} ===")
    check_w1(build_dir, rep)
    check_w2(build_dir, rep)
    check_w3(build_dir, rep)
    check_i1(build_dir, rep, args.expected_imports)
    check_d1(build_dir, rep, args.compare_dir.resolve() if args.compare_dir else None)
    check_f1(build_dir, rep)
    check_s1(build_dir, rep, args.baseline_bytes, args.size_tolerance, args.max_size_bytes)
    check_p1(build_dir, rep)

    failed = [c["id"] for c in rep.checks if c["status"] == "FAIL"]
    verdict = "GO (offline gate)" if not failed else f"NO-GO (failed: {', '.join(failed)})"
    print(f"=== verdict: {verdict} ===")

    if args.json:
        receipt = {
            "gate": "composition-5 pre-staging acceptance gate",
            "worker": "worker-composition5-gate",
            "date": "2026-10-04",
            "build_dir": str(build_dir),
            "compare_dir": str(args.compare_dir.resolve()) if args.compare_dir else None,
            "baseline_bytes": args.baseline_bytes,
            "expected_imports": args.expected_imports,
            "verdict": verdict,
            "failed_checks": failed,
            "checks": rep.checks,
        }
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(receipt, indent=2))
        print(f"receipt: {args.json}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
