#!/usr/bin/env python3
"""epoch_view.py — one verdict table per GAME EPOCH (pid) from the DURABLE sinks.

worker-durable-events · 2026-10-05 · host-side; LOCAL file analysis only; no
console contact, no FTP. Complements evidence.sh (worker-launch10-evidence-
upgrade, + in-flight worker-evidence-gap-fix-2): evidence.sh classifies the
one-pager window; THIS tool recovers the per-epoch (per-pid) history from the
append-only accumulated sinks.

USER DIRECTIVE this tool implements (controller post-verification scope
correction, LAUNCH10-RESULTS.md):
  runtime-status.txt is LATEST-STATE-ONLY (last-writer-wins by design; O_TRUNC
  per open; one file per session). The AUTHORITATIVE activation/terminal
  evidence is the DURABLE per-build/run/PID events — the sdiag jsonl
  (modbridge-supplier-diag.jsonl; accumulates across epochs) + the UE4SS.log
  mirror lines (append-only across epochs). Evidence reading must correlate by
  build/run/pid and NEVER race the status file. Therefore:
    - every epoch fact here comes from sdiag records and/or pid-prefixed
      UE4SS.log lines of THAT pid;
    - the status file (if given) is used ONLY to corroborate the single epoch
      whose pid equals the status session pid — never as history for others;
    - there is no "fetch the status before the terminal write" race anywhere.

Inputs
  sdiag jsonl   records {"marker":"modbridge.supplier-diag/v1","schema":1,
                "seq":N,"pid":P,"tag":"...","value":D,"value_hex":"0xH","t_ms":MS}
                --sdiag FILE or positional; "-" reads stdin.
  --ue4ss F     optional UE4SS.log mirror. Extracts per-pid terminal refusal
                lines "[t=<ms>ms pid=<pid>] modbridge module_start failed:
                <stage> code=<n> detail=<text>" (r4 grammar) and progress
                markers (engine-ready / mods-started / [TownfallItemSpawner]),
                position-attributed to the most recent pid prefix.
  --status F    optional runtime-status.txt, LATEST-STATE-ONLY: only its
                session row (pid/ms) + last state line, applied to that pid's
                epoch alone.
  --baseline-bytes N
                optional append-only baseline cut (byte offset): sdiag records
                starting at or after offset N are marked new=yes in the table
                (the RUN-N+1 epoch discriminator). Default 0 = whole file.
                Unlike evidence.sh, the cut only LABELS epochs; history is
                always shown — the accumulated file is the evidence.
  --build-id S  optional build label (from the controller's staging receipt,
                e.g. EDITION.json). The sdiag record grammar carries NO
                build_id field, so build identity must be pinned by the
                operator at capture time; it is never invented here.
  --json        machine output (full document) instead of the human table.
  --out F       also write the human table to F.

Outputs (human): a summary matrix (one ROW PER EPOCH) + one detail block per
epoch + cross-epoch notes. Outputs (--json): {"tool", "generated_utc",
"source", "baseline_bytes", "build_id", "status": {...latest-state-only...},
"epochs": [...]}.

Stage-chain vocabulary — same tag vocabulary as evidence.sh's stage table,
rendered as coarse tokens (evidence.sh S-stage in parentheses):
  ENTRY (S1_ENTRY_CTORS) -> ACT (S2_ACTIVATION) -> VMMAP (S3_VMMAP_WALK) ->
  lease (S4_LEASE_REG, incl. the spanpub publication tags) ->
  anchors (S5_IDENTITY_ANCHORS) -> conversion (S6_CONVERSION, lmf_*) ->
  cfg (S7_CFG_ATTACH) -> START (S9_START_ACK / runtime_start_failed) ->
  pad (pad_* tags, when a future build emits them; today the pad outcome is a
  START-stage refusal TEXT, surfaced in the verdict column) ->
  harness (buffered-Lua harness tags; present in the sdiag grammar, unmapped
  in evidence.sh's stage_of_tag) -> STOP (S11_STOP; NOTE the evidence.sh G4
  quirk: module_start_return=0 after a refusal means "entry returned 0, game
  continues UNMODDED", not an unload).
Chains are FIRST-SEEN order per pid (the honest record; the evidence.sh
S7-before-S4/S3 display quirk is inherited and annotated, never reordered).
A per-epoch evidence.sh-S-stage chain (chain_evidence_sh) is also emitted.

Verdict classes (oracle vocabulary, ORACLE.md §1/§2 + LAUNCH10-RESULTS §5 G1):
  E2 START      runtime_start_failed code=3 (prepare/start threw) —
                EXPECTED-REFUSAL, verdict >= MINIMAL-ACCEPT when the sdiag
                conversion quartet (D1+D2 extents, D3 anchor_count>0, D4
                intervals_found>0, D5 cfg events) coexists and no D6 line.
  pad phase-6   the E2 START subclass whose detail names the pad provider
                ("PS5Pad input unavailable: phase=6, error=0x…"); run-3's
                confirmed MINIMAL-ACCEPT class.
  B7 MFA        mfa_activate_failed — EXPECTED-REFUSAL, FAIL with named cause;
                nothing downstream ran (no bridge tags by construction).
  engine-ready  F34 state (status engine-ready) — init thread finished.
  mods-started  F35 state (status mods-started) — full ladder.
  Plus the named refusal classes for other codes (D6/D7 conversion, C12
  pre-lease gate, C13 lease throw, E1 context) and STARTED when
  runtime_start_ok lands. E4 `activated` is reported per-epoch as
  evidence-equivalence (mfa_activated_low/high + code=3 reachability) unless
  a literal `activated` state is on file for THAT pid.

Exit codes: 0 classified · 2 usage/missing input. No network, no writes
outside --out.

Usage examples:
  epoch_view.py outputs/.../sdiag-baseline-run5.jsonl
  epoch_view.py --sdiag run5/modbridge-supplier-diag.jsonl \
      --ue4ss run5/UE4SS.log --status run5/runtime-status.txt \
      --baseline-bytes 318910 --build-id launch10-final-a --json
"""

