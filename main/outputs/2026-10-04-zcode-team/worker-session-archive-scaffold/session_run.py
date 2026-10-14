#!/usr/bin/env python3
"""session_run.py — local archive scaffold for the return session.

Implements the run-dir archive law of
outputs/2026-10-04-zcode-team/worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md
(§1-§2), matching worker-console-batch-runner/console_batch.py's make_run_dir:

    RUNS/<YYYYMMDD-HHMMSS>-<flow>/     one fresh run dir per flow run
      events.log     append-only step log, "[HH:MM:SS] msg" lines
      receipt.json   per-flow verdict receipt; the INDEX of the run's artifacts
      <fetched evidence files>   copied here, source paths recorded
      verdict-lines.txt + verdict-input-<label>.txt  captured tool outputs

Laws enforced (ACCEPTANCE-RECORD §1):
  L1  one fresh run dir per flow run; the helper NEVER re-runs into an
      existing dir (start refuses collisions, capture refuses overwrites,
      close refuses rewriting a closed receipt) and has no delete path —
      failed/aborted runs keep their run dirs for forensics.
  L2  every run dir carries at minimum events.log + receipt.json, from the
      moment it is stamped (an aborted run still keeps both).
  L3  all fetched evidence lands as FILES in the run dir; a verdict that
      exists only in stdout is not part of the record, so tool outputs are
      captured to files and their verdict lines recorded in receipt.json.
  L4  receipt.json is the index: every artifact is named with sha256,
      byte count, and the absolute source path it was copied from.
  L5  expected-artifact checklists are advisory: exact filenames are
      whatever the flow saved ("do not guess a filename — read the
      receipt"), so unmatched rows are recorded as honestly MISSING,
      never back-filled, and INCOMPLETE is a legitimate terminal state.
  L6  the record never contains completion or ">8 quality" claims; the
      receipt's honesty note is fixed and statuses reflect this run only.

Subcommands:
    start      --flow FLOW [--run-root DIR]                 stamp a fresh run dir
    note       --run-dir DIR "message"                      append to events.log
    capture    --run-dir DIR --src PATH [--as NAME] ...     archive evidence files
    verdict    --run-dir DIR (--from F | --stdin) [--grep RE] [--label L]
                                                            capture tool output
    close      --run-dir DIR --verdict PASS|FAIL|INCONCLUSIVE|ABORTED [--notes S]
                                                            write receipt + INDEX row
    checklist  --flow FLOW                                  print expected artifacts

Default --run-root is this scaffold's own runs/ (kept pristine for testing).
For the real session pass the canonical root, e.g.:
    --run-root ../../worker-console-batch-runner/runs
"""

import argparse
import datetime as _dt
import hashlib
import json
import os
import re
import shutil
import sys
import fnmatch

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_RUN_ROOT = os.path.join(SCRIPT_DIR, "runs")
LAW_SOURCE = ("outputs/2026-10-04-zcode-team/worker-post-spawn-closure-prep/"
              "ACCEPTANCE-RECORD.md")
HONESTY = ("statuses reflect this run only; no claim beyond the plans' own "
           "verdict rules; missing expected artifacts are recorded as missing, "
           "never back-filled; no completion or quality claim is made")
RESERVED = ("events.log", "receipt.json", "artifacts.jsonl", "verdict-lines.txt",
            "INDEX.md")

# Flow set from ACCEPTANCE-RECORD §1.1 / console_batch.py, plus the return
# session's expected launch-window labels (launches 1-8 are history; 9/10 next).
BASE_FLOWS = ["dll7", "dll8", "e2e", "stage-ridealongs", "stage-observer",
              "stage-qdrv1", "stage-settings", "rollback-ridealongs",
              "game-evidence", "all"]
SESSION_FLOWS = ["game-launch9", "game-launch10"]
GAME_LAUNCH_RE = re.compile(r"^game-launch\d+$")
VERDICTS = ("PASS", "FAIL", "INCONCLUSIVE", "ABORTED")

