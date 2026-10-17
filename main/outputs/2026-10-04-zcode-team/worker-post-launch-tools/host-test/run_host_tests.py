#!/usr/bin/env python3
"""Host tests for post_launch_check.py — fixture-driven, local files only.

Builds one fixture directory per case (UE4SS.log / runtime-status.txt /
townfall_runtime_aligned_entry.log) and runs the script with --from-dir
--json, asserting the verdict, guidance signatures, and exit code.
NO console contact, NO network, NO curl execution (from-dir mode never
spawns subprocesses). Run: python3 run_host_tests.py [receipt-path]
"""

import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, os.pardir, "post_launch_check.py")
FIXTURES = os.path.join(HERE, "fixtures")

BASELINE = [
    "PATH_TEST: /data/plugins/UE4SS.log writable at 8866785633337 monotonic-ns",
    "ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: "
    "6d6f64666f7267652d6c6f61646c6973",
    "DIAG: module_start reached at 1791155755.879 pid=298 build=sdg2-6f404cee6b94 "
    "variant=imp",
]

FRESH_MS = "9123456"    # > 8866785 (path-test monotonic threshold)
STALE_MS = "8000000"    # < threshold

OBS_ENTERED = ("[PPSA33286] modbridge-runtime-aligned-entry/v1 "
               "build=f6530c7905934e5a8cc201aa9400ebb0 "
               "module_start_entered value=0x00000000")
OBS_RETURN = ("[PPSA33286] modbridge-runtime-aligned-entry/v1 "
              "build=f6530c7905934e5a8cc201aa9400ebb0 "
              "module_start_return value=0x00000000")


def status_file(session_ms, states):
    """states: list of (state, code, detail)."""
    out = ["modforge-runtime-status\t1", "session\t299\t%s" % session_ms]
    for i, (state, code, detail) in enumerate(states, 1):
        out.append("state\t%d\t%s\t%s\t%s" % (i, state, code, detail))
    return "\n".join(out) + "\n"


def observer_file(events):
    lines = []
    for tag, val in events:
        lines.append("[PPSA33286] modbridge-runtime-aligned-entry/v1 "
                     "build=f6530c7905934e5a8cc201aa9400ebb0 "
                     "%s value=%s" % (tag, val))
    return "\n".join(lines) + ("\n" if lines else "")


FAIL_NO_LOADLIST = ("modbridge module_start failed: mfa_activate_failed code=-2 "
                    "detail=no-loadlist no loadlist.txt (nothing installed) "
                    "loadlist.txt")
FAIL_OPEN_FILE = ("modbridge module_start failed: mfa_activate_failed code=-6 "
                  "detail=open cannot open listed file "
                  "ue4ss/MemberVariableLayout.ini")
FAIL_SIZE = ("modbridge module_start failed: mfa_activate_failed code=-8 "
             "detail=size file is shorter than the load list says "
             "ue4ss/PS5Pad.ini")
FAIL_CHANNEL = ("modbridge module_start failed: diag_channel_open_failed code=-22 "
                "detail=diagnostic channel unavailable before wiring")
FAIL_BRIDGE_CTX = ("modbridge module_start failed: runtime_start_failed code=2 "
                   "detail=MakeNativeProgramContext refused")

BANNER = [
    "Native console, GUI, crash dumping and filesystem watching are unavailable",
    "UE4SS - v3.0.1 - Git SHA #abcdef1234567890",
    "Setting up mods...",
    "[Lua] [TownfallItemSpawner] Item Spawner loaded",
]

CASES = []


def case(name):
    def deco(fn):
        CASES.append((name, fn))
        return fn
    return deco


def build(name, ue4ss=None, status=None, observer=None, keep=None):
    """keep: list of local file names to leave ABSENT (default: all written)."""
    d = os.path.join(FIXTURES, name)
    os.makedirs(d, exist_ok=True)
    files = {"UE4SS.log": ue4ss, "runtime-status.txt": status,
             "townfall_runtime_aligned_entry.log": observer}
    for fname, content in files.items():
        if content is None:
            continue
        with open(os.path.join(d, fname), "w") as fh:
            fh.write(content)
    return d


def run(d, extra=()):
    return subprocess.run(
        [sys.executable, SCRIPT, "--from-dir", d, "--json"] + list(extra),
        capture_output=True, text=True, timeout=60)


