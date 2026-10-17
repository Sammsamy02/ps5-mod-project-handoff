#!/usr/bin/env python3
"""diff_results.py -- structured diff for UE4SS static conformance logs.

Mac-only / host-tested tool. Compares two log files produced by
conformance_static.lua (or any log following the same format) and reports:

  MATCH    same call key present in both logs with identical results
  MISMATCH same call key present in both logs with differing results
  MISSING  call present in log A (baseline/left) but not in log B
  EXTRA    call present in log B (right) but not in log A

A "call key" is (api_name, occurrence_index): the Nth call to a given api
name within one log file. Logs are emitted in a fixed order by the
conformance suite, so occurrence indexes line up across runs.

Input log format (tab-separated, produced by conformance_static.lua):
  CALL<TAB>api_name<TAB>params_summary<TAB>result_type<TAB>result_value_or_error<TAB>ok_or_fail
Section markers (carried into the diff for grouping):
  # SECTION: category_name
All other lines are ignored.

Usage:
  python3 diff_results.py LOG_A LOG_B [--json OUT.json] [--verbose]

Output:
  - summary table to stdout (totals + per-section breakdown)
  - JSON diff file (default: <LOG_A stem>_vs_<LOG_B stem>.diff.json in cwd)
  - --verbose prints every call line with its status
Exit codes:
  0 = logs equivalent (no MISMATCH / MISSING / EXTRA)
  1 = differences found
  2 = usage or parse error
"""

import argparse
import hashlib
import json
import os
import sys
from collections import OrderedDict

TOOL_NAME = "diff_results.py"
TOOL_VERSION = "1.0"

STATUS_ORDER = ["MATCH", "MISMATCH", "MISSING", "EXTRA"]


class LogParseError(Exception):
    pass


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_log(path):
    """Parse a conformance log.

    Returns OrderedDict mapping (api, occurrence) -> record dict:
      { api, occ, section, params, result_type, value, ok, line_no, raw }
    Raises LogParseError on unreadable files.
    """
    records = OrderedDict()
    section = "(none)"
    occurrences = {}
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for line_no, raw_line in enumerate(f, 1):
                line = raw_line.rstrip("\r\n")
                if line.startswith("# SECTION:"):
                    section = line[len("# SECTION:"):].strip()
                    continue
                if not line.startswith("CALL\t"):
                    continue
                fields = line.split("\t")
                if len(fields) < 6:
                    # Malformed CALL line: keep what we have; the value field
                    # absorbs any extra tabs from later splits anyway.
                    fields = fields + [""] * (6 - len(fields))
                api = fields[1]
                params = fields[2]
                result_type = fields[3]
                # Extra tabs (should not occur -- values are sanitized --
                # but be robust): rejoin into the value field.
                value = "\t".join(fields[4:-1]) if len(fields) > 6 else fields[4]
                ok = fields[-1]
                occ = occurrences.get(api, 0) + 1
                occurrences[api] = occ
                records[(api, occ)] = {
                    "api": api,
                    "occ": occ,
                    "key": "%s#%d" % (api, occ),
                    "section": section,
                    "params": params,
                    "result_type": result_type,
                    "value": value,
                    "ok": ok,
                    "line_no": line_no,
                    "raw": line,
                }
    except OSError as e:
        raise LogParseError("cannot read %s: %s" % (path, e))
    return records


def compare(a_records, b_records):
    """Compare two parsed logs. Returns list of detail dicts in stable order."""
    details = []
    for key, a in a_records.items():
        if key not in b_records:
            details.append({
                "key": a["key"], "api": a["api"], "section": a["section"],
                "status": "MISSING",
                "a": {k: a[k] for k in ("params", "result_type", "value", "ok")},
                "b": None,
            })
            continue
        b = b_records[key]
        diff_fields = []
        for f in ("params", "result_type", "value", "ok"):
            if a[f] != b[f]:
                diff_fields.append(f)
        status = "MATCH" if not diff_fields else "MISMATCH"
        details.append({
            "key": a["key"], "api": a["api"],
            "section": a["section"] if a["section"] == b["section"] else "%s | %s" % (a["section"], b["section"]),
            "status": status,
            "diff_fields": diff_fields,
            "a": {k: a[k] for k in ("params", "result_type", "value", "ok")},
            "b": {k: b[k] for k in ("params", "result_type", "value", "ok")},
        })
    for key, b in b_records.items():
        if key not in a_records:
            details.append({
                "key": b["key"], "api": b["api"], "section": b["section"],
                "status": "EXTRA",
                "a": None,
                "b": {k: b[k] for k in ("params", "result_type", "value", "ok")},
            })
    return details


def summarize(details):
    totals = {s: 0 for s in STATUS_ORDER}
    per_section = OrderedDict()
    for d in details:
        totals[d["status"]] += 1
        sec = d.get("section", "(none)")
        if sec not in per_section:
            per_section[sec] = {s: 0 for s in STATUS_ORDER}
        per_section[sec][d["status"]] += 1
    return totals, per_section


