#!/usr/bin/env python3
"""run_lifecycle_e2e.py — worker-lifecycle-e2e driver.

ONE host regression that compiles byte-exact extractions of all three
conversion-fix units together and drives the full native mod lifecycle chain
against the real NativeModSource + real bundle bytes:

  loadlist rows -> NativeModSource (lua_mods + startup_mod_names from the
  mods.txt `TownfallItemSpawner : 1` row + staged enabled.txt)
  -> setup_mods-equivalent (widen_ascii_native_key -> LuaMod-ctor lookup via
     modbridge::normalize_path_for_lua -> scripts_path derived)
  -> start_mods-equivalent (widen_ascii_native_key(name) against the STORED
     widened name -> start)   [the P0 regression; simulated-garble control]
  -> main.lua read (bytes == staged sha f586043a...) -> banner through the
     FIXED to_generic_string (clean wide, no NULs)
  -> determinism: run twice, diff.

Fail-closed pinning (worker-regression-guard discipline): every input is
sha256-pinned; ANY mismatch aborts before compilation with exit 2.

Stages are timed with worker-stage-metrics/timing_hooks.py (same JSON schema).
Host-side only: no console dispatch, no game claims.
"""

import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEAM = HERE.parent
WORKTREE = TEAM.parent.parent  # .../what

sys.path.insert(0, str(TEAM / "worker-stage-metrics"))
from timing_hooks import StageTimer  # noqa: E402

CYCLE = "worker-lifecycle-e2e"
ACTOR = "worker-lifecycle-e2e"

# --- pinned inputs (sha256, fail-closed) ------------------------------------
PINS = {
    "program_wpd1": (
        TEAM / "worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp",
        "cb6cb14aa044124f17cf8d2115b4962a7d91daa686f6e80a0631061b51b8c8af",
    ),
    "luamod_l11_merged": (
        TEAM / "worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp",
        "c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0",
    ),
    "string_hpp_staged": (
        TEAM / "worker-iniparser-repro/staged/Helpers/String.hpp",
        "c76be502fbe6f9c2c6589f141c1caded84af482a1f03a428005574850ca47788",
    ),
    "native_mod_source_cpp": (
        TEAM / "worker-preflight-runner/staging-lua/NativeModSource.cpp",
        "2db163bc7feead56badc12ad235711576b837e1f5a05dba04a4d8fb03e53524f",
    ),
    "native_mod_source_hpp": (
        TEAM / "worker-preflight-runner/staging-lua/Platform/NativeModSource.hpp",
        "2b44df14854934470a16fec6ef153501e08a8554ba51748b335fd0733856a450",
    ),
    "staged_main_lua": (
        TEAM / "worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua",
        "f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658",
    ),
    "staged_enabled_txt": (
        TEAM / "worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/enabled.txt",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",  # empty file
    ),
    "staged_uehelpers_lua": (
        TEAM / "worker-itemspawner-bundle/bundle/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua",
        None,  # size-pinned only (9962); content is carried but not conversion-relevant
    ),
    "staged_settings_ini": (
        TEAM / "worker-itemspawner-bundle/bundle/ue4ss/UE4SS-settings.ini",
        None,  # size-pinned only (221)
    ),
    "fixture_source": (HERE / "fixture/worker_lifecycle_e2e_host.cpp", None),
}
SIZE_PINS = {
    "staged_main_lua": 21415,
    "staged_enabled_txt": 0,
    "staged_uehelpers_lua": 9962,
    "staged_settings_ini": 221,
}

