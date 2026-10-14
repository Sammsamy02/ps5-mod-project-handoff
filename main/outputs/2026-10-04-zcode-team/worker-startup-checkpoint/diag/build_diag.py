#!/usr/bin/env python3
"""build_diag.py — STARTUP CHECKPOINT diagnostics (worker-startup-checkpoint,
team 2026-10-04).

Vendored from worker-v3-startup-diag/diag/build_ingame.py (frozen igd1
recipe): import_references-first `ld -r` combine, SharpProspero Packager,
frozen finalize_sdk.py SDK 0x11000000 stamp, pinned make_fself. Two frozen
variants, SAME module_start (checkpoint-only writer to /data/plugins/UE4SS.log):

  min (sdg1-)  import surface = the 7 console-proven verbatim rows carried by
               v2/v3/igd1 (oracle: igd1 determinism/run1 dynsym). Tests: is
               /data/plugins/UE4SS.log writable and does a 7-import module
               reach module_start in the composed-runtime slot.
  imp (sdg2-)  import surface = composition-4's EXACT 196-row set
               (supplier-contract.json contract.imports, verbatim rows; oracle:
               composition-4 bare-sdk dynsym). Tests: do ALL composition-4
               imports resolve on console. Same checkpoint-only module_start.

Deterministic builds; payload of record after `determinism` mode lives in
determinism/<variant>/run1/. LOCAL packaging only; no native execution here.
"""
import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WHAT = HERE.parents[3]
assert WHAT.name == "what", WHAT
SRC = HERE / "src"

SDK_BIN = WHAT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
CLANG = SDK_BIN / "prospero-clang"
LLVM = WHAT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
LD = LLVM / "ld.lld"
NM = LLVM / "llvm-nm"
DOTNET = WHAT / ".superpowers/native-dependency-spike/sharpprospero-host-tool-1/dotnet/dotnet"
FINALIZE = WHAT / ".superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = SDK_BIN.parent / "samples/install_app/make_fself.py"

FROZEN_PRX = (WHAT / ".superpowers/native-dependency-spike/"
              "native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx")
FROZEN_SHA = "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654"
FROZEN_CONTRACT = (WHAT / ".superpowers/native-dependency-spike/"
                   "native-full-runtime-aligned-entry-1/import-contract.json")
IGD1_CONTRACT = (WHAT / "outputs/2026-10-04-zcode-team/worker-v3-startup-diag/"
                 "diag/determinism/run1/import-contract.json")
IGD1_BARE = (WHAT / "outputs/2026-10-04-zcode-team/worker-v3-startup-diag/"
             "diag/determinism/run1/bare-sdk/modbridge_ingame_facts.prx")
C4_CONTRACT = (WHAT / "outputs/2026-10-04-zcode-team/worker-composition-4-fix/"
               "prx-candidate-composition4-1/supplier-contract.json")
C4_BARE = (WHAT / "outputs/2026-10-04-zcode-team/worker-composition-4-fix/"
           "prx-candidate-composition4-1/bare-sdk/modbridge_supplier_composition.prx")

MODULE = "modbridge_supplier_composition"  # same module name as the composition
LOG_PATH = "/data/plugins/UE4SS.log"       # composed runtime's expected output

C_SOURCES = ["ingame_libc.c", "module_entry.c", "sentinels.c"]
ASM_SOURCES = ["import_references.S", "boundaries.S"]  # import_references.S is GENERATED
CPP_SOURCES = []

CFLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC", "-fno-stack-protector",
          "-fvisibility=hidden", "-ffreestanding", "-fno-builtin",
          "-fno-asynchronous-unwind-tables"]

MIN_ROWS = ["__error", "sceKernelOpen", "sceKernelWrite", "sceKernelClose",
            "sysctl", "getpid", "clock_gettime"]

# min-only: identities that must NEVER appear (v2 isolation rule)
FORBIDDEN_MIN = [
    "sceKernelGetModuleList", "sceKernelGetModuleInfo",
    "sceKernelGetExecutableModuleHandle",
    "dlopen", "dlclose", "dlsym", "__dlopen", "__dlclose", "__dlsym",
    "_DYNAMIC", "__DYNAMIC",
]