import argparse
import datetime
import json
import re
import sys

TOOL = "epoch_view.py"
VERSION = "1.0.0"

# ---------------------------------------------------------------------------
# Stage vocabulary — tag -> coarse token, with the evidence.sh S-stage kept
# alongside. Mirrors evidence.sh stage_of_tag (worker-launch10-evidence-upgrade
# + gap-fix revisions) extended with the sdiag-only harness/worker tags.
# ---------------------------------------------------------------------------
STAGE_MAP = {
    # ENTRY (S1_ENTRY_CTORS)
    "module_start_entered": "ENTRY",
    "entry_incoming_sp_mod16": "ENTRY",
    "entry_call_sp_mod16": "ENTRY",
    "constructor_count": "ENTRY",
    "constructor_begin": "ENTRY",
    "constructor_return": "ENTRY",
    "ctor-1": "ENTRY",
    "ctor-2": "ENTRY",
    "import_validation_begin": "ENTRY",
    "import_validation_missing_count": "ENTRY",
    "full_runtime_constructors_complete": "ENTRY",
    "probe_lifecycle_result": "ENTRY",
    "runtime_start_skipped": "ENTRY",
    # ACT (S2_ACTIVATION)
    "runtime_wiring_begin": "ACT",
    "mfa_activated_low": "ACT",
    "mfa_activated_high": "ACT",
    "mfa_activate_failed": "ACT",
    "mfa_activate_os_error": "ACT",
    # VMMAP (S3_VMMAP_WALK)
    "bridge_vmentry_entries": "VMMAP",
    "bridge_vm_walk_stop": "VMMAP",
    "bridge_kve_distinct0": "VMMAP",
    "bridge_kve_distinct1": "VMMAP",
    "bridge_kve_distinct2": "VMMAP",
    "bridge_kve_distinct3": "VMMAP",
    "bridge_kve_eboot": "VMMAP",
    "bridge_exepath_valid": "VMMAP",
    "bridge_exepath_sig": "VMMAP",
    # lease (S4_LEASE_REG + spanpub publication)
    "bridge_lease_reg_spans": "lease",
    "bridge_lease_reg": "lease",
    "bridge_lease_cap": "lease",
    "bridge_lease_gap": "lease",
    "bridge_lease_gap_req": "lease",
    "bridge_lease_ref_lo": "lease",
    "bridge_lease_ref_hi": "lease",
    "bridge_lease_ref_len": "lease",
    "bridge_lease_near_lo": "lease",
    "bridge_lease_near_hi": "lease",
    "bridge_lease_near_len": "lease",
    "bridge_spanpub_count": "lease",
    "bridge_spanpub_total": "lease",
    "bridge_spanpub_cap": "lease",
    "bridge_spanpub_env_low": "lease",
    "bridge_spanpub_env_high": "lease",
    # anchors (S5_IDENTITY_ANCHORS)
    "anchor_cov": "anchors",
    "anchor_match": "anchors",
    "anchor_mm": "anchors",
    "anchor_c_either": "anchors",
    "anchor_owned_ref": "anchors",
    "anchor_refuse": "anchors",
    # conversion (S6_CONVERSION)
    "lmf_vmentry_cap": "conversion",
    "lmf_vm_scan_cap": "conversion",
    "lmf_linkmap_cap": "conversion",
    "lmf_provider_code": "conversion",
    "lmf_identity_source": "conversion",
    "lmf_extent_base_low": "conversion",
    "lmf_extent_base_high": "conversion",
    "lmf_extent_end_low": "conversion",
    "lmf_extent_end_high": "conversion",
    "lmf_anchor_count": "conversion",
    "lmf_intervals_found": "conversion",
    # cfg (S7_CFG_ATTACH)
    "bridge_cfg_gamedir": "cfg",
    "bridge_cfg_workdir": "cfg",
    "bridge_exepath_source": "cfg",
    # START (S9_START_ACK / runtime_start_failed)
    "runtime_start_ok": "START",
    "runtime_start_failed": "START",
    # pad (future pad_* tags; today's pad outcome is a refusal TEXT at START)
    # harness (buffered-Lua harness + workers; sdiag grammar, unmapped in
    # evidence.sh stage_of_tag — mapped here so the chain shows harness green)
    "worker_create_result": "harness",
    "worker_entry": "harness",
    "worker_sleep_complete": "harness",
    "buffered_lua_begin": "harness",
    "buffered_lua_result": "harness",
    "lua_state_begin": "harness",
    "lua_load_result": "harness",
    "lua_call_result": "harness",
    "lua_result_match": "harness",
    "lua_memory_check_passed": "harness",
    "lua_state_closed": "harness",
    # STOP (S11_STOP)
    "module_start_return": "STOP",
    "runtime_stop_result": "STOP",
}

