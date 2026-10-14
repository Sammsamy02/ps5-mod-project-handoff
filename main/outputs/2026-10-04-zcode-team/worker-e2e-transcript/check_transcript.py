#!/usr/bin/env python3
"""check_transcript.py — host-side transcript-pairing checker for e2e-status.jsonl.

Validates event discipline in a modbridge installer E2E status transcript:
  - every `begin` has a matched `end` (and vice versa)
  - no unmatched begins, unmatched ends, or duplicate ends
  - status events occur inside their stage's begin..end window (in order)
  - required terminal status events are present (e.g. stage-3-install)
  - forbidden status stages (e.g. stage-6 removals) emit none
  - optional ledger expectations (lines / begins / ends / status counts)

Transcript contract (from worker-e2e-rehearsal/owned/e2e_status.c and
worker-e2e-console-dry/E2E-CMD-SCHEMA.md §7):
  begin : {"event":"begin","cmd":...,"title":...,"stage":...}
  end   : {"event":"end","exit":<int>}          (no stage/cmd ids)
  status: {"event":"status","stage":...,"outcome":...,"detail":...}

Because `end` carries no identifiers, begin/end pairing is strict
last-open-first-closed (stack) discipline, which is exactly how the
installer nests/serializes commands. If an `end` does carry optional
"stage"/"cmd" ids (defensive support for richer variants), it is matched
to the innermost open begin with those ids; an id-matched end with no
open counterpart but a closed counterpart is a duplicate end.

Exit codes: 0 = PASS (no error-severity findings), 1 = FAIL.
Use --fail-on warning to also fail on warnings.

Usage examples:
  python3 check_transcript.py e2e-status.jsonl --expect-lines 44 \
      --expect-begins 21 --expect-ends 20 --expect-unmatched-begins 1
  python3 check_transcript.py run.jsonl \
      --require-terminal-status stage-3-install:ok:movspeed,mm-skin,mm-lua \
      --forbid-status stage-6-removal --json results.json
"""

import argparse
import json
import sys

SEVERITIES = ("note", "warning", "error")


def parse_require_spec(spec):
    """STAGE[:OUTCOME[:DETAIL]] -> (stage, outcome|None, detail|None)."""
    parts = spec.split(":", 2)
    stage = parts[0]
    outcome = parts[1] if len(parts) > 1 and parts[1] else None
    detail = parts[2] if len(parts) > 2 and parts[2] else None
    return stage, outcome, detail


