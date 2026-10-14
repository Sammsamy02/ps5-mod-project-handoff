#!/usr/bin/env python3
"""run_rerun_final.py — final-tools re-rehearsal (worker-evidence-tools-final).

Same chain as worker-evidence-pipeline-rehearsal/run_rehearsal.py (post_launch_check
--from-dir --json [+ --baseline-file], rung_check --from-dir --json [+ --discover on
s03, + --rung RUNG2 on s01], console_batch's own classifiers for CP0-CP4/CP5-6),
driven against TWO fixture trees with the FINAL tool bytes:

  tree A  fixtures-corrected/  — build_fixtures_corrected.py output (tick address
          0x33ac8c0 per CORRECTION-REGISTER A5) + verbatim probe_gap4_stale_driver
  tree B  ../worker-evidence-pipeline-rehearsal/fixtures (committed, byte-preserved;
          s01/s04 still carry the pre-correction 0x37ac8c0 tick line — CP2 is the
          documented PARTIAL(G7) delta, never a PASS)

Plus the seeds_refusal_launch9 fixture (launch-9 expected terminal T-A) through all
three tools, and both freshness refusals (s05 stale launch; probe_gap4_stale_driver
rung exit 5). Writes ONLY under worker-evidence-tools-final/.
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

TREES = {
    "corrected": os.path.join(HERE, "fixtures-corrected"),
    "committed": os.path.join(TEAM, "worker-evidence-pipeline-rehearsal", "fixtures"),
}
SCENARIOS = [
    "s01_full_success",
    "s02_ps_scan_timeout",
    "s03_lease_gate_refusal",
    "s04_script_error_trap",
    "s05_stale_launch",
    "probe_gap4_stale_driver",
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


def scenario(fd, outdir, cb):
    r = {}
    rc, out, err = run_tool([PLT, "--from-dir", fd, "--json"],
                            os.path.join(outdir, "post_launch_check.json.txt"))
    try:
        pl = json.loads(out)
    except ValueError:
        pl = {"verdict": "PARSE_ERROR", "stderr": err[:400]}
    r["post_launch_check"] = {
        "exit": rc, "verdict": pl.get("verdict"), "next": (pl.get("next") or "")[:80],
        "probe_repeats": pl.get("probe_repeats"),
        "failure_lines": [f["raw"] for f in pl.get("failure_lines", [])],
        "status_last": (pl.get("status") or {}).get("last") and
                       (pl.get("status") or {}).get("last", {}).get("state"),
        "fresh": (pl.get("status") or {}).get("fresh"),
        "new_lines_count": len(pl.get("new_lines", [])),
    }
    base_file = os.path.join(fd, "baseline.txt")
    if os.path.isfile(base_file):
        rc2, out2, _ = run_tool([PLT, "--from-dir", fd, "--baseline-file",
                                 base_file, "--json"],
                                os.path.join(outdir, "post_launch_check.baselinefile.json.txt"))
        try:
            r["post_launch_check_baseline_file"] = {
                "exit": rc2, "verdict": json.loads(out2).get("verdict")}
        except ValueError:
            r["post_launch_check_baseline_file"] = {"exit": rc2, "verdict": "PARSE_ERROR"}

    rc3, out3, err3 = run_tool([RNG, "--from-dir", fd, "--json"],
                               os.path.join(outdir, "rung_check.json.txt"))
    try:
        rg = json.loads(out3)
    except ValueError:
        rg = {"verdict": "PARSE_ERROR", "rungs": {}, "stderr": err3[:400]}
    rungs = rg.get("rungs", {})
    r["rung_check"] = {
        "exit": rc3, "verdict": rg.get("verdict"),
        "RUNG1": rungs.get("RUNG1", {}).get("verdict"),
        "RUNG2": rungs.get("RUNG2", {}).get("verdict"),
        "RUNG4_load_lines": rungs.get("RUNG4", {}).get("load_lines"),
        "RUNG5_spawned": rungs.get("RUNG5", {}).get("spawned"),
        "warnings": [w[:60] for w in rg.get("warnings", [])],
    }
    if os.path.basename(os.path.dirname(fd)).startswith("s03"):
        rc4, out4, _ = run_tool([RNG, "--from-dir", fd, "--discover", "--json"],
                                os.path.join(outdir, "rung_check.discover.json.txt"))
        try:
            disc = json.loads(out4).get("discover", {})
            r["rung_check_discover"] = {"exit": rc4,
                                        "data_candidates": disc.get("/data/", {}).get("candidates")}
        except ValueError:
            r["rung_check_discover"] = {"exit": rc4, "error": "PARSE_ERROR"}
    if os.path.basename(os.path.dirname(fd)) == "s01_full_success":
        rc5, out5, _ = run_tool([RNG, "--from-dir", fd, "--rung", "RUNG2", "--json"],
                                os.path.join(outdir, "rung_check.RUNG2.json.txt"))
        try:
            r["cp7_rung2_check"] = {"exit": rc5, "verdict": json.loads(out5).get("verdict")}
        except ValueError:
            r["cp7_rung2_check"] = {"exit": rc5, "error": "PARSE_ERROR"}

    S = read(os.path.join(fd, "runtime-status.txt"))
    L = read(os.path.join(fd, "UE4SS.log"))
    D1 = read(os.path.join(fd, "townfall_runtime_aligned_entry.log"))
    s_info = cb.cp_parse_status_s(S)
    l_info = cb.classify_l_log(L)
    d1_info = cb.classify_d1_log(D1)
    overall, per_cp, hint = cb.cp01234_verdict(s_info, l_info, d1_info)
    r["cp_checks"] = {"overall": overall, "per_cp": per_cp, "hint": hint[:100],
                      "entry_failure_lines": l_info["entry_failure_lines"]}
    if os.path.basename(os.path.dirname(fd)) == "s01_full_success":
        lt1 = read(os.path.join(fd, "l_t1_UE4SS.log"))
        lt2 = read(os.path.join(fd, "l_t2_UE4SS.log"))
        lt3 = read(os.path.join(fd, "l_t3_UE4SS.log"))
        cp5, per56 = cb.cp56_verdict(lt1, lt2, lt3)
        r["cp56_checks"] = {"cp5_pass": cp5, "per_cp": per56}
    return r


def main():
    cb = load_console_batch()
    results = {}
    for tree_name, tree in TREES.items():
        outroot = os.path.join(HERE, "runs", "rerun-" + tree_name)
        os.makedirs(outroot, exist_ok=True)
        results[tree_name] = {}
        for scn in SCENARIOS:
            fd = os.path.join(tree, scn, "from-dir") if \
                os.path.isdir(os.path.join(tree, scn, "from-dir")) \
                else os.path.join(tree, scn)
            if not os.path.isdir(fd):
                results[tree_name][scn] = "MISSING"
                continue
            outdir = os.path.join(outroot, scn)
            os.makedirs(outdir, exist_ok=True)
            results[tree_name][scn] = scenario(fd, outdir, cb)
    with open(os.path.join(HERE, "rerun-results.json"), "w") as fh:
        json.dump(results, fh, indent=1)
    print(json.dumps(results, indent=1))


if __name__ == "__main__":
    main()