COARSE_TO_SSTAGE = {
    "ENTRY": "S1_ENTRY_CTORS",
    "ACT": "S2_ACTIVATION",
    "VMMAP": "S3_VMMAP_WALK",
    "lease": "S4_LEASE_REG",
    "anchors": "S5_IDENTITY_ANCHORS",
    "conversion": "S6_CONVERSION",
    "cfg": "S7_CFG_ATTACH",
    "START": "S9_START_ACK",
    "pad": "S6_CONVERSION(pad)",
    "harness": "(harness)",
    "STOP": "S11_STOP",
}

# value-carrying tags surfaced per epoch (last-write-wins WITHIN the pid)
VALUE_TAGS = [
    "constructor_count", "probe_lifecycle_result",
    "mfa_activated_low", "mfa_activated_high",
    "mfa_activate_failed", "mfa_activate_os_error",
    "bridge_vmentry_entries", "bridge_vm_walk_stop",
    "bridge_exepath_source", "bridge_exepath_valid", "bridge_exepath_sig",
    "bridge_lease_reg_spans", "bridge_lease_reg", "bridge_lease_cap",
    "bridge_spanpub_count", "bridge_spanpub_total", "bridge_spanpub_cap",
    "anchor_c_either",
    "bridge_cfg_gamedir", "bridge_cfg_workdir",
    "lmf_extent_base_low", "lmf_extent_base_high",
    "lmf_extent_end_low", "lmf_extent_end_high",
    "lmf_anchor_count", "lmf_intervals_found",
    "lmf_provider_code", "lmf_identity_source",
    "runtime_start_ok", "runtime_start_failed",
    "stat_ownmod", "stat_ownmod_ec",
    "module_start_return", "runtime_stop_result",
]
EXTENT_TAGS = ["lmf_extent_base_low", "lmf_extent_base_high",
               "lmf_extent_end_low", "lmf_extent_end_high"]

R4_RE = re.compile(
    r"^\[t=(?P<t>\d+)ms pid=(?P<pid>\d+)\] modbridge module_start failed: "
    r"(?P<stage>\S+) code=(?P<code>-?\d+) detail=(?P<detail>.*)$")
BARE_FAIL_RE = re.compile(r"^modbridge module_start failed: (.*)$")
MARKER_RE = re.compile(r"engine-ready|mods-started")
TOWNFALL_RE = re.compile(r"TownfallItemSpawner")


def to_int32(v):
    """diag values are unsigned 32-bit decimals; codes are int32."""
    try:
        v = int(v)
    except (TypeError, ValueError):
        return None
    return v - (1 << 32) if v > (1 << 31) - 1 else v


def hex32(v):
    try:
        return "0x%08x" % (int(v) & 0xFFFFFFFF)
    except (TypeError, ValueError):
        return None


def u64(lo, hi):
    try:
        return (int(lo) & 0xFFFFFFFF) | ((int(hi) & 0xFFFFFFFF) << 32)
    except (TypeError, ValueError):
        return None


# ---------------------------------------------------------------------------
# sdiag parsing — byte-exact offsets so a baseline cut labels records, never
# guesses. File order IS the append order (chronological across epochs).
# ---------------------------------------------------------------------------
def parse_sdiag(path):
    epochs = {}     # pid -> epoch dict
    order = []      # pids in first-appearance (file) order
    pid_occurrences = {}  # pid -> count of disjoint ranges (boot/pid reuse)
    bad_lines = 0
    total = 0
    raw = open(path, "rb").read() if path != "-" else sys.stdin.buffer.read()
    off = 0
    prev_last_t = None
    for chunk in raw.split(b"\n"):
        start = off
        off += len(chunk) + 1
        line = chunk.decode("utf-8", "replace").strip()
        if not line:
            continue
        if '"modbridge.supplier-diag/v1"' not in line:
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:
            bad_lines += 1
            continue
        total += 1
        pid = rec.get("pid")
        tag = rec.get("tag", "")
        t = rec.get("t_ms")
        val = rec.get("value")
        new = start >= BASELINE_BYTES
        ep = epochs.get(pid)
        if ep is None:
            pid_occurrences[pid] = pid_occurrences.get(pid, 0) + 1
            ep = {
                "pid": pid,
                "occurrence": pid_occurrences[pid],
                "records": 0,
                "t_first": None, "t_last": None,
                "seq_first": None, "seq_last": None,
                "first_new_record": new,
                "any_old_record": not new,
                "chain": [], "chain_seen": set(),
                "values": {}, "events": {},
                "boot_gap_suspect": False,
            }
            epochs[pid] = ep
            order.append(pid)
            # console reboot detector: t_ms is CLOCK_MONOTONIC per BOOT; a new
            # epoch whose first t_ms goes backwards vs the previous epoch's
            # last t_ms means a reboot happened between the two epochs.
            if prev_last_t is not None and isinstance(t, int) and t < prev_last_t:
                ep["boot_gap_suspect"] = True
        ep["records"] += 1
        if new:
            ep["first_new_record"] = ep["first_new_record"] or (ep["records"] == 1 and new)
        else:
            ep["any_old_record"] = True
        if isinstance(t, int):
            if ep["t_first"] is None:
                ep["t_first"] = t
            ep["t_last"] = t
            prev_last_t = t
        seq = rec.get("seq")
        if isinstance(seq, int):
            if ep["seq_first"] is None:
                ep["seq_first"] = seq
            ep["seq_last"] = seq
        stage = STAGE_MAP.get(tag)
        if stage and stage not in ep["chain_seen"]:
            ep["chain_seen"].add(stage)
            ep["chain"].append(stage)
        if tag in VALUE_TAGS:
            ep["values"][tag] = val
        # record EVERY event tag once (first-seen value + count) for the JSON
        ev = ep["events"].setdefault(tag, {"count": 0, "first_value": val})
        ev["count"] += 1
        ev["last_value"] = val
    return [epochs[p] for p in order], total, bad_lines