def check(transcript, args):
    findings = []

    def add(sev, kind, line_no, msg, **extra):
        f = {"severity": sev, "kind": kind, "line": line_no, "detail": msg}
        f.update(extra)
        findings.append(f)

    counts = {
        "lines": 0, "blank": 0, "begins": 0, "ends": 0, "status": 0,
        "paired": 0, "failed-ends": 0, "unmatched-begins": 0,
    }
    open_stack = []   # open begin records, innermost last
    closed = []       # {"begin": rec, "end": ev, "end-line": n}
    unmatched_begin_recs = []
    last_event = None
    last_raw = None

    with open(transcript, "r", encoding="utf-8", errors="replace") as fh:
        for line_no, raw in enumerate(fh, 1):
            counts["lines"] += 1
            raw = raw.strip()
            if not raw:
                counts["blank"] += 1
                continue
            try:
                ev = json.loads(raw)
            except ValueError as exc:
                add("error", "malformed-json", line_no,
                    "unparseable JSON line: %s" % exc)
                last_event = None
                continue
            if not isinstance(ev, dict) or not isinstance(ev.get("event"), str):
                add("error", "malformed-event", line_no,
                    "line is not an object with a string 'event' field")
                last_event = None
                continue
            kind = ev["event"]

            if kind == "begin":
                counts["begins"] += 1
                rec = {
                    "line": line_no,
                    "cmd": ev.get("cmd", ""),
                    "title": ev.get("title", ""),
                    "stage": ev.get("stage", ""),
                    "statuses": [],
                }
                for o in open_stack:
                    if o["stage"] == rec["stage"] and o["cmd"] == rec["cmd"]:
                        add("note", "concurrent-overlap", line_no,
                            "begin %s/%s opens while an identical begin "
                            "(line %d) is still open (legitimate for a "
                            "refusal-probe begin left dangling by design)"
                            % (rec["cmd"], rec["stage"], o["line"]),
                            open_line=o["line"])
                        break
                open_stack.append(rec)
                last_event = "begin"
                last_raw = raw

            elif kind == "end":
                counts["ends"] += 1
                exit_code = ev.get("exit")
                estage = ev.get("stage")
                ecmd = ev.get("cmd")
                target = None
                if estage is not None or ecmd is not None:
                    # Identifier-matched close (defensive richer variant).
                    for i in range(len(open_stack) - 1, -1, -1):
                        o = open_stack[i]
                        if ((estage is None or o["stage"] == estage) and
                                (ecmd is None or o["cmd"] == ecmd)):
                            target = i
                            break
                    if target is None:
                        dup = any(
                            c["begin"]["stage"] == estage and
                            c["begin"]["cmd"] == ecmd
                            for c in closed)
                        if dup:
                            add("error", "duplicate-end", line_no,
                                "end for %s/%s matches an already-closed "
                                "begin; no open begin of that stage+job "
                                "remains" % (ecmd, estage), exit=exit_code)
                        else:
                            add("error", "unmatched-end", line_no,
                                "identified end (%s/%s) with no open begin "
                                "to close" % (ecmd, estage), exit=exit_code)
                        last_event = "end"
                        last_raw = raw
                        continue
                    if target != len(open_stack) - 1:
                        add("warning", "interleaved-close", line_no,
                            "id-matched end closed begin at line %d while "
                            "inner begins remain open"
                            % open_stack[target]["line"])
                else:
                    if not open_stack:
                        if last_event == "end" or raw == last_raw:
                            add("error", "duplicate-end", line_no,
                                "extra end beyond the open begins "
                                "(no begin left to close)", exit=exit_code)
                        else:
                            add("error", "unmatched-end", line_no,
                                "end with no open begin to close",
                                exit=exit_code)
                        last_event = "end"
                        last_raw = raw
                        continue
                    target = len(open_stack) - 1
                b = open_stack.pop(target)
                closed.append({"begin": b, "end": ev, "end-line": line_no})
                counts["paired"] += 1
                if isinstance(exit_code, int) and exit_code != 0:
                    counts["failed-ends"] += 1
                    sev = ("note" if counts["failed-ends"] <= args.expect_failed_ends
                           else "error")
                    add(sev, "failed-end", line_no,
                        "end exit=%s for begin %s/%s (line %d) is nonzero"
                        % (exit_code, b["cmd"], b["stage"], b["line"]),
                        begin_line=b["line"], cmd=b["cmd"], stage=b["stage"])
                last_event = "end"
                last_raw = raw

            elif kind == "status":
                counts["status"] += 1
                stage = ev.get("stage", "")
                outcome = ev.get("outcome", "")
                detail = ev.get("detail", "")
                if stage in args.forbid_status:
                    add("error", "forbidden-status", line_no,
                        "status for stage %s is forbidden on this run "
                        "(outcome=%s detail=%r)" % (stage, outcome, detail))
                target = None
                for i in range(len(open_stack) - 1, -1, -1):
                    if open_stack[i]["stage"] == stage:
                        target = i
                        break
                if target is not None:
                    b = open_stack[target]
                    b["statuses"].append(
                        {"line": line_no, "outcome": outcome, "detail": detail})
                    if len(b["statuses"]) > 1:
                        add("note", "duplicate-status", line_no,
                            "second status under begin at line %d (%s/%s)"
                            % (b["line"], b["cmd"], b["stage"]))
                else:
                    after = [c for c in closed if c["begin"]["stage"] == stage]
                    if after:
                        c = after[-1]
                        add("error", "out-of-order-status", line_no,
                            "status for %s appears after its begin/end pair "
                            "closed (begin line %d, end line %d)"
                            % (stage, c["begin"]["line"], c["end-line"]))
                    else:
                        add("error", "orphan-status", line_no,
                            "status for %s with no open or closed begin/end "
                            "pair of that stage (outcome=%s detail=%r)"
                            % (stage, outcome, detail))
                last_event = "status"
                last_raw = raw

            else:
                add("error", "unknown-event", line_no,
                    "unknown event type %r (expected begin|end|status)" % kind)
                last_event = None

    # Anything still open at EOF is an unmatched begin.
    for i, b in enumerate(open_stack):
        counts["unmatched-begins"] += 1
        unmatched_begin_recs.append(b)
        if i < args.expect_unmatched_begins:
            add("note", "expected-unmatched-begin", b["line"],
                "begin %s/%s (title=%r) never closed — within the expected "
                "unmatched-begin allowance (e.g. a refusal probe)"
                % (b["cmd"], b["stage"], b["title"]))
        else:
            add("error", "unmatched-begin", b["line"],
                "begin %s/%s (title=%r) has no matching end"
                % (b["cmd"], b["stage"], b["title"]))

    # Required terminal status events.
    for spec in args.require_terminal_status:
        stage, outcome, detail = parse_require_spec(spec)
        pairs = [c for c in closed if c["begin"]["stage"] == stage]
        if not pairs:
            add("error", "missing-terminal-status", None,
                "required terminal status %r: no begin/end pair exists for "
                "stage %s" % (spec, stage))
            continue
        for c in pairs:
            def status_ok(s):
                if outcome is not None and s["outcome"] != outcome:
                    return False
                if detail is not None and detail not in s["detail"]:
                    return False
                return True
            if not any(status_ok(s) for s in c["begin"]["statuses"]):
                add("error", "missing-terminal-status", c["end-line"],
                    "pair %s/%s (begin line %d, end line %d) lacks terminal "
                    "status %r"
                    % (c["begin"]["cmd"], stage, c["begin"]["line"],
                       c["end-line"], spec))

    # Ledger expectations.
    for key, expect in (("lines", args.expect_lines),
                        ("begins", args.expect_begins),
                        ("ends", args.expect_ends),
                        ("status", args.expect_status)):
        if expect is not None and counts[key] != expect:
            add("error", "expectation-mismatch", None,
                "expected %s %d, found %d" % (key, expect, counts[key]))

    errors = [f for f in findings if f["severity"] == "error"]
    warnings = [f for f in findings if f["severity"] == "warning"]
    notes = [f for f in findings if f["severity"] == "note"]
    gate = SEVERITIES.index(args.fail_on)
    passed = not any(SEVERITIES.index(f["severity"]) >= gate for f in findings)

    report = {
        "transcript": transcript,
        "passed": passed,
        "fail_on": args.fail_on,
        "counts": counts,
        "unmatched_begins": [
            {"line": b["line"], "cmd": b["cmd"], "title": b["title"],
             "stage": b["stage"]} for b in unmatched_begin_recs],
        "paired_pairs": [
            {"stage": c["begin"]["stage"], "cmd": c["begin"]["cmd"],
             "title": c["begin"]["title"], "begin_line": c["begin"]["line"],
             "end_line": c["end-line"],
             "exit": c["end"].get("exit"),
             "statuses": c["begin"]["statuses"]} for c in closed],
        "findings": findings,
        "summary": {"errors": len(errors), "warnings": len(warnings),
                    "notes": len(notes)},
    }
    return report, passed