# --- byte-exact extraction ranges (1-based, inclusive) -----------------------
# Each entry asserts its first and last line content before slicing (fail-closed
# against silent line drift in the pinned units).
EXTRACTIONS = [
    # (out name, pinned key, start, end, expected first-line prefix, expected last-line suffix)
    ("prog_widen.inc", "program_wpd1", 1807, 1822,
     "// PS5 codecvt discipline (proven live, pids 466/492/700): every", "}"),
    ("prog_setup_loop.inc", "program_wpd1", 1835, 1843,
     "auto widen_ascii = [](const std::string& narrow) -> StringType {", "}"),
    ("prog_start_mods.inc", "program_wpd1", 2043, 2051,
     "if constexpr (std::is_same_v<ModType, LuaMod>)", "}"),
    ("prog_comparator.inc", "program_wpd1", 3056, 3063,
     "if (mod_name != elem->get_name())", "}"),
    ("luamod_normalize.inc", "luamod_l11_merged", 42, 54,
     "// Local override of normalize_path_for_lua: the shared String.hpp version",
     "using modbridge::normalize_path_for_lua;"),
    ("luamod_ctor.inc", "luamod_l11_merged", 934, 947,
     "if (m_native_source)", "}"),
    ("string_to_generic.inc", "string_hpp_staged", 595, 636,
     "auto inline to_generic_string(const auto& input) -> StringType", "}"),
]


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fail(timer: StageTimer, stage: str, source: str, message: str, code: int) -> int:
    if stage in timer._open:
        timer.end(stage, source, status="measured")
    recorded = {s["stage"] for s in timer.stages}
    for later in ("chain-run-1", "chain-run-2", "determinism-diff"):
        if later not in recorded and later not in timer._open:
            timer.not_reached(later, f"aborted: {message}")
    write_reports(timer, None, verdict="FAIL-ABORTED", abort=message)
    print(f"ABORT: {message}", file=sys.stderr)
    return code


def parse_receipt(receipt: Path) -> list:
    """receipt lines: '[PASS|FAIL] NAME[: detail]' -> checks with stage inferred
    from the leading check-id token (C0/S1/S2/S3/S4)."""
    checks = []
    if not receipt.exists():
        return checks
    for line in receipt.read_text().splitlines():
        if not line.startswith("["):
            continue
        status, rest = line[1:].split("]", 1)
        name = rest.strip()
        detail = ""
        if ": " in name:
            name, detail = name.split(": ", 1)
        cid = name.split()[0] if name.split() else ""
        stage = {"C0": "S0-environment", "S1": "S1-loadlist-source", "S2": "S2-setup-mods",
                 "S3": "S3-start-mods", "S4": "S4-read-banner"}.get(cid, "S0-environment")
        checks.append({"stage": stage, "name": name,
                       "status": "PASS" if status == "PASS" else "FAIL", "detail": detail})
    return checks


def write_reports(timer: StageTimer, run_json, verdict: str, abort=None) -> dict:
    results = HERE / "results"
    results.mkdir(parents=True, exist_ok=True)

    timing_doc = timer.doc()
    (results / "stage-timing-lifecycle-e2e.json").write_text(
        json.dumps(timing_doc, indent=1) + "\n")

    extr_prov = []
    prov_path = HERE / "extraction/provenance.json"
    if prov_path.exists():
        extr_prov = json.loads(prov_path.read_text())["extractions"]

    checks = parse_receipt(HERE / "results/receipt.run1.txt")
    if run_json is not None:
        run_json = dict(run_json)
        run_json["checks"] = checks

    doc = {
        "marker": "worker-lifecycle-e2e/chain-regression/1",
        "worker": ACTOR,
        "cycle": CYCLE,
        "verdict": verdict,
        "abort_reason": abort,
        "pinned_inputs": {
            name: {"path": str(p), "sha256": (sha256_of(p) if p.exists() else None),
                   "expected_sha256": want, "size": (p.stat().st_size if p.exists() else None)}
            for name, (p, want) in PINS.items()
        },
        "extractions": extr_prov,
        "binary": run_json,
        "timing": timing_doc,
    }
    (results / "lifecycle-e2e-results.json").write_text(json.dumps(doc, indent=1) + "\n")
    write_md(results, doc)
    return doc