# ---------------------------------------------------------------------------
# UE4SS.log mirror — per-pid terminal refusal lines + position-attributed
# progress markers. Bare (unprefixed) failure lines are history from epochs
# whose pid the mirror grammar did not carry (pre-r4 launches): counted, never
# attributed.
# ---------------------------------------------------------------------------
def parse_ue4ss(path):
    refusals = {}      # pid -> list of dict(t, stage, code, detail)
    markers = {}       # pid -> list of str (position-attributed)
    bare = []          # unprefixed failure lines (unattributable)
    current_pid = None
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            m = R4_RE.match(line)
            if m:
                current_pid = int(m.group("pid"))
                refusals.setdefault(current_pid, []).append({
                    "t_ms": int(m.group("t")),
                    "stage": m.group("stage"),
                    "code": int(m.group("code")),
                    "detail": m.group("detail"),
                    "line": line,
                })
                continue
            if BARE_FAIL_RE.match(line):
                bare.append(line)
                continue
            if MARKER_RE.search(line) or TOWNFALL_RE.search(line):
                if current_pid is not None:
                    markers.setdefault(current_pid, []).append(line)
    return refusals, markers, bare


# ---------------------------------------------------------------------------
# runtime-status.txt — LATEST-STATE-ONLY. One session row; the last state
# line belongs to that session pid alone.
# ---------------------------------------------------------------------------
def parse_status(path):
    out = {"present": True, "session_pid": None, "session_ms": None,
           "n_states": 0, "last_state": None, "last_code": None,
           "last_detail": None, "header_ok": False}
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line == "modforge-runtime-status\t1":
                out["header_ok"] = True
            elif line.startswith("session\t"):
                p = line.split("\t")
                if len(p) >= 3:
                    out["session_pid"] = int(p[1])
                    out["session_ms"] = int(p[2])
            elif line.startswith("state\t"):
                p = line.split("\t")
                if len(p) >= 4:
                    out["n_states"] += 1
                    out["last_state"] = p[2]
                    out["last_code"] = p[3]
                    out["last_detail"] = p[4] if len(p) >= 5 else ""
    return out


