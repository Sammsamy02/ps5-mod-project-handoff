#!/usr/bin/env python3
"""run_host_checks.py — worker-fc1-validator-probe (fc1pool1, 2026-10-04).

Builds and runs the fc1pool driver host tests against the REAL vendored
sources (fc1pool_driver.c + record_sink.c + ingame_libc.c + vmmap_walk.c +
sentinels.c + module-independent shims) with a synthetic arena really mapped
at the synthetic addresses, then asserts the produced JSONL and UE4SS.log
line with a strict oracle. stdlib only; no console contact.
Receipt: receipts/host-test-receipt.txt
"""
import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MY = HERE.parent
SRC = MY / "src"
OUT = HERE / "build"
CFG = HERE / "host_config"

CC = os.environ.get("CC", "clang")
WARN = ["-Wall", "-Wextra", "-Werror", "-O1", "-g"]
COMMON_DEFS = [
    '-DIG_BUILD_ID="fc1pool1-host"',
    "-DFC1_RT_GUOBJECT_ARRAY=0x110200000ull",
]
DRIVER_DEFS = COMMON_DEFS + ["-DFC1POOL_HOST_TEST"]

SCENARIOS = {
    # scenario: (verdict, reason_code, polls_expected)
    "main":          ("validated", "none", 1),
    "uninit":        ("refused_after_grace", "pool_uninitialized_after_grace", 8),
    "uncovered":     ("refused", "pool_window_uncovered", 1),
    "none_mismatch": ("refused", "none_ascii_mismatch", 1),
    "blocks0_zero":  ("refused", "blocks0_invalid", 1),
    "map_untrusted": ("refused_after_grace", "map_untrusted", 8),
    "wide":          ("refused", "entry_wide_flag_set", 1),
    "bp_oob":        ("refused", "handle_block_index_oob", 1),
}


