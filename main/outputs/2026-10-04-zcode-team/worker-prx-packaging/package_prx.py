#!/usr/bin/env python3
"""Controller-runnable PRX packaging build for the NEW composed UE runtime.

Takes the composed runtime object (checkpoint zcode_supplier_composition_1,
NEVER rebuilt here, hash-verified against the checkpoint ledger) plus the
retained bootstrap packaging inputs from the frozen proven PRX lineage
(native-full-runtime-aligned-entry-1) and produces a NEW PRX candidate in a
fresh output directory following the proven recipe exactly:

  1. verify lineage hashes (composed object, base receipt, frozen PRX,
     pinned import contract, checkpoint ledger entries) -- any mismatch
     stops the build and writes a gap list instead of linking;
  2. compile the 10 retained bootstrap units with the pinned prospero
     toolchain and the frozen flag set (aligned entry: module_entry.c carries
     __attribute__((force_align_arg_pointer)) on module_start/module_stop);
  3. add typed import-reference shims (.S) for the extended strong imports
     (the composed object references them UNTYPED; the proven Packager
     requires one typed reference per contract import; untyped+typed
     coexistence is explicitly allowed by the pinned Packager);
  4. partial-link (ld.lld -r -m elf_x86_64) bootstrap objects + composed
     runtime object, bootstrap first exactly like the frozen build;
  5. pre-check the combined object's unresolved strong set: it must equal
     the frozen 191 contract names plus exactly the five measured extensions
     (sceKernelDlsym, sceKernelGetModuleInfo, sceKernelLoadStartModule,
     sceKernelStopUnloadModule from libkernel; qsort from libSceLibcInternal)
     with _DYNAMIC linker-provided; anything else is a recorded gap and the
     build stops BEFORE the Packager runs;
  6. derive the extended import contract (196 strong = 191 pinned + 5
     provider-cloned rows, provenance chain preserved) and a patched copy of
     the pinned Packager harness (only the pinned-value adjustments forced by
     the documented delta; every patch recorded in the receipt);
  7. Packager -> bare-original PRX (explicit DT_INIT/DT_FINI module_start/
     module_stop, loader owns no init arrays);
  8. finalize_sdk.py -> SDK stamp 0x2000009 -> 0x11000000 (console-proven);
  9. make_fself.py -> wrapped candidate;
 10. static verification: ELF module type/flags, PT_SCE_MODULE uniqueness,
     SDK stamp value, explicit init/fini tags, init/fini slot counts,
     entry-alignment byte evidence at module_start, import table vs the
     composed object's undefined set (all imports must be SDK-library
     providers or linker-provided; _DYNAMIC must NOT remain an import).

The frozen lineage directory and the composed run directory are read-only.
History is never overwritten: every run requires a fresh output directory.

Usage:
  python3 package_prx.py --verify-only          # lineage + evidence only
  python3 package_prx.py --link                 # full packaging pipeline
  python3 package_prx.py --link --out DIR       # fresh dir (must not exist)
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent
ROOT = BASE.parents[2]
SPIKE = ROOT / ".superpowers/native-dependency-spike"

# ---------------------------------------------------------------- pinned lineage
PIN = {
    "ledger": ROOT / ".superpowers/native-observations/transport-experiments-progress.json",
    "composition_dir": SPIKE / "ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-1/run-1",
    "composition_object_name": "coherent-tick-file-witness-runtime.o",
    "composition_object_sha256": "fb2f4dd16a20997455b39e6857fd2bdf45842c691bd0124a448948ae5bcb775c",
    "composition_object_bytes": 117624920,
    "composition_checkpoint": "zcode_supplier_composition_1",
    "base_receipt": SPIKE / "ue4ss-native-notify-cancel-1/build-1/receipt.json",
    "base_receipt_sha256": "bcd23a2a4abef518d8f220786e80cb131036e351b2218a322d26aee6adca8384",
    "lineage": SPIKE / "native-full-runtime-aligned-entry-1",
    "frozen_prx": SPIKE / "native-full-runtime-aligned-entry-1/build-1/modbridge_runtime_aligned_entry.prx",
    "frozen_prx_sha256": "3b998f098de7e1adba29c129c51586554590b3eadc257749e15223983c4ab654",
    "frozen_prx_bytes": 30151623,
    "frozen_prx_checkpoint": "native_aligned_entry",
    "import_contract": SPIKE / "native-full-runtime-aligned-entry-1/import-contract.json",
    "frozen_init_slots": 232,
    "frozen_fini_slots": 2,
    "frozen_imports_strong": 191,
    "frozen_imports_funcs": 187,
    "frozen_imports_objects": 4,
    "frozen_allowed_weak": ["__dladdr", "__dlclose", "__dlerror", "__dlopen",
                            "__dlsym", "kernel_mprotect"],
    "frozen_boundary": ["__init_array_start", "__init_array_end",
                        "__fini_array_start", "__fini_array_end", "__dso_handle",
                        "__eh_frame_start", "__eh_frame_end",
                        "__eh_frame_hdr_start", "__eh_frame_hdr_end"],
    "finalize_sdk": SPIKE / "native-runtime-import-diagnostics-1/finalize_sdk.py",
    "make_fself": ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/install_app/make_fself.py",
    "dotnet": SPIKE / "sharpprospero-host-tool-1/dotnet/dotnet",
    "llvm": ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin",
    "sdk": ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk",
    "lua_include": SPIKE / "ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include",
}

# Retained bootstrap packaging units, exact order and flags from the frozen
# build_local.py. module_entry.c carries force_align_arg_pointer on
# module_start/module_stop (aligned-entry recipe).
BOOTSTRAP_UNITS = [
    "import_references.S",
    "probe/probe.c",
    "probe/startup_input.c",
    "probe/memory_lua.c",
    "lifecycle/lifecycle.c",
    "lifecycle/module_entry.c",
    "lifecycle/constructor_boundary.cpp",
    "lifecycle/sentinels.c",
    "import_check.c",
    "diagnostic.c",
]
CLANG_FLAGS = ["-Wall", "-Wextra", "-Werror", "-O1", "-fPIC",
               "-fno-stack-protector", "-fvisibility=hidden", "-ffreestanding",
               "-fno-builtin"]

# The measured strong-import extension of the composed runtime: untyped GLOBAL
# UND symbols of the composed object (readelf), absent from the frozen
# 191-import contract because the older stage27 branch satisfied them
# internally. Measured by the combined-object pre-check (first full link run
# surfaced exactly qsort beyond the four sce* names). name -> identity donor.
EXTENDED_IMPORTS = {
    "sceKernelDlsym": "sceKernelOpen",
    "sceKernelGetModuleInfo": "sceKernelOpen",
    "sceKernelLoadStartModule": "sceKernelOpen",
    "sceKernelStopUnloadModule": "sceKernelOpen",
    "qsort": "malloc",
}
# Provider evidence files of the composed run (nm-verified SDK stubs).
EXTENDED_IMPORT_EVIDENCE = {
    "sceKernelDlsym": "sdknm-libkernel.so.stdout",
    "sceKernelGetModuleInfo": "sdknm-libkernel.so.stdout",
    "sceKernelLoadStartModule": "sdknm-libkernel.so.stdout",
    "sceKernelStopUnloadModule": "sdknm-libkernel.so.stdout",
    "qsort": "sdknm-libSceLibcInternal.so.stdout",
}
# Linker-provided names the Packager must accept as boundary symbols (the
# composed object references _DYNAMIC untyped; the dynamic writer provides it).
BOUNDARY_ADDITIONS = ["_DYNAMIC"]

CANDIDATE_BASENAME = "modbridge_supplier_composition.prx"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class Build:
    def __init__(self, out: Path, link: bool):
        self.out = out
        self.link = link
        self.gaps: list[str] = []
        self.records: list[dict] = []
        self.log: list[str] = []

    def note(self, msg: str) -> None:
        stamp = _dt.datetime.now().isoformat(timespec="seconds")
        self.log.append(f"[{stamp}] {msg}")
        print(msg, flush=True)

    def gap(self, msg: str) -> None:
        self.gaps.append(msg)
        self.note("GAP: " + msg)

    def run(self, argv: list, *, env_extra: dict | None = None,
            allow_fail: bool = False):
        env = dict(os.environ)
        if env_extra:
            env.update(env_extra)
        self.note("run: " + " ".join(str(a) for a in argv))
        result = subprocess.run([str(a) for a in argv], env=env,
                                capture_output=True, text=True)
        rec = {"argv": [str(a) for a in argv], "rc": result.returncode,
               "stdout": result.stdout[-20000:], "stderr": result.stderr[-20000:]}
        self.records.append(rec)
        if result.returncode != 0 and not allow_fail:
            raise RuntimeError(f"command failed rc={result.returncode}: "
                               f"{result.stderr[-2000:]}")
        return result

    def write_json(self, rel: str, payload) -> Path:
        target = self.out / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(payload, indent=2) + "\n")
        return target


def readelf_undefined(reader: Path, obj: Path):
    """Return (strong_und, weak_und, defined) name sets of a relocatable object."""
    out = b.run if False else None  # placeholder to keep linters quiet
    res = subprocess.run([str(reader), "-s", str(obj)],
                         capture_output=True, text=True)
    if res.returncode != 0:
        raise RuntimeError("llvm-readelf failed: " + res.stderr[-500:])
    strong, weak, defined = set(), set(), set()
    for line in res.stdout.splitlines():
        m = re.match(r"\s*\d+:\s+[0-9a-fA-F]+\s+\d+\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)$",
                     line)
        if not m:
            continue
        _typ, bind, _vis, ndx, name = m.groups()
        if not name or name.startswith("$"):
            continue
        if ndx == "UND":
            (weak if bind == "WEAK" else strong).add(name)
        else:
            defined.add(name)
    return strong, weak, defined


# ---------------------------------------------------------------- stage 1: verify
def verify_lineage(b: Build) -> dict:
    ev: dict = {"stage": "lineage-verification", "checks": {}}
    ok = True

    def check(name: str, passed: bool, detail) -> None:
        nonlocal ok
        ev["checks"][name] = {"pass": bool(passed), "detail": detail}
        if not passed:
            ok = False
            b.gap(f"lineage check failed: {name}")

    obj = PIN["composition_dir"] / PIN["composition_object_name"]
    check("composition_object_exists", obj.is_file(), str(obj))
    if obj.is_file():
        digest = sha256_file(obj)
        check("composition_object_sha256_matches_checkpoint",
              digest == PIN["composition_object_sha256"],
              {"computed": digest, "expected": PIN["composition_object_sha256"]})
        check("composition_object_bytes",
              obj.stat().st_size == PIN["composition_object_bytes"],
              obj.stat().st_size)
    receipt_path = PIN["composition_dir"] / "receipt.json"
    if receipt_path.is_file():
        try:
            receipt = json.loads(receipt_path.read_text())
            check("composition_receipt_runtime_sha256",
                  receipt.get("runtime_sha256") == PIN["composition_object_sha256"]
                  and receipt.get("status") == "NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED",
                  receipt.get("status"))
            check("composition_receipt_base_matches_pinned",
                  Path(receipt.get("base_receipt", "")) == PIN["base_receipt"],
                  receipt.get("base_receipt"))
        except Exception as exc:  # noqa: BLE001
            check("composition_receipt_parse", False, str(exc))
    else:
        check("composition_receipt_exists", False, str(receipt_path))
    if PIN["base_receipt"].is_file():
        digest = sha256_file(PIN["base_receipt"])
        check("base_receipt_sha256", digest == PIN["base_receipt_sha256"], digest)

    try:
        ledger = json.loads(PIN["ledger"].read_text())
        composed_ck = ledger.get(PIN["composition_checkpoint"])
        check("ledger_checkpoint_present", composed_ck is not None,
              PIN["composition_checkpoint"])
        if composed_ck:
            check("ledger_composed_runtime_sha256",
                  composed_ck.get("runtime_sha256") == PIN["composition_object_sha256"],
                  composed_ck.get("runtime_sha256"))
        frozen_ck = ledger.get(PIN["frozen_prx_checkpoint"])
        check("ledger_frozen_checkpoint_present", frozen_ck is not None,
              PIN["frozen_prx_checkpoint"])
        if frozen_ck:
            check("ledger_frozen_prx_sha256",
                  frozen_ck.get("sha256") == PIN["frozen_prx_sha256"],
                  frozen_ck.get("sha256"))
    except Exception as exc:  # noqa: BLE001
        check("ledger_parse", False, str(exc))

    if PIN["frozen_prx"].is_file():
        digest = sha256_file(PIN["frozen_prx"])
        check("frozen_prx_sha256_unchanged", digest == PIN["frozen_prx_sha256"], digest)
        check("frozen_prx_bytes",
              PIN["frozen_prx"].stat().st_size == PIN["frozen_prx_bytes"],
              PIN["frozen_prx"].stat().st_size)
    else:
        check("frozen_prx_exists", False, str(PIN["frozen_prx"]))

    contract = None
    if PIN["import_contract"].is_file():
        ev["import_contract_sha256"] = sha256_file(PIN["import_contract"])
        contract = json.loads(PIN["import_contract"].read_text())
        counts = contract.get("counts", {})
        check("import_contract_counts",
              counts.get("required_strong") == PIN["frozen_imports_strong"]
              and counts.get("required_weak") == 6
              and counts.get("missing_strong") == 0
              and counts.get("conflicts") == 0, counts)
        names = [r["ImportSymbol"]["Name"] for r in contract["contract"]["imports"]]
        donors = sorted(set(EXTENDED_IMPORTS.values()))
        check("identity_donor_rows_present",
              all(names.count(d) == 1 for d in donors), donors)
        check("extended_imports_absent_from_frozen_contract",
              all(n not in names for n in EXTENDED_IMPORTS),
              [n for n in EXTENDED_IMPORTS if n in names])
        ev["frozen_import_provider_families"] = sorted(
            {r["ImportSymbol"]["Soname"] for r in contract["contract"]["imports"]})
    else:
        check("import_contract_exists", False, str(PIN["import_contract"]))

    delta_path = PIN["composition_dir"] / "undefined-delta.json"
    if delta_path.is_file():
        delta = json.loads(delta_path.read_text())
        check("undefined_delta_added_matches_checkpoint_claim",
              sorted(delta.get("added", [])) == sorted(["sceKernelGetModuleInfo", "_DYNAMIC"]),
              {"added": delta.get("added"),
               "note": "checkpoint zcode_supplier_composition_1 documents exactly "
                       "these two additions vs the notify-cancel composed baseline; "
                       "dlsym/LoadStart/StopUnload predate that baseline"})
        check("undefined_delta_removed_empty", delta.get("removed") == [],
              delta.get("removed"))
        ev["composed_undefined_baseline"] = {
            "current_count": delta.get("current_count"),
            "added": delta.get("added"), "removed": delta.get("removed")}
    else:
        check("undefined_delta_exists", False, str(delta_path))

    ev_files = {n: PIN["composition_dir"] / f
                for n, f in EXTENDED_IMPORT_EVIDENCE.items()}
    if all(p.is_file() for p in ev_files.values()):
        per_symbol = {}
        for n, p in ev_files.items():
            text = p.read_text(errors="replace")
            per_symbol[n] = {"file": p.name, "sha256": sha256_file(p),
                             "matches": text.count(n)}
        check("sdk_nm_evidence_for_all_extended_imports",
              all(v["matches"] > 0 for v in per_symbol.values()), per_symbol)
        ev["sdk_nm_evidence"] = {n: v["sha256"] for n, v in per_symbol.items()}
    else:
        check("sdk_nm_evidence_exists",
              False, [str(p) for p in ev_files.values() if not p.is_file()])

    for key in ("dotnet", "finalize_sdk", "make_fself"):
        check(f"tool_{key}", PIN[key].is_file(), str(PIN[key]))
    for tool in ("ld.lld", "llvm-readelf", "llvm-nm", "llvm-config"):
        check(f"tool_{tool}", (PIN["llvm"] / tool).is_file(), str(PIN["llvm"] / tool))
    check("tool_prospero_clang",
          (PIN["sdk"] / "bin/prospero-clang").is_file(),
          str(PIN["sdk"] / "bin/prospero-clang"))
    check("tool_lua_include", PIN["lua_include"].is_dir(), str(PIN["lua_include"]))

    reader = PIN["llvm"] / "llvm-readelf"
    if obj.is_file() and reader.is_file():
        hdr = b.run([reader, "-h", obj], allow_fail=True)
        if hdr.returncode == 0:
            is_rel = re.search(r"Type:\s+REL\b", hdr.stdout) is not None
            ev["composed_elf_header"] = {
                "type_rel": is_rel,
                "machine_x86_64": "X86-64" in hdr.stdout,
                "class_64_le": "ELF64" in hdr.stdout and "little endian" in hdr.stdout}
            check("composed_object_is_relocatable_x86_64",
                  all(ev["composed_elf_header"].values()), ev["composed_elf_header"])
        try:
            strong, weak, defined = readelf_undefined(reader, obj)
            ev["composed_undefined_measured"] = {
                "strong_count": len(strong), "weak_count": len(weak),
                "extended_imports_status": {
                    n: {"undefined_strong": n in strong, "defined": n in defined}
                    for n in EXTENDED_IMPORTS},
                "dynamic_undefined": "_DYNAMIC" in strong,
            }
            check("extended_imports_undefined_in_composed_object",
                  all(n in strong for n in EXTENDED_IMPORTS),
                  ev["composed_undefined_measured"]["extended_imports_status"])
            check("linker_provided_dynamic_undefined", "_DYNAMIC" in strong, None)
        except Exception as exc:  # noqa: BLE001
            check("composed_undefined_readelf", False, str(exc))
        units = {}
        for name in BOOTSTRAP_UNITS:
            src = PIN["lineage"] / name
            units[name] = {"sha256": sha256_file(src)} if src.is_file() else {"missing": True}
        ev["bootstrap_unit_hashes"] = units
        check("bootstrap_units_all_present",
              all("sha256" in v for v in units.values()), len(units))

    ev["status"] = "LINEAGE_VERIFIED" if ok else "LINEAGE_GAPS"
    return ev


# ------------------------------------------------- stage 2: derived build inputs
def derive_contract(b: Build, out_contract: Path) -> dict:
    """Extend the pinned 191-import contract by exactly the five measured
    provider-cloned FUNC rows (libkernel x4, libSceLibcInternal x1)."""
    contract = json.loads(PIN["import_contract"].read_text())
    parent_sha = sha256_file(PIN["import_contract"])
    rows = contract["contract"]["imports"]
    by_name = {r["ImportSymbol"]["Name"]: r for r in rows}
    for name, donor_name in EXTENDED_IMPORTS.items():
        donor = by_name[donor_name]
        nm_ev = PIN["composition_dir"] / EXTENDED_IMPORT_EVIDENCE[name]
        new_row = json.loads(json.dumps(donor))
        new_row["ImportSymbol"]["Name"] = name
        new_row["ImportSymbol"]["PublishedName"] = name
        new_row["symbol_type"] = "FUNC"
        new_row["binding"] = "GLOBAL"
        new_row["evidence"] = [{
            "source": str(nm_ev),
            "sha256": sha256_file(nm_ev),
            "locator": f"nm match count "
                       f"{nm_ev.read_text(errors='replace').count(name)}",
            "qualification": ("SDK stub nm declaration captured by the composed run; "
                              "measured as an untyped GLOBAL UND symbol of the "
                              "composed object; identity cloned from the pinned "
                              f"{donor_name} row ({donor['ImportSymbol']['Soname']}); "
                              "provider availability on target not observed."),
        }]
        rows.append(new_row)
        contract["diagnostics"].append({
            "name": name, "binding": "GLOBAL", "symbol_type": "FUNC",
            "status": "supplier_extension_added",
            "reference_type_requires_reconciliation": False,
            "declared_symbol_types": ["FUNC"],
            "evidence": new_row["evidence"]})
        contract["actual_resolved_requirements"].append(
            {"name": name, "binding": "GLOBAL", "symbol_type": "NOTYPE"})
        contract["compiled_requirements"].append(
            {"name": name, "binding": "GLOBAL", "symbol_type": "FUNC"})
    added = len(EXTENDED_IMPORTS)
    for key in ("required_strong", "identity_covered_strong", "covered_strong"):
        contract["counts"][key] = PIN["frozen_imports_strong"] + added
    contract["supplier_extension"] = {
        "purpose": ("Strong-import extension required by the composed supplier "
                    "runtime; all original 191 declarations unchanged."),
        "parent_contract_sha256": parent_sha,
        "added_strong": sorted(EXTENDED_IMPORTS),
        "identity_cloned_from": dict(EXTENDED_IMPORTS),
        "evidence_files": EXTENDED_IMPORT_EVIDENCE,
        "composed_runtime_sha256": PIN["composition_object_sha256"],
        "composed_checkpoint": PIN["composition_checkpoint"],
        "derived_by": "worker-prx-packaging/package_prx.py",
    }
    out_contract.write_text(json.dumps(contract, indent=2) + "\n")
    b.note(f"derived extended contract -> {out_contract}")
    return contract


def patch_packager(b: Build, harness_dir: Path, new_contract_sha: str,
                   old_contract_sha: str) -> list[dict]:
    """Copy the pinned Packager harness and apply only the pinned-value
    adjustments forced by the documented import delta."""
    src_cs = PIN["lineage"] / "harness/Program.cs"
    text = src_cs.read_text()
    patches: list[dict] = []
    added = len(EXTENDED_IMPORTS)

    def sub(old: str, new: str, count: int, why: str) -> None:
        nonlocal text
        found = text.count(old)
        if found != count:
            raise RuntimeError(f"patch precondition failed ({found} != {count}): {old[:60]}")
        text = text.replace(old, new)
        patches.append({"old": old, "new": new, "occurrences": count, "reason": why})

    sub(old_contract_sha, new_contract_sha, 1,
        "pin the derived 195-import contract instead of the frozen 191-import one")
    sub('GetProperty("required_strong").GetInt32() != 191',
        f'GetProperty("required_strong").GetInt32() != {191 + added}', 1,
        "strong import count 191 -> " + str(191 + added))
    sub('GetProperty("identity_covered_strong").GetInt32() != 191',
        f'GetProperty("identity_covered_strong").GetInt32() != {191 + added}', 1,
        "covered-strong count follows the extension")
    sub('GetProperty("covered_strong").GetInt32() != 191',
        f'GetProperty("covered_strong").GetInt32() != {191 + added}', 1,
        "covered-strong count follows the extension")
    sub("names.Count != 191", f"names.Count != {191 + added}", 1,
        "contract name-set cardinality follows the extension")
    sub("x.ExplicitType == SymType.Func) != 187",
        f"x.ExplicitType == SymType.Func) != {187 + added}", 1,
        "FUNC import count 187 -> " + str(187 + added) + " (all new imports are FUNC)")
    sub('"__eh_frame_hdr_start", "__eh_frame_hdr_end" };',
        '"__eh_frame_hdr_start", "__eh_frame_hdr_end", "_DYNAMIC" };', 1,
        "_DYNAMIC is linker-provided by the dynamic writer (composed object "
        "references it untyped); verified absent from the final import table")
    harness_dir.mkdir(parents=True, exist_ok=True)
    # Stale obj/bin from earlier harness generations produce duplicate
    # AssemblyAttributes (CS0579) on rebuild; the harness is fully
    # regenerated each run, so scrub its build outputs first.
    for stale in ("obj", "bin"):
        shutil.rmtree(harness_dir / stale, ignore_errors=True)
    (harness_dir / "Program.cs").write_text(text)

    # SharpProspero delta: the composed runtime references _DYNAMIC, which the
    # resolver accepts (Linker.LinkerProvided) but DynamicWriter never gives a
    # value to, so relocations against it fail ("Unresolved symbol '_DYNAMIC'").
    # PayloadWriter already defines it as the dynamic-section address; mirror
    # that one line in a COPIED SharpProspero source tree (frozen original
    # untouched) and build the local copy for the harness.
    tools_src = (SPIKE / "native-runtime-import-probe-1/packager/source/tools")
    tools_dst = harness_dir / "tools"
    anchor = "        ulong dynamicAddr = Align(noteAddr + (ulong)note.Length, 8);"
    addition = ("\n        // worker-prx-packaging: provide the dynamic-section address for "
                "_DYNAMIC\n        // relocations, mirroring PayloadWriter's "
                "[\"_DYNAMIC\"] = dynamicAddr.\n"
                "        linkerDefined[\"_DYNAMIC\"] = dynamicAddr;")
    for proj in ("SharpProspero.Link", "SharpProspero.Prx"):
        (tools_dst / proj).mkdir(parents=True, exist_ok=True)
        for src in (tools_src / proj).glob("*.cs"):
            shutil.copyfile(src, tools_dst / proj / src.name)
        proj_text = (tools_src / proj / f"{proj}.csproj").read_text()
        if "<TargetFramework>" not in proj_text:
            # original built inside a parent context that supplied the TFM
            proj_text = proj_text.replace(
                "  <PropertyGroup>\n",
                "  <PropertyGroup>\n    <TargetFramework>net10.0</TargetFramework>\n", 1)
        (tools_dst / proj / f"{proj}.csproj").write_text(proj_text)
    dw_path = tools_dst / "SharpProspero.Link/DynamicWriter.cs"
    dw_text = dw_path.read_text()
    if dw_text.count(anchor) != 1:
        raise RuntimeError("DynamicWriter anchor not unique; refusing to patch")
    dw_path.write_text(dw_text.replace(anchor, anchor + addition))
    patches.append({"old": anchor, "new": anchor + addition, "occurrences": 1,
                    "reason": "DynamicWriter: define _DYNAMIC = dynamicAddr for "
                              "relocations (mirrors PayloadWriter); applied to a "
                              "copied SharpProspero source tree only"})
    b.run([PIN["dotnet"], "build", str(tools_dst / "SharpProspero.Link/SharpProspero.Link.csproj"),
           "-c", "Release", "--ignore-failed-sources",
           "-p:TargetFramework=net10.0"])  # source csproj omits TFM (parent build context)
    local_dll_dir = tools_dst / "SharpProspero.Link/bin/Release/net10.0"
    for asm in ("SharpProspero.Link.dll", "SharpProspero.Prx.dll"):
        if not (local_dll_dir / asm).is_file():
            raise RuntimeError(f"local SharpProspero build missing {asm}")

    # The pinned csproj references SharpProspero via HintPaths relative to the
    # frozen harness location; point them at the locally built copies.
    csproj_text = (PIN["lineage"] / "harness/Packager.csproj").read_text()
    rel_prefix = "../../native-runtime-import-probe-1/"
    frozen_dll_dir = (tools_src / "SharpProspero.Link/bin/Release/net10.0")
    if csproj_text.count(rel_prefix):
        csproj_text = csproj_text.replace(rel_prefix, str(tools_src) + "/")
        patches.append({"old": rel_prefix,
                        "new": str(tools_src) + "/ (then redirected to the local build)",
                        "occurrences": 2,
                        "reason": "re-anchor SharpProspero HintPaths absolutely, then "
                                  "point them at the locally built copies carrying "
                                  "the _DYNAMIC fix"})
    # redirect any absolute frozen dll dir to the local one
    csproj_text = csproj_text.replace(str(frozen_dll_dir), str(local_dll_dir))
    # tools/ lives INSIDE the Packager project dir; the SDK default **/*.cs
    # glob would compile the SharpProspero sources twice (directly AND via
    # the sub-projects), duplicating every assembly attribute (CS0579).
    exclude = ('\n  <ItemGroup>\n'
               '    <Compile Remove="tools/**/*.cs" />\n'
               '  </ItemGroup>\n</Project>')
    assert '</Project>' in csproj_text
    csproj_text = csproj_text.replace('</Project>', exclude, 1)
    patches.append({"old": "</Project>",
                    "new": "Compile Remove tools/**/*.cs",
                    "occurrences": 1,
                    "reason": "prevent double compilation of SharpProspero "
                              "sources living under the Packager dir"})
    (harness_dir / "Packager.csproj").write_text(csproj_text)
    b.note(f"patched Packager harness -> {harness_dir} ({len(patches)} recorded patches)")
    return patches


def supplier_import_shim() -> str:
    lines = ['.section .data.rel.ro.modbridge_supplier_imports,"aw",@progbits',
             ".p2align 3",
             ".globl modbridge_supplier_import_table",
             ".hidden modbridge_supplier_import_table",
             ".type modbridge_supplier_import_table,@object",
             "modbridge_supplier_import_table:"]
    for name in EXTENDED_IMPORTS:
        lines += [f".globl {name}", f".type {name},@function", f".quad {name}"]
    lines.append(".size modbridge_supplier_import_table, .-modbridge_supplier_import_table")
    return "\n".join(lines) + "\n"


# ------------------------------------------------------------ stage 3: packaging
def link_pipeline(b: Build) -> dict:
    result: dict = {"stage": "packaging"}
    lin = PIN["lineage"]
    build = b.out / "build"
    build.mkdir(parents=True, exist_ok=True)
    env_extra = {"LLVM_CONFIG": str(PIN["llvm"] / "llvm-config")}

    # 3.1 compile bootstrap units (+ typed-import shims) with frozen flags
    objects: list[Path] = []
    unit_records = {}
    shim = build / "supplier_import_extension.S"
    shim.write_text(supplier_import_shim())
    for name in list(BOOTSTRAP_UNITS) + [None]:
        if name is None:
            src, stem = shim, "supplier_import_extension"
        else:
            src, stem = lin / name, Path(name).stem
        target = build / f"{stem}.o"
        compiler = (PIN["sdk"] / "bin" /
                    ("prospero-clang++" if src.suffix == ".cpp" else "prospero-clang"))
        includes = ["-I" + str(p) for p in (
            lin / "generated", lin / "probe", lin / "lifecycle", lin, PIN["lua_include"])]
        b.run([compiler, *CLANG_FLAGS, *includes, "-c", src, "-o", target],
              env_extra=env_extra)
        objects.append(target)
        unit_records[stem] = {"source": str(src), "source_sha256": sha256_file(src),
                              "object": str(target), "object_sha256": sha256_file(target)}
    result["bootstrap_units"] = unit_records

    # 3.2 partial link: bootstrap first, then the composed runtime object
    composed = PIN["composition_dir"] / PIN["composition_object_name"]
    combined = build / "combined-supplier.o"
    b.run([PIN["llvm"] / "ld.lld", "-r", "-m", "elf_x86_64", "-o", combined,
           *objects, composed], env_extra=env_extra)
    result["combined_object"] = {"path": str(combined),
                                 "sha256": sha256_file(combined),
                                 "bytes": combined.stat().st_size}

    # 3.3 fail-closed pre-check of the combined unresolved set BEFORE Packager
    contract = json.loads(PIN["import_contract"].read_text())
    frozen_names = {r["ImportSymbol"]["Name"] for r in contract["contract"]["imports"]}
    expected_names = frozen_names | set(EXTENDED_IMPORTS)
    reader = PIN["llvm"] / "llvm-readelf"
    strong, weak, defined = readelf_undefined(reader, combined)
    boundary = set(PIN["frozen_boundary"]) | set(BOUNDARY_ADDITIONS)
    allowed_weak = set(PIN["frozen_allowed_weak"])
    actual = {n for n in strong - defined - boundary}
    unexpected = sorted(actual - expected_names)
    missing = sorted(expected_names - actual)
    weak_unexpected = sorted((weak - defined) - allowed_weak)
    result["combined_unresolved_precheck"] = {
        "strong_unresolved": len(actual), "expected": len(expected_names),
        "unexpected": unexpected, "missing": missing,
        "weak_unexpected": weak_unexpected}
    if unexpected or missing or weak_unexpected:
        b.gap(f"combined object unresolved set differs from the extended contract: "
              f"unexpected={unexpected} missing={missing} weak={weak_unexpected}")
        return result

    # 3.4 derived contract + patched harness
    contract_path = b.out / "supplier-contract.json"
    extended = derive_contract(b, contract_path)
    contract_sha = sha256_file(contract_path)
    result["supplier_contract"] = {
        "path": str(contract_path), "sha256": contract_sha,
        "required_strong": extended["counts"]["required_strong"]}
    result["packager_patches"] = patch_packager(
        b, b.out / "harness", contract_sha, sha256_file(PIN["import_contract"]))

    # 3.5 build + run Packager -> bare-original PRX
    harness_dll = b.out / "harness/bin/Release/net10.0/Packager.dll"
    if harness_dll.exists():
        raise RuntimeError("refuse to reuse an existing Packager build")
    b.run([PIN["dotnet"], "build", str(b.out / "harness/Packager.csproj"),
           "-c", "Release", "--ignore-failed-sources"])
    bare_original = b.out / "bare-original" / CANDIDATE_BASENAME
    bare_original.parent.mkdir(parents=True, exist_ok=True)
    pack = b.run([PIN["dotnet"], str(harness_dll), str(combined),
                  str(contract_path), str(bare_original)])
    result["packager"] = json.loads(pack.stdout)

    # 3.6 SDK stamp 0x11000000 (console-proven finalize)
    profile = {
        "title_id": "PPSA33286",
        "module_path": f"/data/plugins/{CANDIDATE_BASENAME}",
        "log_path": "/data/townfall_supplier_composition.log",
        "sdk_version": "0x11000000",
        "sdk_stamp_provenance": "console-proven fix for ESDKVERSION 0x80020063",
        "composed_runtime_sha256": PIN["composition_object_sha256"],
        "combined_object_sha256": result["combined_object"]["sha256"],
        "parent_recipe": str(PIN["lineage"]),
        "marker": "modbridge-supplier-composition/v1 build="
                  + result["combined_object"]["sha256"][:16],
        "promotion_allowed": False,
        "native_executed": False,
    }
    profile_path = b.out / "package-profile.json"
    profile_path.write_text(json.dumps(profile, indent=2) + "\n")
    bare_sdk = b.out / "bare-sdk" / CANDIDATE_BASENAME
    bare_sdk.parent.mkdir(parents=True, exist_ok=True)
    fin = b.run(["python3", str(PIN["finalize_sdk"]), str(bare_original),
                 str(profile_path), str(bare_sdk)])
    result["finalize_sdk"] = json.loads(fin.stdout)

    # 3.7 fake-self wrapper
    wrapped = b.out / CANDIDATE_BASENAME
    b.run(["python3", str(PIN["make_fself"]), str(bare_sdk), str(wrapped)])
    result["candidate"] = {"path": str(wrapped), "bytes": wrapped.stat().st_size,
                           "sha256": sha256_file(wrapped)}
    return result


# --------------------------------------------------------- stage 4: verification
def parse_phdrs(elf: bytes):
    phoff = struct.unpack_from("<Q", elf, 32)[0]
    phnum = struct.unpack_from("<H", elf, 56)[0]
    return [struct.unpack_from("<II6Q", elf, phoff + 56 * i) for i in range(phnum)]


def parse_sections(elf: bytes):
    shoff = struct.unpack_from("<Q", elf, 40)[0]
    shnum, shstr = struct.unpack_from("<HH", elf, 60)
    sections = [struct.unpack_from("<II4QII2Q", elf, shoff + 64 * i) for i in range(shnum)]
    st = sections[shstr]
    strings = elf[st[4]:st[4] + st[5]]
    named = {strings[x[0]:].split(b"\0")[0].decode(): x for x in sections}
    return sections, named


def symbols_in(elf: bytes, named: dict, sections: list, section_name: str):
    if section_name not in named:
        return []
    sec = named[section_name]
    strtab_sec = sections[sec[6]]
    strtab = elf[strtab_sec[4]:strtab_sec[4] + strtab_sec[5]]
    out = []
    for off in range(sec[4], sec[4] + sec[5], 24):
        no, info, _other, shndx, value, size = struct.unpack_from("<IBBHQQ", elf, off)
        name = strtab[no:].split(b"\0")[0].decode(errors="replace")
        if name:
            out.append({"name": name, "bind": info >> 4, "type": info & 0xF,
                        "shndx": shndx, "value": value, "size": size})
    return out


def verify_candidate(b: Build, packaging: dict) -> dict:
    bare = (b.out / "bare-sdk" / CANDIDATE_BASENAME).read_bytes()
    wrapped = (b.out / CANDIDATE_BASENAME).read_bytes()
    checks: dict = {}
    ok = True

    def check(name: str, passed: bool, detail) -> None:
        nonlocal ok
        checks[name] = {"pass": bool(passed), "detail": detail}
        if not passed:
            ok = False
            b.gap(f"static verification failed: {name}")
        b.note(f"check {name}: {'PASS' if passed else 'FAIL'}")

    # wrapper structure (ported from the frozen check_artifact.py)
    check("elf_magic_bare", bare[:4] == b"\x7fELF", bare[:4].hex())
    check("fself_magic_wrapped", wrapped[:4] == bytes.fromhex("4f153d1d"),
          wrapped[:4].hex())
    count = struct.unpack_from("<I2HQ2H4x", wrapped, 8)[4]
    elf_at = 32 + 32 * count
    phoff = struct.unpack_from("<Q", bare, 32)[0]
    phnum = struct.unpack_from("<H", bare, 56)[0]
    header_end = max(64, phoff + 56 * phnum)
    expected_header = bytearray(bare[:header_end])
    struct.pack_into("<H", expected_header, 60, 0)  # make_fself removes e_shnum
    check("wrapped_embedded_elf_header_equal",
          wrapped[elf_at:elf_at + header_end] == expected_header, {"segments": count})
    info_at = (elf_at + header_end + 15) & ~15
    check("fself_payload_digest_matches_bare",
          struct.unpack_from("<4Q32s", wrapped, info_at)[4]
          == hashlib.sha256(bare).digest(), None)
    segs = []
    seg_ok = True
    for j in range(1, count, 2):
        props, offset, stored, _ = struct.unpack_from("<4Q", wrapped, 32 + 32 * j)
        index = (props >> 20) & 65535
        ph = struct.unpack_from("<II6Q", bare, phoff + 56 * index)
        if stored != ph[5] or wrapped[offset:offset + stored] != bare[ph[2]:ph[2] + ph[5]]:
            seg_ok = False
        segs.append({"index": index, "type": hex(ph[0]), "bytes": stored})
    check("wrapped_segments_equal", seg_ok, segs)

    # ELF module type/flags
    e_type, e_machine = struct.unpack_from("<HH", bare, 16)
    e_flags = struct.unpack_from("<I", bare, 48)[0]
    checks["elf_module_type_flags"] = {"pass": True, "detail": {
        "e_type": hex(e_type), "e_machine": hex(e_machine),
        "e_flags": hex(e_flags)}}
    check("elf64_x86_64_le", e_machine == 0x3E and bare[4] == 2 and bare[5] == 1,
          {"e_machine": hex(e_machine), "class": bare[4], "data": bare[5]})

    phdrs = parse_phdrs(bare)
    module = [p for p in phdrs if p[0] == 0x61000002]
    check("pt_sce_module_unique", len(module) == 1, [hex(p[0]) for p in phdrs])
    sdk_stamp = struct.unpack_from("<I", bare, module[0][2] + 20)[0] if module else None
    check("sdk_stamp_0x11000000", sdk_stamp == 0x11000000,
          hex(sdk_stamp) if sdk_stamp is not None else None)

    dynamic = next((p for p in phdrs if p[0] == 2), None)
    tags: dict[int, int] = {}
    if dynamic:
        for i in range(0, dynamic[5], 16):
            tag, val = struct.unpack_from("<QQ", bare, dynamic[2] + i)
            tags[tag] = val
    check("explicit_dt_init_dt_fini", tags.get(12) != 0 and tags.get(13) != 0,
          {"DT_INIT": tags.get(12), "DT_FINI": tags.get(13)})
    check("loader_owns_no_arrays",
          all(tags.get(t, 0) == 0 for t in (25, 26, 27, 28)),
          {t: tags.get(t, 0) for t in (25, 26, 27, 28)})

    sections, named = parse_sections(bare)
    init_slots = named[".init_array"][5] // 8 if ".init_array" in named else 0
    fini_slots = named[".fini_array"][5] // 8 if ".fini_array" in named else 0
    checks["init_fini_slots"] = {"pass": True, "detail": {
        "init_slots": init_slots, "frozen_init_slots": PIN["frozen_init_slots"],
        "fini_slots": fini_slots, "frozen_fini_slots": PIN["frozen_fini_slots"],
        "delta_note": "new slots (if any) come from the 10 fresh composed units; "
                      "module_start walks __init_array_start..end explicitly"}}
    check("init_fini_slots_present", init_slots > 0 and fini_slots > 0,
          {"init_slots": init_slots, "fini_slots": fini_slots})
    for name in (".eh_frame", ".eh_frame_hdr", ".gcc_except_table"):
        check(f"section_{name}_present", name in named and named[name][5] > 0, None)

    # imports vs the composed object's undefined baseline
    dynsyms = symbols_in(bare, named, sections, ".dynsym")
    symtabs = symbols_in(bare, named, sections, ".symtab")
    imports = sorted(s["name"] for s in dynsyms
                     if s["shndx"] == 0 and s["bind"] in (1, 2))
    contract_names = {r["ImportSymbol"]["Name"] for r in json.loads(
        (b.out / "supplier-contract.json").read_text())["contract"]["imports"]}
    check("imports_equal_extended_contract", set(imports) == contract_names,
          {"count": len(imports), "expected": len(contract_names),
           "extra": sorted(set(imports) - contract_names),
           "missing": sorted(contract_names - set(imports))})
    check("linker_provided_dynamic_not_imported", "_DYNAMIC" not in imports, None)
    baseline = json.loads(
        (PIN["composition_dir"] / "undefined-delta.json").read_text())
    baseline_names = set(baseline["current"].keys())
    provider_map = {
        n: (f"{'libkernel' if v == 'sceKernelOpen' else 'libSceLibcInternal'} "
            f"(SDK stub, nm-verified in the composed run; identity donor {v})")
        for n, v in EXTENDED_IMPORTS.items()}
    check("imports_resolve_to_sdk_or_linker",
          set(imports) - baseline_names <= set(EXTENDED_IMPORTS),
          {"composed_baseline_count": baseline.get("current_count"),
           "extension_providers": provider_map,
           "imports_outside_composed_baseline":
               sorted(set(imports) - baseline_names)})

    # entry alignment evidence: force_align_arg_pointer prologue at module_start
    def entry_evidence(symbol: str) -> dict:
        cands = [s for s in (symtabs or dynsyms) if s["name"] == symbol and s["shndx"] != 0]
        if not cands:
            return {"symbol": symbol, "found": False}
        sym = max(cands, key=lambda s: s["size"])
        load = next((p for p in phdrs if p[0] == 1
                     and p[2] <= sym["value"] < p[2] + p[4]), None)
        if load is None:
            return {"symbol": symbol, "found": True, "mapped": False}
        off = sym["value"] - load[2] + load[3]  # vaddr -> file offset
        window = bare[off:off + 96]
        patterns = {"and_rsp_imm8_-16": bytes.fromhex("4883e4f0"),
                    "and_rsp_imm32_-16": bytes.fromhex("4881e4f0ffffffff")}
        hits = {k: window.find(v) for k, v in patterns.items()}
        return {"symbol": symbol, "found": True, "mapped": True,
                "value": hex(sym["value"]), "file_offset": off, "size": sym["size"],
                "exported_in_dynsym": any(s["name"] == symbol and s["shndx"] != 0
                                          for s in dynsyms),
                "first_bytes": window[:32].hex(),
                "realignment_pattern_offsets": hits,
                "prologue_source_attribute":
                    "force_align_arg_pointer (frozen lifecycle/module_entry.c; "
                    "SDK fixture-pinned codegen)"}

    start_ev = entry_evidence("module_start")
    stop_ev = entry_evidence("module_stop")
    checks["entry_alignment_evidence"] = {
        "pass": bool(start_ev.get("found") and start_ev.get("mapped")
                     and (start_ev["realignment_pattern_offsets"]["and_rsp_imm8_-16"] >= 0
                          or start_ev["realignment_pattern_offsets"]["and_rsp_imm32_-16"] >= 0)),
        "detail": {"module_start": start_ev, "module_stop": stop_ev}}
    if not checks["entry_alignment_evidence"]["pass"]:
        b.gap("entry alignment byte evidence not found at module_start")

    return {"status": "STATIC_CHECKS_PASSED" if ok else "STATIC_CHECKS_GAPS",
            "checks": checks, "candidate": packaging.get("candidate", {})}


# ------------------------------------------------------------------------ main
def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=None,
                        help="fresh output dir (must not exist); default: prx-candidate-<utc>")
    parser.add_argument("--link", action="store_true",
                        help="run the full packaging pipeline (default: verify-only)")
    parser.add_argument("--verify-only", action="store_true",
                        help="verify lineage + write evidence, no compilation/link")
    args = parser.parse_args()

    out = args.out or (BASE / ("prx-candidate-"
                               + _dt.datetime.now().strftime("%Y%m%d-%H%M%S")))
    if out.exists():
        print(f"refusing to overwrite history: {out} already exists", file=sys.stderr)
        return 2
    out.mkdir(parents=True)
    b = Build(out, link=args.link)
    b.note(f"output dir: {out}")

    try:
        evidence = verify_lineage(b)
        b.write_json("verification-evidence/lineage-verification.json", evidence)
        packaging: dict = {"stage": "skipped", "reason": "verify-only"}
        if args.link:
            if b.gaps:
                b.note("lineage gaps present -> NOT linking the final PRX (fail closed)")
                packaging = {"stage": "refused", "reason": "lineage gaps", "gaps": b.gaps}
            else:
                packaging = link_pipeline(b)
                if not b.gaps:
                    verification = verify_candidate(b, packaging)
                    b.write_json("verification-evidence/static-checks.json", verification)
    except Exception as exc:  # noqa: BLE001
        b.gap(f"exception: {exc}")
        b.note(f"FAILED: {exc}")

    receipt = {
        "worker": "worker-prx-packaging",
        "script": str(Path(__file__).resolve()),
        "mode": "link" if args.link else "verify-only",
        "utc": _dt.datetime.now().isoformat(),
        "pinned": {k: str(v) for k, v in PIN.items() if isinstance(v, Path)},
        "pinned_hashes": {k: v for k, v in PIN.items()
                          if k.endswith(("_sha256", "_bytes"))},
        "gaps": b.gaps,
        "log": b.log,
        "commands": b.records,
    }
    b.write_json("build-receipt.json", receipt)
    if b.gaps:
        b.write_json("GAPS.json", {"gaps": b.gaps,
                                   "action": "do not link/promote; reconcile lineage first"})
        b.note(f"{len(b.gaps)} gap(s) recorded; PRX candidate NOT approved")
        return 1
    b.note("done: " + ("PRX candidate produced and statically verified "
                       "(no in-game claim)" if args.link
                       else "lineage verified (no link requested)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
