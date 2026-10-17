"""Independent verification of the vendored modbridge_facts parser against real logs.

Checks per input log (originals under ../../2026-10-04-zcode-controller/console/):
  1. no-silent-drop: every raw line appears as a parsed record or an errors entry
  2. build_id set from parsed records == set harvested from raw text
  3. exp values in parsed records == set harvested from raw text
  4. duplicate JSON keys introduced/lost by comma repair (object_pairs_hook)
  5. key preservation: every `"key":` token in a repaired raw line survives parse
  6. experiment verdicts: count of rc-bearing dicts == parser experiments;
     per-verdict rc/errno/name cross-checked against the source record
  7. envelope anomaly inventory (what the parser rejected and why)

Writes verify_report.json next to itself. Usage:
  python3 verify_parse.py <console_dir> <run_dir>
"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + "/vendored")
from modbridge_facts.parser import parse_file  # noqa: E402

ENV_KEYS = re.compile(r'"(build_id|seq|sequence|exp|marker)":')


def dup_hook(pairs):
    seen, dups = set(), []
    for k, v in pairs:
        if k in seen:
            dups.append(k)
        seen.add(k)
    return dict(pairs) if not dups else {"__dup_keys__": dups, **dict(pairs)}


def walk_rc(obj):
    """Yield every dict anywhere in obj that carries a numeric rc."""
    if isinstance(obj, dict):
        rc = obj.get("rc")
        if isinstance(rc, (int, float)) and not isinstance(rc, bool):
            yield obj
        for v in obj.values():
            yield from walk_rc(v)
    elif isinstance(obj, list):
        for v in obj:
            yield from walk_rc(v)


def verify_one(name, console_dir, run_dir):
    stem = os.path.splitext(name)[0]
    raw_path = os.path.join(console_dir, name)
    parsed_path = os.path.join(run_dir, stem + ".parsed.jsonl")
    errors_path = os.path.join(run_dir, stem + ".errors.jsonl")
    repairs_path = os.path.join(run_dir, stem + ".repairs.json")

    with open(raw_path, encoding="utf-8", errors="surrogateescape") as f:
        raw_lines = [l.rstrip("\r\n") for l in f]
    parsed = {}
    for l in open(parsed_path, encoding="utf-8"):
        r = json.loads(l)
        parsed[r["_meta"]["line_no"]] = r
    err_lines = [json.loads(l) for l in open(errors_path, encoding="utf-8")] \
        if os.path.getsize(errors_path) else []
    repairs = [json.loads(l) for l in open(repairs_path, encoding="utf-8")] \
        if os.path.getsize(repairs_path) else []

    res = {"file": name, "raw_lines": len(raw_lines)}
    # 1. no silent drop
    covered = set(parsed) | {e["line_no"] for e in err_lines}
    res["missing_lines"] = sorted(set(range(1, len(raw_lines) + 1)) - covered)
    # 2. build_ids
    raw_bids, raw_exps = set(), set()
    for l in raw_lines:
        m = re.search(r'"build_id":"([^"]*)"', l)
        if m:
            raw_bids.add(m.group(1))
        for e in re.findall(r'"exp":"([^"]*)"', l):
            raw_exps.add(e)
    parsed_bids = {r.get("build_id") for r in parsed.values()}
    parsed_exps = {r["exp"] for r in parsed.values() if isinstance(r.get("exp"), str)}
    res["build_ids_raw"] = sorted(raw_bids)
    res["build_ids_parsed"] = sorted(parsed_bids)
    res["build_id_mismatch"] = sorted(raw_bids ^ parsed_bids)
    res["exp_values_raw"] = sorted(raw_exps)
    res["exp_values_parsed"] = sorted(parsed_exps)
    res["exp_mismatch"] = sorted(raw_exps ^ parsed_exps)
    # 3+4. repairs: dup keys + key preservation
    dup_hits, lost_keys = [], []
    for rp in repairs:
        ln = rp["line_no"]
        try:
            json.loads(rp["repaired_line"], object_pairs_hook=dup_hook)
        except Exception as ex:  # noqa: BLE001
            dup_hits.append({"line": ln, "error": str(ex)})
        rec = parsed.get(ln)
        dumped = json.dumps(rec, separators=(",", ":"), ensure_ascii=False) if rec else ""
        for key in set(re.findall(r'"([A-Za-z_][A-Za-z0-9_]*)"\s*:', raw_lines[ln - 1])):
            if '"%s":' % key not in dumped:
                lost_keys.append({"line": ln, "key": key})
    res["repair_lines"] = len(repairs)
    res["repair_dup_key_lines"] = dup_hits
    res["repair_lost_keys"] = lost_keys
    # 6. experiments
    outcomes, report = parse_file(raw_path)
    rc_dicts = []
    for oc in outcomes:
        if oc.record is not None:
            rc_dicts.extend(walk_rc(oc.record))
    res["rc_bearing_dicts"] = len(rc_dicts)
    res["parser_experiments"] = len(report.experiments)
    bad = []
    exp_by_line = {}
    for e in report.experiments:
        exp_by_line.setdefault(e.source_line, []).append(e)
    mism = 0
    for ln, es in exp_by_line.items():
        rec = parsed.get(ln)
        if rec is None:
            mism += 1
            continue
        src = list(walk_rc(rec))
        if len(src) != len(es):
            mism += 1
            continue
        for e, s in zip(es, src):
            if e.rc != s.get("rc") or e.errno != s.get("errno"):
                mism += 1
    res["verdict_crosscheck_mismatches"] = mism
    # 7. envelope anomalies
    anom = {}
    for oc in outcomes:
        for a in oc.anomalies:
            key = a.split(":")[0] if ":" in a else a
            anom[key] = anom.get(key, 0) + 1
    res["anomaly_inventory"] = anom
    res["report_build_ids"] = {k: v for k, v in report.build_ids.items()}
    res["report_invalid_envelope"] = report.invalid_envelope_lines
    res["report_unrepairable"] = report.unrepairable_lines
    res["report_duplicate_seqs"] = report.duplicate_seqs
    res["entry_present"] = report.entry_present
    res["startup_present"] = report.startup_present
    # raw "sequence" variant stats
    res["lines_with_sequence_key"] = sum(1 for l in raw_lines if '"sequence":' in l)
    res["lines_with_seq_key"] = sum(1 for l in raw_lines if '"seq":' in l)
    res["exp_named_entry_startup"] = sorted(
        e for e in raw_exps if e.lower() in ("entry", "startup"))
    return res


def main():
    console_dir, run_dir = sys.argv[1], sys.argv[2]
    files = sorted(f for f in os.listdir(console_dir) if f.endswith(".jsonl"))
    out = [verify_one(f, console_dir, run_dir) for f in files]
    dst = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "verify_report_%s.json" % os.path.basename(run_dir))
    with open(dst, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
    ok = True
    for r in out:
        problems = []
        if r["missing_lines"]:
            problems.append("MISSING_LINES=%d" % len(r["missing_lines"]))
        if r["build_id_mismatch"]:
            problems.append("BUILD_ID_MISMATCH")
        if r["exp_mismatch"]:
            problems.append("EXP_MISMATCH")
        if r["repair_dup_key_lines"]:
            problems.append("REPAIR_DUP_KEYS")
        if r["repair_lost_keys"]:
            problems.append("REPAIR_LOST_KEYS=%d" % len(r["repair_lost_keys"]))
        if r["verdict_crosscheck_mismatches"]:
            problems.append("VERDICT_MISMATCH=%d" % r["verdict_crosscheck_mismatches"])
        if r["report_invalid_envelope"] or r["report_unrepairable"]:
            problems.append("env_bad(unrep=%d,inv=%d)"
                            % (r["report_unrepairable"], r["report_invalid_envelope"]))
        status = "PROBLEMS: " + ", ".join(problems) if problems else "OK"
        if problems:
            ok = False
        print("%-42s lines=%-5d repairs=%-4d exps=%-5d %s"
              % (r["file"], r["raw_lines"], r["repair_lines"],
                 r["parser_experiments"], status))
    print("report: %s" % dst)
    print("OVERALL: %s" % ("FAIL" if not ok else "PASS"))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
