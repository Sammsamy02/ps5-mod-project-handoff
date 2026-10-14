#!/usr/bin/env python3
"""run_tests.py -- regression suite for pe_analyzer.py (capability #13 step 2).

Checks the analyzer's architecture detection, managed/native classification,
import/export extraction and UE4SS dependency reporting against:
  * the hand-crafted cap13 fixtures (FIXTURES/)
  * real Windows PE files already staged on this machine (read-only; originals
    are never modified or moved):
      - UE4SS.dll            (real UE4SS runtime, x64, 4256 exports)
      - dwmapi.dll           (real UE4SS proxy DLL, x64)
      - Probe.dll            (real managed C# assembly from cap10)
      - mscorlib.dll         (real Mono 4.5 mscorlib, 30 metadata tables)
      - worker.dll           (real locally-built x64 fixture from the
                              windows-worker-contract spike)

Emits REPORT/evidence/analyzer-test-results.json and prints a PASS/FAIL line
per assertion. Exit code 0 only when every assertion passes.

Usage: python3 run_tests.py
"""

import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CAP13 = os.path.abspath(os.path.join(HERE, ".."))
FIXTURES = os.path.join(CAP13, "FIXTURES")
EVIDENCE = os.path.join(CAP13, "REPORT", "evidence")
ANALYZER = os.path.join(HERE, "pe_analyzer.py")

WHAT = "/WORKSPACE/.codex/worktrees/translation-core/what"
CAPS = "/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work"

REAL_FILES = {
    "UE4SS.dll": WHAT + "/.superpowers/native-dependency-spike/windows-image-original-inputs-1/UE4SS.dll",
    "dwmapi.dll": WHAT + "/.superpowers/native-dependency-spike/windows-image-original-inputs-1/dwmapi.dll",
    "Probe.dll": CAPS + "/cap10/managed/Probe.dll",
    "mscorlib.dll": CAPS + "/cap10-static-corlib-host/owner-runtime/lib/mono/4.5/mscorlib.dll",
    "worker.dll": WHAT + "/.superpowers/native-dependency-spike/windows-worker-contract-review-1/green-worker-1/worker.dll",
}

results = []


def check(name: str, cond: bool, detail: str = "") -> None:
    results.append({"check": name, "pass": bool(cond), "detail": detail})
    print("%s  %s%s" % ("PASS" if cond else "FAIL", name,
                        ("  -- " + detail) if (detail and not cond) else ""))


def analyze(path: str) -> dict:
    out = subprocess.run([sys.executable, ANALYZER, path, "--json"],
                         capture_output=True, text=True, check=True)
    return json.loads(out.stdout)


def imp_funcs(rep: dict, dll_lower: str):
    for imp in rep.get("imports", []):
        if imp["dll"].lower() == dll_lower:
            return set(imp["functions"])
    return set()


def exp_names(rep: dict):
    return {e["name"]: e.get("rva") for e in rep.get("exports", {}).get("names", [])}


