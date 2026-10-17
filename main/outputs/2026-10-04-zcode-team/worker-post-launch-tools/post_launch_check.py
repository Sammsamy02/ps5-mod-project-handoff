#!/usr/bin/env python3
"""post_launch_check.py — composition-5 post-launch evidence classifier.

Run by the controller AFTER the user launches Townfall (records appear on the
console FTP ~1 min after launch). Pulls the three evidence files over anonymous
FTP (ONE FRESH curl connection per fetch, bounded timeouts), diffs
/data/plugins/UE4SS.log against the recorded 3-line baseline, parses
runtime-status.txt state order, and prints ONE verdict line plus a recommended
next action.

Evidence sources (console):
  ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log
  ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/runtime-status.txt
  ftp://ps5-host-not-configured.invalid:2121/data/townfall_runtime_aligned_entry.log

Verdicts (one per run, precedence top-down):
  FETCH_FAILED      all three fetches failed -> RETRY guidance (exit 4, no crash)
  FAILED_<stage>    'modbridge module_start failed:' line(s) in UE4SS.log
  FAILED_STATUS     runtime-status.txt shows failed (no/other UE4SS failure line)
  MODS_STARTED      fresh status last state mods-started (best case)
  ENGINE_READY      fresh status last state engine-ready
  ACTIVATED         fresh status last state activated
  STOPPED           fresh status last state stopped (unload/stop path, F10)
  UNKNOWN_NEW_LINES no status/failure evidence but non-baseline new log lines
  NOT_STARTED       nothing beyond baseline + stale probe repeats (F0/fresh window)

Standard library only. Testability: --from-dir DIR reads fixture files
(UE4SS.log, runtime-status.txt, townfall_runtime_aligned_entry.log) instead of
FTP; --baseline-file overrides the embedded baseline; --json emits machine-
readable output; --dry-run prints the exact curl commands and exits.

Baseline provenance (embedded verbatim, sha256 per line):
  L1 sha256 02c1950e2add7f1d62ed8de96890baac81c7db43751d4e4894d86b0d88b27a73
     Verbatim ledger record: worker-composition-5-rebuild/
     transport-ledger.composition5-mirror.json -> zcode_path_test_target_run
     .ue4ss_log.
  L2 sha256 a2a609ca32ae5d01bea92e88f3d1caf803451ccd9fd0b11f6f000d1801814d54
     RECONSTRUCTED (not a verbatim console readback): exact emit format from
     worker-minimal-path-test/src/path_test_entry.c:115-146 (lowercase hex,
     '0123456789abcdef' table) + the 16 bytes recorded in ledger
     zcode_path_test_target_run.activation_root (hex 6d6f64666f7267652d6c6f61
     646c6973 = "modforge-loadlis"). Classification does not depend on the
     exact bytes: the ACTIVATION_TEST: prefix rule below absorbs any drift.
  L3 sha256 ee8c12fa6b4f30ed68bf80f4dbedb7b43d4b861f6953e446d4ecaf8379556e71
     Verbatim ledger record: same ledger -> zcode_sdg2_target_run.line.
  Combined (LF-terminated blob) sha256
     8cf921838db920329215252ade8726cf8670b16ffb2ceeb4258ba4bf976fb58f
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

# ---------------------------------------------------------------------------
# Embedded baseline: the 3 known lines of /data/plugins/UE4SS.log before the
# composition-5 launch (2 path-test probe lines + 1 sdg2 DIAG line).
# ---------------------------------------------------------------------------
BASELINE_LINES = [
    "PATH_TEST: /data/plugins/UE4SS.log writable at 8866785633337 monotonic-ns",
    "ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: "
    "6d6f64666f7267652d6c6f61646c6973",
    "DIAG: module_start reached at 1791155755.879 pid=298 build=sdg2-6f404cee6b94 "
    "variant=imp",
]

BASELINE_SHAS = [hashlib.sha256(l.encode()).hexdigest() for l in BASELINE_LINES]

# Stale probe line prefixes (freshness oracle O3, FIRST-RUN-PLAYBOOK §4):
# repeats of these are NOT composition-5 output and never count as new lines.
PROBE_PREFIXES = (
    "PATH_TEST: ",
    "ACTIVATION_TEST: ",
    "DIAG: module_start reached at ",
)

# The wired entry's failure line (entry:166-192, ACTIVATION-PREDICTION §5):
#   modbridge module_start failed: <stage> code=<n> detail=<sanitized>
FAILURE_LINE_RE = re.compile(
    r"^(?:\[t=[0-9]+ms pid=[0-9]+\] )?modbridge module_start failed: (?P<stage>\S+) code=(?P<code>-?\d+)"
    r"(?: detail=(?P<detail>.*))?$"
)

# Observer (aligned-entry diagnostic channel) event line:
#   [PPSA33286] modbridge-runtime-aligned-entry/v1 build=<hex> <tag> value=0x<hex>
OBSERVER_EVENT_RE = re.compile(r"\b(?P<tag>[A-Za-z][A-Za-z0-9_]*) value=0x(?P<value>[0-9a-fA-F]+)")

# runtime-status.txt grammar (mfa .h:137-148 via FIRST-RUN-PLAYBOOK §1.3):
#   modforge-runtime-status\t1
#   session\t<pid>\t<monotonic ms at open>
#   state\t<seq>\t<state>\t<code>\t<detail>       (last line current)
STATUS_STATES_IN_ORDER = ("activated", "engine-ready", "mods-started")

# Freshness oracle (CLAIM freshness_oracle): status session CLOCK_MONOTONIC ms
# is comparable with the PATH_TEST CLOCK_MONOTONIC ns recorded in the ledger.
MONO_NS_AT_PATH_TEST = 8866785633337
FRESH_SESSION_MS_THRESHOLD = MONO_NS_AT_PATH_TEST // 1_000_000  # 8866785 ms

# mfa_result names + guidance (ACTIVATION-PREDICTION §1 refusal table).
MFA_GUIDANCE = {
    -1: ("MFA_E_INVALID",
         "Impossible from the delivered entry (root/title hardcoded): reconcile "
         "which PRX actually ran before acting."),
    -2: ("MFA_E_NO_LOADLIST",
         "/data timing window: loadlist.txt not openable at module_start (audit "
         "UNKNOWN #1). Verify /data/modbridge/roots/PPSA33286/loadlist.txt is "
         "still staged via FTP readback; if present, the open raced the /data "
         "mount — relaunch once before re-staging anything."),
    -3: ("MFA_E_LOADLIST",
         "loadlist.txt grammar/content refused (detail names the line/field). "
         "Regenerate from worker-loadlist-final/final-package and re-stage; do "
         "not hand-edit on console."),
    -4: ("MFA_E_TITLE",
         "loadlist title != PPSA33286 (detail path = the list's title): a "
         "wrong/foreign loadlist is staged. Re-stage the 446-B final-package "
         "loadlist.txt."),
    -5: ("MFA_E_LIMIT",
         "A size/count/path limit tripped (detail says which). Only plausible "
         "via an edited loadlist — compare against final-package."),
    -6: ("MFA_E_OPEN",
         "loadlist/file open failure: staged-vs-final-package divergence; the "
         "detail path names the offending file (first path-sorted candidate: "
         "ue4ss/MemberVariableLayout.ini). Diff the named file over FTP against "
         "worker-loadlist-final/final-package and re-stage exact bytes."),
    -7: ("MFA_E_READ",
         "read() failed mid-file. Retry the launch once; if reproducible, check "
         "the named file's staged state."),
    -8: ("MFA_E_SIZE",
         "Size drift: staged file is shorter/longer than the loadlist says (mfa "
         "verifies sizes only). The detail path names the file — re-stage the "
         "exact final-package bytes for it."),
    -9: ("MFA_E_MEMORY",
         "Out of memory inside activation (not expected at ~36 KB). Capture "
         "full log and reconcile with the controller."),
    -10: ("MFA_E_CANCELLED",
          "Unexpected: entry passes cancel=NULL. Reconcile which entry binary "
          "is resident."),
    -11: ("MFA_E_DEADLINE",
          "Unexpected: entry passes deadline=0 (disabled). Reconcile which "
          "entry binary is resident."),
    -12: ("MFA_E_DUPLICATE",
          "Duplicate path in loadlist.txt — regenerate from final-package."),
}

# Bridge stage codes (bridge.h:36-41 via FIRST-RUN-PLAYBOOK §3) for
# runtime_start_failed code=<n>.
BRIDGE_GUIDANCE = {
    1: ("CONVERSION",
        "bridge stage 1 CONVERSION: loader-facts capture/conversion refused "
        "(VMMAP-first + vm-scan). The detail is a convert message (e.g. "
        "'vm-scan image span is empty or unrepresentable'); the capture route "
        "differed from rehearsal — report detail verbatim to the controller."),
    2: ("CONTEXT",
        "bridge stage 2 CONTEXT: context validation threw — "
        "/data/plugins/modbridge_supplier_composition.prx or the observed "
        "game exe was missing/invalid at check time. Verify the PRX path "
        "exists via FTP; if detail contains 'unregistered bytes' this is the "
        "known launch-8 read-lease gate (r11 fix) — report verbatim, do not "
        "re-stage on this signature alone."),
    3: ("START",
        "bridge stage 3 START: prepare/start threw (F5) — detail is the "
        "exception text or 'returned null owner'. The UE4SS.log banner lines "
        "show how far UE4SSProgram construction got."),
    4: ("INTERNAL",
        "bridge stage 4 INTERNAL: internal start-stage error; report detail "
        "verbatim."),
}

STAGE_GUIDANCE = {
    "diag_channel_open_failed":
        "Diagnostic channel open failed before wiring (postmortem -22). Game "
        "continues UNMODDED. NOTE: this line only appears with the "
        "channel-postmortem 3-line patch resident — the delivered entry is "
        "SILENT on -22 (absence of this line never excludes -22). Check "
        "/data/townfall_runtime_aligned_entry.log accessibility.",
    "init_thread_failed":
        "Init-thread failure, NOT a bridge conversion refusal (outcome #8 "
        "family): conversion already succeeded when this signature fires "
        "(banner + runtime_start_ok 0x0 in the aligned-entry log). Do NOT "
        "run bridge stage-1 CONVERSION vm-scan triage on this detail; report "
        "the status detail verbatim and triage the init-thread scan path.",
    "runtime_stop_failed":
        "Stop path failure (F10): check observer runtime_stop_result value and "
        "init-thread join; owner stop reported failure.",
    "seeds_refused":
        "Seed-table arming refusal (launch-9 expected terminal; LAUNCH10-MATRIX "
        "D1 terminals T-A '[Seeds] Required-slot seed table incomplete' / T-REF "
        "'[Seeds] REFUTED'): loud init-thread throw acked failed(1,<what>) — NOT "
        "a bridge conversion refusal and NOT a 'modbridge module_start failed:' "
        "line; nothing bound, game continues unmodded. The [Seeds] line in "
        "UE4SS.log new_lines above is the terminal evidence. Triage = seed-table "
        "derivation (worker-seed-table-derivation; LAUNCH9-PREDICTION §4-#1/#2/"
        "#5), then a NEW launch for a new evidence window. Never run bridge "
        "stage-1 CONVERSION vm-scan triage on this detail.",
}

INTERESTING_OBSERVER_TAGS = (
    "module_start_entered", "probe_lifecycle_result", "runtime_wiring_begin",
    "mfa_activate_failed", "mfa_activate_os_error", "mfa_activated_low",
    "mfa_activated_high", "runtime_start_skipped", "runtime_start_ok",
    "runtime_start_failed", "module_start_return", "runtime_stop_begin",
    "runtime_stop_result", "runtime_stop_failed",
)

FETCH_FILES = [
    ("ue4ss_log", "/data/plugins/UE4SS.log", "UE4SS.log"),
    ("runtime_status", "/data/modbridge/roots/PPSA33286/runtime-status.txt",
     "runtime-status.txt"),
    ("observer_log", "/data/townfall_runtime_aligned_entry.log",
     "townfall_runtime_aligned_entry.log"),
]


# ---------------------------------------------------------------------------
# Fetch layer
# ---------------------------------------------------------------------------
class FetchResult:
    def __init__(self, key, path, ok, text=None, error=None, source="ftp"):
        self.key = key
        self.path = path
        self.ok = ok
        self.text = text
        self.error = error
        self.source = source

    def lines(self):
        if not self.ok or self.text is None:
            return []
        # Strip CR (defensive: never enable ASCII FTP transfers) and split.
        return [l for l in self.text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]


def curl_command(host, port, path, timeout, max_bytes):
    return [
        "curl", "-fsS", "--connect-timeout", "10",
        "--max-time", str(timeout),
        "--max-filesize", str(max_bytes),
        "ftp://%s:%s%s" % (host, port, path),
    ]


def fetch_ftp(host, port, key, path, timeout, max_bytes):
    """One fresh connection per fetch: a separate curl process per file."""
    cmd = curl_command(host, port, path, timeout, max_bytes)
    try:
        proc = subprocess.run(cmd, capture_output=True, timeout=timeout + 10)
    except FileNotFoundError:
        return FetchResult(key, path, False, error="curl not found on PATH")
    except subprocess.TimeoutExpired:
        return FetchResult(key, path, False, error="curl timed out after %ss" % timeout)
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", "replace").strip().splitlines()
        reason = err[-1] if err else ("curl exit %d" % proc.returncode)
        return FetchResult(key, path, False,
                           error="curl exit %d: %s" % (proc.returncode, reason))
    return FetchResult(key, path, True,
                       text=proc.stdout.decode("utf-8", "replace"))


def fetch_from_dir(directory, key, path, local_name):
    full = os.path.join(directory, local_name)
    if not os.path.isfile(full):
        return FetchResult(key, path, False, source="dir",
                           error="fixture missing: %s" % full)
    with open(full, "rb") as fh:
        return FetchResult(key, path, True, source="dir",
                           text=fh.read().decode("utf-8", "replace"))


def do_fetches(args):
    results = {}
    if args.from_dir:
        for key, path, local in FETCH_FILES:
            results[key] = fetch_from_dir(args.from_dir, key, path, local)
    elif args.dry_run:
        for key, path, _local in FETCH_FILES:
            print(" ".join(curl_command(args.host, args.port, path,
                                        args.timeout, args.max_bytes)))
        sys.exit(0)
    else:
        for key, path, _local in FETCH_FILES:
            results[key] = fetch_ftp(args.host, args.port, key, path,
                                     args.timeout, args.max_bytes)
    return results


# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------
def is_probe_repeat(line):
    return line.startswith(PROBE_PREFIXES)


def parse_failure_line(line):
    m = FAILURE_LINE_RE.match(line)
    if not m:
        return None
    detail = m.group("detail") or ""
    detail = detail.strip()
    parts = detail.split(" ", 1) if detail else ["", ""]
    return {
        "stage": m.group("stage"),
        "code": int(m.group("code")),
        "detail": detail,
        "result_name": parts[0],
        "message": parts[1] if len(parts) > 1 else "",
        "raw": line,
    }


def parse_status(lines):
    """Returns dict with header ok, session(pid, ms, fresh), ordered states,
    last state, ordering anomalies. 'failed'/'stopped' are terminal states
    outside the activated->engine-ready->mods-started ladder."""
    out = {
        "present": bool(lines), "header_ok": False, "session_pid": None,
        "session_ms": None, "fresh": None, "states": [], "last": None,
        "anomalies": [],
    }
    state_rows = []
    for ln in lines:
        if ln == "modforge-runtime-status\t1":
            out["header_ok"] = True
        elif ln.startswith("session\t"):
            parts = ln.split("\t")
            if len(parts) >= 3:
                try:
                    out["session_pid"] = int(parts[1])
                    out["session_ms"] = int(parts[2])
                except ValueError:
                    out["anomalies"].append("unparseable session line: %r" % ln)
        elif ln.startswith("state\t"):
            parts = ln.split("\t")
            if len(parts) >= 4:
                try:
                    row = {"seq": int(parts[1]), "state": parts[2]}
                    row["code"] = int(parts[3]) if len(parts) > 3 else None
                except ValueError:
                    out["anomalies"].append("unparseable state line: %r" % ln)
                    continue
                row["detail"] = parts[4] if len(parts) > 4 else ""
                row["raw"] = ln
                state_rows.append(row)
    if out["session_ms"] is not None:
        out["fresh"] = out["session_ms"] > FRESH_SESSION_MS_THRESHOLD
    out["states"] = state_rows
    out["last"] = state_rows[-1] if state_rows else None

    # Ordering check over the activated->engine-ready->mods-started ladder.
    ladder = [r for r in state_rows if r["state"] in STATUS_STATES_IN_ORDER]
    seen = [STATUS_STATES_IN_ORDER.index(r["state"]) for r in ladder]
    if seen != sorted(seen):
        out["anomalies"].append(
            "ladder states out of order: %s" % [r["state"] for r in ladder])
    if len(ladder) != len(set(r["state"] for r in ladder)):
        out["anomalies"].append(
            "duplicate ladder state in one session (unexpected: O_TRUNC "
            "replaces per session)")
    return out


def parse_observer(lines):
    total = len([l for l in lines if l.strip()])
    tag_counts = {}
    tag_last = {}
    for ln in lines:
        m = OBSERVER_EVENT_RE.search(ln)
        if not m:
            continue
        tag = m.group("tag")
        tag_counts[tag] = tag_counts.get(tag, 0) + 1
        tag_last[tag] = "0x" + m.group("value")
    interesting = {t: {"count": tag_counts[t], "last": tag_last[t]}
                   for t in INTERESTING_OBSERVER_TAGS if t in tag_counts}
    other = {t: c for t, c in sorted(tag_counts.items())
             if t not in INTERESTING_OBSERVER_TAGS}
    return {"total_lines": total, "events": total and sum(tag_counts.values()),
            "interesting": interesting, "other_tags": other}


def guidance_for_failure(f):
    """Recommended next action for one parsed failure line (mfa-audit +
    channel-postmortem signatures)."""
    stage, code = f["stage"], f["code"]
    if stage in STAGE_GUIDANCE:
        return STAGE_GUIDANCE[stage]
    if stage == "mfa_activate_failed" and code in MFA_GUIDANCE:
        return MFA_GUIDANCE[code][1]
    if stage == "runtime_start_failed" and code in BRIDGE_GUIDANCE:
        return BRIDGE_GUIDANCE[code][1]
    if stage == "runtime_start_failed":
        return ("runtime_start_failed code=%d is not a known bridge stage code "
                "(0-4): report verbatim to the controller." % code)
    if stage == "mfa_activate_failed":
        return ("Unknown mfa code %d: report verbatim to the controller "
                "(ACTIVATION-PREDICTION refusal table does not list it)." % code)
    return "Unknown failure stage %r: report the raw line verbatim." % stage


# ---------------------------------------------------------------------------
# Classification
# ---------------------------------------------------------------------------
def classify(results, baseline_lines):
    rep = {
        "fetches": {}, "baseline": {}, "probe_repeats": 0, "failure_lines": [],
        "new_lines": [], "status": None, "observer": None, "warnings": [],
        "verdict": None, "next": "", "detail_lines": [],
    }
    for key, path, _local in FETCH_FILES:
        r = results[key]
        rep["fetches"][key] = {
            "path": path, "ok": r.ok, "source": r.source,
            "bytes": len(r.text.encode("utf-8")) if r.ok and r.text is not None else None,
            "error": r.error,
        }

    all_failed = all(not r.ok for r in results.values())
    if all_failed:
        rep["baseline"] = {"expected_lines": len(baseline_lines), "missing": []}
        rep["verdict"] = "FETCH_FAILED"
        rep["next"] = (
            "RETRY: no evidence reachable (connection refused/timeout on all "
            "three). Records appear ~1 min after launch — wait ~60 s and re-run. "
            "If still down after a second retry, check the console FTP service "
            "and network before touching staging.")
        return rep

    # --- UE4SS.log diff -----------------------------------------------------
    ue = results["ue4ss_log"]
    ulines = ue.lines()
    base_set = set(baseline_lines)
    missing_baseline = [b for b in baseline_lines if b not in ulines]
    if missing_baseline and ue.ok:
        rep["warnings"].append(
            "baseline line(s) absent from fetched UE4SS.log (%d of %d): %s — "
            "the log was replaced/truncated or a different file was fetched"
            % (len(missing_baseline), len(baseline_lines),
               [m[:40] for m in missing_baseline]))
    new_lines, failures, probe_repeats = [], [], 0
    for ln in ulines:
        if ln in base_set:
            continue
        if is_probe_repeat(ln):
            probe_repeats += 1
            continue
        f = parse_failure_line(ln)
        if f:
            failures.append(f)
            continue
        if ln.strip() == "":
            continue
        new_lines.append(ln)
    rep["probe_repeats"] = probe_repeats
    rep["failure_lines"] = failures
    rep["new_lines"] = new_lines
    rep["baseline"]["expected_lines"] = len(baseline_lines)
    rep["baseline"]["missing"] = missing_baseline

    # --- status -------------------------------------------------------------
    st = parse_status(results["runtime_status"].lines())
    rep["status"] = st

    # --- observer -----------------------------------------------------------
    rep["observer"] = parse_observer(results["observer_log"].lines())

    # --- verdict (precedence) ------------------------------------------------
    # A fresh session (session-ms > path-test monotonic threshold) that reached
    # the ladder or ended 'stopped' is per-launch truth (O_TRUNC, playbook O2)
    # and outranks timestamp-less failure lines, which accumulate across
    # launches (O5). A fresh 'failed' session AGREES with failure lines (both
    # sinks fire on the same failure) and must not suppress them.
    last = st["last"]
    fresh_session = bool(st["present"] and st["fresh"])
    last_state = last["state"] if last is not None else None
    fresh_progressed = fresh_session and last_state in STATUS_STATES_IN_ORDER + ("stopped",)
    if failures and not fresh_progressed:
        lastf = failures[-1]
        rep["verdict"] = "FAILED_%s" % lastf["stage"]
        rep["next"] = guidance_for_failure(lastf)
        if len(failures) > 1:
            rep["detail_lines"].append(
                "%d failure lines accumulated (append-only log; playbook O5): "
                "first code=%d, last code=%d — verdict from the LAST line"
                % (len(failures), failures[0]["code"], lastf["code"]))
        if st["present"] and last is not None and last["state"] == "failed":
            rep["detail_lines"].append(
                "status last state also failed (code=%s): the two sinks agree"
                % last.get("code"))
        return rep

    if fresh_progressed:
        state = last_state
        if failures:
            rep["warnings"].append(
                "CONFLICT: fresh status session reached %r but %d UE4SS.log "
                "failure line(s) exist; timestamp-less lines may predate this "
                "session (O5) — re-run to confirm stability" % (state, len(failures)))
        if state == "stopped":
            rep["verdict"] = "STOPPED"
            rep["next"] = (
                "Runtime stopped (clean unload or F10). Check observer "
                "runtime_stop_result: nonzero => owner stop reported failure "
                "(MRS_ERR_STOP_EXN path). Historical failure lines, if any, "
                "predate this session.")
            return rep
        rep["verdict"] = {"mods-started": "MODS_STARTED",
                          "engine-ready": "ENGINE_READY",
                          "activated": "ACTIVATED"}[state]
        lua_hits = [l for l in new_lines if "[TownfallItemSpawner]" in l]
        if state == "mods-started":
            rep["next"] = (
                "Full startup ladder reached. Check UE4SS.log for "
                "'[Lua] [TownfallItemSpawner]' prints (rung 4: mod actually "
                "running — %d found in new lines) before any rung claim; then "
                "proceed to the playbook rung ladder." % len(lua_hits))
        elif state == "engine-ready":
            rep["next"] = (
                "Init thread finished Unreal setup; mods-started ack follows "
                "immediately in-program — re-run in ~30-60 s. If status stays "
                "frozen here (F8), watch file mtimes for a bounded window "
                "before treating as a hang.")
        else:  # activated
            rep["next"] = (
                "Activation consumed; init thread still working (or F7/F8 "
                "hang). Re-run in ~60 s. Frozen at activated with a static "
                "UE4SS.log tail = bounded-wait hang check (F8).")
        return rep

    if st["present"] and last is not None and last["state"] == "failed":
        rep["verdict"] = "FAILED_STATUS"
        rep["next"] = (
            "Status session says failed with NO matching UE4SS.log failure "
            "line: status is the session truth (O_TRUNC, last session wins). "
            "Shape check — activated then failed = F6/F7 silent traps "
            "(start() refused or init thread threw; detail carries the "
            "reason); single failed state line = terminal entry path whose "
            "log append also failed (a7). Code-specific: %s"
            % guidance_for_failure({"stage": stage_hint(last), "code": last.get("code"),
                                    "detail": last.get("detail", "")}))
        return rep

    if st["present"] and last is not None and last["state"] == "stopped":
        rep["verdict"] = "STOPPED"
        rep["next"] = (
            "Runtime stopped (unload or F10). Check observer "
            "runtime_stop_result: nonzero => owner stop reported failure "
            "(MRS_ERR_STOP_EXN path).")
        return rep

    if new_lines:
        rep["verdict"] = "UNKNOWN_NEW_LINES"
        rep["next"] = (
            "Non-baseline, non-failure UE4SS.log lines with no status "
            "evidence. Compare against the FIRST-RUN-PLAYBOOK §2 trace: "
            "runtime banner lines = construction progress (likely mid-launch "
            "— re-run in ~60 s); anything else = an unexpected writer — report "
            "raw lines to the controller.")
        return rep

    rep["verdict"] = "NOT_STARTED"
    if not st["present"]:
        rep["next"] = (
            "No new lines beyond baseline%s and no status file fetched. "
            "Records appear ~1 min after launch: wait ~60 s and re-run. If a "
            "repeat run is still empty, treat as F0 (PRX never ran): verify "
            "staging via FTP readback + ploader logs; observer "
            "module_start_entered count in the aligned-entry log is the "
            "launch counter." % (" + %d stale probe repeat(s)" % probe_repeats
                                 if probe_repeats else ""))
    else:
        rep["next"] = (
            "Status exists but is stale/empty (session ms <= path-test "
            "baseline or no state lines) and UE4SS.log shows nothing new: "
            "either mid-launch window (re-run in ~60 s) or F0 (composition-5 "
            "PRX never ran — verify staging + ploader).")
    return rep


def stage_hint(last_state_row):
    """Best-effort stage name for a status 'failed' row (for guidance lookup)."""
    code = last_state_row.get("code")
    detail = last_state_row.get("detail", "") or ""
    if code is not None and code < 0:
        return "mfa_activate_failed"  # negative codes are mfa/MRS codes
    # GAP 2 (REHEARSAL-RECEIPTS.md): init-thread outcome #8 signatures fire
    # AFTER conversion succeeded — never route them to bridge CONVERSION.
    if "PS scan timed out" in detail or "AOB scans" in detail:
        return "init_thread_failed"
    # GAP 2 family extended (launch-9 audit): loud init-thread throws reach the
    # status file as failed(1, <what>) WITHOUT any bridge failure line (ack path
    # runtime_owner.cpp:111-118) and must never route to bridge CONVERSION.
    # [Seeds] is launch-9's predicted terminal (T-A/T-REF); the GMalloc
    # PostInitialize throw (T-GM) and the tick-detour registration refusal
    # (T-WR) are the same ack family.
    if "[Seeds]" in detail:
        return "seeds_refused"
    if ("GMalloc is uninitialized" in detail
            or "initializer callback registration failed" in detail):
        return "init_thread_failed"
    if code in BRIDGE_GUIDANCE:
        return "runtime_start_failed"
    if "activate" in detail:
        return "mfa_activate_failed"
    return "runtime_start_failed"


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
def render(rep, sha_check):
    out = []
    out.append("== post_launch_check ==")
    for key, info in rep["fetches"].items():
        if info["ok"]:
            out.append("fetch %-14s OK    %7s B  %s" % (key, info["bytes"], info["path"]))
        else:
            out.append("fetch %-14s FAIL  %s (%s)" % (key, info["path"], info["error"]))
    for name, ok, detail in sha_check:
        out.append("baseline sha %s %s" % ("OK  " if ok else "MISMATCH", name))
    if rep["warnings"]:
        for w in rep["warnings"]:
            out.append("WARN: %s" % w)
    if rep["probe_repeats"]:
        out.append("stale probe repeats (not new): %d" % rep["probe_repeats"])
    if rep["baseline"]["missing"]:
        out.append("baseline lines missing from log: %d" % len(rep["baseline"]["missing"]))
    st = rep["status"] or {}
    if st.get("present"):
        out.append("status: session pid=%s ms=%s fresh=%s last=%s"
                   % (st.get("session_pid"), st.get("session_ms"),
                      st.get("fresh"), (st.get("last") or {}).get("state")))
        for a in st.get("anomalies", []):
            out.append("status anomaly: %s" % a)
    if rep["failure_lines"]:
        out.append("failure lines: %d" % len(rep["failure_lines"]))
        for f in rep["failure_lines"]:
            out.append("  - stage=%s code=%d detail=%s" % (f["stage"], f["code"], f["detail"]))
            out.append("    NEXT(signatures): %s" % guidance_for_failure(f))
    if rep["new_lines"]:
        out.append("new non-baseline lines: %d" % len(rep["new_lines"]))
        for l in rep["new_lines"][:20]:
            out.append("  | %s" % l)
        if len(rep["new_lines"]) > 20:
            out.append("  | ... (%d more)" % (len(rep["new_lines"]) - 20))
    if rep["observer"]:
        obs = rep["observer"]
        out.append("observer: %d non-empty lines, %s events tracked"
                   % (obs["total_lines"], obs["events"]))
        for tag, info in obs["interesting"].items():
            out.append("  %-24s n=%-4d last=%s" % (tag, info["count"], info["last"]))
    out.append("")
    out.append("VERDICT: %s" % rep["verdict"])
    out.append("NEXT: %s" % rep["next"])
    for d in rep["detail_lines"]:
        out.append("NOTE: %s" % d)
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Classify composition-5 post-launch evidence (FTP pull + "
                    "baseline diff + status parse).")
    ap.add_argument("--host", default="ps5-host-not-configured.invalid")
    ap.add_argument("--port", default="2121")
    ap.add_argument("--timeout", type=int, default=60,
                    help="per-fetch curl --max-time seconds (default 60)")
    ap.add_argument("--max-bytes", type=int, default=32 * 1024 * 1024,
                    help="per-fetch curl --max-filesize (default 32 MiB)")
    ap.add_argument("--baseline-file",
                    help="override the embedded 3-line baseline (one line each)")
    ap.add_argument("--from-dir",
                    help="HOST-TEST MODE: read UE4SS.log, runtime-status.txt, "
                         "townfall_runtime_aligned_entry.log from this local "
                         "directory instead of FTP")
    ap.add_argument("--json", action="store_true", help="JSON output")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the exact curl commands and exit (no fetch)")
    args = ap.parse_args(argv)

    if args.baseline_file:
        try:
            with open(args.baseline_file, "r", encoding="utf-8") as fh:
                baseline = [l.rstrip("\n").rstrip("\r") for l in fh
                            if l.strip()]
        except OSError as exc:
            print("error: cannot read --baseline-file: %s" % exc, file=sys.stderr)
            return 2
        if not baseline:
            print("error: --baseline-file is empty", file=sys.stderr)
            return 2
    else:
        baseline = list(BASELINE_LINES)

    sha_check = [(l[:32] + "...", hashlib.sha256(l.encode()).hexdigest() == s, s)
                 for l, s in zip(baseline, BASELINE_SHAS)] if baseline == BASELINE_LINES else []

    results = do_fetches(args)
    rep = classify(results, baseline)
    rep["baseline_file"] = args.baseline_file or "(embedded)"
    rep["baseline_shas_verified"] = all(ok for _n, ok, _s in sha_check) if sha_check else None

    if args.json:
        print(json.dumps(rep, indent=1))
    else:
        print(render(rep, sha_check))

    if rep["verdict"] == "FETCH_FAILED":
        return 4
    return 0


if __name__ == "__main__":
    sys.exit(main())