MINIMAL = [
    ("events.log", "append-only step log (archive law minimum)"),
    ("receipt.json", "per-flow verdict receipt / artifact index (archive law minimum)"),
]

# Expected-artifact checklists, traced to ACCEPTANCE-RECORD §2-§3.
# (glob-or-exact pattern, description). Advisory: unmatched = recorded missing.
DLL8 = [
    ("dll8-preflight-receipt-check.json",
     "preflight identity receipt (candidate 79281191… / 336552 B)"),
    ("*upload*", "ALLFAM8_DLL_TEST upload identity"),
    ("*-readback.elf", "<tag>-readback.elf EXACT gate"),
    ("allfam8-console-result.jsonl",
     "fetched copy of /data/modbridge-owned-dll-candidate.jsonl"),
    ("allfam8-ledger-verdict.json",
     "verdict vs EXPECTED-DELTA-8FAM (four new records at positions 37-40; "
     "static_tls+seh64 family keys; consumer_imports 90; results [86,93]; "
     "benign 'stdout_error 32' delta recorded, not papered over)"),
    ("allfam8-console-receipt.json", "console receipt"),
] + MINIMAL

DLL7_FALLBACK = [
    ("dll7-preflight-receipt-check.json", "preflight identity receipt (dll7)"),
    ("*upload*", "upload identity"),
    ("*-readback.elf", "<tag>-readback.elf EXACT gate"),
    ("allfam7-console-result.jsonl",
     "fetched copy of /data/modbridge-owned-dll-candidate.jsonl"),
    ("allfam7-ledger-verdict.json",
     "verdict vs EXPECTED-DELTA (documented SEH64 fallback; records 41-44)"),
    ("allfam7-console-receipt.json", "console receipt"),
] + MINIMAL

E2E = [
    ("*shasum*", "13/13 cmd-SHASUMS preflight proof"),
    ("e2e-payload.readback.elf",
     "payload MODE=B identity gate (a21771f8… / 681888 B)"),
    ("readback-*", "per-dispatch readback artifacts"),
    ("*.consumed", "per-dispatch consumed-twin proof (.consumed hash == last "
                   "cmdfiles.log line)"),
    ("stale.chk", "stale-cmd check artifact"),
    ("*ledger*", "verbatim fetched ledger copy per evid step"),
    ("*reset*", "§7 full-tree reset proof (only /data/modbridge/** touched)"),
    ("MODBRIDGE_INST_E2E-receipt.json", "e2e receipt"),
] + MINIMAL

GAME_EVIDENCE = [
    ("S-runtime-status.txt", "S sink copy (runtime state lines)"),
    ("UE4SS.log", "UE4SS.log copy"),
    ("*aligned*", "D1 aligned-entry log copy"),
    ("*observer*", "D2 observer log copy"),
    ("*igf*", "C2 / igf facts jsonl"),
    ("*xwp*", "probe verdict sink 1 (xwp jsonl)"),
    ("*fc1*", "probe verdict sink 2 (fc1pool jsonl)"),
    ("probe-verdicts.json", "ge.3b freshness gate"),
    ("cp0-cp4-verdict.json", "ge.3 CP0-CP4 verdict"),
    ("cp5-verdict.json", "ge.5-ge.7 CP5 verdict (menu opens)"),
    ("cp6-verdict.json", "ge.8-ge.9 CP6 verdict (spawn works)"),
    ("rung_check-RUNG1.json", "ge.4/4b RUNG1 strict baseline check"),
    ("rung_check-RUNG2.json", "RUNG2 terminal NQH1 record check"),
    ("*post_launch*", "post_launch_check output"),
    ("game-evidence-receipt.json",
     "ge.10 receipt incl. the two eye confirmations (menu visible, pickup "
     "visible) — eyes facts are receipts, not memories"),
    ("stability-gate-verdict.md", "SG window verdict (extends, never replaces)"),
    ("spawn-count-table.csv", "SG spawn-count table (press # → item id → frame ts)"),
] + MINIMAL