@case("01_not_started_exact_baseline")
def _(checks):
    d = build("01_not_started_exact_baseline",
              ue4ss="\n".join(BASELINE) + "\n",
              observer=observer_file([("module_start_entered", "0x00000000")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(p.returncode == 0, "exit 0 (got %d)" % p.returncode)
    checks(r["verdict"] == "NOT_STARTED", "verdict NOT_STARTED (got %s)" % r["verdict"])
    checks(r["probe_repeats"] == 0, "no probe repeats")
    checks(r["new_lines"] == [], "no new lines")
    checks("60 s" in r["next"], "retry-window guidance present")


@case("02_not_started_probe_repeats")
def _(checks):
    ue = BASELINE + [
        "PATH_TEST: /data/plugins/UE4SS.log writable at 9900000000000 monotonic-ns",
        "ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: deadbeefdeadbeefdeadbeefdeadbeef",
        "DIAG: module_start reached at 1791159999.999 pid=301 build=sdg2-6f404cee6b94 variant=imp",
    ]
    d = build("02_not_started_probe_repeats", ue4ss="\n".join(ue) + "\n")
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "NOT_STARTED", "stale repeats are NOT_STARTED (got %s)" % r["verdict"])
    checks(r["probe_repeats"] == 3, "3 repeats counted (got %d)" % r["probe_repeats"])
    checks(r["new_lines"] == [], "repeats never become new lines")
    checks("stale probe repeat" in r["next"], "guidance mentions repeats")


@case("03_failed_mfa_no_loadlist")
def _(checks):
    d = build("03_failed_mfa_no_loadlist",
              ue4ss="\n".join(BASELINE + [FAIL_NO_LOADLIST]) + "\n",
              status=status_file(FRESH_MS, [("failed", -2,
                                             "mfa_activate_failed no loadlist.txt "
                                             "(nothing installed) loadlist.txt")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "FAILED_mfa_activate_failed",
           "verdict FAILED_mfa_activate_failed (got %s)" % r["verdict"])
    checks("/data timing window" in r["next"], "code=-2 timing-window guidance")
    checks("len(failure_lines) == 1" if False else r["failure_lines"][0]["code"] == -2,
           "code parsed as -2")
    checks(r["failure_lines"][0]["stage"] == "mfa_activate_failed", "stage parsed")
    checks(any("sinks agree" in d for d in r["detail_lines"]),
           "status-failed agreement noted")


@case("04_failed_multiple_lines")
def _(checks):
    d = build("04_failed_multiple_lines",
              ue4ss="\n".join(BASELINE + [FAIL_NO_LOADLIST, FAIL_OPEN_FILE,
                                          FAIL_SIZE]) + "\n")
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "FAILED_mfa_activate_failed",
           "verdict from LAST line (got %s)" % r["verdict"])
    checks(len(r["failure_lines"]) == 3, "3 failure lines parsed")
    checks([f["code"] for f in r["failure_lines"]] == [-2, -6, -8],
           "codes parsed in order")
    checks("3 failure lines accumulated" in " ".join(r["detail_lines"]),
           "cumulative-count note (O5)")
    checks("Size drift" in r["next"], "last-line (-8) guidance wins")
    checks(r["fetches"]["runtime_status"]["ok"] is False, "status absent tolerated")


@case("05_failed_channel_open")
def _(checks):
    d = build("05_failed_channel_open",
              ue4ss="\n".join(BASELINE + [FAIL_CHANNEL]) + "\n")
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "FAILED_diag_channel_open_failed",
           "verdict FAILED_diag_channel_open_failed (got %s)" % r["verdict"])
    checks("postmortem" in r["next"] and "-22" in r["next"],
           "channel guidance cites postmortem -22")
    checks("SILENT" in r["next"], "notes delivered entry is silent on -22")


@case("06_failed_bridge_context")
def _(checks):
    d = build("06_failed_bridge_context",
              ue4ss="\n".join(BASELINE + [FAIL_BRIDGE_CTX]) + "\n",
              status=status_file(FRESH_MS, [("failed", 2,
                                             FAIL_BRIDGE_CTX.split("detail=")[1])]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "FAILED_runtime_start_failed",
           "verdict FAILED_runtime_start_failed (got %s)" % r["verdict"])
    checks("CONTEXT" in r["next"], "bridge code=2 -> CONTEXT guidance")
    checks("modbridge_supplier_composition.prx" in r["next"],
           "names the PRX path check")


@case("07_status_failed_only_fresh")
def _(checks):
    d = build("07_status_failed_only_fresh",
              ue4ss="\n".join(BASELINE) + "\n",
              status=status_file(FRESH_MS, [("failed", -2, "mfa_activate_failed")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "FAILED_STATUS",
           "verdict FAILED_STATUS (got %s)" % r["verdict"])
    checks("F6/F7" in r["next"], "silent-trap shape check present")
    checks("/data timing window" in r["next"], "code=-2 guidance via status")


@case("08_status_failed_only_bridge_code")
def _(checks):
    d = build("08_status_failed_only_bridge_code",
              status=status_file(FRESH_MS, [("failed", 3, "prepare threw: boom")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "FAILED_STATUS", "verdict FAILED_STATUS")
    checks("Prepare/start threw" in r["next"] or "prepare/start" in r["next"].lower(),
           "bridge code=3 START guidance via stage hint")


@case("09_activated")
def _(checks):
    d = build("09_activated",
              ue4ss="\n".join(BASELINE) + "\n",
              status=status_file(FRESH_MS, [("activated", 0, "")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "ACTIVATED", "verdict ACTIVATED (got %s)" % r["verdict"])
    checks("60 s" in r["next"], "re-run guidance")


@case("10_engine_ready")
def _(checks):
    d = build("10_engine_ready",
              status=status_file(FRESH_MS, [("activated", 0, ""),
                                            ("engine-ready", 0, "")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "ENGINE_READY", "verdict ENGINE_READY (got %s)" % r["verdict"])
    checks(r["status"]["fresh"] is True, "session flagged fresh")
    checks("F8" in r["next"], "frozen-hang (F8) note")


@case("11_mods_started_with_new_lines")
def _(checks):
    d = build("11_mods_started_with_new_lines",
              ue4ss="\n".join(BASELINE + BANNER) + "\n",
              status=status_file(FRESH_MS, [("activated", 0, ""),
                                            ("engine-ready", 0, ""),
                                            ("mods-started", 0, "")]),
              observer=observer_file([("module_start_entered", "0x00000000"),
                                      ("mfa_activated_low", "0x00008c39"),
                                      ("runtime_start_ok", "0x00000000"),
                                      ("module_start_return", "0x00000000")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "MODS_STARTED", "verdict MODS_STARTED (got %s)" % r["verdict"])
    checks(len(r["new_lines"]) == 4, "4 new lines listed (got %d)" % len(r["new_lines"]))
    checks("1 found in new lines" in r["next"], "mod print detected for rung 4")
    checks(r["observer"]["interesting"]["runtime_start_ok"]["last"] == "0x00000000",
          "observer runtime_start_ok extracted")
    checks(r["observer"]["interesting"]["mfa_activated_low"]["last"] == "0x00008c39",
           "observer mfa_activated_low extracted (35897 = 0x8c39)")


@case("12_unknown_new_lines")
def _(checks):
    weird = "SOMETHING UNEXPECTED happened in the log"
    d = build("12_unknown_new_lines",
              ue4ss="\n".join(BASELINE + [weird]) + "\n",
              status=status_file(STALE_MS, [("activated", 0, "")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "UNKNOWN_NEW_LINES",
           "verdict UNKNOWN_NEW_LINES (got %s)" % r["verdict"])
    checks(weird in "\n".join(r["new_lines"]), "raw unknown line preserved")


@case("13_fetch_failed_retry")
def _(checks):
    d = os.path.join(FIXTURES, "13_fetch_failed_retry")
    os.makedirs(d, exist_ok=True)  # no files at all => all fetches fail
    p = run(d)
    r = json.loads(p.stdout)
    checks(p.returncode == 4, "exit 4 on total fetch failure (got %d)" % p.returncode)
    checks(r["verdict"] == "FETCH_FAILED", "verdict FETCH_FAILED")
    checks("RETRY" in r["next"], "RETRY guidance, no crash")
    ph = subprocess.run([sys.executable, SCRIPT, "--from-dir", d],
                        capture_output=True, text=True, timeout=60)
    checks(ph.returncode == 4, "human mode also exit 4 (got %d)" % ph.returncode)
    checks("VERDICT: FETCH_FAILED" in ph.stdout and "Traceback" not in ph.stderr,
           "human mode renders without crash")


@case("14_partial_fetch_status_only")
def _(checks):
    d = build("14_partial_fetch_status_only",
              status=status_file(FRESH_MS, [("activated", 0, ""),
                                            ("engine-ready", 0, ""),
                                            ("mods-started", 0, "")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(p.returncode == 0, "exit 0 (got %d)" % p.returncode)
    checks(r["verdict"] == "MODS_STARTED",
           "status alone still classifies (got %s)" % r["verdict"])
    checks(r["fetches"]["ue4ss_log"]["ok"] is False, "missing log flagged")


@case("15_stale_status_not_started")
def _(checks):
    d = build("15_stale_status_not_started",
              ue4ss="\n".join(BASELINE) + "\n",
              status=status_file(STALE_MS, [("activated", 0, "")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "NOT_STARTED",
           "stale session is not progress (got %s)" % r["verdict"])
    checks("stale" in r["next"].lower(), "stale-session guidance")
    checks(r["status"]["fresh"] is False, "freshness oracle flags stale")


@case("16_conflict_fresh_progress_vs_failure_lines")
def _(checks):
    d = build("16_conflict_fresh_progress_vs_failure_lines",
              ue4ss="\n".join(BASELINE + [FAIL_NO_LOADLIST]) + "\n",
              status=status_file(FRESH_MS, [("activated", 0, ""),
                                            ("engine-ready", 0, ""),
                                            ("mods-started", 0, "")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "MODS_STARTED",
           "fresh session outranks old failure lines (got %s)" % r["verdict"])
    checks(any(w.startswith("CONFLICT") for w in r["warnings"]),
           "CONFLICT warning raised")


@case("17_fresh_stopped")
def _(checks):
    d = build("17_fresh_stopped",
              status=status_file(FRESH_MS, [("activated", 0, ""),
                                            ("engine-ready", 0, ""),
                                            ("mods-started", 0, ""),
                                            ("stopped", 0, "")]))
    p = run(d)
    r = json.loads(p.stdout)
    checks(r["verdict"] == "STOPPED", "verdict STOPPED (got %s)" % r["verdict"])
    checks("runtime_stop_result" in r["next"], "F10 observer guidance")


@case("18_baseline_file_override")
def _(checks):
    custom = os.path.join(FIXTURES, "custom-baseline.txt")
    with open(custom, "w") as fh:
        fh.write("CUSTOM_BASELINE_LINE_ONE\n")
    d = build("18_baseline_file_override",
              ue4ss="CUSTOM_BASELINE_LINE_ONE\n"
                    "PATH_TEST: /data/plugins/UE4SS.log writable at 1 monotonic-ns\n"
                    "A GENUINELY NEW LINE\n")
    p = run(d, ["--baseline-file", custom])
    r = json.loads(p.stdout)
    checks(r["verdict"] == "UNKNOWN_NEW_LINES",
           "override baseline respected (got %s)" % r["verdict"])
    checks(r["new_lines"] == ["A GENUINELY NEW LINE"],
           "only the true newcomer is new (probe prefix still absorbed)")
    checks(r["baseline_file"] == custom, "baseline provenance recorded")


@case("19_empty_log_and_status")
def _(checks):
    d = build("19_empty_log_and_status", ue4ss="", status="")
    p = run(d)
    r = json.loads(p.stdout)
    checks(p.returncode == 0, "exit 0, empty fetches tolerated (got %d)" % p.returncode)
    checks(r["verdict"] == "NOT_STARTED", "empty => NOT_STARTED (got %s)" % r["verdict"])
    checks(any("absent from fetched" in w for w in r["warnings"]),
           "missing-baseline warning on empty log")


@case("20_json_and_dry_run")
def _(checks):
    p = subprocess.run([sys.executable, SCRIPT, "--dry-run"],
                       capture_output=True, text=True, timeout=30)
    checks(p.returncode == 0, "dry-run exit 0")
    checks("ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log" in p.stdout,
           "dry-run prints exact UE4SS.log curl URL")
    checks(p.stdout.count("ftp://ps5-host-not-configured.invalid:2121") == 3,
           "one fresh URL per evidence file")
    checks("--max-time 60" in p.stdout and "--max-filesize" in p.stdout,
           "bounded timeouts present")
    # baseline sha self-check against CLAIM.json
    claim_path = os.path.join(HERE, os.pardir, "CLAIM.json")
    with open(claim_path) as fh:
        claim = json.load(fh)
    shas = claim["claims"]["baseline_three_lines"]["shas"]
    import hashlib
    checks(hashlib.sha256(BASELINE[0].encode()).hexdigest() == shas["line1"],
           "line1 sha matches CLAIM")
    checks(hashlib.sha256(BASELINE[1].encode()).hexdigest() == shas["line2"],
           "line2 sha matches CLAIM")
    checks(hashlib.sha256(BASELINE[2].encode()).hexdigest() == shas["line3"],
           "line3 sha matches CLAIM")


def main():
    os.makedirs(FIXTURES, exist_ok=True)
    passed = failed = 0
    lines = ["HOST TEST RECEIPT — post_launch_check.py",
             "runner: host-test/run_host_tests.py (local fixtures, --from-dir "
             "mode only; no console contact, no network, no curl execution)",
             ""]
    for name, fn in CASES:
        checks = []
        fn(lambda ok, msg: checks.append((ok, msg)))
        case_failed = [m for ok, m in checks if not ok]
        if case_failed:
            failed += 1
            lines.append("FAIL %s (%d/%d checks)" % (name, len(checks) - len(case_failed),
                                                     len(checks)))
            for m in case_failed:
                lines.append("  - %s" % m)
        else:
            passed += 1
            lines.append("PASS %s (%d/%d checks)" % (name, len(checks), len(checks)))
    lines.append("")
    lines.append("SUMMARY: %d/%d cases passed (%d failed)" %
                 (passed, passed + failed, failed))
    print("\n".join(lines))
    receipt = os.path.join(HERE, os.pardir, "receipts")
    os.makedirs(receipt, exist_ok=True)
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        receipt, "host-test-pass.txt")
    with open(out, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
