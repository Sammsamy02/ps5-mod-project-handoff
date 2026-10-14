#!/usr/bin/env python3
"""gate_check_composition5_v2.py — offline acceptance gate for the composition-5
PS5 UE4SS runtime candidate, with DECLARED composition-change support (v2).

Fork of worker-composition5-gate/gate_check_composition5.py by
worker-gate-evolution (2026-10-04). WITHOUT --manifest this script runs the
exact original 8 checks (W1 W2 W3 I1 D1 F1 S1 P1) with byte-identical stdout
verdicts; existing receipts stay valid. With --manifest <composition-change
manifest JSON> it additionally verifies (fail-closed) that every deviation of
the composed object from the pinned baseline is DECLARED and RECEIPTED:

  M0  manifest schema/baseline validity
  M1  changed-unit receipts: declared source shas match receipt attestations
  I1+ import delta EXACTLY equals the manifest's declared added/removed sets
      (itemized both directions; undeclared drift refused; count + derived
      contract name-set cross-checked)
  B1  Resolution B: configured-roots symbols present in the declared units
      (nm-level, exact/prefix, defined/undefined binding)
  B2  Resolution B: bridge still refuses substitution — external callee set of
      runtime_start_bridge.o unchanged vs baseline (unless the manifest itself
      declares the bridge callee delta)
  B3  Resolution B: game-identity facts path untouched — identity refusal
      markers still present in the combined object, game_executable_path
      evidence not weakened
  D2  optional analog when the D2 text_base change lands: honesty guard
      strings still present + declared source pins verified by sha256

See MANIFEST-SPEC.md in the same directory for the manifest format.
Baseline: baseline-composition5-r11.json (frozen r11 UND set, derived contract
names, bridge external callees, identity/D2 evidence strings).

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


# ------------------------------------------------- manifest mode (v2 only)
#
# Everything below is inactive without --manifest. The original 8 checks above
# are deliberately untouched so no-manifest output stays byte-identical to the
# live gate.

MANIFEST_VERSION = 1
MANIFEST_BASELINE = Path(__file__).resolve().parent / "baseline-composition5-r11.json"

# Call-range relocation types on x86-64 (same set as W2, documented there).
# Data-address GOT refs excluded from the callee set: _ZTI*/_ZTV* (typeinfo /
# vtable addresses used by throw/landing pads) and __dso_handle.
BRIDGE_CALLEE_EXCLUDE_PREFIXES = ("_ZTI", "_ZTV", "__dso_handle")


def load_baseline(path: Path) -> dict | None:
    try:
        doc = json.loads(path.read_text())
    except (OSError, ValueError):
        return None
    for key in ("baseline_id", "combined_und", "contract_import_names",
                "bridge_call_refs", "game_identity_evidence_strings"):
        if key not in doc:
            return None
    return doc


def elf_sections(path: Path) -> list[dict] | None:
    """All section headers; handles the extended-numbering form (e_shnum==0,
    real count in section 0's sh_size) the 117 MB combined object uses."""
    try:
        data = path.read_bytes()
    except OSError:
        return None
    if len(data) < 64 or data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        return None
    (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]
    if e_shoff == 0 or e_shentsize < 64:
        return None
    count = e_shnum
    if count == 0:
        count = struct.unpack_from("<Q", data, e_shoff + 32)[0]
    if count > 5_000_000:
        return None
    secs = []
    for i in range(count):
        off = e_shoff + i * e_shentsize
        if off + 64 > len(data):
            return None
        sh_name, sh_type = struct.unpack_from("<II", data, off)
        sh_offset, sh_size = struct.unpack_from("<QQ", data, off + 24)
        sh_link, sh_info = struct.unpack_from("<II", data, off + 40)
        secs.append(dict(nameoff=sh_name, type=sh_type, offset=sh_offset,
                         size=sh_size, link=sh_link, info=sh_info))
    strtab = b""
    if 0 < e_shstrndx < len(secs):
        s = secs[e_shstrndx]
        strtab = data[s["offset"]:s["offset"] + s["size"]]
    for s in secs:
        o = s["nameoff"]
        s["name"] = strtab[o:strtab.find(b"\x00", o)].decode("utf-8", "replace") \
            if o < len(strtab) else ""
    return secs


def symtab_und_defined(path: Path) -> tuple[set[str], set[str]] | None:
    """(undefined, defined) name sets across ALL symtabs; extended-numbering
    safe. Used for the combined object UND census in manifest mode."""
    data_path = Path(path)
    try:
        data = data_path.read_bytes()
    except OSError:
        return None
    secs = elf_sections(data_path)
    if secs is None:
        return None
    und: set[str] = set()
    defined: set[str] = set()
    for st in (s for s in secs if s["type"] == SHT_SYMTAB):
        if st["link"] >= len(secs):
            continue
        strsec = secs[st["link"]]
        strdata = data[strsec["offset"]:strsec["offset"] + strsec["size"]]
        for j in range(st["size"] // 24):
            off = st["offset"] + j * 24
            st_name, _, _, st_shndx = struct.unpack_from("<IBBH", data, off)
            if st_name >= len(strdata):
                continue
            end = strdata.find(b"\x00", st_name)
            name = strdata[st_name:end].decode("utf-8", "replace")
            if not name:
                continue
            (defined if st_shndx else und).add(name)
    return und, defined


def bridge_external_callees(path: Path) -> set[str] | None:
    """Call-range relocs against symbols NOT defined in the bridge object,
    excluding local labels and data-address GOT refs — the callee set that
    changes iff the bridge is rewired to a different substitution path."""
    try:
        data = path.read_bytes()
    except OSError:
        return None
    secs = elf_sections(path)
    if secs is None:
        return None
    symtabs = [s for s in secs if s["type"] == SHT_SYMTAB]
    if not symtabs:
        return None
    st = symtabs[0]
    strsec = secs[st["link"]]
    strdata = data[strsec["offset"]:strsec["offset"] + strsec["size"]]
    defined: set[str] = set()
    for j in range(st["size"] // 24):
        off = st["offset"] + j * 24
        st_name, _, _, st_shndx = struct.unpack_from("<IBBH", data, off)
        if st_name >= len(strdata):
            continue
        end = strdata.find(b"\x00", st_name)
        name = strdata[st_name:end].decode("utf-8", "replace")
        if name and st_shndx:
            defined.add(name)

    def symname(idx: int) -> str:
        off = st["offset"] + idx * 24
        (st_name,) = struct.unpack_from("<I", data, off)
        if st_name >= len(strdata):
            return ""
        return strdata[st_name:strdata.find(b"\x00", st_name)].decode("utf-8", "replace")

    callees: set[str] = set()
    for s in secs:
        if s["type"] != SHT_RELA or not s["name"].startswith(".rela.text"):
            continue
        for j in range(s["size"] // 24):
            off = s["offset"] + j * 24
            r_info = struct.unpack_from("<Q", data, off + 8)[0]
            if (r_info & 0xFFFFFFFF) not in CALL_RELOC_TYPES:
                continue
            sym = symname(r_info >> 32)
            if not sym or sym.startswith((".L", ".LCPI")):
                continue
            if sym.startswith(BRIDGE_CALLEE_EXCLUDE_PREFIXES):
                continue
            if sym in defined:
                continue
            callees.add(sym)
    return callees


def resolve_pin(path_str: str, build_dir: Path) -> Path:
    """Manifest source-pin paths: absolute, build-dir-relative, or ROOT-relative."""
    p = Path(path_str)
    if p.is_absolute():
        return p
    cand = build_dir / p
    if cand.exists():
        return cand
    return ROOT / p


def check_m0(manifest: dict, baseline: dict, manifest_path: Path) -> bool:
    ok = True
    if manifest.get("manifest_version") != MANIFEST_VERSION:
        ok = False
    if manifest.get("baseline_id") != baseline.get("baseline_id"):
        ok = False
    detail = f"version={manifest.get('manifest_version')} baseline={manifest.get('baseline_id')}" \
             f" (gate baseline={baseline.get('baseline_id')})"
    if not manifest.get("justification") or not str(manifest.get("justification")).strip():
        ok = False
        detail += "; missing justification"
    return rep_add_manifest("M0", "manifest-valid", ok, detail,
                            {"manifest": str(manifest_path),
                             "manifest_sha256": sha256_file(manifest_path)})


def rep_add_manifest(cid, name, passed, details, info=None):
    print(f"[{'PASS' if passed else 'FAIL'}] {cid} {name} — {details}")
    checks.append(dict(id=cid, name=name, status="PASS" if passed else "FAIL",
                       details=details, info=info or {}))
    return passed


checks: list[dict] = []  # manifest-mode check records, merged into rep by main()


def check_m1(manifest: dict, build_dir: Path) -> bool:
    units = manifest.get("changed_units") or []
    ok, details, info = True, [], {}
    if not units:
        details.append("no changed units declared (delta-only manifest)")
    for u in units:
        name = u.get("unit", "?")
        receipt_path = resolve_pin(u["receipt"], build_dir) if u.get("receipt") else None
        if receipt_path is None or not receipt_path.is_file():
            ok = False
            details.append(f"{name}: receipt missing ({u.get('receipt')})")
            continue
        try:
            receipt = json.loads(receipt_path.read_text())
        except (OSError, ValueError) as e:
            ok = False
            details.append(f"{name}: receipt unreadable: {e}")
            continue
        files = receipt.get("files") if isinstance(receipt, dict) else None
        pins = u.get("sha256_after") or {}
        unit_ok = True
        for src, want in pins.items():
            sp = resolve_pin(src, build_dir)
            got_file = sha256_file(sp) if sp.is_file() else None
            if got_file != want:
                unit_ok = False
                details.append(f"{name}: source pin MISMATCH {src} "
                               f"(declared {want[:12]}…, on disk {str(got_file)[:12]}…)")
            if isinstance(files, dict) and src in files and files[src] != want:
                unit_ok = False
                details.append(f"{name}: receipt attests {src}={str(files[src])[:12]}… "
                               f"but manifest declares {want[:12]}…")
            if isinstance(files, dict) and pins and src not in (files or {}):
                unit_ok = False
                details.append(f"{name}: {src} not attested in receipt {u.get('receipt')}")
        if not pins:
            details.append(f"{name}: receipted (no source pins declared)")
        info[name] = {"receipt": str(receipt_path), "pins_ok": unit_ok}
        ok = ok and unit_ok
    return rep_add_manifest("M1", "changed-unit-receipts", ok,
                            "; ".join(details) or "all receipts verified", info)


def check_i1m(manifest: dict, build_dir: Path, baseline: dict,
              rep: "Report") -> bool:
    combined = build_dir / "build" / "combined-supplier.o"
    base_und = set(baseline["combined_und"])
    base_names = set(baseline["contract_import_names"])
    want_added = set(manifest.get("import_delta", {}).get("added") or [])
    want_removed = set(manifest.get("import_delta", {}).get("removed") or [])

    if not combined.is_file():
        return rep_add_manifest("I1+", "import-delta-exact", False,
                                "combined-supplier.o missing", {})
    parsed = symtab_und_defined(combined)
    if parsed is None:
        return rep_add_manifest("I1+", "import-delta-exact", False,
                                "combined object unreadable", {})
    new_und, _ = parsed
    act_added = new_und - base_und
    act_removed = base_und - new_und

    undeclared_add = act_added - want_added
    undeclared_rem = act_removed - want_removed
    phantom_add = want_added - act_added
    phantom_rem = want_removed - act_removed
    sets_ok = not (undeclared_add or undeclared_rem or phantom_add or phantom_rem)

    contract = build_dir / "supplier-contract.json"
    names_ok, n_names = None, None
    if contract.is_file():
        try:
            doc = json.loads(contract.read_text())
            n_names = sorted(r["ImportSymbol"]["Name"] for r in doc["contract"]["imports"])
            expect_names = (base_names | want_added) - want_removed
            names_ok = set(n_names) == expect_names
        except (OSError, ValueError, KeyError):
            names_ok = False
    receipt = packager_receipt(build_dir / "build-receipt.json")
    imports = receipt.get("imports") if receipt else None
    expect_count = len(base_names) + len(want_added) - len(want_removed)
    count_ok = imports == expect_count
    sc = build_dir / "verification-evidence" / "static-checks.json"
    contract_ok = None
    if sc.exists():
        try:
            doc = json.loads(sc.read_text())
            chk = doc.get("checks", {}).get("imports_equal_extended_contract", {})
            contract_ok = bool(chk.get("pass"))
        except (OSError, ValueError):
            contract_ok = None

    ok = sets_ok and count_ok and (names_ok is not False) and contract_ok is not False
    parts = [f"declared +{sorted(want_added)}/-{sorted(want_removed)}; "
             f"actual +{sorted(act_added)}/-{sorted(act_removed)}"]
    if undeclared_add:
        parts.append(f"UNDECLARED ADDITIONS: {sorted(undeclared_add)}")
    if undeclared_rem:
        parts.append(f"UNDECLARED REMOVALS: {sorted(undeclared_rem)}")
    if phantom_add or phantom_rem:
        parts.append(f"DECLARED BUT ABSENT: +{sorted(phantom_add)}/-{sorted(phantom_rem)}")
    parts.append(f"imports={imports} expected={expect_count}")
    parts.append(f"contract-names={'MATCH' if names_ok else 'MISMATCH/absent'}")
    parts.append(f"imports_equal_extended_contract="
                 f"{'PASS' if contract_ok else 'FAIL/n-a'}")
    info = {"declared_added": sorted(want_added), "declared_removed": sorted(want_removed),
            "actual_added": sorted(act_added), "actual_removed": sorted(act_removed),
            "imports": imports, "expected_imports": expect_count,
            "contract_name_count": len(n_names or [])}
    return rep_add_manifest("I1+", "import-delta-exact", ok, "; ".join(parts), info)


def check_b1(manifest: dict, build_dir: Path) -> bool | None:
    rb = manifest.get("resolution_b")
    if not rb:
        return None
    syms = rb.get("expected_symbols") or []
    if not syms:
        return rep_add_manifest("B1", "configured-roots-symbols", False,
                                "resolution_b declared with no expected_symbols", {})
    combined = build_dir / "build" / "combined-supplier.o"
    combined_syms = symtab_und_defined(combined) if combined.is_file() else None
    cache: dict[str, tuple[set, set] | None] = {}
    ok, details, info = True, [], {}
    for e in syms:
        sym, where = e.get("symbol"), e.get("unit", "combined")
        match = e.get("match", "exact")
        want_binding = e.get("binding", "defined")
        if where == "combined":
            parsed = combined_syms
        elif where in cache:
            parsed = cache[where]
        else:
            obj = build_dir / "build" / where
            parsed = symtab_und_defined(obj) if obj.is_file() else None
            cache[where] = parsed
        if parsed is None:
            ok = False
            details.append(f"{sym}: artifact for unit '{where}' unreadable/missing")
            continue
        und, defined = parsed
        pool = defined if want_binding == "defined" else und
        hit = sorted(n for n in pool
                     if (n == sym if match == "exact" else n.startswith(sym)))
        good = bool(hit)
        ok = ok and good
        details.append(f"{sym} [{where}/{want_binding}/{match}]: "
                       f"{'FOUND ' + str(hit[:2]) if good else 'ABSENT'}")
        info[sym] = {"unit": where, "binding": want_binding, "found": hit[:3]}
    return rep_add_manifest("B1", "configured-roots-symbols", ok, "; ".join(details), info)


def check_b2(manifest: dict, build_dir: Path, baseline: dict) -> bool | None:
    rb = manifest.get("resolution_b")
    if not rb:
        return None
    bridge = build_dir / "build" / "runtime_start_bridge.o"
    if not bridge.is_file():
        return rep_add_manifest("B2", "bridge-substitution-refused", False,
                                "runtime_start_bridge.o missing", {})
    actual = bridge_external_callees(bridge)
    if actual is None:
        return rep_add_manifest("B2", "bridge-substitution-refused", False,
                                "bridge object unreadable", {})
    base = set(baseline["bridge_call_refs"])
    if rb.get("bridge_unchanged", True):
        want_added: set[str] = set()
        want_removed: set[str] = set()
        mode = "bridge declared unchanged"
    else:
        decl = rb.get("bridge_callee_delta") or {}
        want_added = set(decl.get("added") or [])
        want_removed = set(decl.get("removed") or [])
        mode = "bridge callee delta declared"
    act_added = actual - base
    act_removed = base - actual
    ok = (act_added == want_added) and (act_removed == want_removed)
    parts = [f"{mode}; actual +{sorted(act_added)}/-{sorted(act_removed)}"]
    if not ok:
        if act_added - want_added:
            parts.append(f"UNDECLARED NEW CALLEES: {sorted(act_added - want_added)}")
        if act_removed - want_removed:
            parts.append(f"UNDECLARED DROPPED CALLEES: {sorted(act_removed - want_removed)}")
        if want_added - act_added:
            parts.append(f"DECLARED BUT ABSENT CALLEES: {sorted(want_added - act_added)}")
        if want_removed - act_removed:
            parts.append(f"DECLARED REMOVAL NOT REAL: {sorted(want_removed - act_removed)}")
    return rep_add_manifest("B2", "bridge-substitution-refused", ok,
                            "; ".join(parts),
                            {"actual_callees": sorted(actual),
                             "baseline_count": len(base)})


def check_b3(manifest: dict, build_dir: Path, baseline: dict) -> bool | None:
    rb = manifest.get("resolution_b")
    if not rb:
        return None
    gif = rb.get("game_identity_facts") or {}
    combined = build_dir / "build" / "combined-supplier.o"
    if not combined.is_file():
        return rep_add_manifest("B3", "game-identity-path-untouched", False,
                                "combined-supplier.o missing", {})
    blob = combined.read_bytes()
    ok, details, info = True, [], {}
    if gif.get("path_untouched") is not True:
        ok = False
        details.append("path_untouched!=true: identity-path changes need their own "
                       "design receipt, not a composition manifest")
    strings = gif.get("evidence_strings") or baseline["game_identity_evidence_strings"]
    for s in strings:
        present = s.encode() in blob
        ok = ok and present
        details.append(f"identity marker {'present' if present else 'MISSING'}: {s[:60]}…")
    base_count = int(baseline.get("game_executable_path_string_count", 0))
    new_count = blob.count(b"game_executable_path")
    if new_count < base_count:
        ok = False
    details.append(f"game_executable_path occurrences {new_count} (baseline {base_count}, "
                   f"{'ok' if new_count >= base_count else 'REGRESSED'})")
    info = {"string_count": new_count, "baseline_count": base_count}
    return rep_add_manifest("B3", "game-identity-path-untouched", ok,
                            "; ".join(details), info)


def check_d2(manifest: dict, build_dir: Path) -> bool | None:
    d2 = manifest.get("d2_text_base")
    if not d2:
        return None
    combined = build_dir / "build" / "combined-supplier.o"
    ok, details, info = True, [], {}
    if combined.is_file():
        blob = combined.read_bytes()
        for s in d2.get("guard_strings") or []:
            present = s.encode() in blob
            ok = ok and present
            details.append(f"guard {'present' if present else 'MISSING'}: {s[:60]}…")
    else:
        ok = False
        details.append("combined-supplier.o missing")
    for src, want in (d2.get("source_pins") or {}).items():
        sp = resolve_pin(src, build_dir)
        if not sp.is_file():
            ok = False
            details.append(f"source pin UNREACHABLE: {src}")
            continue
        got = sha256_file(sp)
        good = got == want
        ok = ok and good
        details.append(f"pin {'MATCH' if good else 'MISMATCH'}: {src} ({want[:12]}…)")
        info[src] = {"match": good}
    return rep_add_manifest("D2", "d2-textbase-analog", ok,
                            "; ".join(details) or "d2_text_base declared empty", info)


def run_manifest_mode(manifest_path: Path, build_dir: Path, rep: "Report",
                      baseline_path: Path | None = None) -> None:
    """Append manifest checks to the report. Never raises: any internal error
    becomes a FAIL (fail-closed)."""
    baseline = load_baseline(baseline_path or MANIFEST_BASELINE)
    if baseline is None:
        rep.add("M0", "manifest-valid", False,
                f"gate baseline unreadable: {baseline_path or MANIFEST_BASELINE}")
        return
    try:
        manifest = json.loads(manifest_path.read_text())
    except (OSError, ValueError) as e:
        rep.add("M0", "manifest-valid", False, f"manifest unreadable: {e}")
        return
    print(f"=== manifest mode: {manifest_path.name} "
          f"(baseline {baseline.get('baseline_id')}) ===")
    del checks[:]
    try:
        m0 = check_m0(manifest, baseline, manifest_path)
        m1 = check_m1(manifest, build_dir)
        i1 = check_i1m(manifest, build_dir, baseline, rep)
        b1 = check_b1(manifest, build_dir)
        b2 = check_b2(manifest, build_dir, baseline)
        b3 = check_b3(manifest, build_dir, baseline)
        d2 = check_d2(manifest, build_dir)
    except Exception as exc:  # noqa: BLE001 — fail-closed
        rep.add("MX", "manifest-mode-error", False, f"internal error: {exc!r}")
        return
    rep.checks.extend(checks)
    # B1/B2/B3 return None when the manifest section is absent (not applicable).
    if b1 is None and manifest.get("resolution_b"):
        rep.add("B1", "configured-roots-symbols", False, "check did not run")
    if d2 is None and manifest.get("d2_text_base"):
        rep.add("D2", "d2-textbase-analog", False, "check did not run")
    _ = (m0, m1, i1, b2, b3)


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
    ap.add_argument("--manifest", type=Path, default=None,
                    help="composition-change manifest JSON (v2): enables declared-"
                         "drift verification on top of the 8 base checks")
    ap.add_argument("--baseline", type=Path, default=MANIFEST_BASELINE,
                    help="pinned baseline JSON for manifest mode (default: "
                         "baseline-composition5-r11.json next to this script)")
    ap.add_argument("--json", type=Path, default=None, help="write receipt JSON here")
    args = ap.parse_args(argv)

    build_dir = args.build_dir.resolve()
    if not build_dir.exists():
        print(f"ERROR: build dir does not exist: {build_dir}")
        return 2

    rep = Report()
    print(f"=== composition-5 gate: {build_dir} ===")
    # v2: when a manifest declares an import delta, I1's expected count is
    # derived from the manifest (baseline contract size + added - removed);
    # the set-level exactness is verified separately by I1+ below. Without a
    # manifest the expectation stays the frozen 196 (behavior unchanged).
    expected_imports = args.expected_imports
    manifest = None
    if args.manifest is not None:
        manifest = args.manifest.resolve()
        baseline_early = load_baseline(args.baseline)
        if baseline_early is not None and manifest.is_file():
            try:
                mf = json.loads(manifest.read_text())
                if mf.get("baseline_id") == baseline_early.get("baseline_id"):
                    delta = mf.get("import_delta") or {}
                    expected_imports = (len(baseline_early["contract_import_names"])
                                        + len(delta.get("added") or [])
                                        - len(delta.get("removed") or []))
            except (OSError, ValueError):
                pass  # M0 will fail closed on the unreadable manifest
    check_w1(build_dir, rep)
    check_w2(build_dir, rep)
    check_w3(build_dir, rep)
    check_i1(build_dir, rep, expected_imports)
    check_d1(build_dir, rep, args.compare_dir.resolve() if args.compare_dir else None)
    check_f1(build_dir, rep)
    check_s1(build_dir, rep, args.baseline_bytes, args.size_tolerance, args.max_size_bytes)
    check_p1(build_dir, rep)
    if manifest is not None:
        if not manifest.is_file():
            rep.add("M0", "manifest-valid", False,
                    f"manifest file does not exist: {manifest}")
        else:
            run_manifest_mode(manifest, build_dir, rep, args.baseline)

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
            "expected_imports": expected_imports,
            "verdict": verdict,
            "failed_checks": failed,
            "checks": rep.checks,
            # v2 additions (original fields above are byte-compatible):
            "gate_variant": "v2",
            "manifest": str(manifest) if manifest else None,
            "baseline": str(args.baseline) if manifest else None,
        }
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(receipt, indent=2))
        print(f"receipt: {args.json}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