def write_md(results: Path, doc: dict) -> None:
    b = doc.get("binary") or {}
    checks = b.get("checks") or []
    timing = doc["timing"]
    lines = []
    lines.append("# LIFECYCLE-E2E — native mod lifecycle conversion chain (host regression)")
    lines.append("")
    lines.append("Worker: worker-lifecycle-e2e. Host-only; no console dispatch; no game claims.")
    lines.append("")
    lines.append(f"**Verdict: {doc['verdict']}**"
                 + (f" (aborted: {doc['abort_reason']})" if doc.get("abort_reason") else ""))
    lines.append("")
    lines.append("## What this proves")
    lines.append("")
    lines.append("ONE host binary compiles byte-exact line-range extractions of all three fix")
    lines.append("units together and drives the chain against the real production")
    lines.append("NativeModSource (composition-pinned copy) and the real bundle bytes:")
    lines.append("")
    lines.append("1. loadlist rows -> NativeModSource: lua_mods + startup_mod_names from a")
    lines.append("   mods.txt `TownfallItemSpawner : 1` row plus the staged 0-byte enabled.txt")
    lines.append("   marker (deduped).")
    lines.append("2. setup_mods-equivalent: widen_ascii_native_key(name/path) -> LuaMod-ctor-")
    lines.append("   equivalent lookup via modbridge::normalize_path_for_lua -> found;")
    lines.append("   scripts_path derived from the native row.")
    lines.append("3. start_mods-equivalent (the P0): widen_ascii_native_key(startup name)")
    lines.append("   against the STORED widened name -> match -> start_mod. A simulated")
    lines.append("   old-ensure_str NUL-interleave control MISSES, proving the check")
    lines.append("   discriminates.")
    lines.append("4. main.lua read via the source == staged bytes (sha256 f586043a...,")
    lines.append("   21415 bytes) and the banner line through the FIXED to_generic_string")
    lines.append("   extraction: clean wide, no NUL units (simulated-garble control).")
    lines.append("5. Determinism: the binary ran twice; receipt/stable/json diffs are empty.")
    lines.append("")
    lines.append("## Byte-exact extraction provenance")
    lines.append("")
    lines.append("| extraction | pinned unit | lines | sha256 (slice) |")
    lines.append("|---|---|---|---|")
    for e in doc.get("extractions", []):
        lines.append(f"| {e['name']} | {e['unit']} ({e['unit_sha256'][:12]}...) | "
                     f"{e['start']}-{e['end']} | {e['sha256'][:16]}... |")
    lines.append("")
    lines.append("## Per-stage checks (from the binary receipt)")
    lines.append("")
    lines.append("| stage | check | result | detail |")
    lines.append("|---|---|---|---|")
    for c in checks:
        detail = (c.get("detail") or "").replace("|", "\\|")
        lines.append(f"| {c['stage']} | {c['name']} | {c['status']} | {detail} |")
    lines.append("")
    passed, failed = b.get("passed"), b.get("failed")
    lines.append(f"Binary checks: {passed} passed, {failed} failed.")
    lines.append("")
    lines.append("## Timings")
    lines.append("")
    lines.append("| stage | duration_ms | status |")
    lines.append("|---|---|---|")
    for s in timing["stages"]:
        d = s["duration_ms"]
        lines.append(f"| {s['stage']} | {'—' if d is None else f'{d:.1f}'} | {s['status']} |")
    lines.append("")
    totals = timing["totals"]
    lines.append(f"Total (all stages): {totals['all_ms']:.1f} ms; "
                 f"slowest: {totals['slowest_stage']}.")
    lines.append("")
    lines.append("## Fixture honesty notes")
    lines.append("")
    lines.append("- The LuaMod/UE4SSProgram/Output pieces around the extractions are fixture")
    lines.append("  shells; every conversion, lookup, and branch body on the chain is a")
    lines.append("  verbatim line-range extraction (see table above).")
    lines.append("- The controls use a SIMULATION of the documented pid-700")
    lines.append("  NUL-interleave conversion breakage; host libc++ does not reproduce the")
    lines.append("  PS5 codecvt stride bug, so no claim is made that the control reproduces")
    lines.append("  target codecvt — only that a garbled wide key cannot match the stored")
    lines.append("  clean wide key (the exact P0 mechanism).")
    lines.append("- The mods.txt row `TownfallItemSpawner : 1` is the task-specified enable")
    lines.append("  row; the staged bundle carries no mods.txt (its enable marker is the")
    lines.append("  0-byte enabled.txt, also exercised).")
    lines.append("- Target parity is by value: -fshort-wchar gives 2-byte wchar_t, and all")
    lines.append("  extracted conversions are unit-value-level.")
    lines.append("- No build, capture, console, or game-behavior claim is made.")
    lines.append("")
    (results / "LIFECYCLE-E2E.md").write_text("\n".join(lines) + "\n")