# ---------------------------------------------------------------------------
# Verdict per the oracle vocabulary
# ---------------------------------------------------------------------------
def classify_verdict(ep, refusal, status):
    """Return (verdict, verdict_class, notes[], e4)."""
    v = ep["values"]
    notes = []
    ev = ep["events"]
    pid = ep["pid"]

    anchor = v.get("lmf_anchor_count")
    intervals = v.get("lmf_intervals_found")
    spans = v.get("bridge_lease_reg_spans")
    cfg_ok = (v.get("bridge_cfg_gamedir") or 0) > 0 or (v.get("bridge_cfg_workdir") or 0) > 0
    extent4 = all(t in ev for t in EXTENT_TAGS)
    quartet = extent4 and anchor is not None and anchor > 0 \
        and intervals is not None and intervals > 0 and cfg_ok

    detail = refusal["detail"] if refusal else None
    code = refusal["code"] if refusal else None
    stage = refusal["stage"] if refusal else None

    # sdiag-side codes (durable even when the mirror line was not fetched)
    rs_code = to_int32(v.get("runtime_start_failed")) if "runtime_start_failed" in v else None
    mfa_code = to_int32(v.get("mfa_activate_failed")) if "mfa_activate_failed" in v else None
    start_ok = "runtime_start_ok" in v

    if code is None and rs_code is not None:
        code, stage = rs_code, "runtime_start_failed"
    if code is None and mfa_code is not None:
        code, stage = mfa_code, "mfa_activate_failed"

    # no mirror text? the status detail may carry the refusal text for ITS OWN
    # session pid (LATEST-STATE-ONLY: that one epoch, never others)
    if detail is None and status.get("present") and status.get("session_pid") == pid \
            and status.get("last_detail"):
        detail = status.get("last_detail")

    verdict = None
    vclass = None
    e4 = "absent"

    if stage == "mfa_activate_failed":
        verdict = "B7 MFA"
        vclass = "B7_MFA"
        notes.append("oracle B7 EXPECTED-REFUSAL: FAIL with named cause; nothing "
                     "downstream ran (no bridge tags in this epoch by construction).")
    elif stage == "runtime_start_failed" and code == 3:
        if detail and "phase=6" in detail:
            verdict = "pad phase-6"
            vclass = "E2_START_PAD_PHASE6"
            notes.append("oracle E2 START subclass: pad provider init refused "
                         "(pad error domain); EXPECTED-REFUSAL.")
        else:
            verdict = "E2 START"
            vclass = "E2_START"
            notes.append("oracle E2 EXPECTED-REFUSAL: prepare/start threw "
                         "(pad profile, mod source, sink open, ...); verdict "
                         ">= MINIMAL-ACCEPT when the conversion quartet holds.")
    elif stage == "runtime_start_failed" and code == 1:
        if detail and "no observed mapping interval intersects" in detail:
            verdict, vclass = "D6 conversion refusal (FAIL)", "D6_CONVERSION_REFUSAL"
            notes.append("oracle D6 REGRESSION: the launch-9 geometry death.")
        elif detail and "lease coverage gap" in detail:
            verdict, vclass = "C12 pre-lease gate", "C12_LEASE_GATE"
            notes.append("r14 L3 loud pre-lease refusal (MRS_ERR_CONVERSION by design).")
        else:
            verdict, vclass = "D7 named conversion refusal", "D7_CONVERSION_NAMED"
    elif stage == "runtime_start_failed" and code == 2:
        verdict, vclass = "E1 context refusal", "E1_CONTEXT"
    elif start_ok or (stage == "runtime_start_ok"):
        verdict, vclass = "STARTED", "STARTED"
    else:
        # F0 (composition never entered) is the NO-EPOCHS path — an epoch here
        # always has records, so this is the truncated-window case.
        verdict = "entered (no terminal in this file window)"
        vclass = "INCOMPLETE_WINDOW"
        notes.append("records end without a terminal event: epoch truncated, "
                     "mid-launch at fetch time, or the refusal line lives in a "
                     "not-provided sink.")

    # E4 activated: literal only for the status session pid (LATEST-STATE-ONLY);
    # otherwise evidence-equivalence (mfa_activated_low/high pair + a
    # START-stage code reachable only after MFA activation).
    literal_activated = bool(status.get("present")
                             and status.get("session_pid") == pid
                             and status.get("last_state") == "activated")
    if literal_activated:
        e4 = "literal (status `activated` on file for this pid)"
    elif v.get("mfa_activated_low") is not None and v.get("mfa_activated_high") is not None:
        e4 = "evidence-equivalence (mfa_activated_low=%s/high=%s; no literal " \
             "`activated` state on file — LATEST-STATE-ONLY status has no " \
             "capture for this epoch)" % (v.get("mfa_activated_low"),
                                          v.get("mfa_activated_high"))
        if vclass in ("E2_START", "E2_START_PAD_PHASE6"):
            notes.append("E4 by evidence-equivalence per LAUNCH10-RESULTS §2: the "
                         "START-stage code=3 is reachable only AFTER MFA activation; "
                         "the terminal write O_TRUNC-replaced the ack lines.")
    else:
        e4 = "absent (no activation evidence in this epoch)"

    # MINIMAL-ACCEPT qualification (oracle §2; conversion quartet, no D6)
    if quartet and vclass in ("E2_START", "E2_START_PAD_PHASE6"):
        verdict += " — MINIMAL-ACCEPT-grade conversion (D1-D5 quartet met, no D6)"
    elif quartet and vclass in ("STARTED", "E1_CONTEXT", "D7_CONVERSION_NAMED"):
        verdict += " — conversion quartet met (D1-D5)"

    # status corroboration — ONLY for the session pid (never as history)
    if status.get("present") and status.get("session_pid") == pid:
        st = status.get("last_state")
        notes.append("status agrees (LATEST-STATE-ONLY, session pid matches): "
                     "last=%s code=%s detail=%s" % (st, status.get("last_code"),
                                                    status.get("last_detail")))
        if st == "engine-ready":
            verdict = "engine-ready" + ("" if vclass == "STARTED" else " (status; durable START event in sdiag governs)")
            vclass = "ENGINE_READY"
        elif st == "mods-started":
            verdict = "mods-started"
            vclass = "MODS_STARTED"
        elif st == "failed" and vclass in ("E2_START", "E2_START_PAD_PHASE6",
                                           "B7_MFA", "D6_CONVERSION_REFUSAL",
                                           "C12_LEASE_GATE", "E1_CONTEXT",
                                           "D7_CONVERSION_NAMED"):
            notes.append("status `failed` corroborates the durable refusal of "
                         "this epoch (no race: the file is not the history).")
    return verdict, vclass, notes, e4, quartet


# ---------------------------------------------------------------------------
# rendering
# ---------------------------------------------------------------------------
def short(s, n):
    s = s or ""
    return s if len(s) <= n else s[: n - 1] + "…"


def fmt_pair(v):
    a, i, s = v.get("lmf_anchor_count"), v.get("lmf_intervals_found"), v.get("bridge_lease_reg_spans")
    return "anchor=%s intervals=%s spans=%s" % (
        a if a is not None else "-", i if i is not None else "-", s if s is not None else "-")


def fmt_extent(v):
    b, e = u64(v.get("lmf_extent_base_low"), v.get("lmf_extent_base_high")), \
        u64(v.get("lmf_extent_end_low"), v.get("lmf_extent_end_high"))
    if b is None and e is None:
        return "-"
    return "[%s, %s)" % (hex(b) if b is not None else "?", hex(e) if e is not None else "?")