def sh(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        print(r.stdout)
        print(r.stderr)
        sys.exit(f"FAILED: {' '.join(str(c) for c in cmd)}")
    return r


def parse_jsonl(path: Path):
    rows = []
    for i, line in enumerate(path.read_text().splitlines()):
        assert len(line.encode()) < 1024, f"{path.name}:{i} line >= 1KiB"
        rows.append(json.loads(line))  # strict: raises on invalid JSON
    return rows


def main() -> None:
    subprocess.run(["rm", "-rf", str(OUT)], check=True)
    (OUT / "tmp").mkdir(parents=True)
    receipt = []

    def log(s):
        print(s)
        receipt.append(s)

    log("== compile (real sources, host shims) ==")
    host_cfg_inc = ["-I", str(CFG), "-I", str(SRC)]
    objs = []
    for f, defs in (
        ("fc1pool_driver.c", DRIVER_DEFS),
        ("record_sink.c", COMMON_DEFS),
        ("ingame_libc.c", COMMON_DEFS),
        ("vmmap_walk.c", COMMON_DEFS),
        ("sentinels.c", COMMON_DEFS),
    ):
        o = OUT / (f.replace(".c", ".o"))
        sh([CC, *WARN, *defs, *host_cfg_inc, "-c", str(SRC / f), "-o", str(o)])
        objs.append(o)
    test_o = OUT / "host_main.o"
    sh([CC, *WARN, *DRIVER_DEFS, *host_cfg_inc, "-c",
        str(HERE / "host_main.c"), "-o", str(test_o)])
    bin_o = OUT / "fc1pool_host_test"
    sh([CC, str(test_o), *[str(o) for o in objs], "-o", str(bin_o)])
    log("compile OK (Wall Wextra Werror clean)")

    failures = []
    for scenario, (verdict, reason, polls) in SCENARIOS.items():
        log(f"== scenario {scenario} ==")
        jsonl = OUT / "tmp" / f"{scenario}.jsonl"
        ue4ss = OUT / "tmp" / f"{scenario}.ue4ss.log"
        r = subprocess.run([str(bin_o), scenario, str(jsonl), str(ue4ss)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            failures.append(f"{scenario}: host binary rc={r.returncode} {r.stderr}")
            continue
        rows = parse_jsonl(jsonl)
        exps = [row.get("exp") for row in rows]
        result = next((x for x in rows if x.get("exp") == "fc1pool_result"), None)
        summary = next((x for x in rows if x.get("exp") == "summary"), None)
        problems = []
        if exps[:2] != ["entry", "startup"]:
            problems.append(f"first exps {exps[:2]}")
        for row in rows:
            if row.get("marker") != "modbridge.ingamefacts" or \
               row.get("schema") != 1 or \
               not str(row.get("build_id", "")).startswith("fc1pool1-"):
                problems.append(f"bad envelope in exp={row.get('exp')}")
        if result is None or summary is None:
            problems.append("missing fc1pool_result or summary")
        else:
            f_ = result["fields"]
            if f_.get("verdict") != verdict:
                problems.append(f"verdict {f_.get('verdict')} != {verdict}")
            if f_.get("reason_code") != reason:
                problems.append(f"reason {f_.get('reason_code')} != {reason}")
            if f_.get("poll") != polls:
                problems.append(f"polls {f_.get('poll')} != {polls}")
            if f_.get("checks_total") != 11:
                problems.append("checks_total != 11")
            if f_.get("ue4ss_log_line_written") != 1:
                problems.append("ue4ss_log_line_written != 1")
            if verdict == "validated":
                if f_.get("fc1pool_validated") != 1 or f_.get("checks_passed") != 11:
                    problems.append("validated flags wrong")
                if f_.get("id_none") != 0:
                    problems.append("id_none != 0")
                # per-entry ASCII evidence lives in the fc1pool_entries record
                entries = next(
                    (x for x in rows if x.get("exp") == "fc1pool_entries"), None)
                if entries is None:
                    problems.append("missing fc1pool_entries")
                else:
                    e_ = entries["fields"]
                    if e_.get("none_ascii_ok") != 1 or \
                       e_.get("byteproperty_ascii_ok") != 1:
                        problems.append("ascii flags wrong")
                    if e_.get("pool_window_prot") != 3:
                        problems.append("pool_window_prot != 3")
                # HOST-derived arithmetic: base = 0x110200000 - 0x93ba110,
                # pool = base + 0x9365ec0. The real-launch prediction
                # (base 0x400000, pool 0x9765ec0) is in the STAGE-PLAN; the
                # synthetic arena cannot sit at 0x97ba110 (macOS arm64 mmap
                # floor), so the host exercises the DERIVATION, divergent-bias
                # reporting included.
                if f_.get("base_derived") != "0x0000000106e45ef0":
                    problems.append(f"base_derived {f_.get('base_derived')}")
                if f_.get("pool_runtime") != "0x00000001101abdb0":
                    problems.append(
                        f"pool_runtime {f_.get('pool_runtime')} != 0x00000001101abdb0 (16-digit hex)")
                if f_.get("base_matches_observed_bias") != 0:
                    problems.append("base bias flag != 0 (synthetic base IS "
                                    "divergent by construction)")
                if f_.get("current_block") != 2:
                    problems.append("current_block != 2")
                if f_.get("byteprop_id_for_find_crosscheck") != f_.get("id_byteproperty"):
                    problems.append("crosscheck id mismatch")
                poll_rows = [x for x in rows if x.get("exp") == "poll"]
                if len(poll_rows) != 1 or poll_rows[0]["fields"]["anchor_rcode"] != 0:
                    problems.append("poll envelope wrong")
        if scenario == "map_untrusted" or scenario == "uninit":
            pass  # grace scenarios: no poll envelope by design
        else:
            poll_rows = [x for x in rows if x.get("exp") == "poll"]
            if len(poll_rows) > 1:
                problems.append("more than one poll envelope")
        ue4ss_lines = ue4ss.read_text().splitlines()
        if len(ue4ss_lines) != 1:
            problems.append(f"ue4ss lines {len(ue4ss_lines)} != 1")
        elif not ue4ss_lines[0].startswith("FC1POOL: build=fc1pool1-") or \
                f" verdict={verdict}" not in ue4ss_lines[0]:
            problems.append(f"ue4ss line shape: {ue4ss_lines[0][:80]}")
        if summary is not None and summary["fields"].get("write_failures") != 0:
            problems.append("write_failures != 0")
        if problems:
            failures.append(f"{scenario}: " + "; ".join(problems))
        else:
            log(f"{scenario}: PASS verdict={verdict} reason={reason}")

    (MY / "receipts").mkdir(exist_ok=True)
    (MY / "receipts/host-test-receipt.txt").write_text(
        "\n".join(receipt) + "\n" +
        ("ALL HOST TESTS PASS" if not failures else
         "FAILURES:\n" + "\n".join(failures)) + "\n")
    if failures:
        print("FAILURES:")
        for f in failures:
            print(" -", f)
        sys.exit(1)
    log("ALL HOST TESTS PASS")


if __name__ == "__main__":
    main()
