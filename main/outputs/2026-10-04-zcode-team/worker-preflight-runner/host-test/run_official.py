#!/usr/bin/env python3
"""worker-preflight-runner official driver.

Ordering (studio contract + coordinator directive):
  1. FAIL-CLOSED LINEAGE GATE: worker-regression-guard/regression_preflight.py
     (verify-only) MUST pass before any suite runs. If it fail-closes, no suite
     is executed and the driver exits 2 (correct fail-closed behavior while
     luamod-lineage refreshes EDITIONS.json).
  2. Compilation of the three harness binaries (repro / corrected / livegap).
  3. Tests: 3 modes x 3 runs (run1 official + run2/run3 determinism), then
     timing-free stable-output diffs.
Per-stage elapsed also recorded by the C++ harness itself
(clock_gettime CLOCK_MONOTONIC); this driver emits the shared
modbridge.stage-timing/v1 document via worker-stage-metrics/timing_hooks.py.
Host-only: never contacts a console, engine, or provider.
"""
import json
import subprocess
import sys
from pathlib import Path

WS = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
W = WS / "outputs/2026-10-04-zcode-team/worker-preflight-runner"
sys.path.insert(0, str(WS / "outputs/2026-10-04-zcode-team/worker-stage-metrics"))
from timing_hooks import StageTimer  # noqa: E402

CYCLE = "preflight-3mode-3run"
GATE = WS / "outputs/2026-10-04-zcode-team/worker-regression-guard/regression_preflight.py"
MANIFEST = WS / "outputs/2026-10-04-zcode-team/worker-regression-guard/regression-input-manifest.json"

t = StageTimer(cycle=CYCLE, actor="worker-preflight-runner")


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=str(W), capture_output=True, text=True, **kw)


# ---- 1. fail-closed lineage gate (verify-only; NEVER --exec here) ----------
t.begin("lineage_gate")
gate = run([sys.executable, str(GATE), "--manifest", str(MANIFEST),
            "--json-out", str(W / "results/lineage-gate-official.json")])
try:
    gate_json = json.loads((W / "results/lineage-gate-official.json").read_text())
except Exception:
    gate_json = {}
t.mark("lineage_gate", f"gate exit={gate.returncode}")
t.end("lineage_gate", "regression_preflight.py verify-only (fail-closed contract)")
gate_ok = gate.returncode == 0 and not gate_json.get("fail_closed_triggered", True)
if not gate_ok:
    # fail-closed: no suites run
    for canonical in ("tests", "compilation"):
        t.not_reached(canonical, f"lineage gate not clean (exit {gate.returncode}); suites withheld")
    for canonical in ("packaging", "staging", "injection", "initialization"):
        t.skip(canonical, "host-only preflight; stage not applicable")
    (W / "results/stage-timing-official.json").write_text(json.dumps(t.doc(), indent=1))
    print(f"GATE FAIL-CLOSED (exit {gate.returncode}); suites withheld", file=sys.stderr)
    sys.exit(2)

# ---- 2. compilation --------------------------------------------------------
t.begin("compilation")
sha_old = run(["shasum", "-a", "256", "staging/SeedValidation.hpp"]).stdout.split()[0]
sha_new = run(["shasum", "-a", "256", "staging-corrected/SeedValidation.hpp"]).stdout.split()[0]
builds = [
    (["clang++", "-std=c++20", "-Wall", "-Wextra", "-O2", "-Istaging",
      "host-test/test_preflight_host.cpp", "-o", "host-test/bin/test_preflight_repro"],
     "repro vs frozen original header " + sha_old[:12]),
    (["clang++", "-std=c++20", "-Wall", "-Wextra", "-O2", "-Istaging-corrected",
      "host-test/test_preflight_host.cpp", "-o", "host-test/bin/test_preflight_corrected"],
     "corrected vs controller header " + sha_new[:12]),
    (["clang++", "-std=c++20", "-Wall", "-Wextra", "-O2", "-DRC_SEED_V5=1",
      "-Istaging-corrected",
      "host-test/test_preflight_host.cpp", "-o", "host-test/bin/test_preflight_livegap"],
     "livegap vs controller header (v5 API) " + sha_new[:12]),
]
for cmd, note in builds:
    b = run(cmd)
    t.mark("compilation", note + f" exit={b.returncode}")
    if b.returncode != 0:
        t.end("compilation", "clang++ x3", status="measured")
        print(b.stderr, file=sys.stderr)
        sys.exit(8)
t.end("compilation", "clang++ x3 (repro/corrected/livegap)")

# ---- 3. tests: 3 modes x 3 runs + determinism diffs -------------------------
t.begin("tests")
modes = [
    ("repro", "host-test/bin/test_preflight_repro", sha_old),
    ("corrected", "host-test/bin/test_preflight_corrected", sha_new),
    ("livegap", "host-test/bin/test_preflight_livegap", sha_new),
]
all_ok = True
for mode, binary, sha in modes:
    for label in ("run1", "run2", "run3"):
        r = run([binary, mode,
                 f"results/{mode}.{label}.json",
                 f"results/{mode}.{label}.stable.txt",
                 f"results/{mode}.{label}.receipt.txt", sha])
        t.mark("tests", f"{mode}.{label} exit={r.returncode}")
        if r.returncode != 0:
            all_ok = False
# canonical names = run1
import shutil  # noqa: E402
for mode, _, _ in modes:
    shutil.copyfile(W / f"results/{mode}.run1.json", W / f"results/{mode}.json")
    shutil.copyfile(W / f"results/{mode}.run1.stable.txt", W / f"results/{mode}.stable.txt")
    shutil.copyfile(W / f"results/{mode}.run1.receipt.txt", W / f"results/{mode}.receipt.txt")
# determinism diffs (timing-free stable outputs)
diff_lines = []
for mode, _, _ in modes:
    for a, b in (("run1", "run2"), ("run2", "run3")):
        sa = (W / f"results/{mode}.{a}.stable.txt").read_text()
        sb = (W / f"results/{mode}.{b}.stable.txt").read_text()
        same = sa == sb
        diff_lines.append(f"{mode}: {a} vs {b}: " + ("IDENTICAL" if same else "DIFFERENT"))
        if not same:
            all_ok = False
(W / "results/determinism.diff.txt").write_text("\n".join(diff_lines) + "\n")
t.end("tests", "3 modes x 3 runs + stable-output diffs; all_ok=" + str(all_ok))
for canonical in ("packaging", "staging", "injection", "initialization"):
    t.skip(canonical, "host-only preflight; stage not applicable")
(W / "results/stage-timing-official.json").write_text(json.dumps(t.doc(), indent=1))
print("\n".join(diff_lines))
sys.exit(0 if all_ok else 1)
