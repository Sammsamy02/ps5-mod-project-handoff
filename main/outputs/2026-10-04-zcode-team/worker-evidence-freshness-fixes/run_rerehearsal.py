#!/usr/bin/env python3
"""run_rerehearsal.py — worker-evidence-freshness-fixes re-run of the
worker-evidence-pipeline-rehearsal against the FIXED tools.

Differences from the original run_rehearsal.py (which is left byte-preserved
in the rehearsal worker's directory, along with its original results):
  * fixtures are read from the ORIGINAL rehearsal fixtures (unmodified);
  * all outputs land in worker-evidence-freshness-fixes/runs/ and
    rerehearsal-results.json (never overwriting the original worker's);
  * Step A/B run the FIXED post_launch_check.py / rung_check.py in place;
  * Step C runs console_batch's cp01234_verdict TWICE:
      - cp_checks_asfound: the in-tree console_batch.py bytes as found
        (owned mid-flight by worker-batch-runner-8fam-patch; NOT edited here);
      - cp_checks_freshness_patch: the worker-evidence-freshness-fixes shadow
        copy with the delivered console_batch.freshness-gate.patch applied
        (the exact bytes the owner is asked to produce);
  * adds the probe_gap4_stale_driver rung_check proof (GAP 4 refusal).
"""
import importlib.util
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TEAM = os.path.dirname(HERE)
REH = os.path.join(TEAM, "worker-evidence-pipeline-rehearsal")
FIX = os.path.join(REH, "fixtures")
PLT = os.path.join(TEAM, "worker-post-launch-tools", "post_launch_check.py")
RNG = os.path.join(TEAM, "worker-rung-evidence-reader", "rung_check.py")
CB = os.path.join(TEAM, "worker-console-batch-runner", "console_batch.py")
CB_SHADOW = os.path.join(HERE, "shadow", "console_batch.py")

SCENARIOS = [
    "s01_full_success",
    "s02_ps_scan_timeout",
    "s03_lease_gate_refusal",
    "s04_script_error_trap",
    "s05_stale_launch",
]


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
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


def step_c(cb, fd):
    S = read(os.path.join(fd, "runtime-status.txt"))
    L = read(os.path.join(fd, "UE4SS.log"))
    D1 = read(os.path.join(fd, "townfall_runtime_aligned_entry.log"))
    s_info = cb.cp_parse_status_s(S)
    l_info = cb.classify_l_log(L)
    d1_info = cb.classify_d1_log(D1)
    overall, per_cp, hint = cb.cp01234_verdict(s_info, l_info, d1_info)
    return {"overall": overall, "per_cp": per_cp, "hint": hint,
            "d1_counts": {"module_start_entered": d1_info["module_start_entered"],
                          "runtime_start_failed": d1_info["runtime_start_failed"]}}