def render_human(eps, ue4ss_refusals, markers, bare, status, args, total, bad):
    L = []
    w = L.append
    w("== epoch_view — one row per GAME EPOCH (pid), from the DURABLE sinks ==")
    w("sdiag: %s (%d records%s)" % (args.sdiag, total,
                                    ", %d unparseable line(s) skipped" % bad if bad else ""))
    if args.ue4ss:
        w("ue4ss mirror: %s" % args.ue4ss)
    if args.status:
        w("runtime-status: %s — LATEST-STATE-ONLY: applies to session pid %s "
          "ALONE; never read as history (user directive)" % (
              args.status, status.get("session_pid")))
    if args.baseline_bytes:
        w("baseline cut: %d B — records at/after this offset are new=yes "
          "(epoch discriminator for RUN-N+1); history is always shown" % args.baseline_bytes)
    w("")
    w("epoch matrix (one row per pid, file/append order):")
    w("  %-9s %-7s %-10s %-10s %-6s %-46s %-30s %-34s %s" % (
        "pid", "recs", "t_first", "t_last", "spanms", "stage chain (first-seen)",
        "terminal", "predicted pair", "verdict (oracle)"))
    for ep in eps:
        v = ep["values"]
        refusal = (ue4ss_refusals.get(ep["pid"]) or [None])[-1] if args.ue4ss else None
        verdict, vclass, notes, e4, quartet = classify_verdict(ep, refusal, status)
        ep["_verdict"], ep["_vclass"], ep["_e4"] = verdict, vclass, e4
        term = "-"
        if refusal:
            term = "%s code=%s" % (refusal["stage"], to_int32(refusal["code"]) if refusal["code"] < 0 else refusal["code"])
        elif "runtime_start_failed" in v:
            term = "runtime_start_failed code=%s" % to_int32(v["runtime_start_failed"])
        elif "mfa_activate_failed" in v:
            term = "mfa_activate_failed code=%s" % to_int32(v["mfa_activate_failed"])
        elif "runtime_start_ok" in v:
            term = "runtime_start_ok"
        span = (ep["t_last"] - ep["t_first"]) if (ep["t_first"] is not None and ep["t_last"] is not None) else "-"
        chain = " -> ".join(ep["chain"]) if ep["chain"] else "(no staged tags)"
        occ = "#%d" % ep["occurrence"] if ep["occurrence"] > 1 else ""
        newmark = " [NEW]" if (args.baseline_bytes > 0 and ep["first_new_record"]
                               and not ep["any_old_record"]) else ""
        w("  %-9s %-7s %-10s %-10s %-6s %-46s %-30s %-34s %s%s" % (
            "%s%s" % (ep["pid"], occ), ep["records"],
            ep["t_first"] if ep["t_first"] is not None else "-",
            ep["t_last"] if ep["t_last"] is not None else "-",
            span, short(chain, 46), short(term, 30), short(fmt_pair(v), 34),
            short(verdict, 68), newmark))
    w("")
    for ep in eps:
        v = ep["values"]
        w("-- epoch pid=%s%s %s" % (
            ep["pid"], " (occurrence #%d)" % ep["occurrence"] if ep["occurrence"] > 1 else "",
            "[NEW since baseline cut] " if (args.baseline_bytes > 0 and ep["first_new_record"]
                                            and not ep["any_old_record"]) else ""))
        w("   records=%d seq=%s..%s t_first=%s t_last=%s span_ms=%s" % (
            ep["records"], ep["seq_first"], ep["seq_last"], ep["t_first"], ep["t_last"],
            (ep["t_last"] - ep["t_first"]) if ep["t_first"] is not None else "-"))
        if ep["boot_gap_suspect"]:
            w("   NOTE: t_ms went BACKWARDS vs the previous epoch — console "
              "reboot between epochs; t_ms is per-boot CLOCK_MONOTONIC, order "
              "above is file/append order.")
        w("   stage chain (coarse):    %s" % (" -> ".join(ep["chain"]) or "(none)"))
        w("   stage chain (evidence.sh S-stages): %s" % (
            " -> ".join(COARSE_TO_SSTAGE.get(c, c) for c in ep["chain"]) or "(none)"))
        if "cfg" in ep["chain_seen"] and ("lease" in ep["chain_seen"] or "anchors" in ep["chain_seen"]) \
                and ep["chain"].index("cfg") < max(
                    ep["chain"].index(x) for x in ("lease", "anchors") if x in ep["chain_seen"]):
            w("   note: cfg appears before lease/anchors — bridge_exepath_source "
              "is emitted mid-walk (evidence.sh GAP-3 display quirk; first-seen "
              "order preserved, not reordered).")
        w("   terminal: %s" % (
            "see refusal line below" if args.ue4ss and ue4ss_refusals.get(ep["pid"]) else (
                "runtime_start_failed code=%s (sdiag value %s)" % (
                    to_int32(v["runtime_start_failed"]), v["runtime_start_failed"]) if "runtime_start_failed" in v else
                "mfa_activate_failed code=%s (%s; int32)" % (
                    v["mfa_activate_failed"], hex32(v["mfa_activate_failed"])) if "mfa_activate_failed" in v else
                "runtime_start_ok" if "runtime_start_ok" in v else
                "(none observed in this file window)")))
        if args.ue4ss and ue4ss_refusals.get(ep["pid"]):
            for r in ue4ss_refusals[ep["pid"]]:
                w("   refusal line: %s" % r["line"])
                if "phase=6" in r["detail"]:
                    w("   pad error code: %s = %s (pad error domain)"
                      % (r["detail"].split("error=")[-1],
                         hex32(int(r["detail"].split("error=")[-1]))))
        w("   module_start_return: %s (0 after a refusal = entry returned 0, "
          "game continues UNMODDED — evidence.sh G4 quirk, not an unload)"
          % v.get("module_start_return", "-"))
        w("   predicted pair: %s ; lease_reg=%s ; extent=%s ; cfg gamedir/workdir=%s/%s"
          % (fmt_pair(v), v.get("bridge_lease_reg", "-"), fmt_extent(v),
             v.get("bridge_cfg_gamedir", "-"), v.get("bridge_cfg_workdir", "-")))
        w("   activation: mfa_activated_low/high=%s/%s ; E4: %s"
          % (v.get("mfa_activated_low", "-"), v.get("mfa_activated_high", "-"), ep.get("_e4", "-")))
        harness_green = all(t in ep["events"] for t in
                            ("buffered_lua_begin", "buffered_lua_result", "lua_memory_check_passed")) \
            and ep["events"].get("buffered_lua_result", {}).get("last_value") == 0
        w("   buffered-Lua harness: %s (buffered_lua_result=%s)"
          % ("green" if harness_green else "not green / absent",
             ep["events"].get("buffered_lua_result", {}).get("last_value", "-")))
        if markers.get(ep["pid"]):
            w("   progress markers (position-attributed): %d line(s), e.g. %s"
              % (len(markers[ep["pid"]]), short(markers[ep["pid"]][-1], 80)))
        for n in ep.get("_notes", []):
            w("   note: %s" % n)
        w("")
    if args.ue4ss and bare:
        w("unattributed legacy failure lines in the mirror (no r4 pid prefix; "
          "pre-r4 epochs — counted, never attributed): %d" % len(bare))
        for b in bare[:3]:
            w("   e.g. %s" % short(b, 100))
        w("")
    if args.ue4ss:
        mirror_only = sorted(set(ue4ss_refusals) - {ep["pid"] for ep in eps})
        if mirror_only:
            w("mirror-only epochs (pid-prefixed refusal lines with NO sdiag "
              "records in this file — earlier launches outside the accumulated "
              "window; shown for correlation, never merged into sdiag epochs):")
            for p in mirror_only:
                r = ue4ss_refusals[p][-1]
                cls = "D6 conversion refusal (launch-9 terminal)" \
                    if "no observed mapping interval intersects" in r["detail"] else \
                    ("%s code=%s" % (r["stage"], r["code"]))
                w("   pid=%d t=%s %s — %s" % (p, r["t_ms"], cls, short(r["detail"], 90)))
            w("")
    if status.get("present"):
        w("status (LATEST-STATE-ONLY): session pid=%s ms=%s states=%d last=%s code=%s"
          % (status.get("session_pid"), status.get("session_ms"),
             status.get("n_states"), status.get("last_state"), status.get("last_code")))
        if status.get("last_detail"):
            w("status detail: %s" % status.get("last_detail"))
        matched = [ep for ep in eps if ep["pid"] == status.get("session_pid")]
        if not matched:
            w("NOTE: the status session pid has NO sdiag epoch in this file — "
              "either a newer epoch whose diag records were not fetched yet, or "
              "a stale status. The status is NOT history; do not apply it to "
              "other epochs.")
        w("")
    w("GUARDS: per-epoch (per-pid) values only — last-write-wins WITHIN a pid; "
      "t_ms deltas are never taken across epochs; the status file is "
      "latest-state-only and is never used as epoch history.")
    return "\n".join(L)


