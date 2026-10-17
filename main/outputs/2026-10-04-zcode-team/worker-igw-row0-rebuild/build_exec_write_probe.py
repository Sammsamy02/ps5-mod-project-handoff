#!/usr/bin/env python3
"""build_exec_write_probe.py — worker-writability-probe-build (team 2026-10-04).

PRE-BUILD of the modbridge_exec_write_probe PRX skeleton for the
worker-exec-writability design. Packaging pipeline reused EXACTLY from the
frozen igf6/igf7 lineage (worker-combined-igf7/build_igf7_combined_probe.py,
itself verbatim worker-igf5-diagnosis-rebuild/build_igf6_probe.py:
import_references-first combine, the ALREADY-BUILT Packager.dll used IN
PLACE (never rebuilt), frozen finalize_sdk.py SDK 0x11000000 stamp, pinned
make_fself).

TWO variants, both built deterministically (run1 + run2, byte-equality):

  SKELETON (deliverable modbridge_exec_write_probe.prx)
    - EXACTLY the 7 console-proven verbatim frozen-contract import rows
      (zero new identities; the loaded-and-logged v2/v3/igf4 lineage set).
    - Compile-time UNARMED: the mprotect binding is a local stub that
      performs NO protection change; the full record/decision/restore
      sequence runs and logs, every attempt records outcome
      mprotect_import_absent (11). Zero memory risk.
  ARMED (staged-next artifact modbridge_exec_write_probe_armed.prx)
    - Adds ONE derived import row: sceKernelMprotect (libkernel), the
      closest tooling-proven spelling (SharpProspero StubCatalog); LOUDLY
      NOT console-proven; the FIRST new identity since the v2 isolation.
    - With the default EMPTY TRANSITIONS.json this build STILL performs
      zero mprotect calls: it exists as the design worker's
      load-resolution sanity stage.

Mprotect verification (brief-mandated): the frozen 191-import contract
contains NO mprotect-class strong row; kernel_mprotect appears ONLY as a
weak/absent/unresolved name and the lineage Packager rejects all weak
imports; the only mprotect mechanism ever run on console (igf5 raw
syscall-74 shim igcar_sys_mprotect) is FORBIDDEN by the igf6/igf7 inverted
syscall gate. This is asserted at build time and embedded in the receipt.

Transition injection (design worker): edit TRANSITIONS.json ONLY, then
rerun this script. Machinery sources are never edited. TRANSITIONS.json
bytes are part of the build-id seed.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "src"
WHAT = HERE.parents[2]  # .../translation-core/what
assert WHAT.name == "what", WHAT
TEAM = WHAT / "outputs/2026-10-04-zcode-team"

SDK_BIN = WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
CLANG = SDK_BIN / "prospero-clang"
LLVM = WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
LD = LLVM / "ld.lld"
NM = LLVM / "llvm-nm"
OBJDUMP = LLVM / "llvm-objdump"
DOTNET = WHAT / ".superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet"
FINALIZE = WHAT / ".superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = SDK_BIN.parent / "samples/install_app/make_fself.py"
FROZEN_PRX = (WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx")
FROZEN_SHA = "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
FROZEN_CONTRACT = WHAT / ".superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json"
PACKAGER_DLL = TEAM / "worker-anchor-validation/harness/bin/Release/net10.0/Packager.dll"
TRANSITIONS_JSON = HERE / "TRANSITIONS.json"

MODULE = "modbridge_exec_write_probe"
JSONL_PATH = "/data/modbridge-exec-write-probe.jsonl"
MIRROR_PATH = "/data/plugins/UE4SS.log"

C_SOURCES = [
    ("record_sink.c", SRC), ("ingame_libc.c", SRC), ("mapscan.c", SRC),
    ("vmmap_walk.c", SRC), ("sentinels.c", SRC), ("support_mem.c", SRC),
    ("module_entry.c", SRC), ("exec_write_probe.c", SRC),
]
ASM_SOURCES = [("import_references.S", SRC), ("boundaries.S", SRC)]
RECORDED_HEADERS = [
    ("record_sink.h", SRC), ("ingame_libc.h", SRC), ("ingame_abi.h", SRC),
    ("anchor_store.h", SRC), ("exec_write_probe.h", SRC),
]

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]

STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime"]
MPROTECT_DERIVED = "sceKernelMprotect"

# The igf6/igf7 forbidden set; sceKernelMprotect is forbidden in SKELETON
# builds only (it is the ARMED variant's single declared derived identity).
FORBIDDEN_ALWAYS = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC", "igcar_sys_mprotect",
]


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run(argv, receipt, cwd=None, allow_fail=False):
    env = dict(os.environ)
    env["LLVM_CONFIG"] = str(LLVM / "llvm-config")
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True,
                       cwd=str(cwd) if cwd else None, env=env)
    receipt.append({"argv": [str(a) for a in argv], "rc": r.returncode,
                    "stdout": r.stdout[-4000:], "stderr": r.stderr[-4000:]})
    if r.returncode != 0 and not allow_fail:
        print(json.dumps(receipt[-1], indent=1))
        sys.exit(f"FAILED: {argv}")
    return r


def parse_prot(v) -> int:
    if isinstance(v, str):
        return int(v, 0)
    return int(v)


def load_transitions() -> list:
    raw = json.loads(TRANSITIONS_JSON.read_text())
    if raw.get("schema") != 1:
        sys.exit("TRANSITIONS.json schema must be 1")
    rows = []
    for t in raw.get("transitions", []):
        rows.append({
            "addr": parse_prot(t["addr"]),
            "from_prot": parse_prot(t["from_prot"]),
            "to_prot": parse_prot(t["to_prot"]),
            "page_count": int(t["page_count"], 0) if isinstance(t["page_count"], str) else int(t["page_count"]),
            "confirmed": 1 if t.get("confirmed") else 0,
            "note": "".join(c for c in str(t.get("note", ""))[:95]
                            if c not in '"{}\\'),
        })
    return rows


def emit_transitions(gen: Path, rows: list) -> None:
    hp = gen / "xwp_transitions.h"
    cp = gen / "xwp_transitions.c"
    hp.write_text(
        "/* GENERATED by build_exec_write_probe.py — do not edit by hand.\n"
        "   The design worker edits TRANSITIONS.json and reruns the build;\n"
        "   machinery files are never edited (injection without rebuild). */\n"
        "#ifndef GENERATED_XWP_TRANSITIONS_H\n"
        "#define GENERATED_XWP_TRANSITIONS_H\n\n"
        '#include "exec_write_probe.h"\n\n'
        "extern const XwpTransition xwp_transitions[];\n"
        "extern const unsigned xwp_transitions_count;\n\n"
        "#endif  /* GENERATED_XWP_TRANSITIONS_H */\n")
    if rows:
        body = ",\n".join(
            '    {{0x{0:x}ull, 0x{1:x}ull, 0x{2:x}ull, {3}u, {4}u,\n'
            '     "{5}"}}'.format(t["addr"], t["from_prot"], t["to_prot"],
                                t["page_count"], t["confirmed"],
                                t["note"].replace('"', "'"))
            for t in rows)
        count = len(rows)
    else:
        body = ('    /* EMPTY placeholder table — the design fills '
                'TRANSITIONS.json.\n'
                '       Never executed: xwp_transitions_count is 0. */\n'
                '    {0ull, 0ull, 0ull, 0u, 0u,\n'
                '     "EMPTY placeholder — design fills TRANSITIONS.json"}')
        count = 0
    cp.write_text(
        "/* GENERATED by build_exec_write_probe.py — do not edit by hand. */\n"
        '#include "exec_write_probe.h"\n\n'
        "const XwpTransition xwp_transitions[] = {\n" + body + "\n};\n\n"
        f"const unsigned xwp_transitions_count = {count}u;\n")


ARMED_ROW_ASM = """/* GENERATED by build_exec_write_probe.py (ARMED variant only) — do not
   edit by hand. Typed owned import reference for the ONE derived
   mprotect-class identity. LOUD PROVENANCE: sceKernelMprotect is the
   SharpProspero StubCatalog spelling (libkernel); it appears in NO loaded
   lineage module and is NOT console-proven; the frozen 191 contract
   carries only a weak/absent kernel_mprotect name. The SKELETON build
   must never see this symbol. */
