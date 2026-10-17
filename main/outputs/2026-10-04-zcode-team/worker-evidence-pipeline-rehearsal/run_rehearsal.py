#!/usr/bin/env python3
"""run_rehearsal.py — run the full evidence chain against each fixture tree.

Chain per scenario (mirrors console_batch.py game-evidence order):
  Step A  post_launch_check.py --from-dir <scn>/from-dir [--json]  (+ --baseline-file once)
  Step B  rung_check.py --from-dir <scn>/from-dir (RUNG1+RUNG2 strict; ALL)
          (+ --discover --from-dir once, s03)
  Step C  runbook CP0-CP7 via console_batch.py's own classifier functions
          (cp_parse_status_s / classify_l_log / classify_d1_log /
           cp01234_verdict / cp56_verdict) imported unmodified.

Every raw tool output is preserved under runs/<scn>/. Combined machine summary:
rehearsal-results.json.
"""
import importlib.util
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TEAM = os.path.dirname(HERE)
PLT = os.path.join(TEAM, "worker-post-launch-tools", "post_launch_check.py")
RNG = os.path.join(TEAM, "worker-rung-evidence-reader", "rung_check.py")
CB = os.path.join(TEAM, "worker-console-batch-runner", "console_batch.py")

SCENARIOS = [
    "s01_full_success",
    "s02_ps_scan_timeout",
    "s03_lease_gate_refusal",
    "s04_script_error_trap",
    "s05_stale_launch",
]


def load_console_batch():
    spec = importlib.util.spec_from_file_location("console_batch", CB)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def run_tool(argv, outfile):
    proc = subprocess.run([sys.executable] + argv, capture_output=True, text=True,
                          timeout=120)
    with open(outfile, "w") as fh:
        fh.write("cmd: %s\nrc: %d\n--- stdout ---\n%s\n--- stderr ---\n%s\n"
                 % (" ".join(argv), proc.returncode, proc.stdout, proc.stderr))
    return proc.returncode, proc.stdout, proc.stderr


def read(p):
    with open(p) as fh:
        return fh.read()