CHECKLISTS = {
    "dll7": DLL7_FALLBACK,
    "dll8": DLL8,
    "e2e": E2E,
    "game-evidence": GAME_EVIDENCE,
}
LAUNCH_NOTE = ("launch-window flow label for the return session (launches 1-8 "
               "are fail-open history; this window is fresh evidence only); "
               "archived under the game-evidence law of ACCEPTANCE-RECORD §2-§3")


def checklist_for(flow):
    if flow in CHECKLISTS:
        return CHECKLISTS[flow]
    if GAME_LAUNCH_RE.match(flow) or flow.startswith("game-launch"):
        return GAME_EVIDENCE
    return MINIMAL  # stage-*/rollback-*/all: minimum law + flow-defined names


def utc_now():
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def log_event(run_dir, msg):
    line = "[%s] %s" % (_dt.datetime.now().strftime("%H:%M:%S"), msg)
    with open(os.path.join(run_dir, "events.log"), "a") as f:
        f.write(line + "\n")
    return line


def append_artifact(run_dir, entry):
    with open(os.path.join(run_dir, "artifacts.jsonl"), "a") as f:
        f.write(json.dumps(entry, sort_keys=True) + "\n")


def read_artifacts(run_dir):
    p = os.path.join(run_dir, "artifacts.jsonl")
    if not os.path.exists(p):
        return []
    out = []
    with open(p) as f:
        for line in f:
            line = line.strip()
            if line:
                out.append(json.loads(line))
    return out


def resolve_run_dir(path):
    rd = os.path.abspath(path)
    if not os.path.isdir(rd):
        raise SystemExit("error: run dir does not exist: %s" % rd)
    if not os.path.isfile(os.path.join(rd, "events.log")):
        raise SystemExit("error: not a session run dir (no events.log): %s" % rd)
    return rd


def cmd_start(args):
    flow = args.flow
    if flow not in BASE_FLOWS + SESSION_FLOWS and not GAME_LAUNCH_RE.match(flow):
        raise SystemExit("error: unknown flow %r; known: %s (or game-launch<N>)"
                         % (flow, ", ".join(BASE_FLOWS + SESSION_FLOWS)))
    run_root = os.path.abspath(args.run_root)
    os.makedirs(run_root, exist_ok=True)
    # L1: fresh stamped dir; on a same-second collision wait for the next
    # second — never re-run into an existing dir.
    for attempt in range(4):
        stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
        rd = os.path.join(run_root, "%s-%s" % (stamp, flow))
        if not os.path.exists(rd):
            break
        import time
        time.sleep(1)
    else:
        raise SystemExit("error: could not stamp a fresh run dir in %s" % run_root)
    os.makedirs(rd)  # exist_ok=False: refuses if it appeared meanwhile
    with open(os.path.join(rd, "events.log"), "a") as f:
        f.write("[%s] RUN-START flow=%s law=%s\n"
                % (_dt.datetime.now().strftime("%H:%M:%S"), flow, LAW_SOURCE))
    # L2: receipt exists from the moment the dir is stamped (OPEN until close;
    # an aborted run therefore still carries the minimum pair).
    receipt = {
        "flow": flow,
        "utc": utc_now(),
        "date_utc": utc_now(),
        "status": "OPEN",
        "run_dir": os.path.basename(rd),
        "law": LAW_SOURCE,
        "honesty": HONESTY,
    }
    with open(os.path.join(rd, "receipt.json"), "w") as f:
        json.dump(receipt, f, indent=2)
    with open(os.path.join(rd, "artifacts.jsonl"), "a"):
        pass
    print(rd)
    return 0


def cmd_note(args):
    rd = resolve_run_dir(args.run_dir)
    print(log_event(rd, args.message))
    return 0


