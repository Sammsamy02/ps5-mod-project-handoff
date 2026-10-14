#!/usr/bin/env python3
"""run_host_checks.py — worker-carrier-probe-build (cc1, 2026-10-04).

Builds and runs the cc1 carrier-census driver host tests against the REAL
sources (carrier_driver.c + record_sink.c + ingame_libc.c + vmmap_walk.c +
sentinels.c + module-independent shims) with a synthetic arena really mapped
at the synthetic addresses, then asserts the produced JSONL and UE4SS.log
line with a STRICT oracle: every line must parse as JSON (raises on invalid),
be < 1 KiB, carry the lineage envelope, and match the scenario's expected
verdict / reason / counters. One scenario per refusal class of
carrier_driver.h + the pass paths (main, negative_clean, wide_only, budget).
stdlib only; no console contact.
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
    '-DIG_BUILD_ID="cc1-host"',
    "-DCC1_RT_GUOBJECT_ARRAY=0x110200000ull",
]
DRIVER_DEFS = COMMON_DEFS + ["-DCC1_HOST_TEST"]
BUDGET_DEFS = DRIVER_DEFS + ["-DCC1_CENSUS_BUDGET_ENTRIES=64ull"]

SOURCES = ("carrier_driver.c", "record_sink.c", "ingame_libc.c",
           "vmmap_walk.c", "sentinels.c")

# scenario: dict of expected census_result/summary facts.
# verdict/reason + gates + census counters. "entries" is a (min,max) RANGE:
# the Len-driven entry stride skips string bodies, so the visited-entry count
# is name-length-dependent (census_progress_markers must equal entries//4096,
# asserted from the result's own value — self-consistency).
MAIN_MATCH_IDS = {
    "None": 0, "ByteProperty": 0x40, "ReceiveTick": 0x200, "Tick": 0x400,
    "BlueprintUpdateAnimation": 0x600, "ReceiveBeginPlay": 0xA00,
    "Actor": 0xC00, "Pawn": 0xE00, "ItemDatabaseSubsystem": 0x1000,
    "GameViewportClient": 0x10000,
}
B0_MATCH_IDS = {k: v for k, v in MAIN_MATCH_IDS.items() if v < 0x10000}
S = dict
SCENARIOS = {
    # ---- pass paths ----
    "main": S(verdict="CENSUS:VALIDATED", reason="none", matched=10,
              neg_post=1, neg_zzz=0, truncated=0, wide=0,
              entries=(130900, 131072), none_id_zero=1, bp_ok=1, admitted=30,
              match_ids=MAIN_MATCH_IDS, census_poll=0),
    "negative_clean": S(verdict="CENSUS:VALIDATED", reason="none", matched=2,
                        neg_post=0, neg_zzz=0, truncated=0, wide=0,
                        entries=(130900, 131072), none_id_zero=1, bp_ok=1,
                        admitted=30,
                        match_ids={"None": 0, "ByteProperty": 0x40},
                        census_poll=0),
    "wide_only": S(verdict="CENSUS:VALIDATED", reason="none", matched=9,
                   neg_post=1, neg_zzz=0, truncated=0, wide=1,
                   entries=(130900, 131072), none_id_zero=1, bp_ok=1,
                   admitted=30,
                   match_ids={k: v for k, v in MAIN_MATCH_IDS.items()
                              if k != "ReceiveTick"},
                   census_poll=0),
    "budget": S(verdict="CENSUS:VALIDATED", reason="none", matched=2,
                neg_post=0, neg_zzz=0, truncated=1, wide=0, entries=(64, 64),
                none_id_zero=1, bp_ok=1, admitted=30,
                match_ids={"None": 0, "ByteProperty": 0x40},
                census_poll=0),
    # ---- grace refusals (census_done 0) ----
    "map_untrusted": S(verdict="CENSUS:REFUSED", reason="map_untrusted",
                       matched=0, census_done=0, admitted=0, counters=0,
                       entries=(0, 0)),
    "anchor_refused": S(verdict="CENSUS:REFUSED", reason="anchor_not_admitted",
                        matched=0, census_done=0, admitted=0, counters=0,
                        bindings_absent=30, rcode=6, entries=(0, 0)),
    "gate_threshold": S(verdict="CENSUS:REFUSED",
                        reason="census_gate_closed_after_grace", matched=0,
                        census_done=0, admitted=30, counters=0,
                        gate_closed=30, num_elements=5000, entries=(0, 0)),
    "uninit": S(verdict="CENSUS:REFUSED",
                reason="pool_uninitialized_after_grace", matched=0,
                census_done=0, admitted=30, counters=0, gate_closed_phase=30,
                entries=(0, 0)),
    # ---- hard refusals (census_done 1, validated 0) ----
    "uncovered": S(verdict="CENSUS:REFUSED", reason="pool_window_uncovered",
                   matched=0, census_done=1, admitted=30, counters=0,
                   entries=(0, 0)),
    "pool_unreadable": S(verdict="CENSUS:REFUSED",
                         reason="pool_window_not_readable", matched=0,
                         census_done=1, admitted=30, counters=0, entries=(0, 0)),
    "cb_insane": S(verdict="CENSUS:REFUSED", reason="current_block_insane",
                   matched=0, census_done=1, admitted=30, counters=0,
                   entries=(0, 0)),
    "blocks0_zero": S(verdict="CENSUS:REFUSED", reason="blocks0_invalid",
                      matched=0, census_done=1, admitted=30, counters=0,
                      entries=(0, 0)),
    "blocks0_uncovered": S(verdict="CENSUS:REFUSED",
                           reason="blocks0_uncovered", matched=0,
                           census_done=1, admitted=30, counters=0,
                           entries=(0, 0)),
    "handles_uncovered": S(verdict="CENSUS:REFUSED",
                           reason="handles_window_uncovered", matched=0,
                           census_done=1, admitted=30, counters=0,
                           entries=(0, 0)),
    "ids_degenerate": S(verdict="CENSUS:REFUSED",
                        reason="bootstrap_ids_degenerate", matched=0,
                        census_done=1, admitted=30, counters=0, entries=(0, 0)),
    "bp_oob": S(verdict="CENSUS:REFUSED", reason="handle_block_index_oob",
                matched=0, census_done=1, admitted=30, counters=0,
                entries=(0, 0)),
    "bp_blockptr_zero": S(verdict="CENSUS:REFUSED",
                          reason="block_pointer_invalid", matched=0,
                          census_done=1, admitted=30, counters=0,
                          entries=(0, 0)),
    # resolve's Blocks[]-slot window is covered by construction (the pool
    # header window already passed), so the uncovered leg surfaces at the
    # ENTRY read: block_pointer_window_uncovered stays defensive-only.
    "bp_blockptr_uncovered": S(verdict="CENSUS:REFUSED",
                               reason="entry_window_uncovered", matched=0,
                               census_done=1, admitted=30, counters=0,
                               entries=(0, 0)),
    "bp_entry_uncovered": S(verdict="CENSUS:REFUSED",
                            reason="entry_window_uncovered", matched=0,
                            census_done=1, admitted=30, counters=0,
                            entries=(0, 0)),
    "bp_wide": S(verdict="CENSUS:REFUSED", reason="entry_wide_flag_set",
                 matched=0, census_done=1, admitted=30, counters=0,
                 entries=(0, 0)),
    "none_mismatch": S(verdict="CENSUS:REFUSED", reason="none_ascii_mismatch",
                       matched=0, census_done=1, admitted=30, counters=0,
                       entries=(0, 0), none_id_zero=0, bp_ok=1),
    "bp_mismatch": S(verdict="CENSUS:REFUSED",
                     reason="byteproperty_ascii_mismatch", matched=0,
                     census_done=1, admitted=30, counters=0, entries=(0, 0),
                     none_id_zero=1, bp_ok=0),
    "census_blockptr_bad": S(verdict="CENSUS:REFUSED",
                             reason="census_block_pointer_invalid", matched=10,
                             neg_post=1, census_done=1, admitted=30,
                             counters=0, entries=(130900, 131072),
                             none_id_zero=1, bp_ok=1,
                             match_ids=MAIN_MATCH_IDS),
    "census_entry_uncovered": S(verdict="CENSUS:REFUSED",
                                reason="census_entry_window_uncovered",
                                matched=9, neg_post=1, census_done=1,
                                admitted=30, counters=0,
                                entries=(65400, 65536), none_id_zero=1,
                                bp_ok=1, match_ids=B0_MATCH_IDS),
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


def compile_driver(defs, out_bin, extra_objs):
    objs = []
    for f in SOURCES:
        d = BUDGET_DEFS if defs is BUDGET_DEFS else DRIVER_DEFS
        o = OUT / f"{out_bin}.{f.replace('.c', '.o')}"
        sh([CC, *WARN, *d, "-I", str(CFG), "-I", str(SRC), "-c",
            str(SRC / f), "-o", str(o)])
        objs.append(o)
    test_o = OUT / f"{out_bin}.host_main.o"
    sh([CC, *WARN, *defs, "-I", str(CFG), "-I", str(SRC), "-c",
        str(HERE / "host_main.c"), "-o", str(test_o)])
    sh([CC, str(test_o), *[str(o) for o in objs], "-o", str(OUT / out_bin)])
    return OUT / out_bin


def main() -> None:
    subprocess.run(["rm", "-rf", str(OUT)], check=True)
    (OUT / "tmp").mkdir(parents=True)
    receipt = []

    def log(s):
        print(s)
        receipt.append(s)

    log("== compile (real sources, host shims) ==")
    bin_main = compile_driver(DRIVER_DEFS, "cc1_host_test", None)
    bin_budget = compile_driver(BUDGET_DEFS, "cc1_host_test_budget", None)
    log("compile OK (Wall Wextra Werror clean; main + tiny-budget binary)")

    failures = []
    for scenario, exp in SCENARIOS.items():
        log(f"== scenario {scenario} ==")
        jsonl = OUT / "tmp" / f"{scenario}.jsonl"
        ue4ss = OUT / "tmp" / f"{scenario}.ue4ss.log"
        binp = bin_budget if scenario == "budget" else bin_main
        r = subprocess.run([str(binp), scenario, str(jsonl), str(ue4ss)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            failures.append(f"{scenario}: host binary rc={r.returncode} {r.stderr}")
            continue
        rows = parse_jsonl(jsonl)  # strict-JSONL oracle
        exps = [row.get("exp") for row in rows]
        result = next((x for x in rows if x.get("exp") == "census_result"), None)
        summary = next((x for x in rows if x.get("exp") == "summary"), None)
        counters = [x for x in rows if x.get("exp") == "qh_counter"]
        problems = []
        if exps[:3] != ["entry", "startup", "mapscan"]:
            problems.append(f"first exps {exps[:3]}")
        for row in rows:
            if row.get("marker") != "modbridge.ingamefacts" or \
               row.get("schema") != 1 or \
               not str(row.get("build_id", "")).startswith("cc1-"):
                problems.append(f"bad envelope in exp={row.get('exp')}")
        if result is None or summary is None:
            problems.append("missing census_result or summary")
        else:
            f_ = result["fields"]
            if f_.get("verdict") != exp["verdict"]:
                problems.append(f"verdict {f_.get('verdict')} != {exp['verdict']}")
            if f_.get("reason_code") != exp["reason"]:
                problems.append(f"reason {f_.get('reason_code')} != {exp['reason']}")
            if f_.get("matched_count") != exp["matched"]:
                problems.append(f"matched {f_.get('matched_count')} != {exp['matched']}")
            if f_.get("census_truncated") != exp.get("truncated", 0):
                problems.append("truncated flag wrong")
            if f_.get("wide_entries_skipped") != exp.get("wide", 0):
                problems.append(f"wide {f_.get('wide_entries_skipped')} != {exp.get('wide', 0)}")
            if not (exp["entries"][0] <= f_.get("census_entries_scanned", 0)
                    <= exp["entries"][1]):
                problems.append(f"entries {f_.get('census_entries_scanned')} "
                                f"outside {exp['entries']}")
            if f_.get("census_progress_markers") != \
                    f_.get("census_entries_scanned", 0) // 4096:
                problems.append("progress markers not entries//4096")
            if f_.get("budget_entries") != (64 if scenario == "budget" else 1048576):
                problems.append("budget_entries wrong")
            neg = f_.get("negative_hits", {})
            if neg.get("PostRender") != exp.get("neg_post", 0) or \
               neg.get("ZZZNotARealFunctionQQ") != exp.get("neg_zzz", 0):
                problems.append(f"negative_hits {neg}")
            ctrl = f_.get("controls", {})
            if ctrl.get("None_id_zero") != exp.get("none_id_zero", 0) or \
               ctrl.get("ByteProperty_ascii_ok") != exp.get("bp_ok", 0):
                problems.append(f"controls {ctrl}")
            # summary shape
            s_ = summary["fields"]
            if s_.get("polls_run") != 30 or s_.get("write_failures") != 0 or \
               s_.get("ue4ss_log_line_written") != 1:
                problems.append("summary shape wrong")
            if s_.get("polls_admitted") != exp["admitted"]:
                problems.append(f"polls_admitted {s_.get('polls_admitted')} != {exp['admitted']}")
            if s_.get("census_done") != exp.get("census_done", 1):
                problems.append("census_done wrong")
            if s_.get("census_validated") != (1 if exp["verdict"] == "CENSUS:VALIDATED" else 0):
                problems.append("census_validated wrong")
            # counter discipline
            if len(counters) != 30:
                problems.append(f"qh_counter lines {len(counters)} != 30")
            else:
                polls = [c["fields"]["poll"] for c in counters]
                if polls != list(range(30)):
                    problems.append("poll index sequence wrong")
                last = counters[-1]["fields"]
                if last.get("refused_bindings_absent") != \
                        exp.get("bindings_absent", 0):
                    problems.append("refused_bindings_absent wrong")
                if last.get("refused_layout_gate_closed") != \
                        exp.get("gate_closed", 0):
                    problems.append("refused_layout_gate_closed wrong")
                if "rcode" in exp and \
                        counters[0]["fields"]["admission_rcode"] != exp["rcode"]:
                    problems.append("admission_rcode wrong")
                if "num_elements" in exp and \
                        counters[0]["fields"]["gua_num_elements"] != exp["num_elements"]:
                    problems.append("gua_num_elements wrong")
                if last.get("census_entries_scanned") != \
                        f_.get("census_entries_scanned"):
                    problems.append("counter entries total != census_result")
            # census_match lines
            matches = [x for x in rows if x.get("exp") == "census_match"]
            got_ids = {m["fields"]["name"]: m["fields"]["id"] for m in matches}
            if got_ids != exp.get("match_ids", {}):
                problems.append(f"match ids {sorted(got_ids.items())[:4]}... != expected")
            # census phase marker on the poll where the census ran
            if "census_poll" in exp:
                cp = counters[exp["census_poll"]]["fields"]["phase"]
                if cp != "census":
                    problems.append(f"census poll phase {cp} != census")
            if exp.get("gate_closed_phase") == 30:
                if any(c["fields"]["phase"] not in ("gate_closed",)
                       for c in counters):
                    problems.append("uninit scenario must stay phase gate_closed")
        # exactly ONE UE4SS.log line, lineage shape
        ue4ss_lines = ue4ss.read_text().splitlines()
        if len(ue4ss_lines) != 1:
            problems.append(f"ue4ss lines {len(ue4ss_lines)} != 1")
        elif not ue4ss_lines[0].startswith("CC1: build=cc1-") or \
                f" verdict={exp['verdict']}" not in ue4ss_lines[0] or \
                f" reason={exp['reason']}" not in ue4ss_lines[0]:
            problems.append(f"ue4ss line shape: {ue4ss_lines[0][:80]}")
        if problems:
            failures.append(f"{scenario}: " + "; ".join(problems))
        else:
            log(f"{scenario}: PASS verdict={exp['verdict']} reason={exp['reason']}")

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