def print_summary(totals, per_section, path_a, path_b):
    total_calls = sum(totals.values())
    print("=" * 78)
    print("Conformance log diff")
    print("  A (baseline): %s" % path_a)
    print("  B (right)   : %s" % path_b)
    print("=" * 78)
    print("%-12s %8s" % ("STATUS", "COUNT"))
    for s in STATUS_ORDER:
        print("%-12s %8d" % (s, totals[s]))
    print("%-12s %8d" % ("TOTAL", total_calls))
    verdict = "EQUIVALENT" if (totals["MISMATCH"] + totals["MISSING"] + totals["EXTRA"]) == 0 else "DIFFERENT"
    print("-" * 78)
    print("Verdict: %s" % verdict)
    if per_section:
        print("-" * 78)
        secs = [s for s in per_section if any(per_section[s][x] for x in ("MISMATCH", "MISSING", "EXTRA"))]
        if secs:
            print("Sections with differences:")
            print("%-28s %9s %9s %9s %9s" % ("SECTION", "MATCH", "MISMATCH", "MISSING", "EXTRA"))
            for sec in secs:
                row = per_section[sec]
                print("%-28s %9d %9d %9d %9d" % (sec, row["MATCH"], row["MISMATCH"], row["MISSING"], row["EXTRA"]))
        else:
            print("All sections fully matched.")


def print_verbose(details):
    print("=" * 78)
    print("Verbose: every call line")
    print("=" * 78)
    for d in details:
        print("[%s] %s  (section: %s)" % (d["status"], d["key"], d.get("section", "?")))
        if d["status"] == "MISSING":
            print("    A: %s | %s | %s | ok=%s" % (d["a"]["params"], d["a"]["result_type"], d["a"]["value"], d["a"]["ok"]))
            print("    B: <absent>")
        elif d["status"] == "EXTRA":
            print("    A: <absent>")
            print("    B: %s | %s | %s | ok=%s" % (d["b"]["params"], d["b"]["result_type"], d["b"]["value"], d["b"]["ok"]))
        else:
            print("    A: %s | %s | %s | ok=%s" % (d["a"]["params"], d["a"]["result_type"], d["a"]["value"], d["a"]["ok"]))
            print("    B: %s | %s | %s | ok=%s" % (d["b"]["params"], d["b"]["result_type"], d["b"]["value"], d["b"]["ok"]))
            if d["status"] == "MISMATCH":
                print("    differing fields: %s" % ", ".join(d["diff_fields"]))


def build_json(path_a, path_b, totals, per_section, details, json_path):
    doc = {
        "tool": TOOL_NAME,
        "tool_version": TOOL_VERSION,
        "inputs": {
            "a": {"path": os.path.abspath(path_a), "sha256": sha256_of(path_a)},
            "b": {"path": os.path.abspath(path_b), "sha256": sha256_of(path_b)},
        },
        "summary": {
            "totals": totals,
            "total_calls": sum(totals.values()),
            "verdict": "EQUIVALENT" if (totals["MISMATCH"] + totals["MISSING"] + totals["EXTRA"]) == 0 else "DIFFERENT",
            "per_section": [
                {"section": sec, **counts} for sec, counts in per_section.items()
            ],
        },
        "details": details,
    }
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2, ensure_ascii=False)
        f.write("\n")
    return doc


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Structured diff of two UE4SS static conformance logs.")
    parser.add_argument("log_a", help="baseline (left) log file")
    parser.add_argument("log_b", help="right log file")
    parser.add_argument("--json", metavar="OUT.json",
                        help="path for the JSON diff (default: <a>_vs_<b>.diff.json in cwd)")
    parser.add_argument("--verbose", action="store_true",
                        help="print every call line with its status")
    parser.add_argument("--no-json", action="store_true",
                        help="do not write a JSON diff file")
    args = parser.parse_args(argv)

    for p in (args.log_a, args.log_b):
        if not os.path.isfile(p):
            print("error: no such file: %s" % p, file=sys.stderr)
            return 2

    try:
        a_records = parse_log(args.log_a)
        b_records = parse_log(args.log_b)
    except LogParseError as e:
        print("error: %s" % e, file=sys.stderr)
        return 2

    details = compare(a_records, b_records)
    totals, per_section = summarize(details)

    print_summary(totals, per_section, args.log_a, args.log_b)
    if args.verbose:
        print_verbose(details)

    if not args.no_json:
        stem_a = os.path.splitext(os.path.basename(args.log_a))[0]
        stem_b = os.path.splitext(os.path.basename(args.log_b))[0]
        json_path = args.json or ("%s_vs_%s.diff.json" % (stem_a, stem_b))
        build_json(args.log_a, args.log_b, totals, per_section, details, json_path)
        print("\nJSON diff written to: %s" % os.path.abspath(json_path))

    return 0 if (totals["MISMATCH"] + totals["MISSING"] + totals["EXTRA"]) == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
