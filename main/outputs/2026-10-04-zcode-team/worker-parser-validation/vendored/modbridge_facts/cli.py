"""CLI for the modbridge-ingame-facts parser.

Usage:
  python3 cli.py parse   LOG.jsonl [LOG2.jsonl ...] [--expect-build-id ID ...]
                         [--out-dir DIR]
  python3 cli.py summarize LOG.jsonl [...] [--expect-build-id ID ...] [--json]

Exit codes (parse): 0 = every line parsed/validated clean;
                    1 = at least one unrepairable or invalid-envelope line.
Summarize exits 0 (anomalies are reported in the table, never hidden).
Nothing is ever silently dropped: unrepairable lines go verbatim to
<stem>.errors.jsonl and also appear in the summary.
"""
from __future__ import annotations

import argparse
import json
import os
import sys

if __package__ in (None, ""):  # allow direct-script invocation
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from modbridge_facts.parser import parse_file
    from modbridge_facts.summarize import summary_dict, summary_table
else:
    from .parser import parse_file
    from .summarize import summary_dict, summary_table


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="modbridge_facts",
        description="Parse/summarize modbridge-ingame-facts.jsonl "
                    "(handles v1/v2 missing-comma malformation).")
    sub = p.add_subparsers(dest="mode", required=True)
    for name in ("parse", "summarize"):
        s = sub.add_parser(name)
        s.add_argument("logs", nargs="+", help="input .jsonl file(s)")
        s.add_argument("--expect-build-id", action="append", default=[],
                       help="build_id allowed in this log (repeatable); "
                            "others are flagged as unknown-build")
        if name == "parse":
            s.add_argument("--out-dir", default=None,
                           help="output directory (default: alongside each input)")
        else:
            s.add_argument("--json", action="store_true",
                           help="emit machine-readable JSON instead of a table")
    return p


def _do_parse(args: argparse.Namespace) -> int:
    any_fatal = False
    for log in args.logs:
        outcomes, report = parse_file(log, expect_build_ids=args.expect_build_id)
        base = os.path.splitext(os.path.basename(log))[0]
        out_dir = args.out_dir or os.path.dirname(os.path.abspath(log))
        os.makedirs(out_dir, exist_ok=True)
        parsed_path = os.path.join(out_dir, base + ".parsed.jsonl")
        repairs_path = os.path.join(out_dir, base + ".repairs.json")
        errors_path = os.path.join(out_dir, base + ".errors.jsonl")
        n_rec = 0
        with open(parsed_path, "w", encoding="utf-8") as pf, \
                open(repairs_path, "w", encoding="utf-8") as rf:
            for oc in outcomes:
                if oc.record is None:
                    continue  # its raw text goes to errors file below
                rec = dict(oc.record)
                rec["_meta"] = {"line_no": oc.line_no, "repairs": oc.repairs,
                                "anomalies": [a for a in oc.anomalies
                                              if a != "blank"]}
                pf.write(json.dumps(rec, ensure_ascii=False) + "\n")
                n_rec += 1
                if oc.repairs:
                    rf.write(json.dumps({
                        "line_no": oc.line_no, "insertion_offsets": oc.repairs,
                        "repaired_line": oc.repaired_line,
                    }, ensure_ascii=False) + "\n")
        with open(errors_path, "w", encoding="utf-8") as ef:
            for oc in outcomes:
                if oc.record is not None:
                    continue
                ef.write(json.dumps({
                    "line_no": oc.line_no, "raw": oc.raw,
                    "error": "; ".join(oc.anomalies),
                }, ensure_ascii=False) + "\n")
        print("%s: %d lines -> %d records, %d repaired, %d unrepairable, "
              "%d invalid-envelope"
              % (log, report.total_lines, n_rec, report.repaired_lines,
                 report.unrepairable_lines, report.invalid_envelope_lines))
        print("  wrote %s, %s, %s" % (parsed_path, repairs_path, errors_path))
        if report.fatal:
            any_fatal = True
    return 1 if any_fatal else 0


def _do_summarize(args: argparse.Namespace) -> int:
    reports = [parse_file(log, expect_build_ids=args.expect_build_id)[1]
               for log in args.logs]
    if args.json:
        print(json.dumps(summary_dict(reports), indent=2, ensure_ascii=False))
    else:
        print(summary_table(reports))
    return 0


def main(argv=None) -> int:
    args = _build_parser().parse_args(argv)
    if args.mode == "parse":
        return _do_parse(args)
    return _do_summarize(args)


if __name__ == "__main__":
    sys.exit(main())
