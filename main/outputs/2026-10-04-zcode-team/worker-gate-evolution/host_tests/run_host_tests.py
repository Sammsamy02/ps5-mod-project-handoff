#!/usr/bin/env python3
"""Host tests for gate_check_composition5_v2.py (worker-gate-evolution).

Offline, stdlib-only. Produces fixture build dirs under host-tests/ and runs:

  A  parity:        no-manifest run on the real r11 build dir — stdout
                    byte-identical to the live gate, exit codes equal, and the
                    8 checks/verdict in the receipt equal to the live receipt.
  B  pass:          synthetic build dir + synthetic baseline + manifest whose
                    declared import delta EXACTLY matches the synthetic
                    object's delta (+ resolution_b, + d2 pins) -> full GO.
  C  undeclared:    combined object carries an EXTRA undefined symbol not in
                    the manifest -> I1+ FAIL (undeclared additions).
  D  wrong-delta:   manifest declares a symbol that is NOT actually added
                    -> I1+ FAIL (phantom addition).
  E  real-dir:      manifest mode (empty delta + resolution_b/D2 sections) on
                    the real r11 build dir -> manifest checks PASS, verdict GO
                    with --compare-dir.

Each case writes a receipt JSON under host-tests/receipts/ and this script
prints a compact PASS/FAIL summary. Exit 0 iff every assertion holds.
"""
from __future__ import annotations

import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MYDIR = HERE.parent
ROOT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
TEAM = ROOT / "outputs/2026-10-04-zcode-team"
GATE_V2 = MYDIR / "gate_check_composition5_v2.py"
GATE_LIVE = TEAM / "worker-composition5-gate/gate_check_composition5.py"
BASELINE_R11 = MYDIR / "baseline-composition5-r11.json"
R11 = TEAM / "worker-composition-5-rebuild/prx-candidate-composition5-1"
R11_2 = TEAM / "worker-composition-5-rebuild/prx-candidate-composition5-2"
LLVM = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
CLANG = LLVM / "clang"

SYNTH = HERE / "synth"
RECEIPTS = HERE / "receipts"

IDENTITY_STRING = ("observed executable path is empty; refusing to fabricate "
                   "game_executable_path")
D2_GUARDS = ["Mapped metadata or spans exceed supplied readable bytes",
             "Mapped text base must be registered readable executable bytes"]

STATIC_CHECK_NAMES = [
    "elf_magic_bare", "fself_magic_wrapped", "wrapped_embedded_elf_header_equal",
    "fself_payload_digest_matches_bare", "wrapped_segments_equal", "elf_module_type_flags",
    "elf64_x86_64_le", "pt_sce_module_unique", "sdk_stamp_0x11000000",
    "explicit_dt_init_dt_fini", "loader_owns_no_arrays", "init_fini_slots",
    "init_fini_slots_present", "section_.eh_frame_present", "section_.eh_frame_hdr_present",
    "section_.gcc_except_table_present", "imports_equal_extended_contract",
    "linker_provided_dynamic_not_imported", "imports_resolve_to_sdk_or_linker",
    "entry_alignment_evidence",
]

failures: list[str] = []


def check(cond: bool, label: str) -> bool:
    print(f"  [{'PASS' if cond else 'FAIL'}] {label}")
    if not cond:
        failures.append(label)
    return cond


def run_gate(args: list[str]) -> tuple[int, str]:
    p = subprocess.run([sys.executable, str(GATE_V2)] + args,
                       capture_output=True, text=True, timeout=600)
    return p.returncode, p.stdout


def compile_c(src: Path, out: Path) -> None:
    # Pinned LLVM clang with an explicit x86-64 linux target so the synthetic
    # "combined objects" are ELF64 relocatables (the host default is Mach-O).
    rc = subprocess.run([str(CLANG), "-c", "-O1", "-fPIC",
                         "--target=x86_64-unknown-linux-gnu",
                         "-o", str(out), str(src)],
                        capture_output=True, text=True)
    if rc.returncode != 0:
        raise RuntimeError(f"clang failed: {rc.stderr}")
    if out.read_bytes()[:4] != b"\x7fELF":
        raise RuntimeError(f"{out} is not ELF")


