#!/usr/bin/env python3
"""run_host_checks.py — JSONL field checks over the pad probe host-test
logs (worker-pad-hardware-probe, team 2026-10-04). Vendored contract from
the v3 lineage host checks: every NON-truncated line must json.loads, the
marker must be this module's, seq strictly increasing per file, and the
scenario-specific record vocabulary/orderings must hold exactly.

NOTE: the host binary exercises the PROBE CORE only (pp_run_probe);
the module-level "entry"/"startup" records (seq 1/2 on console, written by
pad_probe_module.c before the core runs) are console-side and therefore
expected ABSENT here — their presence/shape is a console-log expectation
documented in STAGE-PLAN.md."""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUILD = HERE / "build"
MARKER = "modbridge.padprobe"
BUILD_ID = "padp-hosttest"

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def load(name):
    lines = (BUILD / name).read_text().splitlines()
    recs = []
    for i, ln in enumerate(lines):
        if not ln.strip():
            continue
        try:
            rec = json.loads(ln)
        except json.JSONDecodeError as e:
            failures.append(f"{name}:{i + 1} not valid JSON: {e}")
            continue
        check(rec.get("__trunc") is None, f"{name}:{i + 1} truncated")
        check(rec.get("marker") == MARKER, f"{name}:{i + 1} marker")
        check(rec.get("build_id") == BUILD_ID, f"{name}:{i + 1} build_id")
        recs.append(rec)
    seqs = [r.get("seq") for r in recs]
    check(all(isinstance(s, int) for s in seqs) and
          all(b > a for a, b in zip(seqs, seqs[1:])),
          f"{name} seq strictly increasing: {seqs}")
    return recs


def exps(recs):
    return [(r["exp"], r.get("fields", {}).get("event")) for r in recs]


def f(rec, key):
    return rec.get("fields", {}).get(key)


# Scenario A: golden path (core records only; seq starts at 1).
a = load("pad_host_a.jsonl")
check([e for e, _ in exps(a)] == [
    "probe_start", "activate", "register",
    "key_fired", "key_fired", "probe_end", "teardown"],
    f"A record sequence wrong: {exps(a)}")
check(f(a[0], "binding") == "OPTIONS->F7" and f(a[0], "requested_user") == -1,
      "A probe_start binding/user")
check(f(a[0], "window_ms") == 20000 and f(a[0], "grace_ms") == 500 and
      f(a[0], "clock_ok") == 1, "A probe_start window params")
kf = [r for r in a if r["exp"] == "key_fired"]
check(len(kf) == 2, "A exactly 2 key_fired")
for r in kf:
    check(f(r, "key") == "F7" and f(r, "button") == "OPTIONS", "A kf fields")
    check(f(r, "user_id") == 5 and f(r, "handle") == 42, "A kf user/handle")
check(f(kf[1], "polls_so_far") > f(kf[0], "polls_so_far"),
      "A repress later than first press")
check(f(a[1], "phase") == "Active" and f(a[1], "user_id") == 5 and
      f(a[1], "handle") == 42, "A activate fields")
check(f(a[2], "registered") == 1 and f(a[2], "key") == "F7" and
      f(a[2], "button") == "OPTIONS", "A register record")
pe = a[-2]
check(f(pe, "keys_fired") == 2 and f(pe, "final_phase") == "Active" and
      f(pe, "end_reason") == "grace_elapsed" and f(pe, "polls") >= 6,
      "A probe_end fields")
check(f(a[-1], "phase") == "Inactive" and f(a[-1], "handle") == -1 and
      f(a[-1], "owned_user_service_released") == 1, "A teardown Inactive")

# Scenario B: user_initialize failure.
b = load("pad_host_b.jsonl")
check([e for e, _ in exps(b)] == ["probe_start", "probe_failed"],
      f"B sequence: {exps(b)}")
pf = b[-1]
check(f(pf, "stage") == "activate" and f(pf, "detail") == "UserInitialize",
      "B stage/detail")
check(pf["rc"] == 5, "B rc carries backend error 5")

# Scenario C: NoUser (empty login list, requested -1).
c = load("pad_host_c.jsonl")
check(exps(c)[-1] == ("probe_failed", "probe_failed"), "C final failed")
check(f(c[-1], "detail") == "NoUser" and f(c[-1], "stage") == "activate",
      "C NoUser detail/stage")

# Scenario D: healthy run (fast window); constructor gate checked in C code.
d = load("pad_host_d.jsonl")
check(exps(d)[-1] == ("teardown", "teardown"), "D ends with teardown")
check(any(r["exp"] == "key_fired" for r in d) is False, "D no fire (no press)")

# Scenario E: unplug/replug.
e = load("pad_host_e.jsonl")
pcs = [r for r in e if r["exp"] == "phase_change"]
check(any(f(r, "to") == "Disconnected" for r in pcs),
      "E Disconnected phase recorded")
check(any(f(r, "from") == "Disconnected" and f(r, "to") == "Active"
          for r in pcs), "E reconnect recorded Active")
kf = [r for r in e if r["exp"] == "key_fired"]
check(len(kf) == 1, "E exactly 1 key_fired (no phantom replug edge)")
check(f(kf[0], "user_id") == 9 and f(kf[0], "handle") == 7, "E kf user/handle")

# Scenario F: factory failure.
fl = load("pad_host_f.jsonl")
check(exps(fl)[-1] == ("probe_failed", "probe_failed"), "F final failed")
check(f(fl[-1], "stage") == "factory", "F factory stage")

if failures:
    print(f"HOST JSONL CHECKS FAILED ({len(failures)}):")
    for m in failures:
        print("  -", m)
    sys.exit(1)
print("HOST JSONL CHECKS PASSED "
      f"(a={len(a)} b={len(b)} c={len(c)} d={len(d)} e={len(e)} f={len(fl)} records)")
