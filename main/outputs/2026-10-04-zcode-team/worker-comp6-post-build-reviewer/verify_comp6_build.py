#!/usr/bin/env python3
"""verify_comp6_build.py — independent post-build verification (worker-comp6-post-build-reviewer).

Read-only. Re-derives evidence from artifacts; does not trust gate prose.
Run: python3 verify_comp6_build.py <run-1 dir> <candidate dir> <driver staging dir>
"""
import hashlib, json, struct, subprocess, sys, os
from pathlib import Path

ROOT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
R3 = ROOT / ".superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3"

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def nm_und(p):
    out = subprocess.run(["nm", "-u", str(p)], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr
    return sorted(l.split()[-1] for l in out.stdout.splitlines() if l.strip())

def nm_def(p):
    out = subprocess.run(["nm", "-g", "--defined-only", str(p)], capture_output=True, text=True)
    assert out.returncode == 0, out.stderr
    return sorted(l.split()[-1] for l in out.stdout.splitlines() if l.strip())

def nm_all_und_file(p):
    out = subprocess.run(["nm", "-u", str(p)], capture_output=True, text=True)
    return sorted(l.split()[-1] for l in out.stdout.splitlines() if l.strip()), out.returncode

report = {}
run_dir = Path(sys.argv[1])
cand = Path(sys.argv[2])
staging = Path(sys.argv[3])
obj = run_dir / "coherent-tick-file-witness-runtime.o"

# ---------- 1. receipt vs run-3 ----------
r1 = json.loads((run_dir / "receipt.json").read_text())
r3 = json.loads((R3 / "receipt.json").read_text())
report["status"] = r1["status"]
report["status_ok"] = r1["status"] == "NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED"
report["retained_units_replaced"] = r1.get("retained_units_replaced")
report["freshly_compiled_units"] = r1.get("freshly_compiled_units")
f1 = set(r1["freshly_compiled_units"]); f3 = set(r3["freshly_compiled_units"])
report["fresh_delta_vs_r3"] = {"added": sorted(f1 - f3), "removed": sorted(f3 - f1)}
report["counts"] = {
    "r1_fresh": len(f1), "r3_fresh": len(f3),
    "r1_retained": r1.get("retained_runtime_units"), "r3_retained": r3["retained_runtime_units"],
    "r1_units": r1.get("runtime_compilation_units"), "r3_units": r3["runtime_compilation_units"],
}
pu = r1["selected_runtime_units"].get("program")
report["program_unit"] = None if pu is None else {
    "source": pu.get("source"), "source_sha256": pu.get("source_sha256"),
    "object_sha256": pu.get("object_sha256"), "runtime_replaced": pu.get("runtime_replaced"),
}
# expected pins from preflight
report["program_source_matches_preflight"] = bool(pu) and pu.get("source_sha256") == "75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb574" and pu.get("source", "").endswith("staging/comp6-program/UE4SSProgram.cpp")
iu = r1["selected_runtime_units"].get("unreal-unrealinitializer")
report["initializer_unit"] = None if iu is None else {
    "source": iu.get("source"), "source_sha256": iu.get("source_sha256"),
    "object_sha256": iu.get("object_sha256"),
}
report["initializer_source_is_merged"] = bool(iu) and "worker-initializer-integration/staging/UnrealInitializer.cpp" in iu.get("source", "") and iu.get("source_sha256") == "c85c42f7aaab0630569d65db599fbf8cfcbd0b79b8e99c6e27ea709aee5a8b7b"
au = r1["selected_runtime_units"].get("lmf-loader-adapter")
report["adapter_unit"] = None if au is None else {"source": au.get("source"), "source_sha256": au.get("source_sha256")}
lc = r1["selected_runtime_units"].get("native-loadercontext")
report["loadercontext_unit"] = None if lc is None else {"source": lc.get("source"), "source_sha256": lc.get("source_sha256")}
report["loadercontext_source_is_edited_W"] = bool(lc) and lc.get("source_sha256") == "bc982594c68ce61f3116a92b5b55fbd9eae7d81289d82ce4e608ba1ca2db30ec"
report["adapter_source_is_edited_W"] = bool(au) and au.get("source_sha256") == "b63f482a37d9337fb4a6a1d75021e81e62e842ab7b7f3943acd5c76df5be26a8"
# bootstrap pair
bp = {k: {"source_sha256": v.get("source_sha256"), "object_sha256": v.get("object_sha256")} for k, v in r1.get("bootstrap_units", {}).items()}
report["bootstrap_units"] = bp
report["bootstrap_same_as_r3"] = bp == {k: {"source_sha256": v.get("source_sha256"), "object_sha256": v.get("object_sha256")} for k, v in r3["bootstrap_units"].items()}
# undefined delta
d1, d3 = r1["undefined_delta"], r3["undefined_delta"]
report["undefined_delta_r1"] = {k: d1.get(k) for k in ("added", "removed", "changed_bindings", "current_count")}
report["undefined_delta_same_as_r3"] = (d1.get("added") == d3.get("added") and d1.get("removed") == d3.get("removed")
                                        and d1.get("changed_bindings") == d3.get("changed_bindings")
                                        and d1.get("current") == d3.get("current"))
report["runtime_sha256"] = r1.get("runtime_sha256"); report["runtime_bytes"] = r1.get("runtime_bytes")

# ---------- 2. composed object symbol shape vs run-3 ----------
und1 = nm_und(obj); def1 = nm_def(obj)
und3 = sorted(l.split()[-1] for l in subprocess.run(["nm", "-u", str(R3 / "coherent-tick-file-witness-runtime.o")], capture_output=True, text=True).stdout.splitlines() if l.strip())
def3 = sorted(l.split()[-1] for l in subprocess.run(["nm", "-g", "--defined-only", str(R3 / "coherent-tick-file-witness-runtime.o")], capture_output=True, text=True).stdout.splitlines() if l.strip())
report["und_identical"] = und1 == und3
report["und_delta"] = {"added": sorted(set(und1) - set(und3)), "removed": sorted(set(und3) - set(und1))}
report["def_delta"] = {"added": sorted(set(def1) - set(def3)), "removed": sorted(set(def3) - set(def1))}
report["def_counts"] = {"r1": len(def1), "r3": len(def3)}

# ---------- 3. Shape-A initializer proof ----------
# unit object level
iobj = run_dir / "unreal-unrealinitializer.o"
if iobj.is_file():
    report["initializer_object_sha256"] = sha(iobj)
    report["initializer_object_bytes"] = iobj.stat().st_size
    iund, rc = nm_all_und_file(iobj)
    report["initializer_und_ps_scan"] = [s for s in iund if "ps_scan" in s]
    report["initializer_und_count"] = len(iund)
# composed level: refusal string + ScanGame symbol + no ps_scan reloc in ScanGame section
data = obj.read_bytes()
needle = b"Required-slot seed table incomplete"
report["composed_has_refusal_string"] = needle in data
idx = data.find(needle)
report["refusal_context"] = data[max(0, idx - 120):idx + 200].decode("utf-8", "replace") if idx >= 0 else None
defs = subprocess.run(["nm", "-g", "--defined-only", str(obj)], capture_output=True, text=True).stdout
scan_lines = [l for l in defs.splitlines() if "ScanGame" in l and " defined" not in l]
report["scangame_symbols"] = scan_lines[:5]
# ps_scan import/def in composed
report["composed_ps_scan"] = {
    "undefined": [s for s in und1 if "ps_scan" in s],
    "defined": [s for s in def1 if "ps_scan" in s],
}

# ---------- 4. configured-roots strings ----------
s1 = b"Native program requires an observed game executable path or an explicit configured game directory"
s2 = b"Native configured roots must be absolute normalized directory paths without NUL"
report["validate_fork_strings"] = {"empty_path_requires_configured": s1 in data, "roots_absolute": s2 in data}
report["validate_symbol_defined"] = [l for l in defs.splitlines() if "ValidateNativeProgramContext" in l][:4]
report["make_context_symbols"] = sorted({l.split()[-1] for l in defs.splitlines() if "MakeNativeProgramContext" in l})[:6]
report["allow_configured_roots_token"] = b"allow_configured_roots_for_missing_executable_path" in data

# ---------- 5. gate receipts ----------
cs = json.loads((staging / "chain-symbols.json").read_text())
report["chain_symbols"] = {"count": len(cs), "all_defined": all(cs.values())}
og = json.loads((staging / "odr-gate.json").read_text())
report["odr_gate"] = {k: og.get(k) for k in ("port_sha256", "staged_sha256", "byte_identical")}
report["odr_port_sha_is_edited"] = og.get("port_sha256") not in ("664996150d3fec6df2471b1bf8c0cb231cf9f7a54d17a9d52595cc60d08b1868",) and og.get("byte_identical") is True
br = cand / "build-receipt.json"
if br.is_file():
    b = json.loads(br.read_text())
    sc = b.get("static_checks") or (json.loads((cand / "verification-evidence/static-checks.json").read_text()) if (cand / "verification-evidence/static-checks.json").is_file() else {})
    if isinstance(sc, dict):
        checks = sc.get("checks", sc)
        npass = sum(1 for v in checks.values() if isinstance(v, dict) and v.get("pass"))
        report["static_checks"] = {"total": len(checks), "pass": npass, "all_pass": npass == len(checks)}
        gaps = sc.get("gaps")
        report["static_check_gaps"] = gaps
    report["candidate_prx"] = [str(p.name) for p in cand.glob("*.prx")]
    prx = next(iter(cand.glob("*.prx")), None)
    if prx: report["candidate_prx_sha256"] = sha(prx); report["candidate_prx_bytes"] = prx.stat().st_size
em = Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-odr-emitter-check/receipt-one-pass.json")
if em.is_file():
    e = json.loads(em.read_text())
    report["emitter_receipt"] = {k: e.get(k) for k in ("verdict", "ok", "pass", "status", "checks_failed") if k in e}
    if "checks" in e:
        report["emitter_checks_failed"] = [k for k, v in e["checks"].items() if not v]

print(json.dumps(report, indent=1, default=str))