def build_json(eps, ue4ss_refusals, markers, bare, status, args, total, bad):
    out = {
        "tool": TOOL, "version": VERSION,
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "source": {"sdiag": args.sdiag, "ue4ss": args.ue4ss, "status": args.status,
                   "build_id": args.build_id},
        "baseline_bytes": args.baseline_bytes,
        "records_total": total, "records_unparseable": bad,
        "directive": "runtime-status.txt is LATEST-STATE-ONLY; authoritative "
                     "activation/terminal evidence is the durable per-pid sdiag "
                     "+ UE4SS mirror events; correlate by build/run/pid, never "
                     "race the status file.",
        "status_latest_state_only": status if status.get("present") else None,
        "unattributed_legacy_failure_lines": len(bare),
        "epochs": [],
    }
    for ep in eps:
        v = ep["values"]
        refusal = (ue4ss_refusals.get(ep["pid"]) or [None])[-1] if args.ue4ss else None
        verdict, vclass, notes, e4, quartet = classify_verdict(ep, refusal, status)
        extent = {t: ep["events"][t]["last_value"] for t in EXTENT_TAGS if t in ep["events"]}
        b, e = u64(v.get("lmf_extent_base_low"), v.get("lmf_extent_base_high")), \
            u64(v.get("lmf_extent_end_low"), v.get("lmf_extent_end_high"))
        rec = {
            "pid": ep["pid"],
            "occurrence": ep["occurrence"],
            "build_id": args.build_id,
            "session_status": ("%s/%s" % (status["session_pid"], status["session_ms"]))
            if status.get("present") and status.get("session_pid") == ep["pid"] else None,
            "records": ep["records"],
            "seq_range": [ep["seq_first"], ep["seq_last"]],
            "t_first_ms": ep["t_first"], "t_last_ms": ep["t_last"],
            "span_ms": (ep["t_last"] - ep["t_first"]) if ep["t_first"] is not None else None,
            "new_since_baseline_cut": bool(ep["first_new_record"] and not ep["any_old_record"]) if args.baseline_bytes else None,
            "boot_gap_suspect": ep["boot_gap_suspect"],
            "chain_coarse": ep["chain"],
            "chain_evidence_sh": [COARSE_TO_SSTAGE.get(c, c) for c in ep["chain"]],
            "terminal": {
                "stage": refusal["stage"] if refusal else (
                    "runtime_start_failed" if "runtime_start_failed" in v else
                    "mfa_activate_failed" if "mfa_activate_failed" in v else
                    "runtime_start_ok" if "runtime_start_ok" in v else None),
                "code": refusal["code"] if refusal else (
                    to_int32(v.get("runtime_start_failed")) if "runtime_start_failed" in v else
                    to_int32(v.get("mfa_activate_failed")) if "mfa_activate_failed" in v else None),
                "detail": refusal["detail"] if refusal else None,
                "line": refusal["line"] if refusal else None,
                "module_start_return": v.get("module_start_return"),
            },
            "predicted_pair": {
                "anchor_count": v.get("lmf_anchor_count"),
                "intervals_found": v.get("lmf_intervals_found"),
                "lease_spans": v.get("bridge_lease_reg_spans"),
                "lease_reg": v.get("bridge_lease_reg"),
                "extent": [hex(b) if b is not None else None, hex(e) if e is not None else None],
                "cfg_gamedir": v.get("bridge_cfg_gamedir"),
                "cfg_workdir": v.get("bridge_cfg_workdir"),
                "spanpub_count": v.get("bridge_spanpub_count"),
                "spanpub_total": v.get("bridge_spanpub_total"),
                "spanpub_cap": v.get("bridge_spanpub_cap"),
            },
            "activation": {
                "mfa_activated_low": v.get("mfa_activated_low"),
                "mfa_activated_high": v.get("mfa_activated_high"),
                "E4_activated": e4,
            },
            "conversion_quartet_met": quartet,
            "harness_green": bool(all(t in ep["events"] for t in
                                      ("buffered_lua_begin", "buffered_lua_result",
                                       "lua_memory_check_passed"))
                                  and ep["events"].get("buffered_lua_result", {}).get("last_value") == 0),
            "extent_events": extent,
            "verdict": verdict,
            "verdict_class": vclass,
            "notes": notes,
            "progress_markers_position_attributed": markers.get(ep["pid"], []),
            "event_tag_counts": {t: ev["count"] for t, ev in sorted(ep["events"].items())},
        }
        out["epochs"].append(rec)
    return out