def main(argv=None):
    p = argparse.ArgumentParser(
        description="e2e-status.jsonl begin/end transcript-pairing checker")
    p.add_argument("transcript", help="path to e2e-status.jsonl")
    p.add_argument("--expect-lines", type=int, default=None)
    p.add_argument("--expect-begins", type=int, default=None)
    p.add_argument("--expect-ends", type=int, default=None)
    p.add_argument("--expect-status", type=int, default=None)
    p.add_argument("--expect-unmatched-begins", type=int, default=0,
                   help=" tolerated dangling begins (e.g. refusal probes); "
                        "extras beyond this are errors")
    p.add_argument("--expect-failed-ends", type=int, default=0,
                   help="tolerated nonzero end exits; extras are errors")
    p.add_argument("--require-terminal-status", action="append", default=[],
                   metavar="STAGE[:OUTCOME[:DETAIL]]",
                   help="every begin/end pair of STAGE must contain a status "
                        "event with OUTCOME (default any) and DETAIL "
                        "substring (default any) inside its window")
    p.add_argument("--forbid-status", action="append", default=[],
                   metavar="STAGE",
                   help="no status event may reference STAGE (e.g. stage-6 "
                        "removals emit none)")
    p.add_argument("--json", dest="json_out", default=None,
                   help="also write the full structured report here")
    p.add_argument("--fail-on", choices=SEVERITIES, default="error",
                   help="minimum severity that fails the run (default error)")
    args = p.parse_args(argv)

    report, passed = check(args.transcript, args)

    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(report, fh, indent=2)
            fh.write("\n")

    c = report["counts"]
    print("== e2e-transcript check: %s" % args.transcript)
    print("lines=%d begins=%d ends=%d status=%d paired=%d failed-ends=%d "
          "unmatched-begins=%d"
          % (c["lines"], c["begins"], c["ends"], c["status"], c["paired"],
             c["failed-ends"], c["unmatched-begins"]))
    for b in report["unmatched_begins"]:
        print("unmatched begin: line=%d cmd=%s stage=%s title=%r"
              % (b["line"], b["cmd"], b["stage"], b["title"]))
    print("findings: errors=%d warnings=%d notes=%d"
          % (report["summary"]["errors"], report["summary"]["warnings"],
             report["summary"]["notes"]))
    for f in report["findings"]:
        loc = "line %s" % f["line"] if f["line"] is not None else "-"
        print("  [%s] %s %s: %s" % (f["severity"], loc, f["kind"], f["detail"]))
    print("RESULT: %s" % ("PASS" if report["passed"] else "FAIL"))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