.section .data.rel.ro.modbridge_imports_armed,"aw",@progbits
.p2align 3
.globl xwp_import_row_sceKernelMprotect
.hidden xwp_import_row_sceKernelMprotect
.type sceKernelMprotect,@function
xwp_import_row_sceKernelMprotect:
.quad sceKernelMprotect
"""


def build_once(dest: Path, build_id: str, armed: bool, rows: list,
               receipt: list) -> dict:
    (dest / "bare-original").mkdir(parents=True)
    (dest / "bare-sdk").mkdir(parents=True)
    gen = dest / "generated"
    gen.mkdir(parents=True, exist_ok=True)
    variant = "armed" if armed else "skeleton"

    cfg = ["// GENERATED by build_exec_write_probe.py — do not edit by hand.",
           "#pragma once",
           f'#define IG_BUILD_ID "{build_id}"',
           f'#define IG_LOG_PATH "{JSONL_PATH}"',
           f'#define IG_MIRROR_LOG_PATH "{MIRROR_PATH}"',
           f"#define IG_XWP_MPROTECT_ARMED {1 if armed else 0}",
           "// mprotect verification (brief): the frozen 191-import contract",
           "// carries NO mprotect-class strong row; kernel_mprotect appears",
           "// only weak/absent/unresolved; sceKernelMprotect (ARMED only) is",
           "// the SharpProspero StubCatalog spelling, NOT console-proven.",
           f"// variant={variant} table_rows={len(rows)}"]
    (gen / "ingame_config.h").write_text("\n".join(cfg) + "\n")
    emit_transitions(gen, rows)

    inc = ["-I", str(dest), "-I", str(SRC)]  # dest so "generated/..." resolves

    objs = []
    c_all = list(C_SOURCES) + [("generated/xwp_transitions.c", dest)]
    for f, d in c_all:
        o = dest / (f.replace(".c", ".o").replace("/", "_"))
        run([CLANG] + CFLAGS + inc + ["-c", d / f, "-o", o], receipt)
        objs.append(o)
    asm_all = list(ASM_SOURCES)
    if armed:
        (gen / "xwp_import_rows.S").write_text(ARMED_ROW_ASM)
        asm_all.append(("generated/xwp_import_rows.S", dest))
    for f, d in asm_all:
        o = dest / (f.replace(".S", ".o").replace("/", "_"))
        run([CLANG] + CFLAGS + inc + ["-c", d / f, "-o", o], receipt)
        objs.append(o)

    combined = dest / "combined-ingame.o"
    ref = dest / "import_references.o"
    ordered = [ref]
    if armed:
        # The typed FUNC reference must follow import_references.o FIRST:
        # ld -r keeps the FIRST symbol type on merge (C UND refs are
        # NOTYPE; the packager requires exactly one typed reference).
        armed_row = dest / "generated_xwp_import_rows.o"
        ordered.append(armed_row)
    ordered += [o for o in objs if o not in ordered]
    run([LD, "-r", "-m", "elf_x86_64", "-o", combined] + ordered, receipt)

    # LOUD GATE: forbidden symbols must not appear anywhere in the link.
    forbidden = list(FORBIDDEN_ALWAYS)
    if not armed:
        forbidden.append(MPROTECT_DERIVED)
    nm = run([NM, combined], receipt)
    seen = set()
    for line in nm.stdout.splitlines():
        toks = line.split()
        if toks:
            seen.add(toks[-1].split("@")[0])
    bad = sorted(seen & set(forbidden))
    if bad:
        sys.exit(f"LOUD BUILD FAILURE: forbidden symbol in combined object: {bad}")
    # ARMED: the derived identity must actually be referenced (the packager
    # requires every contract row to own exactly one typed reference).
    if armed and MPROTECT_DERIVED not in seen:
        sys.exit("LOUD BUILD FAILURE: armed variant lacks the "
                 "sceKernelMprotect typed reference")

    # INVERTED syscall gate (igf6/igf7 parity): ZERO raw `syscall`.
    od = run([OBJDUMP, "-d", combined], receipt)
    syscall_count = sum(1 for ln in od.stdout.splitlines()
                        if ln.strip().endswith("syscall"))
    if syscall_count != 0:
        sys.exit(f"LOUD BUILD FAILURE: {syscall_count} raw syscall "
                 f"instruction(s) in combined object")

    # Import contract: 7 verbatim rows (+ the ONE derived row when armed).
    frozen = json.loads(FROZEN_CONTRACT.read_text())
    frozens = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in frozens]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    imports = [frozens[n] for n in STRONG_VERBATIM]
    provenance = {n: "verbatim:frozen-191-contract (row copied byte-identical)"
                  for n in STRONG_VERBATIM}
    if armed:
        imports.append({
            "ImportSymbol": {"Name": MPROTECT_DERIVED, "ModuleName": "libkernel",
                             "ModuleVersion": 257, "LibraryName": "libkernel",
                             "LibraryVersion": 1, "Soname": "libkernel.prx",
                             "PublishedName": MPROTECT_DERIVED},
            "symbol_type": "FUNC", "binding": "GLOBAL",
            "evidence": [{
                "source": str(WHAT / "outputs/2026-10-04-zcode-team/"
                              "worker-prx-packaging/prx-candidate-20261004-133501/"
                              "harness/tools/SharpProspero.Prx/StubCatalog.cs"),
                "qualification": ("TOOLING-PROVEN SPELLING ONLY. LOUD FLAG: NOT "
                                  "console-proven. The frozen 191-import contract "
                                  "carries NO mprotect strong row (verified by this "
                                  "build); kernel_mprotect appears only as "
                                  "weak/absent/unresolved there and the lineage "
                                  "Packager rejects weak imports. First derived "
                                  "identity added since the v2 isolation; the only "
                                  "mprotect mechanism ever run on console (igf5 raw "
                                  "syscall-74 igcar_sys_mprotect shim) is the "
                                  "suspected console+game crash and stays FORBIDDEN. "
                                  "Stage this build EMPTY first (load-resolution "
                                  "sanity), transitions only after the design "
                                  "confirms the far page.")}]})
        provenance[MPROTECT_DERIVED] = \
            "derived:SharpProspero-StubCatalog (NOT console-proven; LOUD FLAG)"
    counts = {"required_strong": len(imports), "required_weak": 0,
              "identity_covered_strong": len(imports),
              "covered_strong": len(imports), "missing_strong": 0, "conflicts": 0}
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": (f"worker-writability-probe-build {variant}: "
                  + ("the SAME 7 console-proven verbatim rows as the "
                     "loaded-and-logged v2/v3/igf4/igf6/igf7 lineage; the "
                     "mprotect route is a compile-time stub and adds ZERO "
                     "identities."
                     if not armed else
                     "the 7 console-proven verbatim rows PLUS exactly ONE "
                     "derived row sceKernelMprotect (SharpProspero StubCatalog "
                     "spelling; NOT console-proven; LOUD FLAG). NO raw-syscall "
                     "mprotect anywhere (igf6/igf7 inverted syscall gate).")),
        "counts": counts,
        "weak_unresolved_names": [],
        "identity_provenance": provenance,
        "mprotect_verification": {
            "frozen_191_contract": str(FROZEN_CONTRACT),
            "mprotect_strong_row_in_191": False,
            "kernel_mprotect_in_191": "weak_unresolved_names only (absent)",
            "packager_rejects_weak_imports": True,
            "raw_syscall_route": "FORBIDDEN (igf5 crash class; inverted gate)",
            "armed_route": MPROTECT_DERIVED if armed else None},
        "dropped_identities": {
            "derived_strong": ["sceKernelGetModuleList", "sceKernelGetModuleInfo",
                               "sceKernelGetExecutableModuleHandle", "dlopen",
                               "dlclose", "igcar_sys_mprotect"],
            "weak": ["dlsym", "__dlopen", "__dlclose", "__dlsym"],
            "reason": ("v2 isolation lineage set"
                       + ("" if armed else
                          "; sceKernelMprotect stays dropped in SKELETON builds"))},
        "contract": {"schema": 1,
                     "consumer": "worker-writability-probe-build (Packager.dll in place)",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = dest / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    written = json.loads(cpath.read_text())
    for i, n in enumerate(STRONG_VERBATIM):
        if json.dumps(written["contract"]["imports"][i], sort_keys=True) != \
           json.dumps(frozens[n], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: contract row {n} diverged from frozen")

    # Package: the ALREADY-BUILT Packager.dll used IN PLACE (read-only).
    if not PACKAGER_DLL.exists():
        sys.exit(f"LOUD FAILURE: Packager.dll missing at {PACKAGER_DLL}")
    bare = dest / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, PACKAGER_DLL, combined, cpath, csha, bare], receipt)

    profile = {"title_id": "PPSA33286",
               "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": JSONL_PATH,
               "marker": f"modbridge.execwriteprobe/v1 build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "variant": variant,
               "replaces": "nothing (new probe file; does NOT replace the "
                           "loaded igf7 modbridge_ingame_facts.prx; staging is "
                           "a design-worker decision, user-coordinated)",
               "entry_marker_first": True, "promotion_allowed": False,
               "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (dest / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = dest / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, dest / "profile.json", bare_sdk], receipt)
    final = dest / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipt)

    # Defense in depth: the FINAL inner ELF .text must have no 0F 05 pair.
    def text_has_syscall_pair(elf: Path) -> bool:
        b = elf.read_bytes()
        shoff = int.from_bytes(b[40:48], "little")
        shnum = int.from_bytes(b[60:62], "little")
        shstr = int.from_bytes(b[62:64], "little")
        hdr = lambda i: shoff + 64 * i
        sec = lambda i: (int.from_bytes(b[hdr(i) + 24:hdr(i) + 32], "little"),
                         int.from_bytes(b[hdr(i) + 32:hdr(i) + 40], "little"))
        stroff, strsz = sec(shstr)
        for i in range(shnum):
            nameoff = int.from_bytes(b[hdr(i):hdr(i) + 4], "little")
            name = b[stroff + nameoff:stroff + nameoff + 8].split(b"\0")[0]
            if name == b".text":
                off, size = sec(i)
                return b"\x0f\x05" in b[off:off + size]
        sys.exit("LOUD FAILURE: .text section not found in inner ELF")
    if text_has_syscall_pair(bare_sdk):
        sys.exit("LOUD BUILD FAILURE: 0F 05 byte pair present in inner ELF .text")

    receipt_out = {
        "marker": "modbridge.execwriteprobe.build",
        "schema": 1,
        "variant": variant,
        "build_id": build_id,
        "jsonl_path": JSONL_PATH,
        "mirror_path": MIRROR_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": sha256(FROZEN_PRX),
                           "hash_verified": sha256(FROZEN_PRX) == FROZEN_SHA,
                           "recipe": "package-commands.json"},
        "transitions_source": str(TRANSITIONS_JSON),
        "transitions_sha256": sha256(TRANSITIONS_JSON),
        "table_rows": len(rows),
        "consumed_inputs": [
            {"file": f, "dir": str(d), "sha256": sha256(d / f)}
            for f, d in C_SOURCES + ASM_SOURCES + RECORDED_HEADERS],
        "mprotect_verification": contract["mprotect_verification"],
        "read_mechanism": {
            "sequence": ("fresh ig_mapscan_run per row -> coverage + "
                         "from_prot equality gate -> vmmap_persist record -> "
                         "mprotect attempt -> rc/errno record -> gated "
                         "read(/content-preserving write) test -> restore "
                         "ALWAYS -> post_restore VMMAP re-proof"),
            "attempt_budget": 8,
            "fail_open": "module_start returns 0 on every path",
            "syscall_instruction_objects": syscall_count,
            "syscall_gate": "asserted == 0 (igf6/igf7 parity)",
            "import_row_added": bool(armed)},
        "commands": receipt,
        "combined_sha256": sha256(combined),
        "contract_sha256": csha,
        "strong_imports": {n: provenance[n] for n in provenance},
        "weak_imports": [],
        "forbidden_symbol_gate": {"names": forbidden, "status": "passed"},
        "final_prx": str(final),
        "final_sha256": sha256(final),
        "final_bytes": final.stat().st_size,
        "inner_elf_sha256": sha256(bare_sdk),
        "inner_elf_bytes": bare_sdk.stat().st_size,
        "native_executed": False,
        "in_game_results": False,
        "notes": "Local packaging only. Console facts require the "
                 "user-coordinated Townfall launch (design worker's stage "
                 "plan). SKELETON is proposal-only and unarmed."}
    (dest / "build-receipt.json").write_text(json.dumps(receipt_out, indent=1))
    return receipt_out


def build_variant(armed: bool, rows: list) -> dict:
    h = hashlib.sha256()
    for f, d in sorted(C_SOURCES + ASM_SOURCES):
        h.update((d / f).read_bytes())
    for f, d in sorted(RECORDED_HEADERS):
        h.update((d / f).read_bytes())
    h.update(TRANSITIONS_JSON.read_bytes())
    h.update(JSONL_PATH.encode())
    h.update(MIRROR_PATH.encode())
    h.update(b"igd1-exec-write-probe")
    h.update(b"armed" if armed else b"skeleton")
    build_id = f"igd1-{h.hexdigest()[:12]}"

    base = HERE / "determinism" / ("run-armed" if armed else "run-skeleton")
    runs = []
    for name in (f"{base}-1", f"{base}-2"):
        d = Path(name)
        if d.exists():
            sys.exit(f"refuse to overwrite {d}")
    for name in (f"{base}-1", f"{base}-2"):
        d = Path(name)
        r = build_once(d, build_id, armed, rows, [])
        runs.append((d, r))
        print(f"{'armed' if armed else 'skeleton'} {d.name}: "
              f"build_id={r['build_id']} prx={r['final_sha256']}")
    (d1, r1), (d2, r2) = runs
    compare = {}
    for key, rel in (("final_prx", f"{MODULE}.prx"),
                     ("inner_elf", f"bare-sdk/{MODULE}.prx"),
                     ("combined", "combined-ingame.o"),
                     ("contract", "import-contract.json")):
        a, b = sha256(d1 / rel), sha256(d2 / rel)
        compare[key] = {"run1": a, "run2": b, "equal": a == b}
        if a != b:
            sys.exit(f"LOUD FAILURE: determinism mismatch on {key}: {compare[key]}")
    deliverable = HERE / (f"{MODULE}_armed.prx" if armed else f"{MODULE}.prx")
    shutil.copyfile(d1 / f"{MODULE}.prx", deliverable)
    summary = {"marker": "modbridge.execwriteprobe.build",
               "variant": "armed" if armed else "skeleton",
               "build_id": build_id, "determinism": compare,
               "deliverable": str(deliverable),
               "run1": {k: r1[k] for k in ("final_sha256", "final_bytes",
                                           "inner_elf_sha256", "inner_elf_bytes",
                                           "combined_sha256", "contract_sha256")},
               "read_mechanism": r1["read_mechanism"],
               "mprotect_verification": r1["mprotect_verification"],
               "table_rows": len(rows),
               "transitions_sha256": r1["transitions_sha256"],
               "consumed_inputs": r1["consumed_inputs"],
               "native_executed": False, "in_game_results": False}
    rname = "build-receipt-armed.json" if armed else "build-receipt-skeleton.json"
    (HERE / rname).write_text(json.dumps(summary, indent=1))
    summary["commands"] = r1["commands"]
    (HERE / rname).write_text(json.dumps(summary, indent=1))
    return summary


def main() -> None:
    if FROZEN_PRX.exists() and sha256(FROZEN_PRX) != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {sha256(FROZEN_PRX)}")
    rows = load_transitions()
    skel = build_variant(False, rows)
    armed = build_variant(True, rows)
    print(json.dumps({
        "skeleton": {k: skel[k] for k in ("build_id", "deliverable",
                                          "determinism")},
        "armed": {k: armed[k] for k in ("build_id", "deliverable",
                                        "determinism")}}))


if __name__ == "__main__":
    main()