def cmd_capture(args):
    rd = resolve_run_dir(args.run_dir)
    srcs = args.src
    if args.as_name and len(srcs) != 1:
        raise SystemExit("error: --as NAME works with exactly one --src")
    names = []
    for src in srcs:
        sp = os.path.abspath(src)
        if not os.path.isfile(sp):
            raise SystemExit("error: source file not found: %s" % sp)
        name = args.as_name or os.path.basename(sp)
        if name in RESERVED:
            raise SystemExit("error: %r is a reserved run-dir name" % name)
        dst = os.path.join(rd, name)
        # L1: never overwrite — evidence is copied once; re-capture under a
        # new name (or a new run dir), never into the same slot.
        if os.path.exists(dst):
            raise SystemExit("error: %s already exists in run dir; never "
                             "re-run into existing evidence" % name)
        shutil.copy2(sp, dst)
        entry = {
            "name": name,
            "source": sp,
            "sha256": sha256_file(dst),
            "bytes": os.path.getsize(dst),
            "captured_utc": utc_now(),
        }
        append_artifact(rd, entry)
        log_event(rd, "CAPTURE %s <- %s sha256=%s bytes=%d"
                  % (name, sp, entry["sha256"], entry["bytes"]))
        names.append(name)
    print("\n".join(names))
    return 0


def cmd_verdict(args):
    rd = resolve_run_dir(args.run_dir)
    if args.stdin:
        text = sys.stdin.read()
        src_label = "stdin"
    else:
        if not args.from_file:
            raise SystemExit("error: verdict needs --from FILE or --stdin")
        fp = os.path.abspath(args.from_file)
        if not os.path.isfile(fp):
            raise SystemExit("error: tool output file not found: %s" % fp)
        with open(fp, "r", errors="replace") as f:
            text = f.read()
        src_label = fp
    label = re.sub(r"[^A-Za-z0-9._-]", "-", args.label)
    # L3: a verdict only in stdout is not archived → archive the tool output
    # itself, then extract the verdict lines into verdict-lines.txt.
    raw_name = "verdict-input-%s.txt" % label
    if os.path.exists(os.path.join(rd, raw_name)):
        raise SystemExit("error: %s already exists in run dir" % raw_name)
    with open(os.path.join(rd, raw_name), "w") as f:
        f.write("source: %s\ncaptured_utc: %s\n---\n" % (src_label, utc_now()))
        f.write(text)
    pat = re.compile(args.grep)
    lines = []
    for line in text.splitlines():
        if pat.search(line):
            lines.append(line[:2000] + ("…[truncated]" if len(line) > 2000 else ""))
    vname = "verdict-lines.txt"
    with open(os.path.join(rd, vname), "a") as f:
        f.write("# %s (source: %s, captured_utc: %s)\n" % (label, src_label, utc_now()))
        for line in lines:
            f.write(line + "\n")
    for name in (raw_name, vname):
        append_artifact(rd, {
            "name": name,
            "source": src_label,
            "sha256": sha256_file(os.path.join(rd, name)),
            "bytes": os.path.getsize(os.path.join(rd, name)),
            "captured_utc": utc_now(),
        })
    log_event(rd, "VERDICT-CAPTURE %s: %d line(s) from %s" % (label, len(lines), src_label))
    print("\n".join(lines) if lines else "(no matching verdict lines)")
    return 0


def checklist_status(run_dir, flow):
    rows = []
    names = sorted(n for n in os.listdir(run_dir)
                   if os.path.isfile(os.path.join(run_dir, n)))
    for pattern, desc in checklist_for(flow):
        hit = next((n for n in names if fnmatch.fnmatchcase(n, pattern)), None)
        rows.append({"expected": pattern, "description": desc,
                     "present_as": hit})
    return rows