def elf_und(path: Path) -> set[str]:
    data = path.read_bytes()
    (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 0x3A)
    if e_shnum == 0:
        e_shnum = struct.unpack_from("<Q", data, e_shoff + 32)[0]
    secs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh_type, = struct.unpack_from("<I", data, off + 4)
        sh_offset, sh_size = struct.unpack_from("<QQ", data, off + 24)
        sh_link, = struct.unpack_from("<I", data, off + 40)
        secs.append((sh_type, sh_offset, sh_size, sh_link))
    und: set[str] = set()
    for typ, off, size, link in secs:
        if typ != 2:
            continue
        _, soff, ssize, _ = secs[link]
        strdata = data[soff:soff + ssize]
        for j in range(size // 24):
            o = off + j * 24
            st_name, _, _, st_shndx = struct.unpack_from("<IBBH", data, o)
            if st_name >= len(strdata):
                continue
            end = strdata.find(b"\x00", st_name)
            name = strdata[st_name:end].decode("utf-8", "replace")
            if name and st_shndx == 0:
                und.add(name)
    return und


def sha256_file(path: Path) -> str:
    import hashlib
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_synth_objects() -> dict[str, Path]:
    """Two synthetic 'combined objects': baseline UND {a,b,c}, changed UND
    {a,b,c,d}; both define the W1 chain symbols + an anchor; both embed the
    identity + D2 guard strings via asm so B3/D2 string evidence works."""
    SYNTH.mkdir(parents=True, exist_ok=True)
    common_defs = r'''
void mfa_activate(void) {}
void _ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE(void) {}
void _ZN9modbridge6native12RuntimeOwner5startEv(void) {}
void _ZN2RC8Platform15NativeModSourceC2EPv(void) {}
'''
    strings_asm = (
        '__asm__(".ascii \\"' + IDENTITY_STRING + '\\"\\n");\n'
        + "".join('__asm__(".ascii \\"' + g + '\\"\\n");\n' for g in D2_GUARDS)
    )
    base = SYNTH / "synth_base.c"
    base.write_text(
        "extern void sa(void); extern void sb(void); extern void sc(void);\n"
        "void synth_anchor_base(void){sa();sb();sc();}\n"
        + common_defs + strings_asm)
    new = SYNTH / "synth_new.c"
    new.write_text(
        "extern void sa(void); extern void sb(void); extern void sc(void);"
        " extern void sd(void);\n"
        "void synth_anchor_base(void){sa();sb();sc();sd();}\n"
        + common_defs + strings_asm)
    extra = SYNTH / "synth_extra.c"
    extra.write_text(
        "extern void sa(void); extern void sb(void); extern void sc(void);"
        " extern void sd(void); extern void se(void);\n"
        "void synth_anchor_base(void){sa();sb();sc();sd();se();}\n"
        + common_defs + strings_asm)
    outs = {}
    for name, src in (("base", base), ("new", new), ("extra", extra)):
        obj = SYNTH / f"combined-{name}.o"
        compile_c(src, obj)
        outs[name] = obj
    return outs


def synth_baseline(obj_base: Path, bridge_refs: list[str]) -> Path:
    und = sorted(elf_und(obj_base))
    doc = {
        "baseline_id": "synthetic-host-test",
        "date": "2026-10-04",
        "build_dir": str(SYNTH),
        "combined_object_sha256": sha256_file(obj_base),
        "combined_und": und,
        "combined_und_count": len(und),
        "supplier_contract_sha256": "synthetic",
        "contract_import_names": sorted(n for n in und if n in ("sa", "sb", "sc")),
        "contract_import_count": 3,
        "bridge_call_refs": bridge_refs,
        "game_identity_evidence_strings": [IDENTITY_STRING],
        "game_executable_path_string_count": obj_base.read_bytes().count(b"game_executable_path"),
        "d2_guard_strings": D2_GUARDS,
    }
    p = SYNTH / "baseline-synthetic.json"
    p.write_text(json.dumps(doc, indent=1))
    return p


def make_synth_build_dir(obj: Path, imports: int, contract_names: list[str]) -> Path:
    """A build dir shaped like the real one: real entry/bridge/PRX hardlinked,
    synthetic combined object + contract + receipts."""
    bd = SYNTH / f"builddir-{obj.stem}-imports{imports}"
    if bd.exists():
        shutil.rmtree(bd)
    (bd / "build").mkdir(parents=True)
    (bd / "bare-original").mkdir(parents=True)
    (bd / "verification-evidence").mkdir(parents=True)
    shutil.copy(obj, bd / "build" / "combined-supplier.o")
    os_link(R11 / "build" / "runtime_module_entry.o", bd / "build" / "runtime_module_entry.o")
    os_link(R11 / "build" / "runtime_start_bridge.o", bd / "build" / "runtime_start_bridge.o")
    os_link(R11 / "bare-original" / "modbridge_supplier_composition.prx",
            bd / "bare-original" / "modbridge_supplier_composition.prx")
    # supplier contract: synthetic names
    contract = {"contract": {"imports": [
        {"ImportSymbol": {"Name": n}} for n in contract_names]},
        "counts": {"required_strong": len(contract_names)}}
    (bd / "supplier-contract.json").write_text(json.dumps(contract, indent=1))
    # build receipt: packager stdout JSON embedded as a string (what the gate scans for)
    packager = json.dumps({
        "imports": imports, "functions": imports,
        "output_sha256": "synthetic", "explicit_init": "module_start",
        "explicit_fini": "module_stop", "loader_array_ownership": False,
        "runtime_or_hooks_executed": False})
    (bd / "build-receipt.json").write_text(json.dumps({"packager_stdout": packager}))
    # static checks: synthetic all-pass (documented fixture, not engine evidence)
    (bd / "verification-evidence" / "static-checks.json").write_text(json.dumps({
        "status": "STATIC_CHECKS_PASSED",
        "checks": {n: {"pass": True} for n in STATIC_CHECK_NAMES}}))
    # determinism: synthetic double-build receipt (hash of the hardlinked PRX)
    prx = bd / "bare-original" / "modbridge_supplier_composition.prx"
    h = sha256_file(prx)
    (bd / "determinism.json").write_text(json.dumps(
        {"build1_sha256": h, "build2_sha256": h, "bare_sha256": h}))
    return bd


def os_link(src: Path, dst: Path) -> None:
    import os
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy(src, dst)


def stdout_body(text: str) -> str:
    """Drop the trailing 'receipt: <path>' line (paths differ by design)."""
    lines = text.splitlines(keepends=True)
    if lines and lines[-1].startswith("receipt:"):
        lines = lines[:-1]
    return "".join(lines)


def write_manifest(path: Path, doc: dict) -> Path:
    path.write_text(json.dumps(doc, indent=1))
    return path


def manifest_checks(receipt_path: Path) -> dict:
    d = json.loads(receipt_path.read_text())
    return {c["id"]: c["status"] for c in d["checks"]}


def main() -> int:
    RECEIPTS.mkdir(parents=True, exist_ok=True)
    import hashlib

    # ---------------- A: parity (no manifest, real r11 dir) ----------------
    # NOTE: parity is asserted against the LIVE GATE RUN IN THE SAME MOMENT on
    # the same dir — the archived gate-receipt-composition5-r5.json predates a
    # rebuild of the r11 dir (its PRX sha 3695d100… differs from the current
    # 7774b663…), so a checks-array comparison against that stale file is
    # meaningless. Verdict equality with the stale receipt is still asserted.
    print("== A: no-manifest parity vs live gate (real r11 dir, same-moment runs) ==")
    live_json = RECEIPTS / "a-parity-live.json"
    v2_json = RECEIPTS / "a-parity-v2.json"
    args = ["--build-dir", str(R11), "--compare-dir", str(R11_2)]
    p1 = subprocess.run([sys.executable, str(GATE_LIVE)] + args + ["--json", str(live_json)],
                        capture_output=True, text=True, timeout=900)
    p2 = subprocess.run([sys.executable, str(GATE_V2)] + args + ["--json", str(v2_json)],
                        capture_output=True, text=True, timeout=900)
    (RECEIPTS / "a-live-stdout.txt").write_text(p1.stdout)
    (RECEIPTS / "a-v2-stdout.txt").write_text(p2.stdout)
    check(p1.returncode == 0 and p2.returncode == 0, "A: both exit 0 (GO)")
    check(stdout_body(p1.stdout) == stdout_body(p2.stdout),
          "A: stdout byte-identical (modulo receipt-path line)")
    live_rcpt = json.loads(live_json.read_text())
    v2_rcpt = json.loads(v2_json.read_text())
    check(v2_rcpt["checks"] == live_rcpt["checks"], "A: receipt checks arrays equal (same-moment)")
    stale = json.loads((TEAM / "worker-composition5-gate/gate-receipt-composition5-r5.json").read_text())
    check(v2_rcpt["verdict"] == stale["verdict"], "A: verdict equal to archived r5 receipt")

    # ---------------- synthetic fixtures ----------------
    objs = make_synth_objects()
    bridge_refs = json.loads(BASELINE_R11.read_text())["bridge_call_refs"]
    sbase = synth_baseline(objs["base"], bridge_refs)
    und_new = elf_und(objs["new"])
    print(f"  synth UND baseline={sorted(elf_und(objs['base']))} new={sorted(und_new)}")
    check(und_new == {"sa", "sb", "sc", "sd"}, "B: synthetic delta is exactly {sd}")

    overlay_header = ROOT / ("outputs/2026-10-04-zcode-team/worker-composition-5-rebuild/"
                             "staging/lineage-overlay-1/PsMappedImageCaller.hpp")
    overlay_sha = sha256_file(overlay_header)

    # ---------------- B: matching synthetic manifest -> GO ----------------
    print("== B: matching synthetic manifest (delta +{sd}, Resolution B, D2 pins) ==")
    bd_b = make_synth_build_dir(objs["new"], imports=4, contract_names=["sa", "sb", "sc", "sd"])
    man_b = write_manifest(SYNTH / "manifest-pass.json", {
        "manifest_version": 1,
        "manifest_id": "host-test-pass-1",
        "baseline_id": "synthetic-host-test",
        "justification": "host-test: synthetic composition change adds unit symbol sd",
        "design_receipts": ["outputs/2026-10-04-zcode-team/worker-gate-evolution/REPORT.md"],
        "changed_units": [{
            "unit": "synth-demo-unit",
            "role": "replace",
            "sha256_after": {str(objs["new"]): sha256_file(objs["new"])},
            "receipt": str(SYNTH / "unit-receipt-pass.json"),
            "justification": "synthetic changed unit for host test"}],
        "import_delta": {"added": ["sd"], "removed": []},
        "resolution_b": {
            "expected_symbols": [
                {"symbol": "synth_anchor_base", "unit": "combined",
                 "binding": "defined", "match": "exact"}],
            "bridge_unchanged": True,
            "game_identity_facts": {"path_untouched": True}},
        "d2_text_base": {
            "guard_strings": D2_GUARDS,
            "source_pins": {str(overlay_header): overlay_sha}},
    })
    (SYNTH / "unit-receipt-pass.json").write_text(json.dumps({
        "bundle": "host-test synthetic unit receipt",
        "provenance": {"synth-demo-unit": "synthetic; compiled by run_host_tests.py"},
        "files": {str(objs["new"]): sha256_file(objs["new"])}}))
    rc_b, _ = run_gate(["--build-dir", str(bd_b), "--manifest", str(man_b),
                        "--baseline", str(sbase),
                        "--json", str(RECEIPTS / "b-pass.json")])
    st_b = manifest_checks(RECEIPTS / "b-pass.json")
    check(rc_b == 0, "B: gate exit 0 (GO)")
    for cid in ("M0", "M1", "I1+", "B1", "B2", "B3", "D2"):
        check(st_b.get(cid) == "PASS", f"B: {cid} PASS")

    # ---------------- C: undeclared extra import -> FAIL ----------------
    print("== C: undeclared extra undefined symbol se ==")
    bd_c = make_synth_build_dir(objs["extra"], imports=5,
                                contract_names=["sa", "sb", "sc", "sd", "se"])
    man_c = write_manifest(SYNTH / "manifest-undeclared.json", {
        "manifest_version": 1,
        "manifest_id": "host-test-undeclared-1",
        "baseline_id": "synthetic-host-test",
        "justification": "host-test: declares sd but the object also adds se",
        "import_delta": {"added": ["sd"], "removed": []},
    })
    rc_c, _ = run_gate(["--build-dir", str(bd_c), "--manifest", str(man_c),
                        "--baseline", str(sbase),
                        "--json", str(RECEIPTS / "c-undeclared.json")])
    st_c = manifest_checks(RECEIPTS / "c-undeclared.json")
    check(rc_c == 1, "C: gate exit 1 (NO-GO)")
    check(st_c.get("I1+") == "FAIL", "C: I1+ FAIL")
    c_i1 = json.loads((RECEIPTS / "c-undeclared.json").read_text())
    i1p = next(c for c in c_i1["checks"] if c["id"] == "I1+")
    check("'se'" in i1p["details"], "C: receipt names undeclared addition se")
    check(st_c.get("I1") == "FAIL", "C: base I1 FAIL (count 5 != manifest-expected 4)")

    # ---------------- D: manifest declares the wrong delta -> FAIL ----------
    print("== D: manifest declares sx but object adds sd ==")
    bd_d = make_synth_build_dir(objs["new"], imports=4, contract_names=["sa", "sb", "sc", "sd"])
    man_d = write_manifest(SYNTH / "manifest-wrong.json", {
        "manifest_version": 1,
        "manifest_id": "host-test-wrong-1",
        "baseline_id": "synthetic-host-test",
        "justification": "host-test: declares sx which never appears",
        "import_delta": {"added": ["sx"], "removed": []},
    })
    rc_d, _ = run_gate(["--build-dir", str(bd_d), "--manifest", str(man_d),
                        "--baseline", str(sbase),
                        "--json", str(RECEIPTS / "d-wrong.json")])
    st_d = manifest_checks(RECEIPTS / "d-wrong.json")
    check(rc_d == 1, "D: gate exit 1 (NO-GO)")
    check(st_d.get("I1+") == "FAIL", "D: I1+ FAIL")
    d_i1 = json.loads((RECEIPTS / "d-wrong.json").read_text())
    i1d = next(c for c in d_i1["checks"] if c["id"] == "I1+")
    check("'sx'" in i1d["details"], "D: receipt names phantom addition sx")

    # ---------------- E: manifest mode on the REAL r11 dir ----------------
    print("== E: real r11 dir + empty-delta manifest (Resolution B/D2 machinery) ==")
    man_e = write_manifest(SYNTH / "manifest-realdelta.json", {
        "manifest_version": 1,
        "manifest_id": "host-test-real-empty-1",
        "baseline_id": "composition5-r11",
        "justification": "host-test: no-op change receipt against the real r11 build",
        "changed_units": [],
        "import_delta": {"added": [], "removed": []},
        "resolution_b": {
            "expected_symbols": [
                {"symbol": "_ZN2RC8Platform17NativeLoaderFactsC2", "unit": "combined",
                 "binding": "defined", "match": "prefix"}],
            "bridge_unchanged": True,
            "game_identity_facts": {"path_untouched": True}},
        "d2_text_base": {
            "guard_strings": D2_GUARDS,
            "source_pins": {str(overlay_header): overlay_sha}},
    })
    rc_e, _ = run_gate(["--build-dir", str(R11), "--compare-dir", str(R11_2),
                        "--manifest", str(man_e),
                        "--json", str(RECEIPTS / "e-real-manifest.json")])
    st_e = manifest_checks(RECEIPTS / "e-real-manifest.json")
    check(rc_e == 0, "E: gate exit 0 (GO)")
    for cid in ("M0", "M1", "I1+", "B1", "B2", "B3", "D2"):
        check(st_e.get(cid) == "PASS", f"E: {cid} PASS")
    check(st_e.get("I1") == "PASS", "E: base I1 PASS (expected count still 196)")

    print()
    if failures:
        print(f"HOST-TEST RESULT: {len(failures)} FAILURE(S): {failures}")
        return 1
    print("HOST-TEST RESULT: ALL PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
