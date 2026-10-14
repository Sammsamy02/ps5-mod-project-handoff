#!/usr/bin/env python3
"""check_fixtures.py — assert analyzer_v2.py classifies the SYNTHETIC fixtures
exactly as the RUNBOOK-V2-ADDENDUM requires.

worker-runbook-v2 · 2026-10-06. Runs analyzer_v2.py --json-only per fixture
dir into dryrun-fixtures/<name>/ and checks the classification table below.
Exit 0 = ALL FIXTURE EXPECTATIONS MET; 1 = mismatch (printed).

Usage: check_fixtures.py [FIXTURES_DIR]
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
V2 = os.path.join(HERE, "analyzer_v2.py")

# ---------------------------------------------------------------- expectations
# per fixture: which (rung, observable-contains) rows must have which status,
# plus top-level classification assertions.
E = {
    "fx01-refused-copyin0": {
        "rungs": {"C0_boot_frame": "PROVEN", "C1_pool_verdict": "REFUSED"},
        "cascade_stop": ("C1", "REFUSED"),
        "subverdict": "REFUSED-HONEST",
        "corrupt": False,
        "identity": "PROVEN",
        "insession_commands": 1,
        "rows": [
            ("C1", "copyin probe", "PROVEN"),
            ("C1", "copyin raw captures", "PROVEN"),
            ("C1", "probe order", "PROVEN"),
            ("C1", "copyin vs verdict consistency", "PROVEN"),
            ("C1", "branch-zero memo", "PROVEN"),
            ("C1", "resolution note text", "PROVEN"),
            ("GI", "eboot sha w0..w3", "PROVEN"),
            ("GI", "content version packed", "PROVEN"),
            ("GI", "runtime-status gamedata row", "PROVEN"),
        ],
        "absent_rows": [("C1", "copyin route availability")],
    },
    "fx02-refused-copyin2": {
        "rungs": {"C1_pool_verdict": "REFUSED"},
        "cascade_stop": ("C1", "REFUSED"),
        "subverdict": "REFUSED-HONEST",
        "corrupt": False,
        "identity": "PENDING",
        "rows": [
            ("C1", "copyin route availability", "REFUSED"),
            ("C1", "probe order", "PROVEN"),
            ("C1", "copyin vs verdict consistency", "PROVEN"),
        ],
        "absent_rows": [("C1", "branch-zero memo")],
    },
    "fx03-refused-copyin3": {
        "rungs": {"C1_pool_verdict": "REFUSED"},
        "cascade_stop": ("C1", "REFUSED"),
        "subverdict": "NOT-RATED",
        "corrupt": False,
        "identity": "PENDING",
        "rows": [
            ("C1", "copyin request validity", "REFUSED"),
            ("C1", "probe order", "PROVEN"),
        ],
        "absent_rows": [("C1", "branch-zero memo")],
    },
    "fx04-admitted-copyin-full": {
        "rungs": {"C0_boot_frame": "PROVEN", "C1_pool_verdict": "PROVEN",
                  "C2_post_seeds": "PROVEN", "C3_load_question": "PROVEN",
                  "C4_input_arrival": "PROVEN", "C5_delivery_menu": "PROVEN"},
        "cascade_stop": None,
        "load_question": "PASS",
        "subverdict": "ADMITTED-VIA-COPYIN",
        "corrupt": False,
        "identity": "PROVEN",
        "rows": [
            ("C1", "probe order", "PROVEN"),
            ("C1", "copyin vs verdict consistency", "PROVEN"),
            ("C1", "admission provenance", "PROVEN"),
        ],
        "absent_rows": [("C1", "branch-zero memo")],
    },
    "fx05-admitted-span": {
        "rungs": {"C1_pool_verdict": "PROVEN", "C2_post_seeds": "PENDING"},
        "cascade_stop": ("C2", "PENDING"),
        "subverdict": "ADMITTED-VIA-SPAN",
        "corrupt": False,
        "identity": "PENDING",
        "insession_commands": 1,
        "rows": [
            ("C1", "copyin probe", "PENDING"),
            ("INSESSION", "insession_*", "PROVEN"),
        ],
    },
    "fx06-order-violation": {
        "rungs": {"C1_pool_verdict": "REFUSED"},
        "cascade_stop": ("C1", "REFUSED"),
        "subverdict": "UNRATABLE",
        "corrupt": True,
        "identity": "PENDING",
        "rows": [
            ("C1", "probe order", "REFUSED"),
            ("C1", "copyin vs verdict consistency", "REFUSED"),
        ],
    },
    "fx07-identity-drift": {
        "rungs": {"GI_game_identity": "REFUSED", "C1_pool_verdict": "PROVEN"},
        "cascade_stop": ("GI", "REFUSED"),
        "subverdict": "ADMITTED-VIA-SPAN",
        "corrupt": False,
        "identity": "REFUSED",
        "insession_commands": 1,
        "rows": [
            ("GI", "eboot sha w0..w3", "REFUSED"),
            ("GI", "content version packed", "REFUSED"),
            ("INSESSION", "insession_*", "PROVEN"),
        ],
        "row_not_contains": [
            ("INSESSION", "insession_*", "not-reached per cascade"),
        ],
        "row_contains": [
            ("GI", "GAME IDENTITY verdict", "IDENTITY DRIFT — HARD STOP"),
        ],
    },
}


def find_row(observables, rung, obs_sub):
    return [r for r in observables if r["rung"] == rung and obs_sub in r["observable"]]


def main():
    fxdir = os.path.abspath(sys.argv[1] if len(sys.argv) > 1
                            else os.path.join(HERE, "fixtures"))
    outroot = os.path.join(HERE, "dryrun-fixtures")
    failures = []
    for name, exp in sorted(E.items()):
        rundir = os.path.join(fxdir, name)
        outdir = os.path.join(outroot, name)
        os.makedirs(outdir, exist_ok=True)
        cp = subprocess.run([sys.executable, V2, "--dir", rundir, "--out", outdir,
                             "--json-only", "--label", "fixture-" + name,
                             "--build-id", "synthetic-v2"],
                            capture_output=True, text=True)
        if cp.returncode != 0:
            failures.append("%s: analyzer rc=%d %s" % (name, cp.returncode, cp.stderr[:200]))
            continue
        R = json.load(open(os.path.join(outdir, "results.json")))
        obs = R["observables"]

        def bad(msg):
            failures.append("%s: %s" % (name, msg))

        for rk, want in exp.get("rungs", {}).items():
            got = R["rungs"].get(rk)
            if got != want:
                bad("rungs.%s = %r, want %r" % (rk, got, want))
        want_cs = exp.get("cascade_stop")
        got_cs = R["cascade_stop"]
        got_cs_t = (got_cs["rung"], got_cs["status"]) if got_cs else None
        if got_cs_t != want_cs:
            bad("cascade_stop = %r, want %r" % (got_cs_t, want_cs))
        if R["pool_c1_subverdict"] != exp["subverdict"]:
            bad("subverdict = %r, want %r" % (R["pool_c1_subverdict"], exp["subverdict"]))
        if bool(R["pool_c1_capture_corrupt"]) != bool(exp["corrupt"]):
            bad("capture_corrupt = %r, want %r" % (R["pool_c1_capture_corrupt"], exp["corrupt"]))
        if R["game_identity"]["status"] != exp["identity"]:
            bad("identity = %r, want %r" % (R["game_identity"]["status"], exp["identity"]))
        if "insession_commands" in exp and R["insession"].get("commands", 0) != exp["insession_commands"]:
            bad("insession.commands = %r, want %r" % (R["insession"].get("commands", 0), exp["insession_commands"]))
        if "load_question" in exp and R["load_question_verdict"] != exp["load_question"]:
            bad("load_question = %r, want %r" % (R["load_question_verdict"], exp["load_question"]))
        for rung, sub, want in exp.get("rows", []):
            hits = find_row(obs, rung, sub)
            if not hits:
                bad("missing row %s/%r" % (rung, sub))
            elif hits[0]["status"] != want:
                bad("row %s/%r status=%r want %r" % (rung, sub, hits[0]["status"], want))
        for rung, sub in exp.get("absent_rows", []):
            if find_row(obs, rung, sub):
                bad("row %s/%r must be ABSENT" % (rung, sub))
        for rung, sub, needle in exp.get("row_contains", []):
            hits = find_row(obs, rung, sub)
            if not hits or needle not in hits[0]["evidence"]:
                bad("row %s/%r evidence must contain %r" % (rung, sub, needle))
        for rung, sub, needle in exp.get("row_not_contains", []):
            hits = find_row(obs, rung, sub)
            if hits and needle in hits[0]["evidence"]:
                bad("row %s/%r evidence must NOT contain %r" % (rung, sub, needle))

    if failures:
        print("FIXTURE FAILURES (%d):" % len(failures))
        for f in failures:
            print("  " + f)
        return 1
    print("ALL %d FIXTURE EXPECTATIONS MET (%s)" % (len(E), ", ".join(sorted(E))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