VARIANTS = {
    "min": {"prefix": "sdg1", "title": "7-import checkpoint (path probe)"},
    "imp": {"prefix": "sdg2", "title": "composition-4 196-import checkpoint (resolution probe)"},
}


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


def dynsym_und(path: Path):
    """(name, bind, type) of every SHN_UNDEF named dynsym entry."""
    d = path.read_bytes()
    e_shoff, = struct.unpack_from("<Q", d, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", d, 0x3A)
    secs = []
    for i in range(e_shnum):
        v = struct.unpack_from("<IIQQQQIIQQ", d, e_shoff + i * e_shentsize)
        secs.append(dict(zip(("name", "typ", "flags", "addr", "offset", "size",
                              "link", "info", "align", "entsize"), v)))
    sh = secs[e_shstrndx]
    def sn(n):
        s = sh["offset"] + n
        return d[s:d.index(b"\0", s)].decode()
    for s in secs:
        if sn(s["name"]) in (".dynsym", ".symtab"):
            st = secs[s["link"]]
            out = []
            for i in range(s["size"] // 24):
                nm, info, _o, shndx, _v, _sz = struct.unpack_from(
                    "<IBBHQQ", d, s["offset"] + i * 24)
                if shndx == 0 and nm:
                    p = st["offset"] + nm
                    out.append((d[p:d.index(b"\0", p)].decode(),
                                str(info >> 4), str(info & 0xF)))
            return out
    raise AssertionError(".dynsym not found")


BIND = {1: "GLOBAL", 2: "WEAK"}
TYPE = {0: "NOTYPE", 1: "OBJECT", 2: "FUNC"}


def dynsym_und_named(path: Path):
    return [(n, BIND.get(int(b), b), TYPE.get(int(t), t)) for (n, b, t) in dynsym_und(path)]


def gen_import_refs(rows):
    """rows: list of (name, symbol_type). Emits the lineage-format import table."""
    lines = [
        "// import_references.S — GENERATED by build_diag.py",
        "// (worker-startup-checkpoint, team 2026-10-04). Owned import",
        "// references; every row is pinned in import-contract.json built from",
        "// the same list. sdg1: 7 verbatim console-proven rows.",
        "// sdg2: composition-4's exact 196-row surface (verbatim contract rows).",
        '.section .data.rel.ro.diag_imports,"aw",@progbits',
        ".p2align 3",
        ".globl diag_import_table",
        ".hidden diag_import_table",
        ".type diag_import_table,@object",
        "diag_import_table:",
    ]
    for name, sym_type in rows:
        gas_type = {"FUNC": "function", "OBJECT": "object"}.get(sym_type, "function")
        lines += [f".globl {name}", f".type {name},@{gas_type}", f".quad {name}", ""]
    lines += [
        ".globl diag_import_table_end",
        ".hidden diag_import_table_end",
        "diag_import_table_end:",
        ".size diag_import_table, diag_import_table_end - diag_import_table",
        "",
    ]
    return "\n".join(lines)


def variant_inputs(variant: str):
    """(generated_rows, contract_dict, oracle_und_named, provenance)"""
    if variant == "min":
        frozen = json.loads(FROZEN_CONTRACT.read_text())
        rows_by_name = {r["ImportSymbol"]["Name"]: r
                        for r in frozen["contract"]["imports"]}
        missing = [n for n in MIN_ROWS if n not in rows_by_name]
        if missing:
            sys.exit(f"frozen contract lacks needed identities: {missing}")
        imports = [rows_by_name[n] for n in MIN_ROWS]
        # row-level equality against the console-proven igd1 contract
        igd1 = json.loads(IGD1_CONTRACT.read_text())
        igd1_rows = {r["ImportSymbol"]["Name"]: r for r in igd1["contract"]["imports"]}
        for n in MIN_ROWS:
            if json.dumps(rows_by_name[n], sort_keys=True) != \
               json.dumps(igd1_rows[n], sort_keys=True):
                sys.exit(f"LOUD BUILD FAILURE: row {n} diverged from igd1 contract")
        gen_rows = [(n, "FUNC") for n in MIN_ROWS]
        contract = {
            "schema": 1,
            "identity_complete": True,
            "scope": ("Owned MINIMAL STARTUP CHECKPOINT import identities: the "
                      "SAME 7 console-proven verbatim rows carried by v2/v3/"
                      "igd1. module_start only writes one DIAG line to "
                      "/data/plugins/UE4SS.log and returns 0."),
            "counts": {"required_strong": len(imports), "required_weak": 0,
                       "identity_covered_strong": len(imports),
                       "covered_strong": len(imports), "missing_strong": 0,
                       "conflicts": 0},
            "weak_unresolved_names": [],
            "identity_provenance": {n: "verbatim:frozen-191-contract "
                                       "(row copied byte-identical; equality "
                                       "verified vs igd1 run1 contract)"
                                    for n in MIN_ROWS},
            "contract": {"schema": 1,
                         "consumer": "worker-startup-checkpoint derived Packager",
                         "imports": imports, "unresolved_weak_names": []}}
        oracle = dynsym_und_named(IGD1_BARE)
        prov = {"rows_source": str(FROZEN_CONTRACT),
                "row_equality_checked": str(IGD1_CONTRACT),
                "oracle": str(IGD1_BARE)}
        return gen_rows, contract, oracle, prov
    elif variant == "imp":
        c4 = json.loads(C4_CONTRACT.read_text())
        imports = c4["contract"]["imports"]  # 196 verbatim rows, order preserved
        if len(imports) != 196:
            sys.exit(f"expected 196 composition-4 rows, got {len(imports)}")
        gen_rows = [(r["ImportSymbol"]["Name"], r.get("symbol_type", "FUNC"))
                    for r in imports]
        contract = {
            "schema": 1,
            "identity_complete": True,
            "scope": ("Owned IMPORT-SURFACE CHECKPOINT: composition-4's exact "
                      "196-row import surface, every row copied verbatim from "
                      "the composition-4 supplier contract. module_start only "
                      "writes one DIAG line to /data/plugins/UE4SS.log and "
                      "returns 0. Purpose: prove the whole surface resolves."),
            "counts": {"required_strong": len(imports), "required_weak": 0,
                       "identity_covered_strong": len(imports),
                       "covered_strong": len(imports), "missing_strong": 0,
                       "conflicts": 0},
            "weak_unresolved_names": [],
            "identity_provenance": {r["ImportSymbol"]["Name"]:
                                    "verbatim:composition-4 supplier-contract "
                                    "contract.imports row" for r in imports},
            "contract": {"schema": 1,
                         "consumer": "worker-startup-checkpoint derived Packager",
                         "imports": imports, "unresolved_weak_names": []}}
        oracle = dynsym_und_named(C4_BARE)
        prov = {"rows_source": str(C4_CONTRACT),
                "row_equality_checked": "(source of the rows themselves)",
                "oracle": str(C4_BARE)}
        return gen_rows, contract, oracle, prov
    sys.exit(f"unknown variant {variant}")


def build_once(variant: str, tag: str) -> Path:
    receipts = []
    frozen_actual = sha256(FROZEN_PRX)
    if frozen_actual != FROZEN_SHA:
        sys.exit(f"frozen lineage hash mismatch: {frozen_actual}")

    gen_rows, contract, oracle_und, prov = variant_inputs(variant)
    prefix = VARIANTS[variant]["prefix"]

    seed = hashlib.sha256(b"".join(
        (SRC / f).read_bytes() for f in sorted(C_SOURCES + CPP_SOURCES)
        if (SRC / f).exists()
    ) + json.dumps([list(r) for r in gen_rows]).encode()
      + LOG_PATH.encode() + variant.encode() + tag.encode()).hexdigest()[:12]
    out = HERE / f"build-{variant}-{seed}"
    if out.exists():
        sys.exit(f"refuse to overwrite existing build dir {out}")
    (out / "bare-original").mkdir(parents=True)
    (out / "bare-sdk").mkdir(parents=True)
    gen = SRC / "generated"
    gen.mkdir(parents=True, exist_ok=True)

    build_id = f"{prefix}-{seed}"
    (gen / "diag_config.h").write_text(
        "// GENERATED by build_diag.py (worker-startup-checkpoint) — do not edit.\n"
        "#pragma once\n"
        f"#define DIAG_BUILD_ID \"{build_id}\"\n"
        f"#define DIAG_LOG_PATH \"{LOG_PATH}\"\n"
        f"#define DIAG_VARIANT \"{variant}\"\n")
    (SRC / "import_references.S").write_text(gen_import_refs(gen_rows))

    inc = ["-I", str(gen), "-I", str(SRC)]

    objs = []
    for f in C_SOURCES:
        o = out / (f.replace(".c", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)
    for f in ASM_SOURCES:
        o = out / (f.replace(".S", ".o"))
        run([CLANG] + CFLAGS + inc + ["-c", SRC / f, "-o", o], receipts)
        objs.append(o)

    combined = out / "combined-diag.o"
    ref = out / "import_references.o"
    ordered = [ref] + [o for o in objs if o != ref]
    run([LD, "-r", "-m", "elf_x86_64", "-o", combined] + ordered, receipts)

    nm = run([NM, combined], receipts)
    seen = set()
    for line in nm.stdout.splitlines():
        toks = line.split()
        if toks:
            seen.add(toks[-1].split("@")[0])
    if variant == "min":
        bad = sorted(seen & set(FORBIDDEN_MIN))
        if bad:
            sys.exit(f"LOUD BUILD FAILURE: forbidden symbol present: {bad}")

    # UND set of the combined object must be exactly the generated rows.
    obj_und = sorted(n for (n, b, t) in dynsym_und_named(combined)
                     if n not in ("__init_array_start", "__init_array_end",
                                  "__fini_array_start", "__fini_array_end"))
    want = sorted(n for (n, _t) in gen_rows)
    if obj_und != want:
        sys.exit(f"LOUD BUILD FAILURE: combined UND != contract rows\n"
                 f"extra={sorted(set(obj_und) - set(want))} "
                 f"missing={sorted(set(want) - set(obj_und))}")

    cpath = out / "import-contract.json"
    cpath.write_text(json.dumps(contract, indent=1))
    csha = sha256(cpath)
    written = json.loads(cpath.read_text())
    for i, (n, _t) in enumerate(gen_rows):
        if json.dumps(written["contract"]["imports"][i], sort_keys=True) != \
           json.dumps(contract["contract"]["imports"][i], sort_keys=True):
            sys.exit(f"LOUD BUILD FAILURE: contract row {n} diverged on write")

    harness_dll = HERE / "harness" / "bin" / "Release" / "net10.0" / "Packager.dll"
    build_note = "dotnet build (frozen recipe)"
    rb = run([DOTNET, "build", HERE / "harness" / "Packager.csproj", "-c", "Release",
              "--ignore-failed-sources"], receipts, cwd=HERE, allow_fail=True)
    if rb.returncode != 0:
        if not harness_dll.exists():
            sys.exit("dotnet build failed and no prebuilt Packager.dll present")
        build_note = ("dotnet build failed offline; using vendored prebuilt "
                      "Packager.dll (worker-v3-startup-diag artifact)")

    bare = out / "bare-original" / f"{MODULE}.prx"
    run([DOTNET, harness_dll, combined, cpath, csha, bare], receipts)

    profile = {"title_id": "PPSA33286",
               "module_path": f"/data/plugins/{MODULE}.prx",
               "log_path": LOG_PATH,
               "marker": f"modbridge.startupcheckpoint/{variant} build={build_id}",
               "build_id": build_id, "sdk_version": "0x11000000",
               "variant": variant,
               "variant_title": VARIANTS[variant]["title"],
               "minimal_variant": variant == "min",
               "replaces": ("composition-4 de372302 (ploader OK, zero runtime "
                            "output; startup localization diagnostic)"),
               "promotion_allowed": False, "engine_initialization": False,
               "provenance": prov,
               "game_launch": "user-coordinated, never automated"}
    (out / "profile.json").write_text(json.dumps(profile, indent=1))

    bare_sdk = out / "bare-sdk" / f"{MODULE}.prx"
    run(["python3", FINALIZE, bare, out / "profile.json", bare_sdk], receipts)

    final = out / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, final], receipts)

    # oracle: final dynsym UND surface must equal the variant oracle exactly
    got = sorted(dynsym_und_named(bare_sdk))
    want = sorted(oracle_und)
    if got != want:
        g, w = set(got), set(want)
        sys.exit(f"ORACLE FAILURE ({variant}): final dynsym != target\n"
                 f"extra={sorted(g - w)}\nmissing={sorted(w - g)}\n"
                 f"got={len(got)} want={len(want)}")

    (out / "expected-imports.json").write_text(json.dumps(
        {"variant": variant, "module": MODULE, "oracle": prov["oracle"],
         "und": [{"name": n, "bind": b, "type": t} for (n, b, t) in oracle_und]},
        indent=1))

    run(["python3", HERE / "check_diag_artifact.py", out], receipts)

    receipt = {
        "marker": "modbridge.startupcheckpoint.build",
        "schema": 1,
        "variant": variant,
        "build_id": build_id,
        "log_path": LOG_PATH,
        "module": MODULE,
        "module_path": f"/data/plugins/{MODULE}.prx",
        "frozen_lineage": {"prx": str(FROZEN_PRX), "sha256": frozen_actual,
                           "hash_verified": True},
        "commands": receipts,
        "combined_sha256": sha256(combined),
        "contract_sha256": csha,
        "import_rows": len(gen_rows),
        "oracle_dynsym_und": len(oracle_und),
        "oracle_status": "final dynsym UND set == target set (exact)",
        "provenance": prov,
        "packager": build_note,
        "final_prx": str(final),
        "final_sha256": sha256(final),
        "final_bytes": final.stat().st_size,
        "native_executed": False,
        "in_game_results": False,
        "notes": "STARTUP CHECKPOINT: open /data/plugins/UE4SS.log + one DIAG "
                 "line + close + return 0. Local packaging only; staging per "
                 "STAGE-PLAN.md."}
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=1))
    print(json.dumps({k: receipt[k] for k in
                      ("variant", "build_id", "final_prx", "final_sha256",
                       "final_bytes", "import_rows", "oracle_dynsym_und")}))
    return out


