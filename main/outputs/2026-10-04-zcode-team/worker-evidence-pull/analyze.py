#!/usr/bin/env python3
"""analyze.py — offline launch-evidence analyzer for the PS5 UE4SS runtime port.

worker-evidence-pull · 2026-10-05 · implements the evidence procedure of
`outputs/2026-10-04-zcode-team/worker-launch-readiness/NEXT-LAUNCH-RUNBOOK.md`
§2 (rung cascade C0-C5), §6 (pull procedure: fresh-baseline cut, same-pid
correlation, greps in order, NUL-interleave check) and §7 (failure-decision
rows). LOCAL file analysis only: no network, no console contact, no writes
outside --out (default: the analyzed run dir).

Honesty: this tool classifies what the fetched bytes SHOW, per rung
PROVEN / REFUSED / PENDING (PENDING includes "not-reached per cascade",
annotated with the blocking rung). It never upgrades a claim past the first
unmet rung (§2), never treats runtime-status as history (LATEST-STATE-ONLY;
applies to its session pid alone), never subtracts t_ms across epochs, and
never rates a rung whose observables were not fetched.

Inputs (a run dir with any of these names; aliases auto-detected):
  UE4SS.log | fullfix-ue4ss.log          pid-prefixed or legacy UE4SS mirror
  runtime-status.txt | fullfix-status.txt  LATEST-STATE-ONLY status
  modbridge-supplier-diag.jsonl | fullfix-sdiag.jsonl  durable sdiag (append-only)
  modbridge-exec-write-probe.jsonl       drift guard (absence is NORMAL)
  townfall_runtime_aligned_entry.log     history guard

Usage:
  analyze.py --dir RUNDIR [--baseline-bytes N] [--build-id S] [--out DIR]
             [--max-pid P] [--label S] [--epoch-view PATH] [--json-only]

Exit codes: 0 analyzed · 2 usage/input error.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys

TOOL = "analyze.py"
VERSION = "1.0.0"

# runbook §1: the ONLY mod-executed proof, exact expected text
BANNER_EXACT = ("[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only "
                "release. F7 opens the menu.")
PAD_DEFERRED_ERR = 2157051908  # 0x80920004
POOL_VERDICT_MEANING = {
    0: "0 = not-mapped (ENOMEM) -> wrong-base suspicion; seed-table derivation audit first (row 3)",
    1: "1 = mapped, read permission UNPROVEN -> expected honest mapped-but-unreadable refusal (row 2)",
    2: "2 = unavailable (other errno) -> syscall class UNSUPPORTED; stop the mincore route (row 1)",
    3: "3 = invalid request (len 0 or >16 MiB) -> instrumentation bug; pool NOT RATED this boot (row 4)",
}

# aliases per sink (canonical first)
ALIASES = {
    "ue4ss":  ["UE4SS.log", "fullfix-ue4ss.log"],
    "status": ["runtime-status.txt", "fullfix-status.txt"],
    "sdiag":  ["modbridge-supplier-diag.jsonl", "fullfix-sdiag.jsonl"],
    "probe":  ["modbridge-exec-write-probe.jsonl", "fullfix-exec-write-probe.jsonl"],
    "entry":  ["townfall_runtime_aligned_entry.log", "fullfix-entry.log",
               "fullfix-townfall_runtime_aligned_entry.log"],
}
# expected console paths (for the manifest / next-operator hints)
REMOTE_PATHS = {
    "ue4ss":  "/data/plugins/UE4SS.log",
    "status": "/data/modbridge/roots/PPSA33286/runtime-status.txt",
    "sdiag":  "/data/modbridge-supplier-diag.jsonl",
    "probe":  "/data/modbridge-exec-write-probe.jsonl",
    "entry":  "/data/townfall_runtime_aligned_entry.log",
}

PID_PREFIX_RE = re.compile(r"^\[t=(\d+)ms pid=(\d+)\]")
R4_FAIL_RE = re.compile(r"^\[t=(\d+)ms pid=(\d+)\] modbridge module_start failed: (\S+) code=(-?\d+) detail=(.*)$")

# runbook §6.3 greps, IN ORDER (window-filtered per §6.6)
GREPS = [
    ("load_banner",      r"Loaded v1\.0",                       "THE ONE QUESTION: mod-executed banner"),
    ("mod_absent",       r"Mod is absent",                      "must be ZERO in the pid window"),
    ("seeds",            r"Seeds\] REFUTED|Seeds\] slot|FName pool validated|Required-slot seed table armed", "seeds arm / chain / pool verdict"),
    ("resolution",       r"range-verdict resolution|not consulted \(failure class not rescuable\)|Setting up mods|Finding GameEngine::LoadMap", "NEW range-verdict note + ordering anchors"),
    ("r3",               r"\[R3\]",                             "marshal arm / pump / refusals"),
    ("delivery",         r"spawnable pickups found|Menu created|Scan failed|EngineTick", "C5 delivery + menu rungs"),
]
# extra discriminator greps (failure-decision table rows)
EXTRA_GREPS = [
    ("mods_start",       r"Starting mods|starting mod\.",       "C3 entry lines"),
    ("load_fail_variants", r"Failed to execute main script|Exception during mod startup", "row 11"),
    ("marshal_fail_variants", r"marshal carrier candidate not found|marshal arm refused|marshal arm failed", "row 8"),
    ("walkguard",        r"WalkRxGuard\] REFUSED|guard_off_no_rx_verdict", "row 20 GATE-1"),
    ("engine_markers",   r"engine-ready|mods-started",          "engine-ready / mods-started mirror lines"),
    ("path_test",        r"PATH_TEST:.*monotonic-ns",           "inter-plugin gap stamp"),
]
SNDIAG_INTEREST = re.compile(r"^(?:pad_.+|pool_range_verdict_fresh|module_start_entered|module_start_return|runtime_start_ok|runtime_start_failed|bridge_spanpub_count|bridge_spanpub_total)$")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def find_file(rundir, names):
    for n in names:
        p = os.path.join(rundir, n)
        if os.path.isfile(p) and os.path.getsize(p) > 0:
            return p
    return None


def to_int32(v):
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


# ---------------------------------------------------------------------------
# sinks
# ---------------------------------------------------------------------------
def parse_status(path):
    """LATEST-STATE-ONLY: session row + last state belong to that pid ALONE."""
    out = {"present": True, "path": path, "session_pid": None, "session_ms": None,
           "n_states": 0, "states": [], "last_state": None, "last_code": None,
           "last_detail": None, "header_ok": False}
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line == "modforge-runtime-status\t1":
                out["header_ok"] = True
            elif line.startswith("session\t"):
                p = line.split("\t")
                if len(p) >= 3:
                    try:
                        out["session_pid"] = int(p[1])
                        out["session_ms"] = int(p[2])
                    except ValueError:
                        pass
            elif line.startswith("state\t"):
                p = line.split("\t")
                if len(p) >= 4:
                    out["n_states"] += 1
                    st = {"state": p[2], "code": p[3],
                          "detail": p[4] if len(p) >= 5 else ""}
                    out["states"].append(st)
                    out["last_state"], out["last_code"], out["last_detail"] = \
                        st["state"], st["code"], st["detail"]
    return out


def parse_sdiag(path, baseline_bytes):
    """Single pass; byte offsets so the baseline cut LABELS records (§6.2/epoch_view law)."""
    epochs = {}
    order = []
    total = 0
    bad = 0
    raw = open(path, "rb").read()
    off = 0
    for chunk in raw.split(b"\n"):
        start = off
        off += len(chunk) + 1
        if not chunk.strip():
            continue
        if b'"modbridge.supplier-diag/v1"' not in chunk:
            continue
        try:
            rec = json.loads(chunk.decode("utf-8", "replace"))
        except json.JSONDecodeError:
            bad += 1
            continue
        total += 1
        pid = rec.get("pid")
        if not isinstance(pid, int):
            continue
        ep = epochs.get(pid)
        if ep is None:
            ep = {"pid": pid, "records": 0, "n_new": 0, "n_old": 0,
                  "seq_first": None, "seq_last": None,
                  "t_first": None, "t_last": None,
                  "tags": {}, "tag_counts": {},
                  "tag_t": {}, "tag_t_last": {}, "pad_seq": []}
            epochs[pid] = ep
            order.append(pid)
        ep["records"] += 1
        if start >= baseline_bytes:
            ep["n_new"] += 1
        else:
            ep["n_old"] += 1
        seq, t, tag, val = rec.get("seq"), rec.get("t_ms"), rec.get("tag", ""), rec.get("value")
        if isinstance(seq, int):
            if ep["seq_first"] is None:
                ep["seq_first"] = seq
            ep["seq_last"] = seq
        if isinstance(t, int):
            if ep["t_first"] is None:
                ep["t_first"] = t
            ep["t_last"] = t
        ep["tag_counts"][tag] = ep["tag_counts"].get(tag, 0) + 1
        if SNDIAG_INTEREST.match(tag):
            ep["tags"][tag] = val  # last-write-wins within pid
            if isinstance(t, int):
                if tag not in ep["tag_t"]:
                    ep["tag_t"][tag] = t
                ep["tag_t_last"][tag] = t
        if tag.startswith("pad_") or tag == "pool_range_verdict_fresh":
            ep["pad_seq"].append({"seq": seq, "t_ms": t, "tag": tag, "value": val})
    return [epochs[p] for p in order], total, bad


def parse_log(path):
    """Return (lines, prefixed_mode) where lines = [(lineno, t_ms, pid, text, raw_bytes)]."""
    raw = open(path, "rb").read()
    lines = []
    n_prefixed = 0
    for i, rb in enumerate(raw.split(b"\n")):
        txt = rb.decode("utf-8", "replace").rstrip("\r")
        t = pid = None
        m = PID_PREFIX_RE.match(txt)
        if m:
            t, pid = int(m.group(1)), int(m.group(2))
            n_prefixed += 1
        lines.append((i + 1, t, pid, txt, rb))
    return lines, (n_prefixed > 0), raw


def window(lines, prefixed_mode, max_pid):
    """§6.6 same-pid correlation: pid-prefixed logs -> ONLY max-pid lines.
    Legacy (no prefixes) logs: the whole file is the window, loudly flagged —
    correlate via the status session pid; never invented attribution."""
    if prefixed_mode:
        return [l for l in lines if l[2] == max_pid], False
    return lines, True


def run_greps(wl, specs):
    out = {}
    for key, pat, why in specs:
        rx = re.compile(pat)
        hits = [(ln, t, txt) for (ln, t, _p, txt, _rb) in wl if rx.search(txt)]
        out[key] = {"why": why, "count": len(hits), "hits": hits}
    return out


def nul_check(wl):
    """§6.4 NUL-interleave check over the window (raw bytes)."""
    bad = [(ln, t, rb[:60]) for (ln, t, _p, _txt, rb) in wl if b"\x00" in rb]
    return {"nul_lines": len(bad), "examples": [(ln, tb.decode("utf-8", "replace")) for (ln, t, tb) in bad[:10]]}


def line_with_nul(wl, pattern):
    """The §1 banner NUL check: any window line matching pattern must be NUL-free."""
    rx = re.compile(pattern)
    for (ln, t, _p, txt, rb) in wl:
        if rx.search(txt):
            return {"found": True, "line": ln, "t_ms": t, "clean": b"\x00" not in rb,
                    "text": txt}
    return {"found": False, "clean": None, "line": None, "t_ms": None, "text": None}


def class_of_refusal(text):
    if not text:
        return None, None
    m = re.search(r"range-verdict resolution:\s*(.*?)\s*$", text)
    note = m.group(1).rstrip() if m else None
    if "snapshot truncated" in text or "cannot prove heap coverage" in text:
        cls = "CaptureTruncated"
    elif "not consulted (failure class not rescuable)" in text:
        cls = "Structural/SpanTableUnavailable (probe never fired)"
    elif "omitted mapping" in text:
        cls = "NotCovered"
    else:
        cls = None
    return cls, note


# ---------------------------------------------------------------------------
# rung cascade
# ---------------------------------------------------------------------------
def eval_rungs(W, sd, status, max_pid, log_pid_mode, pool_fresh):
    """Evaluate the runbook §2 cascade on the max-pid window only.
    Returns (rows, load_verdict, cascade_stop)."""
    rows = []          # (rung, observable, status, evidence)
    def add(rung, obs, status, ev=""):
        rows.append({"rung": rung, "observable": obs, "status": status, "evidence": ev})

    g = W["greps"]
    tags = sd["tags"] if sd else {}

    # ---- C0 boot frame (always collected) -------------------------------
    trio = [
        ("pad_input_deferred", 6),
        ("pad_input_deferred_error", PAD_DEFERRED_ERR),
        ("pad_input_deferred_input_gate", 1),
    ]
    c0_core_ok = True
    for tag, want in trio:
        got = tags.get(tag)
        ok = (got == want)
        c0_core_ok = c0_core_ok and ok
        add("C0", "%s=%s" % (tag, want), "PROVEN" if ok else "PENDING",
            "sdiag value=%s%s" % (got, "" if ok else " (expected %s)" % want))
    rsf = tags.get("runtime_start_failed")
    if rsf is not None:
        add("C0", "runtime_start_failed ABSENT", "REFUSED",
            "sdiag runtime_start_failed=%s (%s) — startup refused (row 21)" % (rsf, hex32(rsf)))
    else:
        add("C0", "runtime_start_failed ABSENT", "PROVEN", "no START refusal tag in pid window")
    rso = tags.get("runtime_start_ok")
    add("C0", "runtime_start (module_start chain)", "PROVEN" if rso is not None else "PENDING",
        "runtime_start_ok=%s" % rso if rso is not None else "no runtime_start_ok tag yet in window")
    span = tags.get("bridge_spanpub_count")
    add("C0", "spanpub publication", "PROVEN" if span is not None else "PENDING",
        "bridge_spanpub_count=%s (total=%s)" % (span, tags.get("bridge_spanpub_total", "-"))
        if span is not None else "no spanpub tags in pid window")
    setting_up = g["resolution"]["hits"] and any("Setting up mods" in h[2] for h in g["resolution"]["hits"])
    add("C0", "'Setting up mods...'", "PROVEN" if setting_up else "PENDING",
        "present in pid window (ctor-time, precedes [Seeds]; §5 quirk)" if setting_up else "not in window")
    armed = any("[Seeds] Required-slot seed table armed" in h[2] for h in g["seeds"]["hits"])
    add("C0", "[Seeds] seed table armed", "PROVEN" if armed else "PENDING",
        "present" if armed else "not in window")
    c0_status = "REFUSED" if rsf is not None else ("PROVEN" if (c0_core_ok and armed) else "PENDING")

    # ---- C1 pool v5 verdict (THE fork) ----------------------------------
    seeds_hits = g["seeds"]["hits"]
    ref_line = next((h for h in seeds_hits if "Seeds] REFUTED" in h[2]), None)
    val_line = next((h for h in seeds_hits if "FName pool validated" in h[2]), None)
    # status refused detail applies ONLY if session pid == max pid (LATEST-STATE-ONLY)
    st_ref = None
    if status.get("present") and status.get("session_pid") == max_pid \
            and status.get("last_detail") and "REFUTED" in status["last_detail"]:
        st_ref = status["last_detail"]
    refusal_text = (ref_line[2] if ref_line else None) or st_ref
    cls = note = None
    if refusal_text:
        cls, note = class_of_refusal(refusal_text)
    if val_line is not None:
        add("C1", "pool verdict", "PROVEN", val_line[2])
        for h in seeds_hits:
            if "Seeds] slot" in h[2]:
                add("C1", "[Seeds] slot bind", "PROVEN", h[2])
        c1_status = "PROVEN"
    elif refusal_text:
        add("C1", "pool verdict", "REFUSED", refusal_text)
        add("C1", "failure_class", "PROVEN" if cls else "PENDING", cls or "class not decodable from refusal text")
        add("C1", "range-verdict resolution note", "PROVEN" if note else "PENDING",
            note or "no 'range-verdict resolution:' suffix in refusal text")
        if pool_fresh is not None:
            add("C1", "pool_range_verdict_fresh", "PROVEN",
                "value=%s — %s" % (pool_fresh, POOL_VERDICT_MEANING.get(pool_fresh, "?")))
        else:
            add("C1", "pool_range_verdict_fresh", "PENDING",
                "no fresh-probe tag in pid window (pre-v5 build, or failure class not rescuable — row 5)")
        c1_status = "REFUSED"
    else:
        add("C1", "pool verdict", "PENDING",
            "no REFUTED / validated line in window; status refused detail applies only to its session pid")
        c1_status = "PENDING"

    # ---- C2 post-seeds chain (only after C1 PASS) ------------------------
    c2_items = [
        ("Using engine version", any("Using engine version" in h[2] for h in _all(g, "resolution", "mods_start", "engine_markers"))),
        ("Waiting for object construction", any("Waiting for object construction" in h[2] for h in _all(g, "resolution", "mods_start", "engine_markers"))),
        ("Finding GameEngine::LoadMap", any("Finding GameEngine::LoadMap" in h[2] for h in g["resolution"]["hits"])),
        ("[R3] fold lines", g["r3"]["count"] > 0),
        ("engine-ready", (status.get("present") and status.get("session_pid") == max_pid
                          and status.get("last_state") == "engine-ready")
                         or any("engine-ready" in h[2] for h in g["engine_markers"]["hits"])),
    ]
    for obs, ok in c2_items:
        add("C2", obs, "PROVEN" if ok else "PENDING", "in pid window" if ok else "not in window")
    c2_status = "PROVEN" if (c1_status == "PROVEN" and all(ok for _o, ok in c2_items)) else "PENDING"

    # ---- C3 the LOAD question (THE ONE QUESTION) -------------------------
    banner = g["load_banner"]
    banner_line = line_with_nul(wl_for_banner := W["win"], r"Loaded v1\.0")
    absent_n = g["mod_absent"]["count"]
    absent_clean = all(b"\x00" not in rb for (_ln, _t, _p, _tx, rb) in
                       [(h[0], h[1], None, h[2], _raw_of(W, h)) for h in g["mod_absent"]["hits"]]) \
        if absent_n else None
    if banner["count"] > 0 and banner_line["found"] and banner_line["clean"] and absent_n == 0:
        add("C3", "LOAD banner", "PROVEN", banner_line["text"] + ("" if banner_line["text"] == BANNER_EXACT else "  [text differs from runbook §1 exact banner]"))
        add("C3", "'Mod is absent' zero", "PROVEN", "0 lines in pid window")
        add("C3", "banner line NUL-clean (to_generic_string fix)", "PROVEN", "no interleave NUL on the banner line")
        load_verdict = "PASS"
        row = "row 9: LOAD RUNG CONFIRMED (scope: main.lua top level + six binds; menu/spawn NOT claimed)"
        c3_status = "PROVEN"
    else:
        if banner["count"] == 0:
            add("C3", "LOAD banner", "REFUSED" if absent_n else "PENDING",
                "no 'Loaded v1.0' line in pid window")
        else:
            add("C3", "LOAD banner", "PENDING", "banner line present but NUL/truncation check unresolved")
        if absent_n:
            garbled = not absent_clean
            add("C3", "'Mod is absent' zero", "REFUSED",
                "%d line(s); path garbled(NUL)=%s" % (absent_n, garbled))
            add("C3", "banner NUL-clean", "REFUSED" if garbled else "PENDING",
                "row 17: double-garble fix NOT in the linked build (build identity)" if garbled
                else "row 10: clean path but lookup STILL missed — source registration audit")
        load_verdict = "FAIL" if (absent_n or any(h[2] for h in g["load_fail_variants"]["hits"])) else "PENDING"
        c3_status = "REFUSED" if load_verdict == "FAIL" else "PENDING"
        row = ("row 17: build identity — compare staged unit shas before touching the route" if absent_n and (absent_clean is False)
               else "row 10: source registration audit" if absent_n
               else "row 11: capture exact text + NUL check; reconcile read-path chain" if g["load_fail_variants"]["count"]
               else "banner not yet in window (mid-flight or truncated window)")

    # marshal arm/pump (bank whenever visible, §5 #13)
    armed_line = next((h for h in g["r3"]["hits"] if "FuncPtr-marshal armed" in h[2]), None)
    pump_line = next((h for h in g["r3"]["hits"] if "marshal is the game-thread pump" in h[2]), None)
    add("C3", "marshal arm", "PROVEN" if armed_line else "PENDING", armed_line[2] if armed_line else "no arm line in window")
    add("C3", "marshal pump-live", "PROVEN" if pump_line else "PENDING",
        pump_line[2] if pump_line else ("armed-without-pump proves nothing about drain latency (F-1; row 7)" if armed_line else "not reached"))
    if g["marshal_fail_variants"]["count"]:
        add("C3", "marshal failure variants", "REFUSED", g["marshal_fail_variants"]["hits"][0][2] + " (row 8: game-thread-pump rung NOT EXERCISED)")
    ms = status.get("last_state") == "mods-started" and status.get("session_pid") == max_pid
    add("C3", "mods-started ack", "PROVEN" if ms else "PENDING",
        "status last state (LATEST-STATE-ONLY, session pid matches)" if ms else
        "NOT proof of mod execution even if seen (fires at engine-ready on ctor inventory OK; §1)")

    # ---- C4 input ARRIVES rung ------------------------------------------
    pa = tags.get("pad_available")
    pr = tags.get("pad_retry")
    prf = tags.get("pad_retry_failed")
    poc = tags.get("pad_open_class")
    ps = sd.get("pad_seq", [])
    if pa is not None and isinstance(pa, int) and pa > 0:
        avail_recs = [e for e in ps if e["tag"] == "pad_available"]
        a0 = avail_recs[0] if avail_recs else None
        ordered = False
        order_ev = "arrival record not in pad_seq (degraded window)"
        if a0 is not None:
            idx = ps.index(a0)
            later_retries = [e for e in ps[idx + 1:] if e["tag"] in ("pad_retry", "pad_retry_failed")]
            prev_rfail = [e for e in ps[:idx] if e["tag"] == "pad_retry_failed"]
            after_last = (not prev_rfail) or (
                isinstance(a0["seq"], int) and isinstance(prev_rfail[-1]["seq"], int)
                and a0["seq"] > prev_rfail[-1]["seq"])
            ordered = (not later_retries) and after_last
            order_ev = ("t_ms=%s seq=%s; after-last-retry_failed=%s; retries-after=%d"
                        % (a0["t_ms"], a0["seq"],
                           ("yes (seq %s)" % prev_rfail[-1]["seq"]) if prev_rfail else "no prior retries",
                           len(later_retries)))
        add("C4", "pad_available handle>0", "PROVEN", "value=%s" % pa)
        add("C4", "arrival ordering (once, after last pad_retry_failed)", "PROVEN" if ordered else "REFUSED",
            order_ev + ("" if ordered else " — row 24: out-of-order arrival, capture corrupt, do NOT rate"))
        c4_status = "PROVEN" if ordered else "REFUSED"
    else:
        if prf is not None:
            if prf != PAD_DEFERRED_ERR:
                add("C4", "pad_retry_failed", "REFUSED",
                    "value=%s (%s) != %s — AUDIT FINDING-1 discriminator, new failure mode (row 15)"
                    % (prf, hex32(prf), PAD_DEFERRED_ERR))
            else:
                add("C4", "pad_retry_failed", "PENDING",
                    "value=%s (0x80920004 expected class) — handle never freed; outcome A intact (row 14)" % prf)
            c4_status = "PENDING"
        elif tags.get("pad_input_deferred") is not None and pr is None:
            add("C4", "pad_retry present", "PENDING",
                "deferral present but NO pad_retry at all — linked binary predates the pad_coexist fold; "
                "input rung NOT EXERCISED (build identity, row 16)")
            c4_status = "PENDING"
        else:
            add("C4", "pad tags", "PENDING", "no pad_available/retry tags in pid window (pre-deferral build or not reached)")
            c4_status = "PENDING"
    if poc is not None:
        add("C4", "pad_open_class", "PROVEN", "value=%s (0 expected once)" % poc)

    # ---- C5 delivery + menu rungs ---------------------------------------
    d_hits = g["delivery"]["hits"]
    menu_n = sum(1 for h in d_hits if "spawnable pickups found" in h[2])
    menu_created = next((h for h in d_hits if "Menu created" in h[2]), None)
    scan_fail = next((h for h in d_hits if "Scan failed" in h[2]), None)
    etick_fail = [h for h in d_hits if "Tried to install hook" in h[2]
                  or "Failed to add hook" in h[2] or "EngineTick callback registration failed" in h[2]
                  or "AOB scan failed" in h[2]]
    delivery_any = bool(menu_n or menu_created or scan_fail or etick_fail)
    add("C5", "menu chain '<N> spawnable pickups found'", "PROVEN" if menu_n else "PENDING",
        "%d line(s)" % menu_n if menu_n else "not in window")
    add("C5", "'Menu created.'", "PROVEN" if menu_created else "PENDING", menu_created[2] if menu_created else "menu never promised (§0)")
    if scan_fail:
        add("C5", "Scan failed (drain alive, no save)", "PROVEN", scan_fail[2] + " (row 19)")
    if etick_fail:
        add("C5", "ExecuteInGameThread failure lines", "PROVEN", etick_fail[0][2] + " (input delivery evidence, row 18)")
    c5_status = "PROVEN" if (menu_n and menu_created) else ("PROVEN" if delivery_any else "PENDING")
    if c5_status == "PROVEN" and not (menu_n and menu_created):
        add("C5", "menu rung", "PENDING", "delivery evidenced; menu chain incomplete — absence does not fail delivery (§2 C5)")

    cascade_stop = None
    for rung, st in (("C0", c0_status), ("C1", c1_status), ("C2", c2_status),
                     ("C3", c3_status), ("C4", c4_status), ("C5", c5_status)):
        if st != "PROVEN":
            cascade_stop = (rung, st)
            break
    for r in rows:
        if cascade_stop and r["rung"] > cascade_stop[0] and r["status"] != "REFUSED":
            r["evidence"] = (r["evidence"] + " " if r["evidence"] else "") + \
                "[not-reached per cascade: %s %s]" % (cascade_stop[0], cascade_stop[1])
    return rows, load_verdict, cascade_stop, c0_status, c1_status, c2_status, c3_status, c4_status, c5_status, row


def _all(g, *keys):
    for k in keys:
        for h in g[k]["hits"]:
            yield h


def _raw_of(W, hit):
    """raw bytes of a matched hit line (lineno-keyed)."""
    ln = hit[0]
    for (n, _t, _p, _txt, rb) in W["win"]:
        if n == ln:
            return rb
    return b""


# ---------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(add_help=True, description=__doc__.splitlines()[0])
    ap.add_argument("--dir", required=True, help="run dir containing the fetched files")
    ap.add_argument("--baseline-bytes", type=int, default=None,
                    help="sdiag append-only baseline cut (records at/after are new). "
                         "Default: stage-time recorded size if discoverable, else 0 "
                         "(whole file; recorded-absent baselines make the whole file new).")
    ap.add_argument("--build-id", default="unknown",
                    help="build label from the controller staging receipt (sdiag carries none)")
    ap.add_argument("--max-pid", type=int, default=None, help="override the epoch pid (default: max pid in sdiag)")
    ap.add_argument("--label", default=None, help="human label for this analysis")
    ap.add_argument("--out", default=None, help="output dir (default: --dir)")
    ap.add_argument("--epoch-view", default=os.path.normpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "worker-durable-events", "tools", "epoch_view.py")),
        help="path to epoch_view.py (embedded per-epoch correlation; failures are non-fatal)")
    ap.add_argument("--json-only", action="store_true", help="suppress the human table on stdout")
    a = ap.parse_args(argv)

    rundir = a.dir
    if not os.path.isdir(rundir):
        print("%s: run dir not found: %s" % (TOOL, rundir), file=sys.stderr)
        return 2
    outdir = a.out or rundir
    os.makedirs(outdir, exist_ok=True)

    # ---- discover sinks ---------------------------------------------------
    files = {}
    manifest = []
    for sink, names in ALIASES.items():
        p = find_file(rundir, names)
        files[sink] = p
        manifest.append({
            "sink": sink, "remote_path": REMOTE_PATHS[sink],
            "local": p, "present": p is not None,
            "bytes": os.path.getsize(p) if p else 0,
            "sha256": sha256_file(p) if p else None,
            "absence_normal": sink in ("probe", "entry", "ue4ss"),
        })
    if not files["sdiag"] and not files["ue4ss"] and not files["status"]:
        print("%s: no recognizable evidence files in %s" % (TOOL, rundir), file=sys.stderr)
        return 2

    # ---- baseline provenance ----------------------------------------------
    baseline_bytes = a.baseline_bytes
    baseline_src = "explicit"
    if baseline_bytes is None:
        baseline_bytes = 0
        # stage-time recorded baselines (l10.6): absent -> 0 (whole file new)
        for root, _dirs, fnames in os.walk(os.path.join(rundir, "..")):
            if "l10-baselines.json" in fnames:
                try:
                    doc = json.load(open(os.path.join(root, "l10-baselines.json")))
                    baseline_bytes = int(doc["baselines"]["diag"]["bytes"] or 0)
                    baseline_src = os.path.join(root, "l10-baselines.json")
                except Exception:
                    pass
                break
        if baseline_bytes == 0:
            baseline_src = "0 (stage-time baseline recorded ABSENT — whole file is the new window; max-pid correlation governs)"

    # ---- status -------------------------------------------------------------
    status = parse_status(files["status"]) if files["status"] else {"present": False}

    # ---- sdiag --------------------------------------------------------------
    sd_all = None
    sdiag_info = {"present": files["sdiag"] is not None}
    if files["sdiag"]:
        eps, total, bad = parse_sdiag(files["sdiag"], baseline_bytes)
        sdiag_info.update({"records_total": total, "records_unparseable": bad,
                           "epochs": [{"pid": e["pid"], "records": e["records"],
                                       "n_new": e["n_new"], "n_old": e["n_old"],
                                       "seq": [e["seq_first"], e["seq_last"]],
                                       "t": [e["t_first"], e["t_last"]]} for e in eps]})
        sd_all = eps
    max_pid = a.max_pid
    max_pid_src = "explicit"
    if max_pid is None:
        max_pid = max((e["pid"] for e in sd_all), default=None)
        max_pid_src = "max pid in sdiag"
    if max_pid is None and status.get("session_pid"):
        max_pid, max_pid_src = status["session_pid"], "status session pid"
    if max_pid is None:
        print("%s: no epoch pid discoverable (no sdiag records, no status session)" % TOOL, file=sys.stderr)
        return 2

    sd = next((e for e in (sd_all or []) if e["pid"] == max_pid), None)
    if sd is None:
        sd = {"pid": max_pid, "records": 0, "n_new": 0, "n_old": 0,
              "seq_first": None, "seq_last": None, "t_first": None, "t_last": None,
              "tags": {}, "tag_counts": {}, "tag_t": {}, "tag_t_last": {}, "pad_seq": []}
    pool_fresh = sd["tags"].get("pool_range_verdict_fresh")
    try:
        pool_fresh = int(pool_fresh) if pool_fresh is not None else None
    except (TypeError, ValueError):
        pass

    # ---- log window + greps --------------------------------------------------
    W = {"win": []}
    log_info = {"present": files["ue4ss"] is not None}
    greps = {}
    nul = {"nul_lines": 0, "examples": []}
    banner_line = {"found": False, "clean": None, "line": None, "t_ms": None, "text": None}
    if files["ue4ss"]:
        lines, prefixed_mode, _raw = parse_log(files["ue4ss"])
        win, legacy = window(lines, prefixed_mode, max_pid)
        W["win"] = win
        log_info.update({"pid_prefixed": prefixed_mode, "legacy_no_pid_window": legacy,
                         "lines_total": len(lines), "lines_in_window": len(win),
                         "window_note": ("log has NO pid prefixes (legacy format) — whole file is "
                                         "the window; correlate via status session pid %s" % status.get("session_pid")
                                         ) if legacy else "window = pid %d lines only" % max_pid})
        greps = run_greps(win, GREPS + EXTRA_GREPS)
        nul = nul_check(win)
        banner_line = line_with_nul(win, r"Loaded v1\.0")
    W.update({"greps": greps, "win": W["win"]})

    # ---- rungs ----------------------------------------------------------------
    (rows, load_verdict, cascade_stop,
     c0s, c1s, c2s, c3s, c4s, c5s, fail_row) = eval_rungs(
        W, sd, status, max_pid, log_info.get("pid_prefixed"), pool_fresh)

    # ---- timing -----------------------------------------------------------------
    tt = sd.get("tag_t", {})
    timing = {
        "module_start_entered_t_ms": tt.get("module_start_entered", sd["t_first"]),
        "module_start_return_t_ms": tt.get("module_start_return"),
    }
    if isinstance(timing["module_start_entered_t_ms"], int) and isinstance(timing["module_start_return_t_ms"], int):
        timing["module_start_delta_ms"] = timing["module_start_return_t_ms"] - timing["module_start_entered_t_ms"]
    pt_hit = greps.get("path_test", {}).get("hits", [])
    if pt_hit:
        m = re.search(r"at (\d+) monotonic-ns", pt_hit[0][2])
        if m:
            ns = int(m.group(1))
            timing["path_test_monotonic_ns"] = ns
            timing["path_test_monotonic_ms_equiv"] = ns / 1e6
            if isinstance(timing.get("module_start_return_t_ms"), int):
                gap = ns / 1e6 - timing["module_start_return_t_ms"]
                timing["inter_plugin_gap_ms_provisional"] = round(gap, 1)
                timing["inter_plugin_gap_note"] = (
                    "PROVISIONAL cross-writer subtraction (pathtest ns stamp vs sdiag t_ms; "
                    "runbook band 70.7-71.8 s, 4/4 boots). Never use for rung claims; "
                    "a value far outside the band is a discriminator only (§5).")
                timing["inter_plugin_gap_in_runbook_band"] = 70700.0 <= gap <= 71800.0

    # ---- epoch_view correlation (best effort; never fatal) ------------------------
    ev_note = None
    ev_epochs = None
    if files["sdiag"] and a.epoch_view and os.path.isfile(a.epoch_view):
        try:
            cmd = [sys.executable, a.epoch_view, "--sdiag", files["sdiag"],
                   "--baseline-bytes", str(baseline_bytes), "--build-id", str(a.build_id), "--json"]
            if files["ue4ss"]:
                cmd += ["--ue4ss", files["ue4ss"]]
            if files["status"]:
                cmd += ["--status", files["status"]]
            cp = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            if cp.returncode == 0:
                doc = json.loads(cp.stdout)
                ev_epochs = [{"pid": e["pid"], "occurrence": e["occurrence"],
                              "records": e["records"],
                              "new_since_baseline_cut": e.get("new_since_baseline_cut"),
                              "verdict": e.get("verdict"),
                              "verdict_class": e.get("verdict_class"),
                              "terminal": e.get("terminal", {}).get("stage")}
                             for e in doc.get("epochs", [])]
            else:
                ev_note = "epoch_view rc=%s: %s" % (cp.returncode, (cp.stderr or "").strip()[:200])
        except Exception as exc:  # non-fatal by contract
            ev_note = "epoch_view invocation failed: %s" % exc
    elif not a.epoch_view or not os.path.isfile(a.epoch_view):
        ev_note = "epoch_view.py not found at %s — per-epoch correlation table skipped" % a.epoch_view

    # ---- results ------------------------------------------------------------------
    max_ep_new = bool(sd and sd["n_new"] > 0 and sd["n_old"] == 0) if baseline_bytes > 0 else None
    results = {
        "tool": TOOL, "version": VERSION,
        "generated_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "label": a.label, "run_dir": os.path.abspath(rundir),
        "build_id": a.build_id,
        "runbook": "outputs/2026-10-04-zcode-team/worker-launch-readiness/NEXT-LAUNCH-RUNBOOK.md §2/§6/§7",
        "baseline_bytes": baseline_bytes,
        "baseline_provenance": baseline_src,
        "max_pid": max_pid, "max_pid_source": max_pid_src,
        "max_pid_epoch_new_since_baseline": max_ep_new,
        "status_latest_state_only": {k: status.get(k) for k in
                                     ("present", "session_pid", "session_ms", "n_states",
                                      "last_state", "last_code", "last_detail")} if status.get("present") else {"present": False},
        "files": manifest,
        "log": log_info,
        "sdiag": sdiag_info,
        "epoch_pid": {
            "pid": max_pid, "records": sd["records"],
            "n_new_records": sd["n_new"], "n_old_records": sd["n_old"],
            "seq": [sd["seq_first"], sd["seq_last"]], "t_ms": [sd["t_first"], sd["t_last"]],
            "tags_of_interest": {k: v for k, v in sorted(sd["tags"].items())},
            "tag_t_ms_first": dict(sorted(sd.get("tag_t", {}).items())),
            "pad_and_pool_seq": sd.get("pad_seq", []),
        },
        "pool_range_verdict_fresh": pool_fresh,
        "pool_verdict_meaning": POOL_VERDICT_MEANING.get(pool_fresh) if pool_fresh is not None else None,
        "greps": {k: {"why": v["why"], "count": v["count"],
                      "lines": [{"n": ln, "t_ms": t, "text": txt} for (ln, t, txt) in v["hits"][:50]]}
                  for k, v in {**greps}.items()},
        "nul_interleave_check": nul,
        "banner_line_check": banner_line,
        "banner_exact_expected": BANNER_EXACT,
        "timing": timing,
        "rungs": {
            "C0_boot_frame": c0s, "C1_pool_verdict": c1s, "C2_post_seeds": c2s,
            "C3_load_question": c3s, "C4_input_arrival": c4s, "C5_delivery_menu": c5s,
        },
        "load_question_verdict": load_verdict,
        "cascade_stop": {"rung": cascade_stop[0], "status": cascade_stop[1]} if cascade_stop else None,
        "failure_table_row_hint": fail_row,
        "epoch_view_correlation": {"epochs": ev_epochs, "note": ev_note},
        "observables": rows,
        "guards": [
            "runtime-status is LATEST-STATE-ONLY: applied to session pid %s alone" % status.get("session_pid"),
            "t_ms never subtracted across epochs/pids; per-writer clocks never mixed for claims",
            "PENDING includes not-reached-per-cascade; claims never pass the first unmet rung (§2)",
        ],
    }

    out_json = os.path.join(outdir, "results.json")
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2, ensure_ascii=False)

    # next-baseline pin (§6.5): the NEXT cut is this run's sdiag size+sha
    if files["sdiag"]:
        with open(os.path.join(outdir, "next-baseline.json"), "w", encoding="utf-8") as f:
            json.dump({"law": "pin THIS run's sdiag byte size + sha as the NEXT baseline cut (§6.5)",
                       "sdiag": os.path.abspath(files["sdiag"]),
                       "bytes": os.path.getsize(files["sdiag"]),
                       "sha256": sha256_file(files["sdiag"])}, f, indent=2)

    if not a.json_only:
        print(render_table(results))
    print("results.json: %s" % out_json)
    return 0


def render_table(R):
    L = []
    w = L.append
    w("== launch-evidence verdict table — rung cascade C0-C5 (NEXT-LAUNCH-RUNBOOK §2/§6/§7) ==")
    w("label=%s  build=%s  run_dir=%s" % (R["label"], R["build_id"], R["run_dir"]))
    w("epoch pid=%s (%s)  sdiag new/old records=%s/%s  baseline=%s B [%s]" % (
        R["max_pid"], R["max_pid_source"],
        R["epoch_pid"]["n_new_records"], R["epoch_pid"]["n_old_records"],
        R["baseline_bytes"], R["baseline_provenance"][:80]))
    st = R["status_latest_state_only"]
    w("status (LATEST-STATE-ONLY, session pid=%s): %s states, last=%s code=%s" % (
        st.get("session_pid"), st.get("n_states"), st.get("last_state"), st.get("last_code")))
    w("")
    w("files:")
    for f in R["files"]:
        w("  %-7s %-52s %s" % (f["sink"], f["remote_path"],
                               ("present %d B" % f["bytes"]) if f["present"] else
                               ("ABSENT (normal)" if f["absence_normal"] else "ABSENT (recorded honestly)")))
    w("")
    w("rung  status   observable                                   evidence")
    cur = None
    for r in R["observables"]:
        if r["rung"] != cur:
            cur = r["rung"]
        w("  %-4s %-8s %-44s %s" % (r["rung"], r["status"], r["observable"][:44], r["evidence"][:110]))
    w("")
    w("RUNG SUMMARY: C0=%s C1=%s C2=%s C3=%s C4=%s C5=%s" % (
        R["rungs"]["C0_boot_frame"], R["rungs"]["C1_pool_verdict"], R["rungs"]["C2_post_seeds"],
        R["rungs"]["C3_load_question"], R["rungs"]["C4_input_arrival"], R["rungs"]["C5_delivery_menu"]))
    w("THE ONE QUESTION (does the mod LOAD): %s" % R["load_question_verdict"])
    if R["cascade_stop"]:
        w("CASCADE STOP: %s %s — downstream rungs are not-reached per §2" % (
            R["cascade_stop"]["rung"], R["cascade_stop"]["status"]))
    if R.get("failure_table_row_hint"):
        w("FAILURE-TABLE HINT: %s" % R["failure_table_row_hint"])
    if R["pool_range_verdict_fresh"] is not None:
        w("pool_range_verdict_fresh=%s — %s" % (R["pool_range_verdict_fresh"], R["pool_verdict_meaning"]))
    n = R["nul_interleave_check"]
    w("NUL-interleave: %d line(s) with NUL in window%s" % (
        n["nul_lines"], "" if n["nul_lines"] == 0 else " — e.g. %s" % (n["examples"][0][1][:70] if n["examples"] else "")))
    t = R["timing"]
    if "module_start_delta_ms" in t:
        w("timing: module_start %s->%s ms (Δ=%s ms)" % (
            t["module_start_entered_t_ms"], t["module_start_return_t_ms"], t["module_start_delta_ms"]))
    if "inter_plugin_gap_ms_provisional" in t:
        w("timing: PATH_TEST %s ns; inter-plugin gap ≈ %s ms (PROVISIONAL, in-band=%s)" % (
            t["path_test_monotonic_ns"], t["inter_plugin_gap_ms_provisional"],
            t["inter_plugin_gap_in_runbook_band"]))
    if R["epoch_view_correlation"]["epochs"]:
        w("")
        w("epoch_view correlation (per-pid epochs):")
        for e in R["epoch_view_correlation"]["epochs"]:
            mark = " <== THIS EPOCH" if e["pid"] == R["max_pid"] else ""
            w("  pid=%-5s recs=%-5s new=%-5s %-24s %s%s" % (
                e["pid"], e["records"],
                e["new_since_baseline_cut"] if e["new_since_baseline_cut"] is not None else "-",
                (e["verdict"] or "-")[:24], (e["terminal"] or "-"), mark))
    return "\n".join(L)


if __name__ == "__main__":
    sys.exit(main())