def main():
    cb = load_console_batch()
    os.makedirs(os.path.join(HERE, "runs"), exist_ok=True)
    results = {}

    for scn in SCENARIOS:
        fd = os.path.join(HERE, "fixtures", scn, "from-dir")
        outdir = os.path.join(HERE, "runs", scn)
        os.makedirs(outdir, exist_ok=True)
        r = {"scenario": scn, "from_dir": os.path.relpath(fd, HERE)}

        # --- Step A: post_launch_check (default embedded baseline) ----------
        rc, out, err = run_tool([PLT, "--from-dir", fd, "--json"],
                                os.path.join(outdir, "post_launch_check.json.txt"))
        try:
            pl = json.loads(out)
        except ValueError:
            pl = {"verdict": "PARSE_ERROR", "next": "", "raw": out[:400], "stderr": err[:400]}
        r["post_launch_check"] = {
            "exit": rc, "verdict": pl.get("verdict"), "next": pl.get("next"),
            "probe_repeats": pl.get("probe_repeats"),
            "failure_lines": [f["raw"] for f in pl.get("failure_lines", [])],
            "status_last": (pl.get("status") or {}).get("last"),
            "fresh": (pl.get("status") or {}).get("fresh"),
            "warnings": pl.get("warnings", []),
            "new_lines_count": len(pl.get("new_lines", [])),
        }

        # Step A variant: --baseline-file (identical bytes to the embedded
        # baseline, so the verdict must not change).
        base_file = os.path.join(fd, "baseline.txt")
        if os.path.isfile(base_file):
            rc2, out2, _ = run_tool([PLT, "--from-dir", fd,
                                     "--baseline-file", base_file, "--json"],
                                    os.path.join(outdir, "post_launch_check.baselinefile.json.txt"))
            try:
                pl2 = json.loads(out2)
                r["post_launch_check_baseline_file"] = {
                    "exit": rc2, "verdict": pl2.get("verdict"),
                    "same_verdict": pl2.get("verdict") == pl.get("verdict")}
            except ValueError:
                r["post_launch_check_baseline_file"] = {"exit": rc2, "verdict": "PARSE_ERROR"}

        # --- Step B: rung_check (strict RUNG1+RUNG2, ALL) -------------------
        rc3, out3, err3 = run_tool([RNG, "--from-dir", fd, "--json"],
                                   os.path.join(outdir, "rung_check.json.txt"))
        try:
            rg = json.loads(out3)
        except ValueError:
            rg = {"verdict": "PARSE_ERROR", "rungs": {}, "warnings": [], "stderr": err3[:400]}
        rungs = rg.get("rungs", {})
        r["rung_check"] = {
            "exit": rc3, "verdict": rg.get("verdict"), "next": rg.get("next"),
            "RUNG1": rungs.get("RUNG1", {}).get("verdict"),
            "RUNG1_why": rungs.get("RUNG1", {}).get("why"),
            "RUNG2": rungs.get("RUNG2", {}).get("verdict"),
            "RUNG2_why": rungs.get("RUNG2", {}).get("why"),
            "RUNG3": rungs.get("RUNG3", {}).get("pos_tails"),
            "RUNG4_load_lines": rungs.get("RUNG4", {}).get("load_lines"),
            "RUNG5_spawned": rungs.get("RUNG5", {}).get("spawned"),
            "warnings": [w for w in rg.get("warnings", [])],
            "fetches_ok": {k: v.get("ok") for k, v in rg.get("fetches", {}).items()},
        }

        # Step B variant: --discover (exercised once on s03, C2 absent).
        if scn == "s03_lease_gate_refusal":
            rc4, out4, _ = run_tool([RNG, "--from-dir", fd, "--discover", "--json"],
                                    os.path.join(outdir, "rung_check.discover.json.txt"))
            try:
                disc = json.loads(out4).get("discover", {})
                r["rung_check_discover"] = {
                    "exit": rc4,
                    "data_candidates": disc.get("/data/", {}).get("candidates"),
                    "roots_candidates": disc.get("/data/modbridge/roots/", {}).get("candidates")}
            except ValueError:
                r["rung_check_discover"] = {"exit": rc4, "error": "PARSE_ERROR"}

        # Step B variant: CP7 delegation — rung_check --rung RUNG2 (s01 only,
        # only meaningful with the driver staged).
        if scn == "s01_full_success":
            rc5, out5, _ = run_tool([RNG, "--from-dir", fd, "--rung", "RUNG2", "--json"],
                                    os.path.join(outdir, "rung_check.RUNG2.json.txt"))
            try:
                r["cp7_rung2_check"] = {"exit": rc5,
                                        "verdict": json.loads(out5).get("verdict")}
            except ValueError:
                r["cp7_rung2_check"] = {"exit": rc5, "error": "PARSE_ERROR"}

        # --- Step C: runbook CP checks (console_batch's own classifiers) ----
        S = read(os.path.join(fd, "runtime-status.txt"))
        L = read(os.path.join(fd, "UE4SS.log"))
        D1 = read(os.path.join(fd, "townfall_runtime_aligned_entry.log"))
        s_info = cb.cp_parse_status_s(S)
        l_info = cb.classify_l_log(L)
        d1_info = cb.classify_d1_log(D1)
        overall, per_cp, hint = cb.cp01234_verdict(s_info, l_info, d1_info)
        r["cp_checks"] = {"overall": overall,
                          "per_cp": per_cp, "hint": hint,
                          "l_markers": {k: v for k, v in l_info.items()
                                        if k in ("tick_line", "mod_banners",
                                                 "menu_created", "spawned_line",
                                                 "lua_errors", "entry_failure_lines")},
                          "d1_counts": {"module_start_entered":
                                        d1_info["module_start_entered"],
                                        "runtime_start_failed":
                                        d1_info["runtime_start_failed"]}}

        # CP5/CP6 only exist after the CP1-4 gate passes; rehearse the L-diff
        # classifier on s01's staged re-fetch slices (t1 -> t2 -> t3).
        if scn == "s01_full_success":
            lt1 = read(os.path.join(fd, "l_t1_UE4SS.log"))
            lt2 = read(os.path.join(fd, "l_t2_UE4SS.log"))
            lt3 = read(os.path.join(fd, "l_t3_UE4SS.log"))
            cp5, per56 = cb.cp56_verdict(lt1, lt2, lt3)
            r["cp56_checks"] = {"cp5_pass": cp5, "per_cp": per56}

        results[scn] = r

    with open(os.path.join(HERE, "rehearsal-results.json"), "w") as fh:
        json.dump(results, fh, indent=1)
    print(json.dumps(results, indent=1))


if __name__ == "__main__":
    main()