def determinism(variant: str) -> bool:
    r1 = build_once(variant, "det")
    moved = HERE / "determinism" / variant / "run1"
    moved.parent.mkdir(parents=True, exist_ok=True)
    r1.rename(moved)
    r2 = build_once(variant, "det")
    moved2 = HERE / "determinism" / variant / "run2"
    r2.rename(moved2)
    ok = True
    for rel in (f"{MODULE}.prx", f"bare-original/{MODULE}.prx",
                f"bare-sdk/{MODULE}.prx", "combined-diag.o",
                "import-contract.json", "expected-imports.json"):
        a, b = (moved / rel).read_bytes(), (moved2 / rel).read_bytes()
        same = hashlib.sha256(a).hexdigest() == hashlib.sha256(b).hexdigest()
        ok &= same
        print(f"{'IDENTICAL' if same else 'DIVERGED'}: {variant}/{rel} "
              f"{hashlib.sha256(a).hexdigest()[:16]}")
    print(f"DETERMINISM-{'PASSED' if ok else 'FAILED'}: {variant}")
    return ok


def main() -> None:
    mode = sys.argv[1] if len(sys.argv) > 1 else "build"
    what = sys.argv[2] if len(sys.argv) > 2 else "min"
    if mode == "build":
        build_once(what, what)
    elif mode == "determinism":
        ok1 = determinism("min")
        ok2 = determinism("imp")
        sys.exit(0 if (ok1 and ok2) else 1)
    else:
        sys.exit(f"unknown mode {mode}")


if __name__ == "__main__":
    main()