def cmd_close(args):
    rd = resolve_run_dir(args.run_dir)
    rp = os.path.join(rd, "receipt.json")
    with open(rp) as f:
        receipt = json.load(f)
    if receipt.get("status") == "CLOSED":
        raise SystemExit("error: %s is already CLOSED (%s); never rewrite a "
                         "closed receipt — open a new run dir"
                         % (rp, receipt.get("verdict")))
    if args.verdict not in VERDICTS:
        raise SystemExit("error: verdict must be one of %s" % ", ".join(VERDICTS))
    flow = receipt.get("flow", "unknown")
    artifacts = read_artifacts(rd)
    vlines_path = os.path.join(rd, "verdict-lines.txt")
    vlines = []
    if os.path.exists(vlines_path):
        with open(vlines_path) as f:
            vlines = [l.rstrip("\n") for l in f if l.strip() and not l.startswith("#")]
    rows = checklist_status(rd, flow)
    missing = [r["expected"] for r in rows if r["present_as"] is None]
    receipt.update({
        "utc": utc_now(),
        "date_utc": utc_now(),
        "status": "CLOSED",
        "verdict": args.verdict,
        "notes": args.notes or "",
        "artifacts": artifacts,
        "verdict_lines": vlines,
        "expected_artifacts": rows,
        "missing_expected_artifacts": missing,
        "events_log_lines": sum(1 for _ in open(os.path.join(rd, "events.log"))),
        "honesty": HONESTY,
    })
    with open(rp, "w") as f:
        json.dump(receipt, f, indent=2)
    log_event(rd, "RUN-CLOSE verdict=%s artifacts=%d missing_expected=%d"
              % (args.verdict, len(artifacts), len(missing)))
    # Session INDEX.md — one row per closed run.
    run_root = os.path.dirname(rd)
    index = os.path.join(run_root, "INDEX.md")
    if not os.path.exists(index):
        with open(index, "w") as f:
            f.write("# SESSION INDEX — return-session run archive\n\n"
                    "Law: `%s` §1 (runs/<YYYYMMDD-HHMMSS>-<flow>/; "
                    "receipt.json is each run's index; failed/aborted runs "
                    "keep their dirs; nothing is deleted).\n\n"
                    "| utc_closed (UTC) | flow | run dir | verdict | artifacts "
                    "| missing expected |\n|---|---|---|---|---|---|\n"
                    % LAW_SOURCE)
    with open(index, "a") as f:
        f.write("| %s | %s | %s | %s | %d | %s |\n"
                % (receipt["utc"], flow, os.path.basename(rd), args.verdict,
                   len(artifacts),
                   ", ".join(missing) if missing else "(none)"))
    print("closed %s verdict=%s artifacts=%d missing_expected=%d"
          % (os.path.basename(rd), args.verdict, len(artifacts), len(missing)))
    print("index: %s" % index)
    return 0


def cmd_checklist(args):
    flow = args.flow
    print("expected artifacts for flow=%s (advisory; source: %s §2-§3)"
          % (flow, LAW_SOURCE))
    for pattern, desc in checklist_for(flow):
        print("  %-34s %s" % (pattern, desc))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("start", help="stamp a fresh run dir")
    p.add_argument("--flow", required=True)
    p.add_argument("--run-root", default=DEFAULT_RUN_ROOT)
    p.set_defaults(fn=cmd_start)

    p = sub.add_parser("note", help="append a step line to events.log")
    p.add_argument("--run-dir", required=True)
    p.add_argument("message")
    p.set_defaults(fn=cmd_note)

    p = sub.add_parser("capture", help="copy fetched evidence into the run dir")
    p.add_argument("--run-dir", required=True)
    p.add_argument("--src", action="append", required=True)
    p.add_argument("--as", dest="as_name", default=None)
    p.set_defaults(fn=cmd_capture)

    p = sub.add_parser("verdict", help="capture tool output + extract verdict lines")
    p.add_argument("--run-dir", required=True)
    p.add_argument("--from", dest="from_file", default=None)
    p.add_argument("--stdin", action="store_true")
    p.add_argument("--grep", default=r"(?i)(pass|fail|verdict|inconclusive|status)")
    p.add_argument("--label", default="tool")
    p.set_defaults(fn=cmd_verdict)

    p = sub.add_parser("close", help="write final receipt + INDEX row")
    p.add_argument("--run-dir", required=True)
    p.add_argument("--verdict", required=True, choices=VERDICTS)
    p.add_argument("--notes", default="")
    p.set_defaults(fn=cmd_close)

    p = sub.add_parser("checklist", help="print expected artifacts for a flow")
    p.add_argument("--flow", required=True)
    p.set_defaults(fn=cmd_checklist)

    args = ap.parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