def main():
    cb_found = load_module("console_batch_asfound", CB)
    cb_patch = load_module("console_batch_freshness_patch", CB_SHADOW)
    outdir_root = os.path.join(HERE, "runs")
    os.makedirs(outdir_root, exist_ok=True)
    results = {"meta": {
        "runner": "worker-evidence-freshness-fixes/run_rerehearsal.py",
        "fixtures": os.path.relpath(FIX, TEAM),
        "post_launch_check": PLT,
        "rung_check": RNG,
        "console_batch_asfound_sha12": None,
        "console_batch_freshness_patch": os.path.relpath(CB_SHADOW, TEAM),
        "note": "console_batch.py was owned mid-flight by "
                "worker-batch-runner-8fam-patch while the patch was prepared "
                "(as-found snapshot sha256 prefix c691cdd4cb8b; their delivered "
                "bytes 8c3031ebf34c). After their terminal REPORT.md appeared, "
                "the freshness-gate patch was applied IN PLACE per dispatch "
                "(result sha256 92b56889cbfe..., selftest 162 ok / 0 failed). "
                "cp_checks_asfound therefore shows the FIXED in-tree bytes; "
                "cp_checks_freshness_patch is the shadow reference copy — "
                "they must agree.",
    }}

    for scn in SCENARIOS + ["probe_gap4_stale_driver"]:
        flat = os.path.join(FIX, scn)
        fd = os.path.join(flat, "from-dir") if os.path.isdir(
            os.path.join(flat, "from-dir")) else flat
        outdir = os.path.join(outdir_root, scn)
        os.makedirs(outdir, exist_ok=True)
        r = {"scenario": scn, "from_dir": os.path.relpath(fd, TEAM)}

        # --- Step A: post_launch_check (FIXED in place: GAP 2 + GAP 3) ------
        rc, out, err = run_tool([PLT, "--from-dir", fd, "--json"],
                                os.path.join(outdir, "post_launch_check.json.txt"))
        try:
            pl = json.loads(out)
        except ValueError:
            pl = {"verdict": "PARSE_ERROR", "next": "", "raw": out[:400]}
        r["post_launch_check"] = {
            "exit": rc, "verdict": pl.get("verdict"), "next": pl.get("next"),
            "probe_repeats": pl.get("probe_repeats"),
            "status_last": (pl.get("status") or {}).get("last"),
            "fresh": (pl.get("status") or {}).get("fresh"),
        }

        # Step A variant: --baseline-file (same bytes as embedded baseline;
        # verdict must not change) — kept from the original rehearsal runner.
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

        # --- Step B: rung_check (FIXED in place: GAP 4 O2 gate) -------------
        rc3, out3, err3 = run_tool([RNG, "--from-dir", fd, "--json"],
                                   os.path.join(outdir, "rung_check.json.txt"))
        try:
            rg = json.loads(out3)
        except ValueError:
            rg = {"verdict": "PARSE_ERROR", "rungs": {}, "stderr": err3[:400]}
        rungs = rg.get("rungs", {})
        r1 = rungs.get("RUNG1", {})
        r["rung_check"] = {
            "exit": rc3, "verdict": rg.get("verdict"), "next": rg.get("next"),
            "RUNG1": r1.get("verdict"), "RUNG1_why": r1.get("why"),
            "RUNG2": rungs.get("RUNG2", {}).get("verdict"),
            "status_session_fresh_cond": next(
                (c for c in r1.get("conditions", [])
                 if c["id"] == "status_session_fresh"), None),
            "warnings": [w[:80] for w in rg.get("warnings", [])],
        }

        # --- Step C: runbook CP checks, as-found vs freshness-patched -------
        if scn != "probe_gap4_stale_driver":
            r["cp_checks_asfound"] = step_c(cb_found, fd)
            r["cp_checks_freshness_patch"] = step_c(cb_patch, fd)

        # CP5/CP6 L-diff classifier on s01 staged slices (unchanged tool).
        if scn == "s01_full_success":
            lt1 = read(os.path.join(fd, "l_t1_UE4SS.log"))
            lt2 = read(os.path.join(fd, "l_t2_UE4SS.log"))
            lt3 = read(os.path.join(fd, "l_t3_UE4SS.log"))
            cp5, per56 = cb_found.cp56_verdict(lt1, lt2, lt3)
            r["cp56_checks"] = {"cp5_pass": cp5, "per_cp": per56}

        # --discover (s03) and CP7 delegation (--rung RUNG2, s01) variants.
        if scn == "s03_lease_gate_refusal":
            rc4, out4, _ = run_tool([RNG, "--from-dir", fd, "--discover", "--json"],
                                    os.path.join(outdir, "rung_check.discover.json.txt"))
            try:
                disc = json.loads(out4).get("discover", {})
                r["rung_check_discover"] = {
                    "exit": rc4,
                    "data_candidates": disc.get("/data/", {}).get("candidates")}
            except ValueError:
                r["rung_check_discover"] = {"exit": rc4, "error": "PARSE_ERROR"}
        if scn == "s01_full_success":
            rc5, out5, _ = run_tool([RNG, "--from-dir", fd, "--rung", "RUNG2", "--json"],
                                    os.path.join(outdir, "rung_check.RUNG2.json.txt"))
            try:
                r["cp7_rung2_check"] = {"exit": rc5,
                                        "verdict": json.loads(out5).get("verdict")}
            except ValueError:
                r["cp7_rung2_check"] = {"exit": rc5, "error": "PARSE_ERROR"}

        results[scn] = r

    import hashlib
    with open(CB, "rb") as fh:
        results["meta"]["console_batch_asfound_sha12"] = \
            hashlib.sha256(fh.read()).hexdigest()[:12]

    with open(os.path.join(HERE, "rerehearsal-results.json"), "w") as fh:
        json.dump(results, fh, indent=1)
    print(json.dumps(results, indent=1))


if __name__ == "__main__":
    main()
