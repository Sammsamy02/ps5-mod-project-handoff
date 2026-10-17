#!/usr/bin/env python3
"""build_igf7_c2_probe.py — igf7- build of the C2(b) ALTERNATIVE CORRESPONDENCE
probe (worker-c2-probe, team 2026-10-04).

Packaging pipeline reused EXACTLY from the frozen igf6 lineage
(worker-igf5-diagnosis-rebuild/build_igf6_probe.py: import_references-first
combine, the ALREADY-BUILT Packager.dll used IN PLACE (never rebuilt), frozen
finalize_sdk.py SDK 0x11000000 stamp, pinned make_fself). Labeled changes vs
the igf6 build:

  - mission: C2(b) ALTERNATIVE correspondence check (worker-c2-alt-correspondence
    C2-ALTERNATIVE-SPEC.md section 7) — map-GATED reads of FIVE distinctive
    DATA windows (B, C, D, E, A order; A last), raw live_hex emitted,
    comparison OFFLINE by the controller. Replaces the igf6 direct
    exec-span reads, which crashed on their first exec byte read (record 4
    never arrived) — exec spans are NEVER dereferenced here;
  - NO expected bytes anywhere in the binary: the generated config carries
    candidate vaddr/rt/file_off/len ONLY. A LOUD build gate asserts that
    none of the five expected byte windows (raw or ASCII-hex, from this
    worker's EXPECTED-VALUES-OFFLINE.json) appears in the combined object
    or the final PRX. No byte_match, no on-console comparison;
  - the same-invocation readable span list is PERSISTED (paged mapspans
    records) before the first candidate read — closes the igf6 gap where
    8685 readable spans were counted but not saved;
  - GUA predicate / anchor_pages / townfall_layout_probes are GONE (not part
    of this mission); the import contract is STILL the SAME 7 VERBATIM
    frozen-contract rows (unchanged, console-proven);
  - zero raw `syscall` instructions asserted in the combined object, and no
    0F 05 byte pair in the final inner ELF .text (igf6 gates, unchanged);
    no mprotect in any form (forbidden-symbol gate, unchanged);
  - module name/path/log path UNCHANGED (same-file replacement lineage);
  - no-arg mode: determinism double-build (run1 + run2), byte-equality
    comparison, final PRX copied to the worker directory.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[2]  # .../translation-core/what
assert WHAT.name == "what", WHAT
SRC = HERE / "src"
TEAM = WHAT / "outputs/2026-10-04-zcode-team"
IGF6 = TEAM / "worker-igf5-diagnosis-rebuild"   # igf6 lineage (read-only)
EXPECTED_JSON = HERE / "EXPECTED-VALUES-OFFLINE.json"

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

# igf6 reused the anchor-validation Packager.dll IN PLACE; same here.
PACKAGER_DLL = TEAM / "worker-anchor-validation/harness/bin/Release/net10.0/Packager.dll"

# Retained-image provenance (this worker's independent offline read; the
# IMAGE bytes themselves never enter the binary — gate-only literals).
IMAGE_SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"

# Checkpoint geometry cross-check constants (RECORDED by the probe, never
# assumed; the derived map values are what the controller compares).
IMAGE_BASE = 0x400000
EXEC_EXPECTED_END = 0x787C000          # base + phdr0 filesz page-rounded
EXEC_EXPECTED_BYTES = EXEC_EXPECTED_END - IMAGE_BASE  # 0x747c000

# The 5 candidates (vaddr -> rt = base + vaddr; file_off for offline compare
# provenance; len <= 64). READ ORDER: B, C, D, E, A (A last per spec).
CANDIDATES = [
    ("B", 0x7826782, 8, "branch_name_UE5"),
    ("C", 0x75CFFDA, 32, "BuildIdOverride_utf16"),
    ("D", 0x7B8845D, 19, "CurrentChangelist_ascii"),
    ("E", 0x7DD2A40, 64, "AES_sbox_64B"),
    ("A", 0xBC5F250, 36, "GNU_build_id_note"),
]
FILE_OFFS = {"B": 0x782A782, "C": 0x75D3FDA, "D": 0x7B8C45D,
             "E": 0x7DD6A40, "A": 0xB93B250}

MODULE = "modbridge_ingame_facts"  # UNCHANGED: same-file replacement lineage
LOG_PATH = "/data/modbridge-ingame-facts.jsonl"
C_SOURCES = ["record_sink.c", "ingame_libc.c", "vmmap_walk.c", "mapscan.c",
             "c2_data_probe.c", "sentinels.c", "module_entry.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]

# Files that MUST be byte-identical to the igf6 lineage (worker-igf5-
# diagnosis-rebuild, the console-crash evidence source for the inverted
# gating design). The two NEW files are listed separately.
VENDORED_VS_IGF6 = ["record_sink.c", "record_sink.h", "ingame_libc.c",
                    "ingame_libc.h", "vmmap_walk.c", "mapscan.c",
                    "anchor_store.h", "ingame_abi.h", "import_references.S",
                    "boundaries.S", "sentinels.c"]
NEW_FILES = ["c2_data_probe.c", "module_entry.c"]

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]

STRONG_VERBATIM = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
                   "sysctl", "getpid", "clock_gettime"]

FORBIDDEN_SYMBOLS = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC", "sceKernelMprotect",
    "igcar_sys_mprotect",
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


def check_vendored() -> list:
    rows = []
    for f in VENDORED_VS_IGF6:
        a, b = IGF6 / "src" / f, SRC / f
        sa, sb = sha256(a), sha256(b)
        if sa != sb:
            sys.exit(f"LOUD FAILURE: vendored {f} diverged from igf6 lineage")
        rows.append({"file": f, "sha256": sb, "identical_to": str(a)})
    for f in NEW_FILES:
        rows.append({"file": f, "sha256": sha256(SRC / f), "new": True})
    return rows


def load_forbidden_bytes() -> list:
    """The offline expected windows are GATE-ONLY literals: they assert the
    binary does NOT contain them (no expected bytes in the probe)."""
    ev = json.loads(EXPECTED_JSON.read_text())
    if ev["retained_capture"]["sha256"] != IMAGE_SHA:
        sys.exit(f"LOUD FAILURE: retained-image sha drifted: "
                 f"{ev['retained_capture']['sha256']}")
    out = []
    for c in ev["candidates"]:
        out.append((c["id"], bytes.fromhex(c["expected_hex"]),
                    c["expected_hex"].encode("ascii")))
    if len(out) != 5:
        sys.exit("LOUD FAILURE: expected 5 candidate windows in "
                 "EXPECTED-VALUES-OFFLINE.json")
    return out


def assert_no_expected_bytes(blob: bytes, where: str, forbidden: list) -> None:
    for cid, raw, ascii_hex in forbidden:
        if raw in blob:
            sys.exit(f"LOUD FAILURE: expected window {cid} RAW bytes present "
                     f"in {where} — probe must not embed expected bytes")
        if ascii_hex in blob:
            sys.exit(f"LOUD FAILURE: expected window {cid} ASCII hex present "
                     f"in {where} — probe must not embed expected bytes")


def text_has_syscall_pair(elf: Path) -> bool:
    """True iff the .text section of the ELF contains the 0F 05 byte pair."""
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


def build_once(dest: Path, build_id: str, forbidden: list, receipt) -> dict:
    (dest / "bare-original").mkdir(parents=True)
    (dest / "bare-sdk").mkdir(parents=True)
    gen = dest / "generated"
    gen.mkdir(parents=True, exist_ok=True)
    cfg = [
        "// GENERATED by build_igf7_c2_probe.py — do not edit by hand.\n"
        "#pragma once\n",
        f'#define IG_BUILD_ID "{build_id}"\n',
        f'#define IG_LOG_PATH "{LOG_PATH}"\n',
        "// C2(b) candidate windows (C2-ALTERNATIVE-SPEC.md section 5).\n"
        "// PROVENANCE ONLY: vaddr/rt/file_off/len. The expected BYTES are\n"
        "// deliberately ABSENT (offline comparison only; a build gate\n"
        "// asserts none of them is present in any artifact).\n",
        f"#define IG_IMAGE_BASE 0x{IMAGE_BASE:x}ull\n",
        f"#define IG_EXEC_EXPECTED_END 0x{EXEC_EXPECTED_END:x}ull\n",
        f"#define IG_EXEC_EXPECTED_BYTES 0x{EXEC_EXPECTED_BYTES:x}ull\n",
        f"#define IG_C2_COUNT {len(CANDIDATES)}\n",
        "#define IG_C2_MAX_LEN 64u\n",
    ]
    for cid, vaddr, ln, what in CANDIDATES:
        cfg.append(f"// {cid}: {what}\n")
        cfg.append(f"#define IG_C2_{cid}_VADDR 0x{vaddr:x}ull\n")
        cfg.append(f"#define IG_C2_{cid}_RT 0x{IMAGE_BASE + vaddr:x}ull\n")
        cfg.append(f"#define IG_C2_{cid}_FILEOFF 0x{FILE_OFFS[cid]:x}ull\n")
        cfg.append(f"#define IG_C2_{cid}_LEN {ln}u\n")
    (gen / "ingame_config.h").write_text("".join(cfg))

    inc = ["-I", str(dest), "-I", str(SRC)]  # dest so "generated/..." resolves

    objs = []
    for f in C_SOURCES:
        o = dest / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipt)
        objs.append(o)
    for f in ASM_SOURCES:
        o = dest / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipt)
        objs.append(o)

    combined = dest / "combined-ingame.o"
    ref = dest / "import_references.o"
    ordered = [ref] + [o for o in objs if o != ref]
    run([LD, "-r", "-m", "elf_x86_64", "-o", combined] + ordered, receipt)

    # LOUD GATE: forbidden symbols must not appear anywhere in the link.
    nm = run([NM, combined], receipt)
    seen = set()
    for line in nm.stdout.splitlines():
        toks = line.split()
        if toks:
            seen.add(toks[-1].split("@")[0])
    bad = sorted(seen & set(FORBIDDEN_SYMBOLS))
    if bad:
        sys.exit(f"LOUD BUILD FAILURE: forbidden symbol in combined object: {bad}")

    # ZERO raw `syscall` instructions (igf6 gate, unchanged).
    od = run([OBJDUMP, "-d", combined], receipt)
    syscall_count = sum(1 for ln in od.stdout.splitlines()
                        if ln.strip().endswith("syscall"))
    if syscall_count != 0:
        sys.exit(f"LOUD BUILD FAILURE: {syscall_count} raw syscall "
                 f"instruction(s) in combined object (igf7 must have 0)")

    # LOUD GATE: no expected bytes (raw or ASCII hex) in the combined object.
    assert_no_expected_bytes(combined.read_bytes(), "combined object", forbidden)

    # Import contract: the SAME 7 verbatim rows, byte-identical.
    frozen = json.loads(FROZEN_CONTRACT.read_text())
    rows = {r["ImportSymbol"]["Name"]: r for r in frozen["contract"]["imports"]}
    missing = [n for n in STRONG_VERBATIM if n not in rows]
    if missing:
        sys.exit(f"frozen contract lacks needed identities: {missing}")
    imports = []
    provenance = {}
    for n in STRONG_VERBATIM:
        imports.append(rows[n])
        provenance[n] = "verbatim:frozen-191-contract (row copied byte-identical)"
    contract = {
        "schema": 1,
        "identity_complete": True,
        "scope": ("Owned igf7 C2(b) data-probe import identities: the "
                  "SAME 7 console-proven verbatim rows the loaded-and-logged "
                  "v2/v3/igf4/igf6 lineage carried. NO mprotect of any kind — "
                  "no import, no shim, no raw syscall."),
        "counts": {"required_strong": len(imports), "required_weak": 0,
                   "identity_covered_strong": len(imports), "covered_strong": len(imports),
                   "missing_strong": 0, "conflicts": 0},
        "weak_unresolved_names": [],
        "identity_provenance": provenance,
        "dropped_identities": {
            "derived_strong": ["sceKernelGetModuleList", "sceKernelGetModuleInfo",
                               "sceKernelGetExecutableModuleHandle", "dlopen",
                               "dlclose", "sceKernelMprotect"],
            "weak": ["dlsym", "__dlopen", "__dlclose", "__dlsym"],
            "reason": "stay dropped from v2 isolation; igf7 adds NO new identity"},
        "contract": {"schema": 1, "consumer": "worker-c2-probe derived Packager",
                     "imports": imports, "unresolved_weak_names": []}}
    cpath = dest / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    written = json.loads(cpath.read_text())
    for i, n in enumerate(STRONG_VERBATIM):
        if json.dumps(written["contract"]["imports"][i], sort_keys=True) != \
           json.dumps(rows[n], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: contract row {n} diverged from frozen")

    # Package: the ALREADY-BUILT Packager.dll used IN PLACE (read-only).
    if not PACKAGER_DLL.exists():
        sys.exit(f"LOUD FAILURE: Packager.dll missing at {PACKAGER_DLL}")
    bare = dest / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, PACKAGER_DLL, combined, cpath, csha, bare], receipt)

    profile = {"title_id": "PPSA33286", "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH, "marker": f"modbridge.ingamefacts/v4 build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "replaces": "igf6-ad9bda1fc50a (same path, same ini line; igf6 "
                           "crashed on its first exec-span byte read; igf7 "
                           "reads ONLY map-gated data windows, persists the "
                           "readable span list, and embeds no expected bytes)",
               "entry_marker_first": True,
               "promotion_allowed": False, "engine_initialization": False,
               "game_launch": "user-coordinated, never automated"}
    (dest / "profile.json").write_text(json.dumps(profile, indent=1))
    bare_sdk = dest / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, dest / "profile.json", bare_sdk], receipt)
    final = dest / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipt)

    # Defense in depth: the FINAL inner ELF .text must have no 0F 05 pair.
    if text_has_syscall_pair(bare_sdk):
        sys.exit("LOUD BUILD FAILURE: 0F 05 byte pair present in inner ELF .text")

    # LOUD GATE: no expected bytes in the final inner ELF or wrapped PRX.
    assert_no_expected_bytes(bare_sdk.read_bytes(), "inner ELF", forbidden)
    assert_no_expected_bytes(final.read_bytes(), "final PRX", forbidden)

    run(["python3", HERE / "check_igf7_artifact.py", dest, EXPECTED_JSON], receipt)

    receipt_out = {
        "marker": "modbridge.ingamefacts.build",
        "schema": 1,
        "variant": "igf7-c2-data-correspondence",
        "build_id": build_id,
        "log_path": LOG_PATH,
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": sha256(FROZEN_PRX),
                           "hash_verified": sha256(FROZEN_PRX) == FROZEN_SHA,
                           "recipe": "package-commands.json"},
        "vendored_byte_identity": receipt_vendored,
        "read_mechanism": {
            "impl": "map-GATED byte-loop reads of 5 data windows (<=64 B), "
                    "B,C,D,E,A order; refusal records otherwise; readable "
                    "span list persisted as paged mapspans records; NO "
                    "mprotect, NO raw syscall, NO exec-span dereference, "
                    "NO expected bytes, NO on-console comparison",
            "justification": [
                "igf6 died on its first EXEC-span byte read (record 4 absent)",
                "C2(b) substitute (C2-ALTERNATIVE-SPEC.md) needs only "
                "file-backed DATA windows in phdr 1 / phdr 8",
                "every read gated on the SAME invocation's VMMAP scan "
                "(covered span with prot&1; untrusted map refuses all)",
                "explicit derived-exec-span intersection refusal (belt and "
                "suspenders: exec spans also fail prot&1)",
                "A (flags-0 aux segment, no saved map evidence) is read LAST"],
            "syscall_instruction_objects": syscall_count,
            "syscall_gate": "asserted == 0",
            "import_row_added": False},
        "expected_bytes_policy": {
            "in_probe": False,
            "gate": "raw + ASCII-hex absence asserted in combined object, "
                    "inner ELF and final PRX",
            "offline_values": str(EXPECTED_JSON),
            "image_sha256": IMAGE_SHA},
        "commands": receipt,
        "combined_sha256": sha256(combined),
        "contract_sha256": csha,
        "strong_imports": {n: provenance[n] for n in STRONG_VERBATIM},
        "weak_imports": [],
        "forbidden_symbol_gate": {"names": FORBIDDEN_SYMBOLS, "status": "passed"},
        "final_prx": str(final),
        "final_sha256": sha256(final),
        "final_bytes": final.stat().st_size,
        "inner_elf_sha256": sha256(bare_sdk),
        "inner_elf_bytes": bare_sdk.stat().st_size,
        "native_executed": False,
        "in_game_results": False,
        "notes": "Local packaging only. In-game facts require the user-coordinated "
                 "Townfall launch via ploader staged by the controller "
                 "(see STAGE-PLAN-IGF7.md)."}
    (dest / "build-receipt.json").write_text(json.dumps(receipt_out, indent=1))
    return receipt_out


receipt_vendored: list = []


def main() -> None:
    global receipt_vendored
    dest_arg = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None

    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    receipt_vendored = check_vendored()
    forbidden = load_forbidden_bytes()

    seed_srcs = sorted(C_SOURCES + ASM_SOURCES)
    h = hashlib.sha256()
    for f in seed_srcs:
        h.update((SRC / f).read_bytes())
    h.update(LOG_PATH.encode())
    h.update(b"igf7")
    seed = h.hexdigest()[:12]
    build_id = f"igf7-{seed}"

    if dest_arg:
        if dest_arg.exists():
            sys.exit(f"refuse to overwrite existing build dir {dest_arg}")
        r = build_once(dest_arg, build_id, forbidden, [])
        print(json.dumps({k: r[k] for k in ("build_id", "variant", "final_prx",
                                            "final_sha256", "final_bytes")}))
        return

    # Determinism double-build.
    runs = []
    for name in ("run1", "run2"):
        d = HERE / "determinism" / name
        if d.exists():
            sys.exit(f"refuse to overwrite {d}")
    for name in ("run1", "run2"):
        d = HERE / "determinism" / name
        r = build_once(d, build_id, forbidden, [])
        runs.append((d, r))
        print(f"{name}: build_id={r['build_id']} prx={r['final_sha256']}")
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
    summary = {"marker": "modbridge.ingamefacts.build",
               "variant": "igf7-c2-data-correspondence",
               "build_id": build_id, "determinism": compare,
               "deliverable": str(HERE / f"{MODULE}.prx"),
               "run1": {k: r1[k] for k in ("final_sha256", "final_bytes",
                                           "inner_elf_sha256", "inner_elf_bytes",
                                           "combined_sha256", "contract_sha256")},
               "read_mechanism": r1["read_mechanism"],
               "expected_bytes_policy": r1["expected_bytes_policy"],
               "vendored_byte_identity": receipt_vendored,
               "native_executed": False, "in_game_results": False}
    shutil.copyfile(d1 / f"{MODULE}.prx", HERE / f"{MODULE}.prx")
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps({k: summary[k] for k in ("build_id", "deliverable",
                                              "determinism", "run1")}))
    summary["commands"] = r1["commands"]
    (HERE / "build-receipt.json").write_text(json.dumps(summary, indent=1))


if __name__ == "__main__":
    main()