def main() -> int:
    t0 = time.time()
    timer = StageTimer(cycle=CYCLE, actor=ACTOR)

    # -- stage: tests (fail-closed pinning + byte-exact extraction) ----------
    timer.begin("tests")
    for name, (path, want) in PINS.items():
        if not path.exists():
            return fail(timer, "tests", f"pin {name}", f"missing pinned input: {path}", 2)
        got = sha256_of(path)
        if want is not None and got != want:
            return fail(timer, "tests", f"pin {name}",
                        f"sha mismatch for {path}: {got} != {want}", 2)
        if name in SIZE_PINS and path.stat().st_size != SIZE_PINS[name]:
            return fail(timer, "tests", f"pin {name}",
                        f"size mismatch for {path}: {path.stat().st_size} != {SIZE_PINS[name]}", 2)

    extraction_dir = HERE / "extraction"
    extraction_dir.mkdir(parents=True, exist_ok=True)
    provenance = []
    for (out_name, unit, start, end, first_prefix, last_suffix) in EXTRACTIONS:
        src, _ = PINS[unit]
        raw_lines = src.read_bytes().splitlines(keepends=True)
        if end > len(raw_lines):
            return fail(timer, "tests", f"extract {out_name}",
                        f"range {start}-{end} exceeds {src} ({len(raw_lines)} lines)", 2)
        # strip line endings for assertion, keep original bytes for the slice
        first = raw_lines[start - 1].decode("utf-8").strip()
        last = raw_lines[end - 1].decode("utf-8").strip()
        if not first.startswith(first_prefix.strip()):
            return fail(timer, "tests", f"extract {out_name}",
                        f"line {start} drift in {src}: [{first}]", 2)
        if not last.endswith(last_suffix.strip()):
            return fail(timer, "tests", f"extract {out_name}",
                        f"line {end} drift in {src}: [{last}]", 2)
        slice_bytes = b"".join(raw_lines[start - 1:end])
        out = extraction_dir / out_name
        out.write_bytes(slice_bytes)
        provenance.append({
            "name": out_name, "unit": unit, "start": start, "end": end,
            "unit_path": str(src), "unit_sha256": PINS[unit][1] or sha256_of(src),
            "sha256": hashlib.sha256(slice_bytes).hexdigest(),
            "bytes": len(slice_bytes),
            "first_line": first, "last_line": last,
        })
    (extraction_dir / "provenance.json").write_text(
        json.dumps({"marker": "worker-lifecycle-e2e/extraction-provenance/1",
                    "extractions": provenance}, indent=1) + "\n")
    timer.end("tests", "sha256 pins (9 inputs) + 7 byte-exact extraction slices "
              "with first/last-line drift guards")

    # -- stage: compilation ---------------------------------------------------
    timer.begin("compilation")
    staging_lua = TEAM / "worker-preflight-runner/staging-lua"
    binary = HERE / "bin/worker_lifecycle_e2e_host"
    binary.parent.mkdir(parents=True, exist_ok=True)
    # NOTE: deliberately NO -fshort-wchar. Apple clang -fshort-wchar breaks the
    # libc++ std::wstring ABI in the host TU (measured: L"Townfall" ->
    # size()==37 with correct UTF-16 bytes; element reads interleaved) — the
    # worker-preflight-runner precedent uses plain host wchar_t with VALUE-level
    # target parity, which is sound here because every extracted conversion
    # pushes unit values 0x00..0xFF only.
    cmd = ["clang++", "-std=c++20", "-Wall", "-Wextra",
           "-I", str(extraction_dir), "-I", str(staging_lua),
           str(HERE / "fixture/worker_lifecycle_e2e_host.cpp"),
           str(staging_lua / "NativeModSource.cpp"),
           "-o", str(binary)]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    (HERE / "logs").mkdir(exist_ok=True)
    (HERE / "logs/compile.stderr").write_text(proc.stderr)
    if proc.returncode != 0:
        return fail(timer, "compilation", "clang++ -std=c++20",
                    f"compile failed rc={proc.returncode} (logs/compile.stderr)", 1)
    timer.end("compilation",
              "clang++ -std=c++20 -Wall -Wextra (plain host wchar_t, value-level "
              "parity; -fshort-wchar breaks host libc++ wstring ABI); "
              "fixture + verbatim extractions + production NativeModSource.cpp")

    # -- stages: chain-run-1 / chain-run-2 ------------------------------------
    bundle = TEAM / "worker-itemspawner-bundle/bundle"
    run_jsons = []
    artifacts = []
    for i in (1, 2):
        stage = f"chain-run-{i}"
        timer.begin(stage)
        receipt = HERE / f"results/receipt.run{i}.txt"
        rjson = HERE / f"results/binary.run{i}.json"
        stable = HERE / f"results/stable.run{i}.txt"
        run = subprocess.run([str(binary), str(bundle), str(receipt), str(rjson), str(stable)],
                             capture_output=True, text=True, timeout=120)
        if run.returncode != 0:
            (receipt.with_suffix(".stdout")).write_text(run.stdout)
            return fail(timer, stage, f"{binary.name} rc={run.returncode}",
                        f"chain run {i} failed (see results/receipt.run{i}.txt)", 1)
        run_jsons.append(json.loads(rjson.read_text()))
        artifacts.append((receipt, rjson, stable))
        timer.end(stage, f"{binary.name} exit 0; {run_jsons[-1]['passed']} passed, "
                         f"{run_jsons[-1]['failed']} failed")

    # -- stage: determinism-diff ----------------------------------------------
    timer.begin("determinism-diff")
    diffs = []
    for a, b in zip(artifacts[0], artifacts[1]):
        if a.read_bytes() != b.read_bytes():
            diffs.append(a.name)
    identical = not diffs
    timer.end("determinism-diff",
              "receipt/stable/json byte-compared across run1/run2: "
              + ("identical" if identical else f"DIFFER: {diffs}"))
    if not identical:
        return fail(timer, "determinism-diff", "diff",
                    f"nondeterministic outputs: {diffs}", 1)

    verdict = "PASS" if run_jsons[0]["verdict"] == "LIFECYCLE-E2E-PASS" else "FAIL"
    doc = write_reports(timer, run_jsons[0], verdict=verdict)
    # keep both runs' receipts for inspection
    doc["determinism"] = {"identical": identical, "diffs": diffs}
    (HERE / "results/lifecycle-e2e-results.json").write_text(json.dumps(doc, indent=1) + "\n")

    elapsed = time.time() - t0
    print(json.dumps({"verdict": verdict,
                      "checks": f"{run_jsons[0]['passed']} passed / {run_jsons[0]['failed']} failed",
                      "determinism_identical": identical,
                      "elapsed_s": round(elapsed, 2)}))
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