def main() -> int:
    os.makedirs(EVIDENCE, exist_ok=True)
    all_reports = {}

    # ---- fixture 1: x64 UE4SS-shaped native mod ------------------------------
    p = os.path.join(FIXTURES, "cap13-ue4ss-native-mod-FIXTURE.dll")
    r = analyze(p)
    all_reports["fixture-x64"] = r
    check("x64 fixture classified native-x64", r["classification"] == "native-x64",
          r["classification"])
    check("x64 fixture machine=x64 format=PE32+", r["machine"] == "x64" and r["format"] == "PE32+")
    check("x64 fixture subsystem WINDOWS_GUI + is_dll",
          r["subsystem"] == "WINDOWS_GUI" and r["is_dll"] is True)
    check("x64 fixture managed=no", r["managed"] is False)
    check("x64 fixture imports UE4SS.dll",
          imp_funcs(r, "ue4ss.dll") == {"is_ue4ss_initialized"},
          str(imp_funcs(r, "ue4ss.dll")))
    check("x64 fixture imports KERNEL32.dll!DisableThreadLibraryCalls",
          imp_funcs(r, "kernel32.dll") == {"DisableThreadLibraryCalls"})
    ex = exp_names(r)
    check("x64 fixture exports start_mod@0x1020 stop_mod@0x1030",
          ex.get("start_mod") == 0x1020 and ex.get("stop_mod") == 0x1030, str(ex))
    check("x64 fixture ue4ss report: imports_ue4ss + start_mod + function",
          r["ue4ss"]["imports_ue4ss_dll"] is True
          and r["ue4ss"]["exports_ue4ss_mod_entrypoints"] == ["start_mod", "stop_mod"]
          and r["ue4ss"]["ue4ss_imported_functions"] == ["is_ue4ss_initialized"])
    check("x64 fixture has 3 sections named .text/.rdata/.idata",
          [s["name"] for s in r["sections"]] == [".text", ".rdata", ".idata"])

    # ---- fixture 2: x86 ------------------------------------------------------
    p = os.path.join(FIXTURES, "cap13-native-x86-FIXTURE.dll")
    r = analyze(p)
    all_reports["fixture-x86"] = r
    check("x86 fixture classified unsupported-arch", r["classification"] == "unsupported-arch",
          r["classification"])
    check("x86 fixture machine=x86 format=PE32", r["machine"] == "x86" and r["format"] == "PE32")
    check("x86 fixture still reports its UE4SS import (precise diagnostic even when unsupported)",
          imp_funcs(r, "ue4ss.dll") == {"is_ue4ss_initialized"})

    # ---- fixture 3: not a PE --------------------------------------------------
    p = os.path.join(FIXTURES, "NOT-A-PE-FIXTURE.dll")
    r = analyze(p)
    all_reports["fixture-notape"] = r
    check("non-PE fixture classified unknown", r["classification"] == "unknown")
    check("non-PE fixture has precise diagnostic",
          any("MZ" in d for d in r["diagnostics"]))

    # ---- real: UE4SS.dll ------------------------------------------------------
    r = analyze(REAL_FILES["UE4SS.dll"])
    all_reports["real-UE4SS.dll"] = r
    check("UE4SS.dll classified native-x64", r["classification"] == "native-x64")
    exp = r.get("exports", {})
    n_exp = len(exp.get("names", []))
    check("UE4SS.dll exports fully parsed (4256 named, 0 forwarders)",
          n_exp == 4256 and exp.get("address_table_entries") == 4256
          and not exp.get("forwarders"), "parsed %d" % n_exp)
    c_api = {"call_script_function", "execute_lua_in_mod", "get_lua_state_by_mod_name",
             "is_ue4ss_initialized", "set_script_variable_default_data",
             "set_script_variable_int32"}
    check("UE4SS.dll unmangled C-API exports found in export table",
          c_api <= set(exp_names(r)), str(c_api - set(exp_names(r))))
    check("UE4SS.dll imports 29 runtime DLLs incl. MSVCP140/VCRUNTIME140/api-ms-win-crt",
          len(r["imports"]) == 29
          and {i["dll"] for i in r["imports"]} >= {"MSVCP140.dll", "VCRUNTIME140.dll",
                                                   "api-ms-win-crt-stdio-l1-1-0.dll"})
    check("UE4SS.dll does NOT import itself (imports_ue4ss=False)",
          r["ue4ss"]["imports_ue4ss_dll"] is False)

    # ---- real: dwmapi.dll proxy ----------------------------------------------
    r = analyze(REAL_FILES["dwmapi.dll"])
    all_reports["real-dwmapi.dll"] = r
    check("dwmapi.dll classified native-x64", r["classification"] == "native-x64")
    check("dwmapi.dll proxy pattern visible (KERNEL32 LoadLibraryW/GetProcAddress)",
          {"LoadLibraryW", "GetProcAddress"} <= imp_funcs(r, "kernel32.dll"))
    ex = exp_names(r)
    check("dwmapi.dll exports the real dwmapi surface (proxy re-exports, e.g. DwmDefWindowProc)",
          r["exports"].get("present") is True
          and r["exports"].get("address_table_entries") == 198
          and "DwmDefWindowProc" in ex)

    # ---- real: Probe.dll (managed, cap10) -------------------------------------
    r = analyze(REAL_FILES["Probe.dll"])
    all_reports["real-Probe.dll"] = r
    check("Probe.dll classified managed-dotnet", r["classification"] == "managed-dotnet")
    m = r["managed_details"]
    check("Probe.dll metadata: module/assembly/refs extracted",
          m.get("module_name") == "Probe.dll" and m.get("assembly_name") == "Probe"
          and m.get("metadata_version") == "v4.0.30319"
          and [a["name"] for a in m.get("assembly_references", [])] == ["mscorlib"])
    check("Probe.dll IL-only (mixed_mode=False)", m.get("ilonly") is True
          and m.get("mixed_mode") is False)
    check("Probe.dll imports mscoree.dll!_CorDllMain",
          imp_funcs(r, "mscoree.dll") == {"_CorDllMain"})

    # ---- real: mscorlib.dll (Mono 4.5) ----------------------------------------
    r = analyze(REAL_FILES["mscorlib.dll"])
    all_reports["real-mscorlib.dll"] = r
    check("mscorlib.dll classified managed-dotnet", r["classification"] == "managed-dotnet")
    m = r["managed_details"]
    check("mscorlib.dll metadata walk complete (30 tables, no warnings)",
          m.get("metadata_parsed") is True
          and len(m.get("table_row_counts", {})) == 30
          and not r["warnings"], str(r["warnings"]))
    check("mscorlib.dll assembly identity correct",
          m.get("assembly_name") == "mscorlib" and m.get("assembly_version") == "4.0.0.0")

    # ---- real: worker.dll (local native fixture from spike) --------------------
    r = analyze(REAL_FILES["worker.dll"])
    all_reports["real-worker.dll"] = r
    check("worker.dll classified native-x64 with 2 exports",
          r["classification"] == "native-x64"
          and len(r["exports"].get("names", [])) == 2)

    # ---- determinism: fixture bytes identical on rebuild ------------------------
    import hashlib
    fx = os.path.join(FIXTURES, "cap13-ue4ss-native-mod-FIXTURE.dll")
    before = hashlib.sha256(open(fx, "rb").read()).hexdigest()
    subprocess.run([sys.executable, os.path.join(HERE, "make_pe_fixtures.py")],
                   capture_output=True, check=True)
    after = hashlib.sha256(open(fx, "rb").read()).hexdigest()
    check("fixture build is byte-deterministic", before == after,
          "%s vs %s" % (before, after))

    out_path = os.path.join(EVIDENCE, "analyzer-test-results.json")
    with open(out_path, "w") as f:
        json.dump({"suite": "pe_analyzer regression (capability #13 step 2)",
                   "passed": sum(1 for x in results if x["pass"]),
                   "failed": sum(1 for x in results if not x["pass"]),
                   "checks": results,
                   "reports": all_reports}, f, indent=2)
        f.write("\n")
    print("\nwrote %s" % out_path)
    npass = sum(1 for x in results if x["pass"])
    print("TOTAL: %d/%d checks passed" % (npass, len(results)))
    return 0 if npass == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