def main(argv=None):
    global BASELINE_BYTES
    ap = argparse.ArgumentParser(add_help=True, description=__doc__.splitlines()[0])
    ap.add_argument("sdiag_pos", nargs="?", help="accumulated supplier-diag jsonl")
    ap.add_argument("--sdiag", dest="sdiag_opt", help="explicit sdiag jsonl path (or '-' for stdin)")
    ap.add_argument("--ue4ss", help="optional UE4SS.log mirror (append-only across epochs)")
    ap.add_argument("--status", help="optional runtime-status.txt — LATEST-STATE-ONLY")
    ap.add_argument("--baseline-bytes", type=int, default=0,
                    help="append-only baseline cut (byte offset); records at/after it are new")
    ap.add_argument("--build-id", default=None,
                    help="build label from the controller receipt (sdiag carries none)")
    ap.add_argument("--json", dest="as_json", action="store_true", help="emit the JSON document")
    ap.add_argument("--out", help="also write the human table to FILE")
    args = ap.parse_args(argv)
    BASELINE_BYTES = max(0, args.baseline_bytes)

    path = args.sdiag_opt or args.sdiag_pos
    if not path:
        ap.error("an sdiag jsonl is required (positional or --sdiag)")
    args.sdiag = path  # normalize for renderers
    try:
        eps, total, bad = parse_sdiag(path)
    except OSError as e:
        print("%s: cannot read sdiag: %s" % (TOOL, e), file=sys.stderr)
        return 2

    ue4ss_refusals, markers, bare = {}, {}, []
    if args.ue4ss:
        try:
            ue4ss_refusals, markers, bare = parse_ue4ss(args.ue4ss)
        except OSError as e:
            print("%s: cannot read ue4ss log: %s" % (TOOL, e), file=sys.stderr)
            return 2
    status = {"present": False}
    if args.status:
        try:
            status = parse_status(args.status)
        except OSError as e:
            print("%s: cannot read status: %s" % (TOOL, e), file=sys.stderr)
            return 2

    # attach verdict/notes for human rendering
    for ep in eps:
        refusal = (ue4ss_refusals.get(ep["pid"]) or [None])[-1]
        verdict, vclass, notes, e4, _q = classify_verdict(ep, refusal, status)
        ep["_verdict"], ep["_vclass"], ep["_notes"], ep["_e4"] = verdict, vclass, notes, e4

    if not eps:
        print("== epoch_view ==\nNO-EPOCHS: no modbridge.supplier-diag/v1 records in %s" % path)
        print("Absence is a normal outcome unless the diagnostic-sink landed; "
              "the composition launch counter is module_start_entered.")
        return 0

    if args.as_json:
        doc = build_json(eps, ue4ss_refusals, markers, bare, status, args, total, bad)
        text = json.dumps(doc, indent=2, ensure_ascii=False)
    else:
        text = render_human(eps, ue4ss_refusals, markers, bare, status, args, total, bad)
    print(text)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
