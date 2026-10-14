#!/usr/bin/env python3
"""rung_check.py — composition-5 acceptance-rung asserter (console files only).

Extends the worker-post-launch-tools/post_launch_check.py pattern (read, not
modified): anonymous FTP at ftp://ps5-host-not-configured.invalid:2121, ONE FRESH curl per file,
bounded timeouts, stdlib only, --from-dir fixture mode, --dry-run, --json.
Adds: a second snapshot of the C2 JSONL (default 3 s apart), rung verdicts,
and an FTP LIST discovery fallback.

Console paths (traced to writer constants; see PATH-MAP.md for file:line):
  C2  /data/modbridge-ingame-facts.jsonl                    (IgSink: record_sink.c:15)
  L   /data/plugins/UE4SS.log                               (UE4SSProgram.cpp:256-258,778-784)
  S   /data/modbridge/roots/PPSA33286/runtime-status.txt    (modforge_activation.c:601 + entry:66)
  D1  /data/townfall_runtime_aligned_entry.log              (lineage-overlay probe_config.h:3)
  D2  /data/townfall_runtime_aligned_observer.log           (observer PRX probe_config.h:6)

Rung definitions (EVIDENCE-SPEC.md; worker-tick-callback-audit rung-1 C2):
  RUNG1  status engine-ready-or-later AND status session FRESH (O2 gate:
         session CLOCK_MONOTONIC ms > FRESH_SESSION_MS_THRESHOLD, the same
         oracle post_launch_check.py uses — evidence from a PRIOR launch is
         refused as INCONCLUSIVE-STALE, never PASS) AND C2 present AND
         ticks_accepted strictly increasing across two snapshots AND
         game_thread_id_key stable non-zero AND >=1 counted guard refusal.
  RUNG2  a terminal native_query_harness record (FormatRecordJsonLine shape):
         status=completed, count_source=engine_uobject_array,
         counts.recorded=2, both counts>10000, monotonic equal|non_decreasing,
         identity_checks=2, key non-zero, ticks.refusals_between=0,
         threshold=10000.
  RUNG3-5  placeholders: count the discriminating UE4SS.log / C2 lines
         (rung 3 scan tails + FC1 records; rung 4 load line; rung 5 spawn
         lines). Never a pass/fail on their own; C4 capture is out of scope.

LOUD CAVEAT: no target-side caller of FormatRecordJsonLine exists in the
composed PRX (worker-query-s2-arm/integration-patch/WIRING.md:120-124 — the
controller driver "needs no target code"). C2 only carries native_query_harness
lines once a driver/probe add-on that links record_sink.c is staged. When no
native_query_harness line is seen, rung_check.py flags UNVERIFIED_DRIVER
rather than passing silently; use --discover to list /data candidates via FTP
LIST when the expected files are absent.

Exit codes: 0 requested strict rungs all PASS (placeholders never affect it);
1 a requested strict rung FAILed or is UNVERIFIED; 2 usage/internal error;
3 requested strict rung CANNOT_ASSERT (evidence missing); 4 all fetches
failed (FTP unreachable — RETRY guidance, same as post_launch_check);
5 INCONCLUSIVE-STALE (O2 freshness gate: the status file dates the evidence
to a prior launch — session ms <= FRESH_SESSION_MS_THRESHOLD — or carries no
datable session line; nothing may be asserted, re-run in a fresh window).
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time

# ---------------------------------------------------------------------------
# Console files (key, console path, fixture basename, needs_snapshot2)
# ---------------------------------------------------------------------------
FETCH_FILES = [
    ("query_jsonl", "/data/modbridge-ingame-facts.jsonl",
     "modbridge-ingame-facts.jsonl", True),
    ("ue4ss_log", "/data/plugins/UE4SS.log", "UE4SS.log", False),
    ("runtime_status", "/data/modbridge/roots/PPSA33286/runtime-status.txt",
     "runtime-status.txt", False),
    ("entry_diag", "/data/townfall_runtime_aligned_entry.log",
     "townfall_runtime_aligned_entry.log", False),
    ("observer_log", "/data/townfall_runtime_aligned_observer.log",
     "townfall_runtime_aligned_observer.log", False),
]

# Discovery roots for --discover (FTP LIST; cheapest-first).
DISCOVER_DIRS = ["/data/", "/data/plugins/", "/data/modbridge/",
                 "/data/modbridge/roots/", "/data/modbridge/roots/PPSA33286/"]
DISCOVER_NAME_RE = re.compile(
    r"(jsonl|\.log$|runtime-status|facts|query|modbridge|townfall)", re.I)

# Status ladder (modforge_activation.h:143; last line current, O_TRUNC/session).
LADDER = ("activated", "engine-ready", "mods-started")
TERMINAL_OK = ("stopped",)  # accepted only with a prior engine-ready row

# O2 freshness oracle — SAME constant as post_launch_check.py
# (MONO_NS_AT_PATH_TEST 8866785633337 // 1_000_000). A status session ms at or
# below this predates the PATH_TEST ledger row: the files are from a PRIOR
# launch and can never prove a rung (REHEARSAL-RECEIPTS.md GAP 4 false-PASS).
FRESH_SESSION_MS_THRESHOLD = 8866785

# FormatRecordJsonLine value names (NativeQueryHarness.cpp:357-375).
QH_COMPLETED = "completed"
QH_INCONCLUSIVE = "inconclusive_below_threshold"
MONOTONIC_OK = ("equal", "non_decreasing")
GUARD_REFUSAL_KEYS = ("refused_wrong_thread", "refused_identity_unknown",
                      "refused_bindings_absent", "refused_layout_gate_closed")

# Mod / runtime log discriminators (EVIDENCE-SPEC 3.1/4.1/5.3).
MOD_TAG = "[TownfallItemSpawner]"
RUNG3_POS_TAIL = "Item database not ready. Load a save first."
RUNG3_FAIL_TAIL = "attempt to call a nil value (global 'FindFirstOf')"
RUNG4_LOAD_LINE = (MOD_TAG + " Loaded v1.0. Clean pickup-only release. "
                   "F7 opens the menu.")
RUNG5_SPAWNED = MOD_TAG + " Spawned"
RUNG5_FAILED_AT = MOD_TAG + " Failed at"
ENGINE_VERSION_LINE_RE = re.compile(r"Using engine version: (\d+)\.(\d+)")


class FetchResult:
    def __init__(self, key, path, ok, text=None, error=None, source="ftp"):
        self.key, self.path, self.ok = key, path, ok
        self.text, self.error, self.source = text, error, source

    def lines(self):
        if not self.ok or self.text is None:
            return []
        return [l for l in self.text.replace("\r\n", "\n")
                .replace("\r", "\n").split("\n")]


def curl_command(host, port, path, timeout, max_bytes):
    return ["curl", "-fsS", "--connect-timeout", "10",
            "--max-time", str(timeout), "--max-filesize", str(max_bytes),
            "ftp://%s:%s%s" % (host, port, path)]


def fetch_ftp(host, port, key, path, timeout, max_bytes):
    """One fresh connection per fetch: a separate curl process per file."""
    cmd = curl_command(host, port, path, timeout, max_bytes)
    try:
        proc = subprocess.run(cmd, capture_output=True, timeout=timeout + 10)
    except FileNotFoundError:
        return FetchResult(key, path, False, error="curl not found on PATH")
    except subprocess.TimeoutExpired:
        return FetchResult(key, path, False,
                           error="curl timed out after %ss" % timeout)
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", "replace").strip().splitlines()
        reason = err[-1] if err else ("curl exit %d" % proc.returncode)
        return FetchResult(key, path, False,
                           error="curl exit %d: %s" % (proc.returncode, reason))
    return FetchResult(key, path, True, text=proc.stdout.decode("utf-8", "replace"))


def fetch_from_dir(directory, key, path, local_name, snap2=False):
    name = local_name + ".snap2" if snap2 else local_name
    full = os.path.join(directory, name)
    if not os.path.isfile(full):
        if snap2:
            # Fixture mode: no second snapshot staged -> same file again
            # (ticks cannot increase -> RUNG1 tick condition will fail).
            return fetch_from_dir(directory, key, path, local_name, False)
        return FetchResult(key, path, False, source="dir",
                           error="fixture missing: %s" % full)
    with open(full, "rb") as fh:
        return FetchResult(key, path, True, source="dir",
                           text=fh.read().decode("utf-8", "replace"))


def do_fetches(args, snapshot=1):
    out = {}
    for key, path, local, _s2 in FETCH_FILES:
        if args.from_dir:
            out[key] = fetch_from_dir(args.from_dir, key, path, local,
                                      snap2=(snapshot == 2))
        else:
            out[key] = fetch_ftp(args.host, args.port, key, path,
                                 args.timeout, args.max_bytes)
    return out


# ---------------------------------------------------------------------------
# C2 line classification
# ---------------------------------------------------------------------------
def classify_c2(lines):
    """Returns dict with counts and extracted per-line facts."""
    rep = {"lines": 0, "nonempty": 0, "native_query_harness": [],
           "ig_envelope": 0, "unparsed": 0, "other_json": 0}
    ticks_vals, keys, refusals = [], [], {}
    for ln in lines:
        rep["lines"] += 1
        s = ln.strip()
        if not s:
            continue
        rep["nonempty"] += 1
        try:
            obj = json.loads(s)
        except ValueError:
            rep["unparsed"] += 1
            continue
        if not isinstance(obj, dict):
            rep["unparsed"] += 1
            continue
        if obj.get("record") == "native_query_harness":
            rec = {"raw": s, "obj": obj}
            ticks = obj.get("ticks") or {}
            try:
                rec["ticks_accepted"] = int(ticks.get("accepted"))
            except (TypeError, ValueError):
                rec["ticks_accepted"] = None
            key = obj.get("game_thread_id_key")
            rec["key_nonzero"] = isinstance(key, str) and key not in (
                "", "0x0000000000000000", "0x0")
            rep["native_query_harness"].append(rec)
            if rec["ticks_accepted"] is not None:
                ticks_vals.append(rec["ticks_accepted"])
            if rec["key_nonzero"]:
                keys.append(key)
            rb = ticks.get("refusals_between")
            if isinstance(rb, int) and rb > 0:
                refusals["refusals_between_counts"] = \
                    refusals.get("refusals_between_counts", 0) + rb
        elif obj.get("marker") == "modbridge.ingamefacts":
            rep["ig_envelope"] += 1
        else:
            rep["other_json"] += 1
        # Guard-refusal counters may ride ANY parsable line (counter
        # snapshots are not format-pinned; accept refused_* keys anywhere).
        for rk in GUARD_REFUSAL_KEYS:
            v = obj.get(rk)
            if isinstance(v, int) and v > 0:
                refusals[rk] = max(refusals.get(rk, 0), v)
            fld = obj.get("fields")
            if isinstance(fld, dict):
                fv = fld.get(rk)
                if isinstance(fv, int) and fv > 0:
                    refusals[rk] = max(refusals.get(rk, 0), fv)
        ta = obj.get("ticks_accepted")
        if not isinstance(ta, int):
            fld = obj.get("fields")
            ta = fld.get("ticks_accepted") if isinstance(fld, dict) else None
        if isinstance(ta, int):
            ticks_vals.append(ta)
    rep["ticks_values"] = ticks_vals
    rep["nonzero_keys"] = sorted(set(keys))
    rep["refusals"] = refusals
    return rep


# ---------------------------------------------------------------------------
# runtime-status.txt parser (grammar: modforge_activation.h:140-148)
# ---------------------------------------------------------------------------
def parse_status(lines):
    out = {"present": bool([l for l in lines if l.strip()]),
           "header_ok": False, "session_pid": None, "session_ms": None,
           "states": [], "last": None, "anomalies": []}
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
                    out["anomalies"].append("bad session line: %r" % ln)
        elif ln.startswith("state\t"):
            parts = ln.split("\t")
            if len(parts) >= 4:
                try:
                    row = {"seq": int(parts[1]), "state": parts[2],
                           "code": int(parts[3])}
                except ValueError:
                    out["anomalies"].append("bad state line: %r" % ln)
                    continue
                row["detail"] = parts[4] if len(parts) > 4 else ""
                out["states"].append(row)
            elif parts and parts[1] == "" and len(parts) >= 2:
                # short form "state\t1\tactivated\t0\t" parses above; this
                # branch only catches malformed rows
                out["anomalies"].append("short state line: %r" % ln)
    out["last"] = out["states"][-1] if out["states"] else None
    ladder = [r["state"] for r in out["states"] if r["state"] in LADDER]
    idx = [LADDER.index(s) for s in ladder]
    if idx != sorted(idx):
        out["anomalies"].append("ladder out of order: %s" % ladder)
    if len(ladder) != len(set(ladder)):
        out["anomalies"].append("duplicate ladder state in one session")
    out["ladder_seen"] = ladder
    return out


def status_session_fresh(st):
    """O2 freshness gate (REHEARSAL-RECEIPTS.md GAP 4): the status session
    line's CLOCK_MONOTONIC ms must postdate FRESH_SESSION_MS_THRESHOLD, the
    same oracle post_launch_check.py applies. A status file that dates the
    session at/below the threshold (prior launch) or carries no datable
    session line can never ground a rung PASS."""
    if not st["present"]:
        return False, "no status file (freshness undatable from status)"
    if st["session_ms"] is None:
        return False, "status has no parsable session line: session undatable"
    if st["session_ms"] > FRESH_SESSION_MS_THRESHOLD:
        return True, "session ms %d > threshold %d" % (
            st["session_ms"], FRESH_SESSION_MS_THRESHOLD)
    return False, ("session ms %d <= threshold %d: PRIOR-launch evidence"
                   % (st["session_ms"], FRESH_SESSION_MS_THRESHOLD))


def status_engine_ready_or_later(st):
    """RUNG1 gate: last state engine-ready/mods-started; stopped accepted only
    with a prior engine-ready (or later) row in the same session."""
    last = st["last"]
    if not st["present"] or last is None:
        return False, "no status file or no state lines"
    if last["state"] in ("engine-ready", "mods-started"):
        return True, "last state %s" % last["state"]
    if last["state"] == "stopped" and "engine-ready" in st["ladder_seen"]:
        return True, "stopped after engine-ready"
    if last["state"] == "failed":
        return False, "session failed (code=%s detail=%r)" % (
            last["code"], last["detail"])
    return False, "last state %r never reached engine-ready" % last["state"]


# ---------------------------------------------------------------------------
# Log-line placeholder scans (RUNG3-5)
# ---------------------------------------------------------------------------
def scan_logs(ue4ss, entry_diag, observer_log):
    def tail(res):
        return res.lines() if res and res.ok else []
    ue_lines = [l for l in tail(ue4ss) if l.strip()]
    d1_lines = [l for l in tail(entry_diag) if l.strip()]
    d2_lines = [l for l in tail(observer_log) if l.strip()]
    all_lines = ue_lines + d1_lines + d2_lines
    eng = None
    for l in ue_lines:
        m = ENGINE_VERSION_LINE_RE.search(l)
        if m:
            eng = "%s.%s" % (m.group(1), m.group(2))
    return {
        "engine_version_line": eng,
        "mod_tag_lines": sum(1 for l in all_lines if MOD_TAG in l),
        "rung3_pos_tails": sum(1 for l in all_lines if RUNG3_POS_TAIL in l),
        "rung3_fail_tails": sum(1 for l in all_lines if RUNG3_FAIL_TAIL in l),
        "rung4_load_lines": sum(1 for l in all_lines if RUNG4_LOAD_LINE in l),
        "rung5_spawned": sum(1 for l in all_lines if RUNG5_SPAWNED in l),
        "rung5_failed_at": sum(1 for l in all_lines if RUNG5_FAILED_AT in l),
        "ue4ss_ok": bool(ue4ss and ue4ss.ok),
    }


def scan_fc1(c2rep):
    n = 0
    for ln in c2rep.get("_raw_lines", []):
        if "fc1" in ln.lower():
            n += 1
    return n


# ---------------------------------------------------------------------------
# Rung verdicts
# ---------------------------------------------------------------------------
def rung1_verdict(status, c2_snap1, c2_snap2):
    conds = []

    ok, why = status_engine_ready_or_later(status)
    conds.append({"id": "status_engine_ready_or_later", "pass": ok, "why": why})

    fresh, fresh_why = status_session_fresh(status)
    conds.append({"id": "status_session_fresh", "pass": fresh, "why": fresh_why})

    present = c2_snap1["nonempty"] > 0
    conds.append({"id": "c2_present", "pass": present,
                  "why": "%d non-empty lines" % c2_snap1["nonempty"]
                  if present else "C2 missing/empty"})

    t1 = c2_snap1["ticks_values"]
    t2 = c2_snap2["ticks_values"] if c2_snap2 is not None else []
    m1 = max(t1) if t1 else None
    m2 = max(t2) if t2 else None
    adv = m1 is not None and m2 is not None and m2 > m1
    conds.append({"id": "ticks_accepted_strictly_increasing", "pass": adv,
                  "why": "snap1 max=%s snap2 max=%s" % (m1, m2)})

    keys = sorted(set(c2_snap1["nonzero_keys"]) |
                  set(c2_snap2["nonzero_keys"] if c2_snap2 else []))
    stable = len(keys) == 1
    conds.append({"id": "game_thread_id_key_stable_nonzero", "pass": stable,
                  "why": "distinct nonzero keys: %s" % (keys or ["none"])})

    ref = {}
    for src in (c2_snap1, c2_snap2) if c2_snap2 is not None else (c2_snap1,):
        for k, v in src["refusals"].items():
            ref[k] = max(ref.get(k, 0), v)
    guard = sum(v for k, v in ref.items()
                if k in GUARD_REFUSAL_KEYS) > 0 or \
        ref.get("refusals_between_counts", 0) > 0
    conds.append({"id": "at_least_one_counted_guard_refusal", "pass": guard,
                  "why": json.dumps(ref, sort_keys=True) if ref else "none seen"})

    driver_seen = bool(c2_snap1["native_query_harness"]) or \
        bool(c2_snap2 and c2_snap2["native_query_harness"])
    passed = all(c["pass"] for c in conds)
    if not c2_snap1["nonempty"] and not status["present"] and \
            not (c2_snap2 and c2_snap2["nonempty"]):
        verdict = "CANNOT_ASSERT"
        why = "no status file and no C2 content: no evidence base at all"
    elif status["present"] and not fresh:
        # O2 freshness gate (GAP 4): evidence dates to a prior launch (or is
        # undatable) — refusing is the ONLY honest verdict; a stale session
        # must never become PASS (or even FAIL: nothing failed THIS launch).
        verdict = "INCONCLUSIVE-STALE"
        why = ("O2 freshness gate: %s. The files predate this launch — no rung "
               "may be asserted from them. Re-run in a fresh launch window "
               "(~60 s after launch)." % fresh_why)
    elif not driver_seen:
        verdict = "UNVERIFIED"
        why = ("NO native_query_harness line in C2: only unpinned counter "
               "snapshots (if any). Rung 1 requires the pinned "
               "FormatRecordJsonLine record; counter movement alone is NOT "
               "rung-1 evidence (WIRING.md:120-124).")
    elif passed:
        verdict, why = "PASS", "all 6 conditions (incl. O2 freshness) + driver line seen"
    else:
        verdict = "FAIL"
        why = "; ".join(c["id"] for c in conds if not c["pass"])
    return {"verdict": verdict, "why": why, "conditions": conds,
            "driver_seen": driver_seen}


def rung2_verdict(c2_snap1, c2_snap2):
    recs = list(c2_snap1["native_query_harness"]) + \
        (c2_snap2["native_query_harness"] if c2_snap2 is not None else [])
    terminal = [r for r in recs if r["obj"].get("status") == QH_COMPLETED]
    inconclusive = [r for r in recs
                    if r["obj"].get("status") == QH_INCONCLUSIVE]
    checks = []
    if not recs:
        return {"verdict": "CANNOT_ASSERT",
                "why": "no native_query_harness line in C2 (driver not "
                       "evidenced on target)",
                "conditions": checks, "records_seen": 0,
                "inconclusive_records": len(inconclusive)}
    best = terminal[-1] if terminal else recs[-1]
    o = best["obj"]
    counts = o.get("counts") or {}
    ticks = o.get("ticks") or {}

    def chk(cid, ok, why):
        checks.append({"id": cid, "pass": bool(ok), "why": why})

    chk("status_completed", o.get("status") == QH_COMPLETED,
        "status=%r" % o.get("status"))
    chk("count_source_engine_uobject_array",
        o.get("count_source") == "engine_uobject_array",
        "count_source=%r" % o.get("count_source"))
    chk("counts_recorded_2", counts.get("recorded") == 2,
        "counts.recorded=%r" % counts.get("recorded"))
    try:
        f, s = int(counts.get("first")), int(counts.get("second"))
        chk("both_counts_gt_10000", f > 10000 and s > 10000,
            "first=%d second=%d threshold=%r" % (f, s, o.get("threshold")))
    except (TypeError, ValueError):
        chk("both_counts_gt_10000", False, "counts unparsable: %r" % (counts,))
    chk("monotonic_equal_or_nondecreasing",
        o.get("monotonic") in MONOTONIC_OK, "monotonic=%r" % o.get("monotonic"))
    chk("identity_checks_2", o.get("identity_checks") == 2,
        "identity_checks=%r" % o.get("identity_checks"))
    chk("key_nonzero", best["key_nonzero"],
        "game_thread_id_key=%r" % o.get("game_thread_id_key"))
    chk("refusals_between_0", ticks.get("refusals_between") == 0,
        "ticks.refusals_between=%r" % ticks.get("refusals_between"))
    chk("threshold_10000", o.get("threshold") == 10000,
        "threshold=%r" % o.get("threshold"))
    chk("gate_source_nonempty", bool(o.get("gate_source")),
        "gate_source=%r" % o.get("gate_source"))
    allpass = all(c["pass"] for c in checks)
    verdict = "PASS" if allpass else "FAIL"
    why = "terminal completed record" if allpass else \
        "; ".join(c["id"] for c in checks if not c["pass"])
    if not terminal and inconclusive:
        verdict, why = "FAIL", (
            "only inconclusive_below_threshold record(s): honest negative, "
            "do NOT re-rate (EVIDENCE-SPEC 2.3)")
    return {"verdict": verdict, "why": why, "conditions": checks,
            "records_seen": len(recs), "completed_records": len(terminal),
            "inconclusive_records": len(inconclusive)}


def placeholders(scans, c2rep):
    fc1 = scan_fc1(c2rep)
    r3 = {"verdict": "PLACEHOLDER", "why": "count-only; no verdict from files",
          "pos_tails": scans["rung3_pos_tails"],
          "fail_tails": scans["rung3_fail_tails"],
          "fc1_records_in_c2": fc1,
          "note": "pos tail -> FindFirstOf callable, DB absent; fail tail -> "
                  "B2 unresolved; FC1 via IgSink under its build_id"}
    r4 = {"verdict": "PLACEHOLDER", "why": "count-only; no verdict from files",
          "load_lines": scans["rung4_load_lines"],
          "mod_tag_lines_total": scans["mod_tag_lines"],
          "note": "expect load line exactly ONCE in /data/plugins/UE4SS.log; "
                  "alternates /data/townfall_runtime_buffered_lua.log and "
                  "/data/townfall_mod.log are older lineages, not fetched"}
    r5 = {"verdict": "PLACEHOLDER", "why": "count-only; no verdict from files",
          "spawned": scans["rung5_spawned"],
          "failed_at": scans["rung5_failed_at"],
          "note": "C4 video/frame correlation is out of scope for file "
                  "assertions (EVIDENCE-SPEC 5.2/5.7)"}
    return {"RUNG3": r3, "RUNG4": r4, "RUNG5": r5}


# ---------------------------------------------------------------------------
# Discovery fallback (FTP LIST of candidate roots)
# ---------------------------------------------------------------------------
def discover_ftp(args):
    found = {}
    for d in DISCOVER_DIRS:
        cmd = ["curl", "-fsS", "--connect-timeout", "10",
               "--max-time", str(args.timeout), "ftp://%s:%s%s" %
               (args.host, args.port, d)]
        try:
            proc = subprocess.run(cmd, capture_output=True,
                                  timeout=args.timeout + 10)
        except (FileNotFoundError, subprocess.TimeoutExpired) as exc:
            found[d] = {"error": str(exc)}
            continue
        if proc.returncode != 0:
            err = proc.stderr.decode("utf-8", "replace").strip().splitlines()
            found[d] = {"error": err[-1] if err else "curl exit %d"
                        % proc.returncode}
            continue
        names = []
        for ln in proc.stdout.decode("utf-8", "replace").splitlines():
            name = ln.split()[-1] if ln.split() else ""
            if name and DISCOVER_NAME_RE.search(name):
                names.append(name)
        found[d] = {"candidates": sorted(set(names))}
    return found


def discover_from_dir(directory):
    found = {}
    for d in DISCOVER_DIRS:
        token = d.strip("/").replace("/", "-") or "data"
        full = os.path.join(directory, "dirlist-%s.txt" % token)
        if not os.path.isfile(full):
            found[d] = {"error": "fixture missing: %s" % full}
            continue
        with open(full, "r", encoding="utf-8", errors="replace") as fh:
            names = [ln.split()[-1] for ln in fh.read().splitlines()
                     if ln.split()]
        found[d] = {"candidates": sorted(set(
            n for n in names if DISCOVER_NAME_RE.search(n)))}
    return found


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------
def render(rep):
    out = ["== rung_check =="]
    for key, info in rep.get("fetches", {}).items():
        if info["ok"]:
            out.append("fetch %-14s OK    %7s B  %s" %
                       (key, info.get("bytes"), info["path"]))
        else:
            out.append("fetch %-14s FAIL  %s (%s)" %
                       (key, info["path"], info["error"]))
    st = rep.get("status") or {"present": False}
    if st["present"]:
        out.append("status: pid=%s ms=%s last=%s ladder=%s" %
                   (st["session_pid"], st["session_ms"],
                    (st["last"] or {}).get("state"), st["ladder_seen"]))
        for a in st["anomalies"]:
            out.append("status anomaly: %s" % a)
    c2 = rep.get("c2") or {"nonempty": 0, "native_query_harness": [],
                           "ig_envelope": 0, "unparsed": 0}
    out.append("c2: %d lines (%d qh records, %d ig envelopes, %d unparsed) "
               "driver_seen=%s" % (c2["nonempty"],
                                   len(c2["native_query_harness"]),
                                   c2["ig_envelope"], c2["unparsed"],
                                   (rep.get("rungs", {})
                                    .get("RUNG1", {}).get("driver_seen"))))
    if (rep.get("scans") or {}).get("engine_version_line"):
        out.append("engine version line: Using engine version: %s" %
                   rep["scans"]["engine_version_line"])
    if rep["warnings"]:
        for w in rep["warnings"]:
            out.append("WARN: %s" % w)
    if rep["discover"]:
        out.append("-- discover --")
        for d, info in rep["discover"].items():
            out.append("  %s -> %s" % (d, json.dumps(info)))
        return "\n".join(out)
    rungs = rep.get("rungs") or {}
    for rung in ("RUNG1", "RUNG2"):
        r = rungs.get(rung)
        if not r:
            continue
        out.append("%s: %s (%s)" % (rung, r["verdict"], r["why"]))
        for c in r.get("conditions", []):
            out.append("  [%s] %-34s %s" %
                       ("PASS" if c["pass"] else "FAIL", c["id"], c["why"]))
    for rung in ("RUNG3", "RUNG4", "RUNG5"):
        r = rungs.get(rung)
        if not r:
            continue
        out.append("%s: %s %s" % (rung, r["verdict"],
                                  json.dumps({k: v for k, v in r.items()
                                              if k not in ("verdict", "why",
                                                           "note")})))
        out.append("  note: %s" % r["note"])
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Assert composition-5 acceptance rungs from console "
                    "files (FTP fresh-curl-per-file; rung1 needs two C2 "
                    "snapshots).")
    ap.add_argument("--host", default="ps5-host-not-configured.invalid")
    ap.add_argument("--port", default="2121")
    ap.add_argument("--timeout", type=int, default=60)
    ap.add_argument("--max-bytes", type=int, default=32 * 1024 * 1024)
    ap.add_argument("--snapshot-gap", type=float, default=3.0,
                    help="seconds between the two C2 snapshots (default 3)")
    ap.add_argument("--rung", default="ALL",
                    choices=["RUNG1", "RUNG2", "RUNG3", "RUNG4", "RUNG5",
                             "ALL"])
    ap.add_argument("--from-dir",
                    help="HOST-TEST MODE: read fixtures from this local "
                         "directory (basenames: modbridge-ingame-facts.jsonl "
                         "[+.snap2], UE4SS.log, runtime-status.txt, "
                         "townfall_runtime_aligned_entry.log, "
                         "townfall_runtime_aligned_observer.log)")
    ap.add_argument("--discover", action="store_true",
                    help="DISCOVERY FALLBACK: FTP LIST the candidate /data "
                         "roots and print matching file names; no rung "
                         "verdicts (in --from-dir mode reads dirlist-*.txt)")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the exact curl commands and exit")
    args = ap.parse_args(argv)

    rep = {"mode": "dir" if args.from_dir else "ftp", "host": args.host,
           "port": args.port, "snapshot_gap_s": args.snapshot_gap,
           "warnings": [], "discover": None}

    if args.dry_run:
        for key, path, _local, _s2 in FETCH_FILES:
            print(" ".join(curl_command(args.host, args.port, path,
                                        args.timeout, args.max_bytes)))
        print("# snapshot 2 (RUNG1):")
        print(" ".join(curl_command(args.host, args.port,
                                    "/data/modbridge-ingame-facts.jsonl",
                                    args.timeout, args.max_bytes)))
        return 0

    if args.discover:
        rep["discover"] = (discover_from_dir(args.from_dir) if args.from_dir
                           else discover_ftp(args))
        if args.json:
            print(json.dumps(rep, indent=1))
        else:
            print(render(rep))
        return 0

    want_strict = [args.rung] if args.rung in ("RUNG1", "RUNG2") else \
        (["RUNG1", "RUNG2"] if args.rung == "ALL" else [])

    # Snapshot 1: everything.
    snap1 = do_fetches(args, snapshot=1)
    all_failed = all(not r.ok for r in snap1.values())
    if all_failed and not args.from_dir:
        rep["fetches"] = {k: {"path": r.path, "ok": r.ok, "error": r.error}
                          for k, r in snap1.items()}
        rep["verdict"] = "FETCH_FAILED"
        rep["next"] = ("RETRY: no evidence reachable. Records appear ~1 min "
                       "after launch — wait ~60 s and re-run; if still down, "
                       "use --discover to probe which /data paths exist.")
        print(json.dumps(rep, indent=1) if args.json else
              "VERDICT: FETCH_FAILED\nNEXT: %s" % rep["next"])
        return 4

    # Snapshot 2 of C2 for RUNG1 (live mode honors the gap; fixture mode
    # reads the .snap2 fixture, falling back to the same file).
    if args.from_dir:
        c2_snap2_res = fetch_from_dir(args.from_dir, "query_jsonl",
                                      "/data/modbridge-ingame-facts.jsonl",
                                      "modbridge-ingame-facts.jsonl",
                                      snap2=True)
    else:
        if want_strict and "RUNG1" in want_strict:
            time.sleep(args.snapshot_gap)
        c2_snap2_res = fetch_ftp(args.host, args.port, "query_jsonl",
                                 "/data/modbridge-ingame-facts.jsonl",
                                 args.timeout, args.max_bytes)

    status = parse_status(snap1["runtime_status"].lines())
    c2_snap1 = classify_c2(snap1["query_jsonl"].lines())
    c2_snap1["_raw_lines"] = snap1["query_jsonl"].lines()
    c2_snap2 = classify_c2(c2_snap2_res.lines())
    c2_snap2["_raw_lines"] = c2_snap2_res.lines()
    scans = scan_logs(snap1["ue4ss_log"], snap1["entry_diag"],
                      snap1["observer_log"])

    rungs = {}
    rungs["RUNG1"] = rung1_verdict(status, c2_snap1, c2_snap2)
    rungs["RUNG2"] = rung2_verdict(c2_snap1, c2_snap2)
    rungs.update(placeholders(scans, c2_snap1))

    if not c2_snap1["native_query_harness"] and \
            not c2_snap2["native_query_harness"]:
        rep["warnings"].append(
            "UNVERIFIED_DRIVER: /data/modbridge-ingame-facts.jsonl contains "
            "no native_query_harness line. The composed PRX has NO "
            "FormatRecordJsonLine caller (WIRING.md:120-124); C2 is written "
            "only by probe/add-on PRXs linking record_sink.c. RUNG1/RUNG2 "
            "cannot be EVIDENCED until a driver add-on is staged. If the C2 "
            "file itself is missing, run --discover to list /data via FTP.")
    if snap1["query_jsonl"].ok and not c2_snap1["nonempty"]:
        rep["warnings"].append(
            "C2 file fetched but EMPTY (probe staged but emitted nothing, or "
            "a stale truncated file).")
    if not snap1["ue4ss_log"].ok:
        rep["warnings"].append(
            "UE4SS.log unreachable: RUNG3-5 placeholders and the engine-"
            "version context are vacuous this run.")

    rep["fetches"] = {}
    for key, _path, _local, _s2 in FETCH_FILES:
        r = snap1[key]
        rep["fetches"][key] = {
            "path": r.path, "ok": r.ok, "source": r.source,
            "bytes": len(r.text.encode("utf-8")) if r.ok else None,
            "error": r.error}
    rep["fetches"]["query_jsonl_snap2"] = {
        "path": c2_snap2_res.path, "ok": c2_snap2_res.ok,
        "source": c2_snap2_res.source,
        "bytes": len(c2_snap2_res.text.encode("utf-8"))
        if c2_snap2_res.ok else None, "error": c2_snap2_res.error}
    rep["status"] = status
    rep["c2"] = {k: v for k, v in c2_snap1.items() if k != "_raw_lines"}
    rep["scans"] = scans
    rep["rungs"] = rungs
    rep["verdict"] = None
    rep["next"] = ""

    requested = [args.rung] if args.rung != "ALL" else \
        ["RUNG1", "RUNG2", "RUNG3", "RUNG4", "RUNG5"]
    strict = [r for r in requested if r in ("RUNG1", "RUNG2")]
    if strict:
        verdicts = [rungs[r]["verdict"] for r in strict]
        if any(v == "INCONCLUSIVE-STALE" for v in verdicts):
            rep["verdict"], code = "INCONCLUSIVE-STALE", 5
            rep["next"] = ("O2 freshness gate: the status file dates this "
                           "evidence to a PRIOR launch (session ms <= %d). "
                           "Nothing may be asserted from it — do not rate, "
                           "re-run in a fresh launch window (~60 s after "
                           "launch)." % FRESH_SESSION_MS_THRESHOLD)
        elif all(v == "PASS" for v in verdicts):
            rep["verdict"], code = "PASS", 0
            rep["next"] = ("Requested rung(s) asserted from console files; "
                           "keep evidence raw (PLAYBOOK honesty rules).")
        elif any(v == "CANNOT_ASSERT" for v in verdicts):
            rep["verdict"], code = "CANNOT_ASSERT", 3
            rep["next"] = ("Evidence missing for a requested rung: re-run "
                           "after the launch window or use --discover to "
                           "locate the C2 file.")
        else:
            rep["verdict"], code = "FAIL", 1
            rep["next"] = ("Requested rung FAILED: read the failing "
                           "conditions above; never re-rate unchanged "
                           "evidence.")
    else:
        rep["verdict"], code = "PLACEHOLDER", 0
        rep["next"] = ("Placeholder counts only — RUNG3-5 verdicts need "
                       "their own acceptance procedure (C4 for rung 5).")

    if args.json:
        print(json.dumps(rep, indent=1))
    else:
        print(render(rep))
        print("")
        print("VERDICT: %s" % rep["verdict"])
        print("NEXT: %s" % rep["next"])
    return code


if __name__ == "__main__":
    sys.exit(main())
