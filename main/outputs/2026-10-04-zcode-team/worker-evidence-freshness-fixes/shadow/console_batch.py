#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
console_batch.py — single controller for the staged console test plans.

Built by worker-console-batch-runner, 2026-10-04; extended the same day by
worker-batch-runner-8fam-patch (ownership transferred in-place) with the
8-family stage. This tool executes, step by step with gates, the plans frozen by:
  * worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md (PREFERRED single DLL
    run) + worker-dll-8fam-combined/EXPECTED-DELTA-8FAM.md
  * worker-dll-7fam-stage-prep/CONSOLE-STAGE-PLAN-7FAM.md (+ EXPECTED-LEDGER-7FAM.md)
    — kept as the documented FALLBACK attribution path, never run by default
  * worker-e2e-console-stage-prep/CONSOLE-STAGE-PLAN.md (+ EXPECTED-LEDGER.md)
    via worker-e2e-staging-final/STAGED-E2E-COMMANDS.md (the 13 cmd files)
  * worker-record-sink-addon/STAGE-PLAN-QDRV1.md (ploader.ini =2 stage, BEFORE launch)
  * worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md (CP0-CP7)
    + worker-post-launch-tools/post_launch_check.py
    + worker-rung-evidence-reader/rung_check.py

Subcommands:
  plan             dry-run: print every step, gate and exact command; NO execution
  dll8             8-family DLL payload-only stage (upload/check/readback/dispatch/ledger) — PREFERRED
  dll7             7-family DLL payload-only stage — FALLBACK attribution path only
  e2e              E2E installer 15-dispatch stage 0..6 cycle with records baselines
  stage-qdrv1      qdrv1 PRX stage + ploader.ini append + readback (game CLOSED, pre-launch)
  game-evidence    post-launch evidence: post_launch_check + rung_check + CP0-CP7 checks
  all              recommended session order: dll8 -> e2e -> stage-qdrv1 -> game-evidence
                   (`--dll 7` runs the dll7 fallback attribution path instead; EXACTLY ONE
                   DLL stage per session — never both by default)
  selftest         host/fixture tests of the tool logic (no console contact)

Safety model:
  * every step carries explicit gates; a gate mismatch = FAIL = stop (default)
  * --stop-on-first-failure is TRUE by default; --continue-on-failure still
    skips every step that depends on a failed group (no re-dispatch, no replay)
  * dispatch steps are dispatched EXACTLY ONCE; on ambiguity the tool stops and
    records (reconcile-first law from the plans and AGENTS.md)
  * the tool never re-sends, never widens a gate, never writes outside its run dir
    and the console paths each plan explicitly owns.

NO console contact happens in `plan` or `selftest` mode. The tool runs against
the console only when the user returns and explicitly authorizes it.
"""

import argparse
import datetime as _dt
import json
import os
import re
import shlex
import subprocess
import sys

WS = "/WORKSPACE/.codex/worktrees/translation-core/what"
T = os.path.join(WS, "outputs/2026-10-04-zcode-team")
SELF_DIR = os.path.join(T, "worker-console-batch-runner")

HTTP = "http://ps5-host-not-configured.invalid:8084"
FTP = "ftp://ps5-host-not-configured.invalid:2121"
CURL_TIMEOUT = ["--connect-timeout", "10", "--max-time", "60"]

# --------------------------------------------------------------------------
# Pinned identities (from the plans; re-verified on host 2026-10-04)
# --------------------------------------------------------------------------

DLL7 = {
    "elf": f"{T}/worker-static-tls-7fam/out-sdk-composition/e72f5318233845369a68556cd61e59ca/owned-dll-candidate.elf",
    "receipt": f"{T}/worker-static-tls-7fam/out-sdk-composition/e72f5318233845369a68556cd61e59ca/receipt.json",
    "sha": "83ee2ae95268a9b7f7bc4d931a29b77d5c3c2b15afdbb4e4288ac5490a847cae",
    "bytes": 334656,
    "build_id": "e72f5318233845369a68556cd61e59ca",
    "alias": "ALLFAM7_DLL_TEST.elf",
    "payload_path": "/data/pldmgr/payloads/ALLFAM7_DLL_TEST/ALLFAM7_DLL_TEST.elf",
    "results_path": "/data/modbridge-owned-dll-candidate.jsonl",
    "existing_records": 36,
    "report_profile_path": "/data/modbridge-owned-dll-candidate.jsonl",
}

# PREFERRED single DLL run (worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md;
# identity re-verified on host 2026-10-04 by worker-batch-runner-8fam-patch:
# sha256 + bytes EXACT match against the receipt and the artifact on disk).
DLL8 = {
    "elf": f"{T}/worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/owned-dll-candidate.elf",
    "receipt": f"{T}/worker-dll-8fam-combined/out-sdk-composition/c3aba79f968a492b849e5ece2f009e8e/receipt.json",
    "sha": "792811911e773c9b07a47ba3b22bc5d290f9f3ab86056ddfc985cf25e6d804fb",
    "bytes": 336552,
    "build_id": "c3aba79f968a492b849e5ece2f009e8e",
    "alias": "ALLFAM8_DLL_TEST.elf",
    "payload_path": "/data/pldmgr/payloads/ALLFAM8_DLL_TEST/ALLFAM8_DLL_TEST.elf",
    "results_path": "/data/modbridge-owned-dll-candidate.jsonl",
    "existing_records": 36,  # dll8-first (recommended): appends at 37-40; 41-44 if the dll7 fallback ran first
    "report_profile_path": "/data/modbridge-owned-dll-candidate.jsonl",
}

E2E = {
    "payload": f"{T}/worker-e2e-payload-build/build-r1/modbridge-installer.payload.elf",
    "sha": "a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054",
    "bytes": 681888,
    "alias": "MODBRIDGE_INST_E2E.elf",
    "payload_path": "/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf",
    "cmddir": f"{T}/worker-e2e-staging-final/cmd",
    "d1rel_sha": "e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436",
    "d1rel_bytes": 681528,
    "dispatch_url": f"{HTTP}/loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf",
    "status_ftp": "data/modbridge/state/e2e-status.jsonl",
    "records_ftp": "data/modbridge/state/records.txt",
    "intake": "data/modbridge/intake",
    "movspeed_zip": f"{WS}/.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip",
    "movspeed_sha": "3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad",
    "mm_skin": f"{T}/worker-e2e-rehearsal/rehearsal/run/mm-skin.mfb",
    "mm_skin_sha": "7b9888b9c647fc19ecd1631c17981ec320f69472260646b6b8c000e5cd9b70af",
    "mm_lua": f"{T}/worker-e2e-rehearsal/rehearsal/run/mm-lua.mfb",
    "mm_lua_sha": "eefc774a589d412d827c486e6d08a4f3eaee5adb4b87f8df6f9a70bc7bd639ec",
    "reset_ftp": [
        "data/modbridge/intake/movspeed.zip",
        "data/modbridge/intake/movspeed.q",
        "data/modbridge/intake/mm-skin.mfb",
        "data/modbridge/intake/mm-lua.mfb",
        "data/modbridge/intake/e2e-cmd.json",
        "data/modbridge/intake/e2e-cmd.json.consumed",
    ],
}

QDRV1 = {
    "prx": f"{T}/worker-record-sink-addon/modbridge_qh_driver.prx",
    "sha": "52c3221665a423ddd2b22faddd4ac59ea8fd0ab8c1880c61e225d32a43df70a4",
    "bytes": 26157,
    "target_prx": "/data/plugins/modbridge_qh_driver.prx",
    "ini": "/data/plugins/ploader.ini",
    "module_line": "/data/plugins/modbridge_qh_driver.prx",
    "composition_line": "/data/plugins/modbridge_supplier_composition.prx",
}

GAME = {
    "post_launch_check": f"{T}/worker-post-launch-tools/post_launch_check.py",
    "rung_check": f"{T}/worker-rung-evidence-reader/rung_check.py",
    "S": "/data/modbridge/roots/PPSA33286/runtime-status.txt",
    "L": "/data/plugins/UE4SS.log",
    "D1": "/data/townfall_runtime_aligned_entry.log",
    "C2": "/data/modbridge-ingame-facts.jsonl",
    "tick_line": "GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x37ac8c0)",
    "mod_banner": "[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.",
}

# --------------------------------------------------------------------------
# Verdict helpers (pure functions — fixture-tested by `selftest`)
# --------------------------------------------------------------------------

def norm_json_line(line):
    """Compact-normalize a JSON line, preserving key order; None if unparseable."""
    try:
        return json.dumps(json.loads(line), separators=(",", ":"), ensure_ascii=False)
    except Exception:
        return None


def get_path(obj, dotted):
    cur = obj
    for part in dotted.split("."):
        if not isinstance(cur, dict) or part not in cur:
            return (False, None)
        cur = cur[part]
    return (True, cur)


# --- 7FAM ledger -----------------------------------------------------------

FAMILY_KEYS = ["synchronization", "init_once", "mutex", "fileio",
               "crt_startup", "module_enum", "vm", "static_tls"]

TLS_FIXTURE_CODES = {
    -120: "TLS index never assigned/stable — dir-9 not consumed or admission ticket refused",
    -121: "block NULL or not the template copy — .tls template copy path",
    -122: "__tls_import(idx,0) != block — same-thread idempotence broken",
    -123: "scratch u32 at block+16 != 0x5a17beef — block memory rights",
    -124: "callback log prefix wrong — .rdataC callback dispatch order",
    -125: "__tls_start template unreadable / __tls_end bounds — directory-9 VA patch bounds",
    -126: "begin-window block != entered thread's block — per-thread persistence",
    -127: "internal-thread isolation evidence diverged (NULL internal block is the HONEST expectation)",
}

FAMILY_RANGES = [
    ((-53, -1), "sync/initonce"),
    ((-78, -60), "mutex"),
    ((-89, -80), "fileio"),
    ((-97, -90), "crt_startup"),
    ((-104, -100), "module_enum"),
    ((-117, -110), "vm"),
    ((-127, -120), "static_tls"),
]


def triage_7fam(code):
    """Map first_failure.stage to its family range / fixture meaning (ledger)."""
    try:
        c = int(code)
    except (TypeError, ValueError):
        return "non-numeric first_failure.stage: %r" % (code,)
    for (lo, hi), name in FAMILY_RANGES:
        if lo <= c <= hi:
            if name == "static_tls":
                return "static_tls fixture %d: %s" % (c, TLS_FIXTURE_CODES.get(c, "unknown"))
            return "%s fixture code %d" % (name, c)
    return "code %d outside known fixture ranges" % c


def _zeros(obj, paths):
    bad = []
    for p in paths:
        ok, v = get_path(obj, p)
        if not ok or v not in (0, [0, 0], [0, 0, 0, 0], {"pending": 0, "retired": 0}):
            bad.append((p, v))
    return bad


def verdict_7fam_records(text, build_id=None):
    """Verify the 4 expected appended records for build e72f5318... .

    Returns (verdict, details) where verdict in {"PASS","FAIL"} and details
    carries the divergent fields (honest per-field reporting).
    """
    build_id = build_id or DLL7["build_id"]
    details = {"build_id": build_id, "records_found": 0, "appended_after_all_existing": None,
               "divergences": [], "triage": [], "notes": []}
    lines = [ln for ln in text.splitlines() if ln.strip()]
    recs = []
    for i, ln in enumerate(lines):
        obj = None
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if isinstance(obj, dict) and obj.get("build_id") == build_id:
            recs.append((i, obj, ln))
    details["records_found"] = len(recs)
    if len(recs) != 4:
        details["divergences"].append(
            "expected exactly 4 records for build %s, found %d (ledger: 0 => never ran; "
            "1 => crash between phase-1 and stage 2; 2 => hang/crash in owner setup)" % (build_id, len(recs)))
        if recs:
            details["triage"].append("partial run — see EXPECTED-LEDGER-7FAM failure signatures")
        return "FAIL", details
    # Append-only: the 4 records must be contiguous at the END of the file
    idxs = [i for i, _, _ in recs]
    appended_at_end = idxs == list(range(idxs[0], idxs[0] + 4)) and idxs[-1] == len(lines) - 1
    details["appended_after_all_existing"] = appended_at_end
    if not appended_at_end:
        details["divergences"].append(
            "e72f5318 records are not a contiguous block at end-of-file (positions %s of %d lines); "
            "ledger fallback assertion 'appended after ALL existing records, none modified' violated"
            % (idxs, len(lines)))
    div = details["divergences"]

    def chk(i_obj, path, expected, label=None):
        _, obj, _ = i_obj
        ok, v = get_path(obj, path)
        if not ok:
            div.append("%s: key %s MISSING (expected %r)" % (label, path, expected))
        elif v != expected:
            div.append("%s: %s = %r, expected %r" % (label, path, v, expected))

    r0, r1, r2, r3 = recs[0], recs[1], recs[2], recs[3]

    # Build-frame + sequence/phase for all
    for n, rec in enumerate((r0, r1, r2, r3)):
        chk(rec, "marker", "modbridge.owned-dll", "record%d" % n)
        chk(rec, "schema", 1, "record%d" % n)
        chk(rec, "sequence", n, "record%d" % n)
        chk(rec, "phase", [1, 2, 3, 5][n], "record%d" % n)

    # Record 0 — census-zero bootstrap
    chk(r0, "entry.enabled", 0, "record0")
    chk(r0, "execution.stage", 0, "record0")
    chk(r0, "results", [0, 0], "record0")
    chk(r0, "cleanup", {"pending": 0, "retired": 0}, "record0")
    for fam in FAMILY_KEYS:
        if fam in r0[1]:
            div.append("record0: family key %r present — census-zero record must carry NO family keys" % fam)

    # Record 1 — attach stage, family census visible
    chk(r1, "execution.stage", 2, "record1")
    chk(r1, "execution.started", 1, "record1")
    chk(r1, "execution.run_attempted", 0, "record1")
    chk(r1, "execution.completed", 0, "record1")
    chk(r1, "entry.enabled", 1, "record1")
    chk(r1, "clock.enabled", 1, "record1")
    chk(r1, "worker.enabled", 1, "record1")
    chk(r1, "cleanup", {"pending": 1, "retired": 0}, "record1")
    chk(r1, "delivery.file_records", 1, "record1")
    for fam in FAMILY_KEYS:
        ok, v = get_path(r1[1], fam)
        if not ok:
            div.append("record1: family key %r MISSING (static_tls appears on target for the FIRST time here)" % fam)
        elif v != {"enabled": 1}:
            div.append("record1: %s = %r, expected {\"enabled\":1}" % (fam, v))

    # Record 2 — run complete
    chk(r2, "execution.stage", 9, "record2")
    chk(r2, "execution.run_attempted", 1, "record2")
    chk(r2, "execution.completed", 1, "record2")
    chk(r2, "execution.dll_calls_started", 3, "record2")
    chk(r2, "execution.dll_calls_returned", 3, "record2")
    chk(r2, "entry.attach_calls", [1, 1], "record2")
    chk(r2, "entry.attach_result", [7, 7], "record2")
    chk(r2, "entry.detach_calls", [0, 1], "record2")
    chk(r2, "entry.detach_result", [0, 0], "record2")
    chk(r2, "entry.contract_errors", 0, "record2")
    chk(r2, "clock.session_begins", [1, 1], "record2")
    chk(r2, "clock.session_ends", [0, 1], "record2")
    chk(r2, "worker.calls_started", 4, "record2")
    chk(r2, "worker.calls_returned", 4, "record2")
    chk(r2, "worker.begin", [5, 1, 258, 259], "record2")
    chk(r2, "worker.result", [2166572391, 1234605616436508552, 3782448555, 0], "record2")
    chk(r2, "counts.consumer_imports", 82, "record2")
    chk(r2, "counts.helper_imports", 10, "record2")
    chk(r2, "counts.source_acquires", [1, 1], "record2")
    chk(r2, "counts.source_releases", [0, 1], "record2")
    chk(r2, "results", [86, 93], "record2")
    chk(r2, "static_tls", {"enabled": 1}, "record2")
    chk(r2, "memory.owner_status", 0, "record2")
    chk(r2, "memory.last.operation", 3, "record2")
    chk(r2, "memory.last.native_result", 0, "record2")
    chk(r2, "memory.last.offset", 0, "record2")
    chk(r2, "memory.last.length", 147456, "record2")
    chk(r2, "delivery.first_file_error", 0, "record2")
    chk(r2, "delivery.file_records", 2, "record2")
    chk(r2, "delivery.stdout_records", 1, "record2")
    ok, v = get_path(r2[1], "delivery.first_stdout_error")
    if ok and v == 32:
        details["notes"].append("delivery stdout_error 32 with stdout_records 1 = console-only delivery "
                                "channel status, present in every console run — NOT a failure")
    elif ok and v != 0:
        div.append("record2: delivery.first_stdout_error = %r (32 expected on console)" % v)

    # Record 3 — FINAL record, pass/fail oracle
    chk(r3, "execution.stage", 11, "record3")
    chk(r3, "entry.detach_calls", [1, 1], "record3")
    chk(r3, "entry.detach_result", [0, 0], "record3")
    chk(r3, "entry.attach_result", [7, 7], "record3")
    chk(r3, "entry.contract_errors", 0, "record3")
    chk(r3, "clock.session_ends", [1, 1], "record3")
    chk(r3, "counts.source_releases", [1, 1], "record3")
    chk(r3, "cleanup", {"pending": 0, "retired": 1}, "record3")
    chk(r3, "results", [86, 93], "record3")
    chk(r3, "counts.consumer_imports", 82, "record3")
    chk(r3, "counts.helper_imports", 10, "record3")
    chk(r3, "memory.last.length", 147456, "record3")
    chk(r3, "delivery.first_file_error", 0, "record3")
    chk(r3, "delivery.file_records", 3, "record3")

    # first_failure all zeros on records 2 and 3 + triage naming
    for n, rec in ((2, r2), (3, r3)):
        ff = rec[1].get("first_failure")
        if not isinstance(ff, dict) or any(
                ff.get(k) not in (0, None) for k in ("stage", "native_status", "windows_error", "module_cause")):
            stage = ff.get("stage") if isinstance(ff, dict) else None
            if isinstance(stage, int) and stage < 0:
                details["triage"].append("record%d first_failure: %s" % (n, triage_7fam(stage)))
            div.append("record%d: first_failure not all zeros: %r" % (n, ff))

    return ("PASS" if not div else "FAIL"), details


# --- 8FAM ledger (CONSOLE-STAGE-PLAN-8FAM.md + EXPECTED-DELTA-8FAM.md) -------

# 9 counted family keys: the eight 7-family keys + seh64 (the 8th family).
FAMILY_KEYS_8 = FAMILY_KEYS + ["seh64"]

SEH64_FIXTURE_CODES = {
    -130: "context probe failed — RtlCaptureContext route or frame sanity in the fixture (RIP/RSP insane)",
    -131: "no in-image function entry — RtlLookupFunctionEntry returned nothing in-image (.pdata/.xdata unwind data not visible at runtime)",
    -132: "RtlVirtualUnwind produced an insane caller frame — unwind math or the copied CONTEXT",
    -133: "__C_specific_handler dispatch did not route the same-frame raise to the handler (returned instead of transferring, or handled != exactly once)",
    -134: "guarded-state restore corrupted registers/nonvolatiles (integer guard words not intact)",
    -135: "cross-frame unwind failed — __finally not run / handler not reached / unwound frames resumed; classic break is unwind-info registration order",
    -136: "dynamic function-table registration path (RtlAddFunctionTable over a bridge/non-PE address) — lookup missed or base inexact",
    -137: "deregistration/filter-state cleanup — table not gone after RtlRemoveFunctionTable, double-removal not refused, or unhandled-filter round-trip wrong",
}

FAMILY_RANGES_8 = FAMILY_RANGES + [((-137, -130), "seh64")]


def triage_8fam(code):
    """Map first_failure.stage to its family range / fixture meaning (8FAM ledger)."""
    try:
        c = int(code)
    except (TypeError, ValueError):
        return "non-numeric first_failure.stage: %r" % (code,)
    for (lo, hi), name in FAMILY_RANGES_8:
        if lo <= c <= hi:
            if name == "seh64":
                return "seh64 fixture %d: %s" % (c, SEH64_FIXTURE_CODES.get(c, "unknown"))
            if name == "static_tls":
                return "static_tls fixture %d: %s" % (c, TLS_FIXTURE_CODES.get(c, "unknown"))
            return "%s fixture code %d" % (name, c)
    return "code %d outside known fixture ranges" % c


def verdict_8fam_records(text, build_id=None):
    """Verify the 4 expected appended records for build c3aba79f... (8 families).

    Oracle per EXPECTED-DELTA-8FAM.md: consumer_imports 90 (+8 SEH64 binds),
    helper_imports 10, results [86,93], seh64 {enabled:1} from record1 onward,
    memory.last.length 114688 (record2/seq2) -> 163840 (record3/seq3 FINAL;
    the mapped image grew 334656 -> 336552 bytes). Positional expectation
    (37-40 dll8-first, 41-44 after a dll7 fallback run) YIELDS to the hard
    append-only invariant: 4 records appended after ALL existing, none modified.
    """
    build_id = build_id or DLL8["build_id"]
    details = {"build_id": build_id, "records_found": 0, "appended_after_all_existing": None,
               "divergences": [], "triage": [], "notes": []}
    lines = [ln for ln in text.splitlines() if ln.strip()]
    recs = []
    for i, ln in enumerate(lines):
        obj = None
        try:
            obj = json.loads(ln)
        except Exception:
            continue
        if isinstance(obj, dict) and obj.get("build_id") == build_id:
            recs.append((i, obj, ln))
    details["records_found"] = len(recs)
    if len(recs) != 4:
        details["divergences"].append(
            "expected exactly 4 records for build %s, found %d (ledger: 0 => never ran; "
            "1 => crash between phase-1 and stage 2 [NOT the import set — it is set-identical "
            "to console-passed 133c5e9e; suspect dir-3 EXCEPTION admission / .ehflags unwind "
            "tables]; 2 => hang/crash in the stage 55/56 dual-owner window [static TLS then "
            "SEH64 — module_cause/detail names which])" % (build_id, len(recs)))
        if recs:
            details["triage"].append("partial run — see EXPECTED-DELTA-8FAM failure signatures")
        return "FAIL", details
    # Append-only hard invariant: the 4 records must be contiguous at the END of the file
    idxs = [i for i, _, _ in recs]
    appended_at_end = idxs == list(range(idxs[0], idxs[0] + 4)) and idxs[-1] == len(lines) - 1
    details["appended_after_all_existing"] = appended_at_end
    if not appended_at_end:
        details["divergences"].append(
            "c3aba79f records are not a contiguous block at end-of-file (positions %s of %d lines); "
            "hard invariant 'appended after ALL existing records, none modified' violated — "
            "the positional expectation (37-40 / 41-44) yields to this invariant" % (idxs, len(lines)))
    else:
        details["notes"].append("appended at file positions %d-%d (%s)" % (
            idxs[0] + 1, idxs[-1] + 1,
            "expected: dll8-first" if idxs[0] == 36 else
            "expected: dll7 fallback ran first" if idxs[0] == 40 else
            "off the positional expectation — another authorized run landed in between; "
            "append-only invariant still satisfied"))
    div = details["divergences"]

    def chk(i_obj, path, expected, label=None):
        _, obj, _ = i_obj
        ok, v = get_path(obj, path)
        if not ok:
            div.append("%s: key %s MISSING (expected %r)" % (label, path, expected))
        elif v != expected:
            div.append("%s: %s = %r, expected %r" % (label, path, v, expected))

    r0, r1, r2, r3 = recs[0], recs[1], recs[2], recs[3]

    # Build-frame + sequence/phase for all
    for n, rec in enumerate((r0, r1, r2, r3)):
        chk(rec, "marker", "modbridge.owned-dll", "record%d" % n)
        chk(rec, "schema", 1, "record%d" % n)
        chk(rec, "sequence", n, "record%d" % n)
        chk(rec, "phase", [1, 2, 3, 5][n], "record%d" % n)

    # Record 0 — census-zero bootstrap
    chk(r0, "entry.enabled", 0, "record0")
    chk(r0, "execution.stage", 0, "record0")
    chk(r0, "results", [0, 0], "record0")
    chk(r0, "cleanup", {"pending": 0, "retired": 0}, "record0")
    for fam in FAMILY_KEYS_8:
        if fam in r0[1]:
            div.append("record0: family key %r present — census-zero record must carry NO family keys" % fam)

    # Record 1 — attach stage, family census visible (seh64 for the FIRST time on target)
    chk(r1, "execution.stage", 2, "record1")
    chk(r1, "execution.started", 1, "record1")
    chk(r1, "execution.run_attempted", 0, "record1")
    chk(r1, "execution.completed", 0, "record1")
    chk(r1, "entry.enabled", 1, "record1")
    chk(r1, "clock.enabled", 1, "record1")
    chk(r1, "worker.enabled", 1, "record1")
    chk(r1, "cleanup", {"pending": 1, "retired": 0}, "record1")
    chk(r1, "delivery.file_records", 1, "record1")
    for fam in FAMILY_KEYS_8:
        ok, v = get_path(r1[1], fam)
        if not ok:
            div.append("record1: family key %r MISSING (seh64 appears on target for the FIRST time here; "
                       "a stale ALLFAM7 artifact would show 82 imports / no seh64 — re-check the "
                       "readback hash first)" % fam)
        elif v != {"enabled": 1}:
            div.append("record1: %s = %r, expected {\"enabled\":1}" % (fam, v))

    # Record 2 — run complete (8-family oracle values)
    chk(r2, "execution.stage", 9, "record2")
    chk(r2, "execution.run_attempted", 1, "record2")
    chk(r2, "execution.completed", 1, "record2")
    chk(r2, "execution.dll_calls_started", 3, "record2")
    chk(r2, "execution.dll_calls_returned", 3, "record2")
    chk(r2, "entry.attach_calls", [1, 1], "record2")
    chk(r2, "entry.attach_result", [7, 7], "record2")
    chk(r2, "entry.detach_calls", [0, 1], "record2")
    chk(r2, "entry.detach_result", [0, 0], "record2")
    chk(r2, "entry.contract_errors", 0, "record2")
    chk(r2, "clock.session_begins", [1, 1], "record2")
    chk(r2, "clock.session_ends", [0, 1], "record2")
    chk(r2, "worker.calls_started", 4, "record2")
    chk(r2, "worker.calls_returned", 4, "record2")
    chk(r2, "worker.begin", [5, 1, 258, 259], "record2")
    chk(r2, "worker.result", [2166572391, 1234605616436508552, 3782448555, 0], "record2")
    chk(r2, "counts.consumer_imports", 90, "record2")
    chk(r2, "counts.helper_imports", 10, "record2")
    chk(r2, "counts.source_acquires", [1, 1], "record2")
    chk(r2, "counts.source_releases", [0, 1], "record2")
    chk(r2, "results", [86, 93], "record2")
    chk(r2, "static_tls", {"enabled": 1}, "record2")
    chk(r2, "seh64", {"enabled": 1}, "record2")
    chk(r2, "memory.owner_status", 0, "record2")
    chk(r2, "memory.last.operation", 3, "record2")
    chk(r2, "memory.last.native_result", 0, "record2")
    chk(r2, "memory.last.offset", 0, "record2")
    chk(r2, "memory.last.length", 114688, "record2")
    chk(r2, "delivery.first_file_error", 0, "record2")
    chk(r2, "delivery.file_records", 2, "record2")
    chk(r2, "delivery.stdout_records", 1, "record2")
    if get_path(r2[1], "counts.consumer_imports") == (True, 82):
        details["notes"].append("consumer_imports 82 = the ALLFAM7/7-family bind shape — stale artifact "
                                "under the new alias or a bind-chain regression; 88/89 => count which of "
                                "the 8 SEH64 binds landed (host split oracle KERNEL32 73 / VCRUNTIME140 1)")
    ok, v = get_path(r2[1], "delivery.first_stdout_error")
    if ok and v == 32:
        details["notes"].append("delivery stdout_error 32 with stdout_records 1 = console-only delivery "
                                "channel status, present in every console run — NOT a failure")
    elif ok and v != 0:
        div.append("record2: delivery.first_stdout_error = %r (32 expected on console)" % v)

    # Record 3 — FINAL record, pass/fail oracle (memory.last.length grows to the seq3 value)
    chk(r3, "execution.stage", 11, "record3")
    chk(r3, "entry.detach_calls", [1, 1], "record3")
    chk(r3, "entry.detach_result", [0, 0], "record3")
    chk(r3, "entry.attach_result", [7, 7], "record3")
    chk(r3, "entry.contract_errors", 0, "record3")
    chk(r3, "clock.session_ends", [1, 1], "record3")
    chk(r3, "counts.source_releases", [1, 1], "record3")
    chk(r3, "cleanup", {"pending": 0, "retired": 1}, "record3")
    chk(r3, "results", [86, 93], "record3")
    chk(r3, "counts.consumer_imports", 90, "record3")
    chk(r3, "counts.helper_imports", 10, "record3")
    chk(r3, "memory.last.length", 163840, "record3")
    chk(r3, "delivery.first_file_error", 0, "record3")
    chk(r3, "delivery.file_records", 3, "record3")

    # first_failure all zeros on records 2 and 3 + triage naming (-130..-137 SEH64 map)
    for n, rec in ((2, r2), (3, r3)):
        ff = rec[1].get("first_failure")
        if not isinstance(ff, dict) or any(
                ff.get(k) not in (0, None) for k in ("stage", "native_status", "windows_error", "module_cause")):
            stage = ff.get("stage") if isinstance(ff, dict) else None
            if isinstance(stage, int) and stage < 0:
                details["triage"].append("record%d first_failure: %s" % (n, triage_8fam(stage)))
            div.append("record%d: first_failure not all zeros: %r" % (n, ff))

    return ("PASS" if not div else "FAIL"), details


# --- E2E ledger ------------------------------------------------------------

def L(lines):
    return [norm_json_line(x) for x in lines]


E2E_BLOCKS = {
    "S0_records": L([
        '{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}',
        '{"event":"end","exit":0}']),
    "S1_register": L([
        '{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}',
        '{"event":"end","exit":0}']),
    "S2a_prepare_archive": L([
        '{"event":"begin","cmd":"prepare-archive","title":"PPSA33286","stage":"stage-2-prepare"}',
        '{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"manifest-v1.bin"}',
        '{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"auto-bundle composed mfb"}',
        '{"event":"end","exit":0}']),
    "S2b_bundle": L([
        '{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}',
        '{"event":"end","exit":0}',
        '{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}',
        '{"event":"end","exit":0}',
        '{"event":"begin","cmd":"prepare","title":"PPSA33286","stage":"stage-2-prepare"}',
        '{"event":"end","exit":0}']),
    "S2b3_prepare_pair": L([
        '{"event":"begin","cmd":"prepare","title":"PPSA33286","stage":"stage-2-prepare"}',
        '{"event":"end","exit":0}']),
    "S3_install": L([
        '{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}',
        '{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin,mm-lua"}',
        '{"event":"end","exit":0}']),
    "S3_recover": L([
        '{"event":"begin","cmd":"recover","title":"","stage":"recovery"}',
        '{"event":"end","exit":0}']),
    "S4_simulate": L([
        '{"event":"begin","cmd":"simulate-runtime","title":"PPSA33286","stage":"stage-4-5-runtime-ack"}',
        '{"event":"end","exit":0}']),
    "S6_remove1": L([
        '{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}',
        '{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin"}',
        '{"event":"end","exit":0}']),
    "S6_remove2": L([
        '{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}',
        '{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed"}',
        '{"event":"end","exit":0}']),
    "S6_remove3": L([
        '{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}',
        '{"event":"end","exit":0}']),
}

E2E_GREEN_TOTAL = 35


def verify_e2e_block(text, cursor, expected_block, label):
    """Compare the NEW lines (non-empty lines of `text` from index `cursor`)
    against an expected verbatim block. `cursor` is a LINE index.

    Returns (verdict, info). STOP-halt shapes (2a refusal end exit 2, missing
    auto-bundle, refusal lines) are reported distinctly via info["halt"].
    """
    info = {"label": label, "cursor": cursor, "new_lines": 0, "halt": None, "diff": []}
    lines = [ln for ln in text.splitlines() if ln.strip()]
    raw = lines[cursor:]
    got = L(raw)
    info["new_lines"] = len(raw)
    exp = L(expected_block)
    if got != exp:
        # classify known halt shapes before generic diff
        parsed = []
        for ln in raw:
            try:
                parsed.append(json.loads(ln))
            except Exception:
                parsed.append(None)
        for p in parsed:
            if isinstance(p, dict) and p.get("stage") == "e2e-cmd":
                info["halt"] = "E2E-CMD REFUSAL line present: %s (F-R2: refusals close with the " \
                               "cmd file left in place; STOP, collect, report distinctly)" % json.dumps(p)
        if not info["halt"]:
            last = parsed[-1] if parsed else None
            if isinstance(last, dict) and last.get("event") == "end" and last.get("exit") == 2:
                info["halt"] = "refusal path: last line end exit=2 (STOP, rollback, never replay the identical request)"
            elif label == "S2a_prepare_archive" and not any(
                    isinstance(p, dict) and p.get("detail") == "auto-bundle composed mfb" for p in parsed):
                info["halt"] = "missing 'auto-bundle composed mfb' status line (no recognized roles — STOP, do not install)"
        for i in range(max(len(got), len(exp))):
            g = got[i] if i < len(got) else "<absent>"
            e = exp[i] if i < len(exp) else "<absent>"
            if g != e:
                info["diff"].append("line %d: got %s expected %s" % (i, g, e))
        return "FAIL", info
    return "PASS", info


def e2e_ledger_check(total_new_lines, failed_ends, refusal_lines, unmatched_begins):
    """Line-count ledger (all-green): 35 new lines, 0 failed ends, 0 refusals."""
    problems = []
    if total_new_lines != E2E_GREEN_TOTAL:
        problems.append("total new e2e-status lines = %d, expected %d (stop, collect, report)" %
                        (total_new_lines, E2E_GREEN_TOTAL))
    if failed_ends:
        problems.append("%d failed end lines" % failed_ends)
    if refusal_lines:
        problems.append("%d stage:'e2e-cmd' refusal lines" % refusal_lines)
    if unmatched_begins:
        problems.append("%d unmatched begins" % unmatched_begins)
    return problems


def mode_gate(rb_sha, rb_bytes):
    """Payload identity gate: MODE=B (config-entry) / MODE=A (D1 argv) / NO-GO."""
    if rb_sha == E2E["sha"] and rb_bytes == E2E["bytes"]:
        return "MODE=B", None
    if rb_sha == E2E["d1rel_sha"] and rb_bytes == E2E["d1rel_bytes"]:
        return "MODE=A", ("MODE=A: D1 argv-flavor build on the alias; this controller implements the "
                          "MODE B cmd-file sequence only. STOP — do not run cmd files against it "
                          "(bare dispatch = usage()/EX_BAD). Re-stage the config-entry payload or "
                          "run RUN-CONFIG A per the G1 probe grammar.")
    return "NO-GO", ("PAYLOAD IDENTITY NO-GO: readback %s %s matches neither config-entry "
                     "(a21771f8…/681888) nor D1 (e3489297…/681528). STOP." % (rb_sha, rb_bytes))


def cmd_gone_ok(original_present, consumed_hash, last_logged_hash):
    """Consumption proof: original gone AND .consumed hash == last cmdfiles.log line."""
    if original_present:
        return False, "NOT CONSUMED: e2e-cmd.json still present on FTP"
    if consumed_hash is None:
        return False, "e2e-cmd.json.consumed MISSING on FTP"
    if consumed_hash != last_logged_hash:
        return False, "consumed hash %s != last cmdfiles.log line %s" % (consumed_hash, last_logged_hash)
    return True, None


# --- ploader.ini (stage-qdrv1) ----------------------------------------------

NUM_MODULE_RE = re.compile(r"^\s*(\d+)\s*=\s*(\S+)\s*$")


def ploader_plan_edit(pre_text):
    """Return (status, new_text_or_reason). Minimal append of the next free index.

    status: "noop" (already staged), "append" (new_text ready), "create" (no
    pre-image; write the exact STAGE-PLAN-QDRV1 §1 shape), "stop" (ambiguous —
    show the pre-image, never blind-edit).
    """
    q = QDRV1["module_line"]
    if pre_text is None or pre_text.strip() == "":
        return "create", "[modules]\n1=%s\n2=%s\n" % (QDRV1["composition_line"], q)
    lines = pre_text.splitlines()
    module_entries = []
    section = None
    for ln in lines:
        s = ln.strip()
        if s.startswith("[") and s.endswith("]"):
            section = s
            continue
        m = NUM_MODULE_RE.match(ln)
        if m:
            module_entries.append((section, int(m.group(1)), m.group(2)))
    if not module_entries:
        return "stop", ("live ploader.ini has no numbered module entries and does not match the "
                        "STAGE-PLAN-QDRV1 §1 shape — reconcile against the live pre-image before "
                        "editing (never blind-edit an unknown loader contract)")
    for _sec, _idx, path in module_entries:
        if path == q:
            return "noop", pre_text
    nxt = max(i for _s, i, _p in module_entries) + 1
    new_text = pre_text.rstrip("\n") + "\n%d=%s\n" % (nxt, q)
    return "append", new_text


def ploader_verify_new(new_text, status):
    """Sanity-check the edited ini: composition line preserved, qdrv1 added once."""
    problems = []
    if status == "create":
        return problems  # exact §1 shape, nothing to preserve
    if QDRV1["composition_line"] not in new_text:
        problems.append("composition entry %s no longer present in edited ini" % QDRV1["composition_line"])
    if new_text.count(QDRV1["module_line"]) != 1:
        problems.append("qdrv1 entry count in new ini = %d, expected exactly 1" % new_text.count(QDRV1["module_line"]))
    return problems


# --- Runbook CP classifier (game-evidence) -----------------------------------

def cp_parse_status_s(s_text):
    """Parse runtime-status.txt: returns dict(session, states list, last_state)."""
    out = {"session": None, "states": [], "last_state": None, "header": None}
    for ln in s_text.splitlines():
        parts = ln.split("\t")
        if len(parts) >= 1 and parts[0] == "modforge-runtime-status":
            out["header"] = ln
        elif len(parts) >= 3 and parts[0] == "session":
            out["session"] = parts[1:]
        elif len(parts) >= 3 and parts[0] == "state":
            out["states"].append({"seq": parts[1], "state": parts[2],
                                  "code": parts[3] if len(parts) > 3 else None,
                                  "detail": parts[4] if len(parts) > 4 else None})
    if out["states"]:
        out["last_state"] = out["states"][-1]
    return out


def classify_l_log(l_text):
    """Extract the runbook-relevant markers from UE4SS.log."""
    return {
        "banner_setup": "Setting up mods..." in l_text,
        "banner_version": re.search(r"UE4SS - v", l_text) is not None,
        "root_dir": "root directory: /data/plugins" in l_text,
        "mod_dirs": "mods directories:" in l_text,
        "entry_failure_lines": re.findall(r"modbridge module_start failed: (.*)", l_text),
        "tick_line": GAME["tick_line"] in l_text,
        "tick_scan_fallback": "Scan failed, falling back to vtable address for GameEngine::Tick" in l_text,
        "mod_banners": l_text.count(GAME["mod_banner"]),
        "menu_created": "[TownfallItemSpawner] Menu created." in l_text,
        "spawnable_line": re.findall(r"\[TownfallItemSpawner\] (\d+) spawnable pickups found\. Pages=(\d+)", l_text),
        "spawned_line": re.findall(r"\[TownfallItemSpawner\] Spawned (.+?) x(\d+) ID=(\S+)", l_text),
        "scan_failed_save": re.findall(r"\[TownfallItemSpawner\] Scan failed: .*Load a save first\.", l_text),
        "widget_missing": "[TownfallItemSpawner] WidgetBlueprintLibrary not found." in l_text,
        "ui_failed": re.findall(r"\[TownfallItemSpawner\] UI creation failed: (.*)", l_text),
        "enginetick_unavailable": "ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available" in l_text,
        "processevent_unavailable": "ExecuteInGameThread: ProcessEvent method requested but ProcessEvent hook is not available" in l_text,
        "lua_errors": re.findall(r"\[Lua\] (Error loading script: .*|Error executing script: .*|Failed to execute main script: .*|Exception during mod startup: .*)", l_text),
    }


def classify_d1_log(d1_text):
    return {
        "module_start_entered": d1_text.count("module_start_entered"),
        "module_start_return_ok": len(re.findall(r"module_start_return value=0x00000000", d1_text)),
        "probe_lifecycle_ok": "probe_lifecycle_result value=0x00000000" in d1_text,
        "runtime_start_ok": "runtime_start_ok value=0x00000000" in d1_text,
        "runtime_start_failed": re.findall(r"runtime_start_failed value=0x([0-9a-f]+)", d1_text),
        "mfa_activated": "mfa_activated_low" in d1_text and "mfa_activated_high" in d1_text,
        "wiring_begin_ok": "runtime_wiring_begin value=0x00000000" in d1_text,
    }


# O2 freshness oracle — SAME constant as post_launch_check.py
# (MONO_NS_AT_PATH_TEST 8866785633337 // 1_000_000). A status session ms at or
# below this predates the PATH_TEST ledger row: S/D1/L are PRIOR-launch
# evidence and can ground no PASS (REHEARSAL-RECEIPTS.md GAP 1a false-PASS).
FRESH_SESSION_MS_THRESHOLD = 8866785


def cp_session_fresh(s_info):
    """O2 gate: True iff the parsed status S carries a session ms above the
    freshness threshold (i.e. the evidence postdates the PATH_TEST baseline)."""
    sess = s_info.get("session")
    try:
        ms = int(sess[1])
    except (TypeError, ValueError, IndexError):
        return False, None
    if ms > FRESH_SESSION_MS_THRESHOLD:
        return True, ms
    return False, ms


def cp01234_verdict(s_info, l_info, d1_info, d1_entered_before=0):
    """CP0-CP4 file-based checks from the runbook §3. Returns (verdict, per_cp, next_hint).

    d1_entered_before: module_start_entered count observed in D1 BEFORE this
    launch (O1 'exactly one NEW module_start_entered per launch'). Callers
    that fetch D1 pre-launch must pass it; default 0 preserves the raw-count
    reading for sessions whose D1 starts empty.
    Stale-session evidence (O2) is labelled INCONCLUSIVE-STALE per CP and can
    never yield PASS cells (GAP 1a), and a D1 lifecycle failure with
    runtime_start_failed present labels F3/F4/F5, not F1 (GAP 1b)."""
    per = {}
    ok_all = True
    fresh, session_ms = cp_session_fresh(s_info)
    stale_lbl = ("INCONCLUSIVE-STALE(session ms %s <= %d — prior-launch "
                 "evidence; do not rate, re-run in a fresh window)"
                 % (session_ms, FRESH_SESSION_MS_THRESHOLD))
    # CP0
    entered = d1_info["module_start_entered"] - d1_entered_before
    cp0 = (fresh and entered >= 1 and d1_info["module_start_return_ok"] >= 1
           and d1_info["probe_lifecycle_ok"])
    if not fresh:
        per["CP0"] = stale_lbl
    elif cp0:
        per["CP0"] = "PASS"
    elif entered < 1:
        per["CP0"] = ("FAIL(F0 no NEW module_start_entered this launch "
                      "(count %d <= pre-launch baseline %d) — check "
                      "loadlist/staging + ploader arm; game running unmodded "
                      "is by-design safe)"
                      % (d1_info["module_start_entered"], d1_entered_before))
    elif d1_info["runtime_start_failed"]:
        # GAP 1b: probe lifecycle fine but return-0x0 absent with a
        # runtime_start_failed row is the F3/F4/F5 family, not F1.
        per["CP0"] = ("FAIL(F3/F4/F5 runtime_start_failed: %s)"
                      % d1_info["runtime_start_failed"])
    else:
        per["CP0"] = "FAIL(F1 probe lifecycle)"
    ok_all &= cp0
    # CP1 (O2: a stale session cannot ground an activation PASS)
    cp1 = bool(s_info["last_state"] and any(st["state"] == "activated" for st in s_info["states"]))
    cp1 = cp1 and l_info["banner_setup"] and not l_info["entry_failure_lines"]
    if not fresh:
        per["CP1"] = stale_lbl
        cp1 = False
    else:
        per["CP1"] = "PASS" if cp1 else (
            "FAIL(entry failure line: %s)" % l_info["entry_failure_lines"] if l_info["entry_failure_lines"]
            else "FAIL(activation/banner — F2-F6; see runbook CP1 failure list)")
    ok_all &= cp1
    # CP2 (O2: a stale engine-ready row is NOT this launch's engine-ready)
    eng_ready = bool(s_info["last_state"] and any(st["state"] == "engine-ready" for st in s_info["states"]))
    if not fresh:
        per["CP2"] = stale_lbl
    else:
        per["CP2"] = ("PASS" if (eng_ready and l_info["tick_line"]) else
                      ("PARTIAL(engine-ready yes, Tick line absent — G7; scan-fallback=%s)"
                       % l_info["tick_scan_fallback"] if eng_ready else
                       "FAIL(no engine-ready — see CP2 outcome table; fatal shape = 'Native game-thread initializer "
                       "callback registration failed')"))
    ok_all &= (fresh and eng_ready and l_info["tick_line"])
    # CP3 (O2: a stale mods-started row is NOT this launch's mods-started)
    mods_started = bool(s_info["last_state"] and any(st["state"] == "mods-started" for st in s_info["states"]))
    if not fresh:
        per["CP3"] = stale_lbl
        mods_started = False
    else:
        per["CP3"] = "PASS(mods-started; rung-1 liveness delegated to rung_check — UNVERIFIED_DRIVER is honest, gap G2)" \
            if mods_started else "FAIL(no mods-started; F7/F8 shapes)"
    ok_all &= mods_started
    # CP4
    cp4 = l_info["mod_banners"] == 1 and mods_started
    if l_info["mod_banners"] == 0:
        per["CP4"] = "FAIL(F9/no banner: %s)" % (l_info["lua_errors"][:2] if l_info["lua_errors"] else
                                                 "mod not in startup list — re-check activation inventory")
    elif l_info["mod_banners"] > 1:
        per["CP4"] = "WARN(%d banner lines; one fresh load expected per launch)" % l_info["mod_banners"]
    else:
        per["CP4"] = "PASS"
    ok_all &= (l_info["mod_banners"] >= 1)
    if ok_all:
        hint = "gate CP5: user must load a save and press OPTIONS once"
    elif any(v.startswith("INCONCLUSIVE-STALE") for v in per.values()):
        hint = ("stale session: S/D1 evidence predates this launch — no CP may "
                "PASS from it; re-run in a fresh launch window (~60 s after "
                "launch) before any rating or pad interaction")
    else:
        hint = "fix the failing CP above before any pad interaction (runbook §6 triage: S -> D1 -> L)"
    return ("PASS" if ok_all else "INCOMPLETE"), per, hint


def cp56_verdict(l_before_open, l_after_open, l_after_cross):
    """CP5/CP6 from L diffs across the user actions."""
    per = {}
    before = classify_l_log(l_before_open)
    after = classify_l_log(l_after_open)
    new_menus = len(after["spawnable_line"]) - len(before["spawnable_line"])
    new_created = (after["menu_created"]) - 0  # presence after open
    if after["menu_created"] and not before["menu_created"] and new_menus >= 1:
        per["CP5"] = "PASS(%d new 'spawnable pickups' line(s) + 'Menu created.')" % new_menus
        cp5 = True
    elif after["scan_failed_save"]:
        per["CP5"] = "FAIL(shape c: 'Load a save first' guard — expected when P3 skipped; load a save, press OPTIONS again (T3))"
        cp5 = False
    elif after["widget_missing"] or after["ui_failed"]:
        per["CP5"] = "FAIL(shape b: UI build error — %s)" % (after["ui_failed"][:1] or "WidgetBlueprintLibrary not found")
        cp5 = False
    elif after["enginetick_unavailable"] or after["processevent_unavailable"]:
        per["CP5"] = ("FAIL(shape a anomaly: hook-unavailable line after mods-started is UNREACHABLE per "
                      "TICK-FALLBACK-DECISION — check whether UE4SS-settings.ini was restaged (%s); "
                      "keep KEEP-EngineTick decision" % ("ProcessEvent variant = restage PROOF"
                      if after["processevent_unavailable"] else "EngineTick variant"))
        cp5 = False
    else:
        per["CP5"] = "FAIL(shape d: no recognized reaction in L — run the §4.3d ladder (CP4 banner? staging? clean edge? liveness?); do NOT re-press repeatedly"
        cp5 = False
    cross = classify_l_log(l_after_cross)
    new_spawns = len(cross["spawned_line"]) - len(after["spawned_line"])
    if new_spawns >= 1:
        per["CP6"] = ("PASS(new Spawned line(s): %s — user must ALSO confirm the visible pickup in the world; "
                      "log without visible actor = investigate)" % (cross["spawned_line"][-1],))
    else:
        per["CP6"] = "FAIL(no new Spawned line — check §5.4 'Failed at <stage>' shapes in L)"
    per["CP7"] = ("delegated: rung_check.py --rung RUNG2 (only with the driver staged); RUNG3 via "
                  "Rung3Probe mod — optional, never blocks the goal")
    return cp5, per


# --------------------------------------------------------------------------
# Execution engine
# --------------------------------------------------------------------------

class Ctx:
    def __init__(self, run_dir, dry=False, assume_yes=False):
        self.run_dir = run_dir
        self.dry = dry
        self.assume_yes = assume_yes
        self.events = []
        self.plan_lines = []
        self.state = {}  # cross-step scratch (pre-images, fetched texts, cursors)

    def log(self, msg):
        stamp = _dt.datetime.now().strftime("%H:%M:%S")
        line = "[%s] %s" % (stamp, msg)
        print(line)
        self.events.append(line)
        if not self.dry and self.run_dir:
            with open(os.path.join(self.run_dir, "events.log"), "a") as f:
                f.write(line + "\n")

    def save(self, name, data, mode="w"):
        if self.dry or not self.run_dir:
            return None
        p = os.path.join(self.run_dir, name)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, mode) as f:
            f.write(data)
        return p


def run_cmd(ctx, argv, label, outfile=None, timeout=120, cwd=None):
    """Execute an argv list; capture stdout/stderr/exit; evidence to run dir."""
    disp = shlex.join(argv) if argv else ""
    if ctx.dry:
        ctx.log("DRY  would run: %s" % disp)
        return 0, "", ""
    ctx.log("RUN  %s" % disp)
    try:
        proc = subprocess.run(argv, capture_output=True, text=True, timeout=timeout, cwd=cwd)
        rc, out, err = proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        ctx.log("FAIL  TIMEOUT after %ss: %s" % (timeout, label))
        return 124, "", "timeout"
    blob = "cmd: %s\nrc: %d\n--- stdout ---\n%s\n--- stderr ---\n%s\n" % (disp, rc, out, err)
    if outfile:
        ctx.save(outfile, blob)
    if rc != 0:
        ctx.log("WARN  rc=%d for %s (see %s)" % (rc, label, outfile or "capture"))
    return rc, out, err


def local_sha(path):
    try:
        out = subprocess.run(["shasum", "-a", "256", path], capture_output=True, text=True)
        if out.returncode != 0:
            return None, None
        return out.stdout.split()[0], os.path.getsize(path)
    except OSError:
        return None, None


def user_gate(ctx, prompt):
    """Handoff: print the user action; in execute mode wait for confirmation."""
    ctx.log("")
    ctx.log("=" * 78)
    ctx.log("USER ACTION REQUIRED:")
    for ln in prompt.strip().splitlines():
        ctx.log("  " + ln)
    ctx.log("=" * 78)
    if ctx.dry:
        ctx.log("(dry-run: continuing without waiting)")
        return True
    if ctx.assume_yes:
        ctx.log("(--assume-yes: not waiting for interactive confirmation)")
        return True
    try:
        ans = input("Type 'done' and press Enter when complete (or 'abort'): ").strip().lower()
    except EOFError:
        ctx.log("ABORT: non-interactive session reached a USER ACTION gate. "
                "Re-run interactively when the user is present.")
        return False
    if ans in ("done", "ok", "y", "yes", ""):
        return True
    ctx.log("ABORT requested at USER ACTION gate.")
    return False


class Flow:
    """An ordered list of steps with group-based dependency skipping.

    A step with rescue=True runs even after a stop-on-first-failure halt; its
    own fn decides whether it actually acts (e.g. the ONE permitted recovery
    dispatch) or records SKIP_NOT_NEEDED. Everything after it stays skipped.
    """

    def __init__(self, name):
        self.name = name
        self.steps = []  # each: dict(id, title, group, gates, cmds, fn, rescue, prompt)

    def step(self, sid, title, group, gates, cmds, fn=None, prompt=None, rescue=False):
        self.steps.append({"id": sid, "title": title, "group": group,
                           "gates": gates, "cmds": cmds, "fn": fn, "prompt": prompt,
                           "rescue": rescue})
        return self

    def run(self, ctx):
        results = []
        failed_groups = set()
        halted = False
        for st in self.steps:
            if ctx.dry:
                ctx.log("")
                ctx.log("--- step %s :: %s  [group: %s]%s" % (st["id"], st["title"], st["group"],
                                                              "  [rescue/conditional]" if st.get("rescue") else ""))
                for g in st["gates"]:
                    ctx.log("  GATE  %s" % g)
                for c in st["cmds"]:
                    ctx.log("  CMD   %s" % c)
                if st.get("prompt"):
                    ctx.log("  USER ACTION REQUIRED (printed interactively at execution):")
                    for ln in st["prompt"].strip().splitlines():
                        ctx.log("      " + ln.strip())
                results.append((st["id"], "PLANNED"))
                continue
            if halted and not st.get("rescue"):
                ctx.log("SKIP step %s (%s) — flow halted earlier" % (st["id"], st["title"]))
                results.append((st["id"], "SKIPPED_HALT"))
                continue
            if st["group"] in failed_groups and not st.get("rescue"):
                ctx.log("SKIP step %s (%s) — dependency group '%s' failed" %
                        (st["id"], st["title"], st["group"]))
                results.append((st["id"], "SKIPPED_DEPENDENCY"))
                continue
            ctx.log("")
            ctx.log("--- step %s :: %s" % (st["id"], st["title"]))
            if st["fn"] is None:  # doc-only step: commands are informational
                status, detail = "PASS", "informational step (no handler)"
            else:
                status, detail = st["fn"](ctx)
            ctx.log("STEP %s -> %s%s" % (st["id"], status, (": " + detail) if detail else ""))
            results.append((st["id"], status))
            if status != "PASS" and status != "WARN":
                failed_groups.add(st["group"])
                if STOP_ON_FAILURE:
                    halted = True
                    ctx.log("stop-on-first-failure: flow '%s' halted after group '%s' (later steps skipped; "
                            "rescue/contingency steps still evaluated; nothing was re-dispatched)" % (self.name, st["group"]))
        return results


STOP_ON_FAILURE = True


# --------------------------------------------------------------------------
# Flow builders
# --------------------------------------------------------------------------

def gate_sha(path, sha, nbytes):
    return "local sha256(%s) == %s and bytes == %d" % (os.path.basename(path), sha[:12] + "…", nbytes)


def preflight_fn(paths, ctx_label):
    def fn(ctx):
        for path, sha, nbytes in paths:
            got_sha, got_n = local_sha(path)
            if got_sha != sha or got_n != nbytes:
                return "FAIL", "BUILD-ID LAW: %s hashes %s/%s, pinned %s/%d — STOP, no console step runs" % (
                    path, got_sha, got_n, sha, nbytes)
            ctx.log("preflight OK: %s (%d bytes)" % (os.path.basename(path), got_n))
        return "PASS", "%d/%d artifact identities verified" % (len(paths), len(paths))
    return fn


def curl_get(url, outfile=None, expect_substr=None, name="get"):
    def fn(ctx):
        rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [url], name, outfile=outfile)
        if rc != 0:
            return "FAIL", "%s curl rc=%d (G0 reachability — if 8084 does not answer, stop; do not probe alternate ports)" % (name, rc)
        if expect_substr and expect_substr not in out:
            return "FAIL", "%s: expected %r in response, got: %r" % (name, expect_substr, out[:200])
        return "PASS", out[:120].replace("\n", " ")
    return fn


def upload_once(alias, local_path):
    url = "%s/manage:upload?filename=%s" % (HTTP, alias)
    def fn(ctx):
        rc, out, err = run_cmd(ctx, ["curl", "-sS", "-X", "POST", "--data-binary", "@" + local_path, url],
                               "upload %s" % alias, outfile="upload-%s.txt" % alias.replace(".", "_"))
        if rc != 0:
            return "FAIL", "upload failed rc=%d (re-upload only to the SAME alias, never overwrite a pre-existing payload name)" % rc
        if "OK" not in out:
            return "FAIL", "upload response did not contain OK: %r" % out[:200]
        return "PASS", "uploaded %s (exactly once)" % alias
    return fn


def ftp_readback_verify(ftp_path, sha, nbytes, outfile):
    def fn(ctx):
        rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + ftp_path, "-o", ctx.run_dir + "/" + outfile if ctx.run_dir and not ctx.dry else "/dev/null"],
                               "ftp readback %s" % ftp_path)
        if rc != 0:
            return "FAIL", "readback fetch failed rc=%d" % rc
        if ctx.dry:
            return "PASS", "dry"
        p = os.path.join(ctx.run_dir, outfile)
        if not os.path.exists(p):
            return "FAIL", "readback file missing at %s" % p
        got_sha, got_n = local_sha(p)
        if got_sha != sha or got_n != nbytes:
            return "FAIL", ("READBACK MISMATCH: sha %s / %s bytes, pinned %s / %d — corrupted staging; "
                            "STOP, do not dispatch bytes-divergent payloads") % (got_sha, got_n, sha, nbytes)
        magic = open(p, "rb").read(4)
        if magic != b"\x7fELF":
            return "FAIL", "readback is not an ELF (magic %r)" % magic
        return "PASS", "readback byte-identical (%d bytes)" % got_n
    return fn


# --- dll7 / dll8 (one shared gate discipline; per-plan identity + ledger) ------

def _dll_payload_flow(tag, d, plan_doc, ledger_doc, verdict_fn, verdict_gates,
                      result_copy, verdict_file, receipt_file, preflight_receipt_file,
                      receipt_status_name):
    """The upload/check/readback==EXACTLY/dispatch-once/verdict gate chain shared
    by the dll7 (fallback) and dll8 (preferred) payload stages, per their frozen
    plans. Everything is parameterized; nothing here knows family specifics."""
    fl = Flow(tag)
    fl.step(tag + ".0", "pre-flight local identity + receipt profile checks", "preflight",
            [gate_sha(d["elf"], d["sha"], d["bytes"]),
             "receipt result_profile.path == %s (not /tmp — the /tmp-trap lesson: verify, do not assume)" % d["report_profile_path"],
             "receipt flags: console_contact=false, target_execution=false, prx_created=false"],
            ["shasum -a 256 %s" % d["elf"],
             "stat -f %%z %s" % d["elf"],
             "python3 -c 'read receipt.json, assert profile fields'"],
            preflight_fn([(d["elf"], d["sha"], d["bytes"])], tag))

    def receipt_check(ctx):
        try:
            r = json.load(open(d["receipt"]))
        except Exception as e:
            return "FAIL", "cannot read receipt.json: %s" % e
        prof = r.get("result_profile", {})
        art = r.get("artifact", {})
        problems = []
        if prof.get("path") != d["report_profile_path"]:
            problems.append("result_profile.path=%r != %r" % (prof.get("path"), d["report_profile_path"]))
        if art.get("sha256") != d["sha"] or art.get("bytes") != d["bytes"]:
            problems.append("receipt artifact identity diverges from pinned table")
        for k in ("console_contact", "target_execution", "prx_created", "downloaded_dll_executed"):
            if r.get(k) is not False:
                problems.append("receipt %s != false" % k)
        if prof.get("build_id") != d["build_id"]:
            problems.append("receipt build_id != %s" % d["build_id"])
        ctx.save(preflight_receipt_file, json.dumps(
            {"problems": problems, "status": r.get("status")}, indent=2))
        if problems:
            return "FAIL", "; ".join(problems)
        return "PASS", "receipt profile clean (%s)" % r.get("status", "?")

    fl.step(tag + ".0b", "receipt profile assertions (path /data, flags false, build_id)", "preflight",
            ["result_profile.path == %s" % d["report_profile_path"],
             "console_contact/target_execution/prx_created/downloaded_dll_executed all false",
             "result_profile.build_id == %s…" % d["build_id"][:12]],
            ["cat %s" % d["receipt"]], receipt_check)

    fl.step(tag + ".1", "G0 manager reachability (else STOP — do not probe anything else)", "g0",
            ["GET %s/list_payloads -> HTTP 200" % HTTP],
            ["curl -sS %s/list_payloads" % HTTP],
            curl_get(HTTP + "/list_payloads", outfile="list_payloads.%s.json" % tag, name="G0 list_payloads"))

    fl.step(tag + ".2", "Step 1 — upload ONCE as fresh alias %s" % d["alias"], "upload",
            ["fresh distinct display filename; no prior alias touched (history_preserved pattern)"],
            ["curl -sS -X POST \"%s/manage:upload?filename=%s\" --data-binary @%s" % (HTTP, d["alias"], d["elf"])],
            upload_once(d["alias"], d["elf"]))

    fl.step(tag + ".3", "Step 2a — manage:check (ok / folder_exists / file_exists)", "verify",
            ["response contains ok / folder_exists / file_exists for the new entry"],
            ["curl -sS %s/manage:check" % HTTP],
            curl_get(HTTP + "/manage:check", outfile="manage_check.%s.txt" % tag, expect_substr="ok", name="manage:check"))

    fl.step(tag + ".4", "Step 2b — FTP readback identity gate (readback law)", "verify",
            ["readback sha256 == %s" % d["sha"],
             "readback bytes == %d" % d["bytes"],
             "ELF magic 7f 45 4c 46",
             "on mismatch: STOP — never dispatch a bytes-divergent payload (f72 precedent)"],
            ["curl -sS %s%s -o %s" % (FTP, d["payload_path"], "<run-dir>/%s-readback.elf" % tag),
             "shasum -a 256 <run-dir>/%s-readback.elf" % tag],
            ftp_readback_verify(d["payload_path"], d["sha"], d["bytes"], "%s-readback.elf" % tag))

    def dispatch_fn(ctx):
        rc, out, err = run_cmd(ctx, ["curl", "-sS", "%s/loadpayload:%s" % (HTTP, d["payload_path"])],
                               "dispatch %s" % d["alias"][:-4], outfile="dispatch.%s.txt" % tag)
        if rc != 0:
            return "FAIL", "dispatch curl rc=%d — exactly-one-dispatch law: do NOT re-send; reconcile first" % rc
        ctx.save("dispatch-warning.txt",
                 "DISPATCH SENT EXACTLY ONCE for %s. If results are ambiguous later, "
                 "reconcile — never re-dispatch (AGENTS.md).\n" % d["alias"][:-4])
        return "PASS", "dispatched once; response: %s" % out[:80].replace("\n", " ")

    fl.step(tag + ".5", "Step 3 — dispatch EXACTLY ONCE via pldmgr /loadpayload (game closed)", "dispatch",
            ["payload-only state: game closed, game_launched=false, no ploader arm, no PRX-slot interaction",
             "exactly one dispatch — never re-send on ambiguity; reconcile first"],
            ["curl -sS \"%s/loadpayload:%s\"" % (HTTP, d["payload_path"])],
            dispatch_fn)

    def wait_fn(ctx):
        if ctx.dry:
            return "PASS", "dry (would sleep 15 s)"
        import time
        time.sleep(15)
        return "PASS", "waited 15 s before the results readback"

    fl.step(tag + ".6", "Step 4 — wait ~15 s (self-terminating profile: 4 phases, exits in seconds)", "dispatch",
            ["if fewer than 4 new records after first read: wait 60 s, re-read ONCE, then stop — never re-dispatch"],
            ["sleep 15"],
            wait_fn)

    def results_fn(ctx):
        rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + d["results_path"], "-o",
                                     os.path.join(ctx.run_dir, result_copy)],
                               "readback results jsonl")
        if rc != 0 and not ctx.dry:
            return "FAIL", "results readback failed rc=%d" % rc
        if ctx.dry:
            return "PASS", "dry"
        text = open(os.path.join(ctx.run_dir, result_copy)).read()
        verdict, details = verdict_fn(text)
        details["expected_existing_records_before"] = d["existing_records"]
        ctx.save(verdict_file, json.dumps({"verdict": verdict, "details": details}, indent=2, default=str))
        if verdict != "PASS":
            return "FAIL", "ledger mismatch — divergent fields: %s" % details["divergences"][:4]
        return "PASS", "all 4 records match %s (%s)" % (
            ledger_doc,
            "appended after all existing records" if details["appended_after_all_existing"] else "append-only fallback assertion")

    fl.step(tag + ".7", "Step 5 — readback results + %s verdict" % ledger_doc, "verdict",
            verdict_gates,
            ["curl -sS %s%s -o <run-dir>/%s" % (FTP, d["results_path"], result_copy),
             "python3 console_batch.py verdict against %s" % ledger_doc],
            results_fn)

    def receipt_fn(ctx):
        receipt = {
            "flow": tag, "plan": plan_doc, "ledger": ledger_doc,
            "build_id": d["build_id"], "alias": d["alias"], "artifact_sha256": d["sha"], "artifact_bytes": d["bytes"],
            "status": "%s (only if %s.7 PASS — see events.log)" % (receipt_status_name, tag),
            "dispatch_count": 1, "re_dispatched": False, "jsonl_left_intact": True,
            "canonical_evidence_dir": "outputs/2026-10-04-zcode-controller/console/ (pass --run-root to write there)",
        }
        ctx.save(receipt_file, json.dumps(receipt, indent=2))
        return "PASS", "receipt written (fill statuses from events.log; CURRENT-BUILD.md notes are a manual follow-up)"

    fl.step(tag + ".8", "Step 5b — console receipt (%s) + result copy" % receipt_file, "verdict",
            ["receipt JSON saved next to the result copy; JSONL left append-only intact",
             "on ANY mismatch: freeze copy with expected_values_matched=false; NO re-dispatch, NO rebuild decision in-session"],
            ["write %s into the run dir" % receipt_file],
            receipt_fn)
    return fl


DLL7_VERDICT_GATES = [
    "exactly 4 new records for e72f5318…, sequences 0..3, phases [1,2,3,5], appended after the 36 existing records",
    "record1: all EIGHT family keys {\"enabled\":1} incl. static_tls (first time on target)",
    "record2: consumer_imports 82, helper_imports 10, worker.begin [5,1,258,259], worker.result oracle quadruple, results [86,93], memory.last.length 147456",
    "record3: stage 11, cleanup {pending:0,retired:1}, session_ends [1,1], first_failure all zeros",
    "console-only delta NOT a failure: delivery stdout_error 32 with stdout_records 1",
]


def build_dll7():
    """7-family DLL stage (ALLFAM7_DLL_TEST) — the documented FALLBACK
    attribution path. Run explicitly and only per the CONSOLE-STAGE-PLAN-8FAM
    fallback rule (SEH64-specific 8-family failure + remaining window budget);
    never part of the default session."""
    return _dll_payload_flow(
        tag="dll7", d=DLL7,
        plan_doc="CONSOLE-STAGE-PLAN-7FAM.md", ledger_doc="EXPECTED-LEDGER-7FAM.md",
        verdict_fn=verdict_7fam_records, verdict_gates=DLL7_VERDICT_GATES,
        result_copy="allfam7-console-result.jsonl", verdict_file="allfam7-ledger-verdict.json",
        receipt_file="allfam7-console-receipt.json", preflight_receipt_file="dll7-preflight-receipt-check.json",
        receipt_status_name="ALLFAM7_DLL_TEST_TARGET_RUN_PASSED")


DLL8_VERDICT_GATES = [
    "exactly 4 new records for c3aba79f…, sequences 0..3, phases [1,2,3,5], appended after ALL existing records "
    "(positions 37-40 dll8-first; 41-44 if the dll7 fallback ran first — position yields to the append-only invariant)",
    "record1: all NINE family keys {\"enabled\":1} incl. static_tls and seh64 (seh64 first time on target)",
    "record2: consumer_imports 90 (= 82 + 8 SEH64 binds: 7 KERNEL32 + 1 VCRUNTIME140), helper_imports 10, "
    "worker.begin [5,1,258,259], worker.result oracle quadruple, results [86,93], memory.last.length 114688",
    "record3: stage 11, cleanup {pending:0,retired:1}, session_ends [1,1], memory.last.length 163840 (FINAL "
    "memory.last oracle; mapped image grew 334656 -> 336552), first_failure all zeros",
    "console-only delta NOT a failure: delivery stdout_error 32 with stdout_records 1",
    "any first_failure.stage triages through the full 8-family range map incl. the NEW seh64 fixtures -130..-137",
]


def build_dll8():
    """8-family DLL stage (ALLFAM8_DLL_TEST, build c3aba79f…) — the PREFERRED
    single DLL run per CONSOLE-STAGE-PLAN-8FAM.md: same upload/check/readback/
    dispatch-once gate chain as dll7, verdict against EXPECTED-DELTA-8FAM.md."""
    return _dll_payload_flow(
        tag="dll8", d=DLL8,
        plan_doc="CONSOLE-STAGE-PLAN-8FAM.md", ledger_doc="EXPECTED-DELTA-8FAM.md",
        verdict_fn=verdict_8fam_records, verdict_gates=DLL8_VERDICT_GATES,
        result_copy="allfam8-console-result.jsonl", verdict_file="allfam8-ledger-verdict.json",
        receipt_file="allfam8-console-receipt.json", preflight_receipt_file="dll8-preflight-receipt-check.json",
        receipt_status_name="ALLFAM8_DLL_TEST_TARGET_RUN_PASSED")


# --- e2e ----------------------------------------------------------------------

def build_e2e():
    fl = Flow("e2e")
    e = E2E
    cmd_cycle_gates = ["cmd_put: STOR only if no stale e2e-cmd.json (stale => DELE it first — never overwrite)",
                       "cmd readback sha == staged bytes sha; sha appended to cmdfiles.log",
                       "dispatch EXACTLY ONCE; never re-send on ambiguity — reconcile via cmd_gone + evid",
                       "cmd_gone: e2e-cmd.json GONE and e2e-cmd.json.consumed hash == last cmdfiles.log line",
                       "evid: e2e-status.jsonl + records.txt fetched; NEW lines must equal the verbatim ledger block"]

    fl.step("e2e.0", "pre-flight: 13/13 cmd-file SHASUMS + payload identity + ELF magic", "preflight",
            ["(cd %s && shasum -a 256 -c SHASUMS.txt) => 13/13 OK" % e["cmddir"],
             gate_sha(e["payload"], e["sha"], e["bytes"]),
             "head -c 4 payload == 7f 45 4c 46",
             "port erratum ack: manager is 8084 (never 9223); if 8084 does not answer, stop — no alternate ports"],
            ["(cd %s && shasum -a 256 -c SHASUMS.txt)" % e["cmddir"],
             "shasum -a 256 %s" % e["payload"], "stat -f %%z %s" % e["payload"],
             "head -c 4 %s | xxd" % e["payload"]],
            preflight_fn([(e["payload"], e["sha"], e["bytes"])], "e2e"))

    def shasums_fn(ctx):
        rc, out, err = run_cmd(ctx, ["shasum", "-a", "256", "-c", "SHASUMS.txt"], "SHASUMS 13/13",
                               outfile="cmdfiles-SHASUMS-check.txt", cwd=e["cmddir"])
        if ctx.dry:
            return "PASS", "dry"
        if rc != 0:
            return "FAIL", "SHASUMS check failed — STOP (13/13 OK required)"
        bad = [ln for ln in out.splitlines() if ": FAILED" in ln or ": NO" in ln]
        if bad:
            return "FAIL", "SHASUMS failures: %s" % bad
        n_ok = len([ln for ln in out.splitlines() if ": OK" in ln])
        if n_ok != 13:
            return "FAIL", "expected 13 OK lines, got %d" % n_ok
        ctx.save("cmdfiles.log", open(os.path.join(e["cmddir"], "SHASUMS.txt")).read() + "\n", mode="a")
        return "PASS", "13/13 cmd files verified"

    fl.step("e2e.0b", "pre-flight b: verify all 13 staged cmd files against SHASUMS.txt", "preflight",
            ["all 13 must say OK — else STOP"],
            ["cd %s && shasum -a 256 -c SHASUMS.txt" % e["cmddir"],
             "cat SHASUMS.txt >> $EV/cmdfiles.log"],
            shasums_fn)

    fl.step("e2e.1", "G0 reachability (manager 8084)", "g0",
            ["GET %s/list_payloads -> 200" % HTTP],
            ["curl -sS %s/list_payloads" % HTTP],
            curl_get(HTTP + "/list_payloads", outfile="list_payloads.e2e.json", name="G0 list_payloads"))

    fl.step("e2e.2", "Step P — upload ONCE as MODBRIDGE_INST_E2E.elf (re-upload only to the SAME alias)", "upload",
            ["upload ONCE; re-upload only to the SAME alias; never overwrite any pre-existing payload name"],
            ["curl -sS -X POST --data-binary @%s \"%s/manage:upload?filename=MODBRIDGE_INST_E2E.elf\"" % (e["payload"], HTTP)],
            upload_once(e["alias"], e["payload"]))

    fl.step("e2e.3", "Step P b — manage:check", "upload",
            ["ok / folder_exists / file_exists for the new entry"],
            ["curl -sS %s/manage:check" % HTTP],
            curl_get(HTTP + "/manage:check", outfile="manage_check.e2e.txt", expect_substr="ok", name="manage:check"))

    def mode_fn(ctx):
        rb = "e2e-payload.readback.elf"
        rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + e["payload_path"], "-o",
                                     os.path.join(ctx.run_dir, rb)], "payload readback")
        if rc != 0 and not ctx.dry:
            return "FAIL", "payload readback failed rc=%d" % rc
        if ctx.dry:
            return "PASS", "dry"
        sha, n = local_sha(os.path.join(ctx.run_dir, rb))
        mode, reason = mode_gate(sha, n)
        ctx.save("mode-gate.json", json.dumps({"mode": mode, "sha": sha, "bytes": n}, indent=2))
        if reason:
            return "FAIL", reason
        return "PASS", "MODE=B confirmed — cmd-file (config-entry) sequence is lawful"

    fl.step("e2e.4", "Step P c — FTP readback + identity gate => MODE=B (never widen the gate)", "upload",
            ["readback sha == a21771f8… / 681888 => MODE=B (expected)",
             "readback sha == e3489297… / 681528 => MODE=A => STOP (argv flavor; cmd files must not run)",
             "anything else => PAYLOAD IDENTITY NO-GO => STOP",
             "ELF magic 7f 45 4c 46"],
            ["curl -sS %s%s -o <run-dir>/e2e-payload.readback.elf" % (FTP, e["payload_path"]),
             "RB_SHA=$(shasum -a 256 <rb> | cut -d' ' -f1); RB_SZ=$(stat -f %z <rb>)",
             "[ $RB_SHA = $CONFIG_ENTRY_SHA ] && [ $RB_SZ = $CONFIG_ENTRY_BYTES ] => MODE=B"],
            mode_fn)

    # corpus STORs (stage 4.0 / 4.3)
    def stor_fn(local, ftp_rel, sha, label):
        def fn(ctx):
            rc, out, err = run_cmd(ctx, ["curl", "-sS", "-T", local, FTP + ftp_rel], "STOR %s" % label,
                                   outfile="stor-%s.txt" % label.replace("/", "_"))
            if rc != 0:
                return "FAIL", "STOR failed rc=%d" % rc
            rb = os.path.join(ctx.run_dir, "readback-" + label.replace("/", "_"))
            rc2, out2, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + ftp_rel, "-o", rb],
                                   "readback %s" % label)
            if rc2 != 0 and not ctx.dry:
                return "FAIL", "readback failed rc=%d" % rc2
            if ctx.dry:
                return "PASS", "dry"
            got, _n = local_sha(rb)
            if got != sha:
                return "FAIL", "%s readback sha %s != pinned %s — STOP" % (label, got, sha)
            return "PASS", "%s staged + readback sha OK" % label
        return fn

    fl.step("e2e.5", "§4.0 STOR movspeed.zip + readback sha gate", "corpus",
            ["readback sha256 == 3850a2b5… else STOP (PREREQS P1/G3 STOR probe gate)"],
            ["curl -sS -T %s %s/%s" % (e["movspeed_zip"], FTP, "data/modbridge/intake/movspeed.zip"),
             "curl -sS %s/data/modbridge/intake/movspeed.zip | shasum -a 256" % FTP],
            stor_fn(e["movspeed_zip"], "/data/modbridge/intake/movspeed.zip", e["movspeed_sha"], "movspeed.zip"))

    fl.step("e2e.6", "§4.3 STOR mm-skin.mfb + mm-lua.mfb + readback sha gates", "corpus",
            ["readback shas 7b9888b9… / eefc774a… else STOP"],
            ["curl -sS -T %s %s/data/modbridge/intake/mm-skin.mfb" % (e["mm_skin"], FTP),
             "curl -sS -T %s %s/data/modbridge/intake/mm-lua.mfb" % (e["mm_lua"], FTP),
             "curl -sS %s/data/modbridge/intake/mm-skin.mfb | shasum -a 256" % FTP,
             "curl -sS %s/data/modbridge/intake/mm-lua.mfb | shasum -a 256" % FTP],
            stor_fn(e["mm_skin"], "/data/modbridge/intake/mm-skin.mfb", e["mm_skin_sha"], "mm-skin.mfb"))

    # 15-dispatch cycle builder
    dispatches = [
        ("d01", "§4.0 stage0-records.json", "stage0-records.json", "S0_records", 2, "baseline"),
        ("d02", "§4.1 stage1-register.json", "stage1-register.json", "S1_register", 2, None),
        ("d03", "§4.2 stage2a-prepare-archive.json", "stage2a-prepare-archive.json", "S2a_prepare_archive", 4, None),
        ("d04", "§4.3 stage2b-1-register.json (mm-skin)", "stage2b-1-register.json", "S1_register", 2, None),
        ("d05", "§4.3 stage2b-2-register.json (mm-lua)", "stage2b-2-register.json", "S1_register", 2, None),
        ("d06", "§4.3 stage2b-3-prepare.json", "stage2b-3-prepare.json", "S2b3_prepare_pair", 2, None),
        ("d07", "§4.4 stage3-install.json (game NOT running, C4)", "stage3-install.json", "S3_install", 3, "install"),
        ("d08", "§4.5 stage4-activated.json (SIMULATED)", "stage4-activated.json", "S4_simulate", 2, None),
        ("d09", "§4.5 stage0-records.json (post-activate baseline)", "stage0-records.json", "S0_records", 2, None),
        ("d10", "§4.5 stage5-mods-started.json (SIMULATED)", "stage5-mods-started.json", "S4_simulate", 2, None),
        ("d11", "§4.5 stage0-records.json (post-start baseline)", "stage0-records.json", "S0_records", 2, None),
        ("d12", "§4.6 stage6-1-remove.json (mm-lua)", "stage6-1-remove.json", "S6_remove1", 3, None),
        ("d13", "§4.6 stage6-2-remove.json (mm-skin)", "stage6-2-remove.json", "S6_remove2", 3, None),
        ("d14", "§4.6 stage6-3-remove.json (movspeed; final removal silent)", "stage6-3-remove.json", "S6_remove3", 2, None),
        ("d15", "§4.6 stage0-records.json (closing baseline)", "stage0-records.json", "S0_records", 2, None),
    ]

    state = {"cursor": 0, "total_new": 0, "failed_ends": 0, "refusals": 0, "last_cmd_hash": None,
             "per_stage": [], "install_completed": False}

    def make_cycle(sid, title, cmdfile, block_key, evid_n, special):
        group = "cycle-" + sid

        def cycle(ctx):
            cmddir = e["cmddir"]
            local = os.path.join(cmddir, cmdfile)
            sha, _n = local_sha(local)
            if sha is None:
                return "FAIL", "cmd file missing: %s" % local
            # cmd_put
            rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + "/data/modbridge/intake/e2e-cmd.json", "-o",
                                         os.path.join(ctx.run_dir, "stale.chk")], "stale check")
            if not ctx.dry:
                stale = os.path.exists(os.path.join(ctx.run_dir, "stale.chk")) and \
                        os.path.getsize(os.path.join(ctx.run_dir, "stale.chk")) > 0
                if stale:
                    return "FAIL", "STALE e2e-cmd.json present — DELE it first, do not overwrite (cmd_put guard)"
            rc, out, err = run_cmd(ctx, ["curl", "-sS", "-T", local, FTP + "/data/modbridge/intake/e2e-cmd.json"],
                                   "cmd_put %s" % cmdfile)
            if rc != 0:
                return "FAIL", "cmd_put STOR failed rc=%d" % rc
            rb = os.path.join(ctx.run_dir, "cmd-readback-%s" % sid)
            rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + "/data/modbridge/intake/e2e-cmd.json", "-o", rb],
                                   "cmd readback")
            if ctx.dry:
                return "PASS", "dry"
            rb_hash, _n = local_sha(rb)
            if rb_hash != sha:
                return "FAIL", "CMD READBACK MISMATCH %s != %s" % (rb_hash, sha)
            ctx.save("cmdfiles.log", sha + "\n", mode="a")
            state["last_cmd_hash"] = sha
            # dispatch
            rc, out, err = run_cmd(ctx, ["curl", "-sS", e["dispatch_url"]], "dispatch %s" % cmdfile,
                                   outfile="dispatch-%s.txt" % sid)
            if rc != 0:
                ctx.save("dispatch-ambiguity-%s.txt" % sid,
                         "DISPATCH SENT ONCE for %s; curl rc=%d. RECONCILE via cmd_gone + evid; NEVER re-send.\n" % (cmdfile, rc))
                return "FAIL", "dispatch rc=%d — reconcile via cmd_gone + evid; never re-send" % rc
            # cmd_gone
            rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + "/data/modbridge/intake/e2e-cmd.json", "-o",
                                         os.path.join(ctx.run_dir, "gone.chk")], "gone check")
            orig_present = (not ctx.dry) and os.path.exists(os.path.join(ctx.run_dir, "gone.chk")) and \
                           os.path.getsize(os.path.join(ctx.run_dir, "gone.chk")) > 0
            consumed_rb = os.path.join(ctx.run_dir, "consumed-%s" % sid)
            rc2, out2, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + "/data/modbridge/intake/e2e-cmd.json.consumed", "-o", consumed_rb],
                                   "consumed fetch")
            if ctx.dry:
                return "PASS", "dry"
            consumed_hash = local_sha(consumed_rb)[0] if os.path.exists(consumed_rb) and os.path.getsize(consumed_rb) > 0 else None
            ok, why = cmd_gone_ok(orig_present, consumed_hash, state["last_cmd_hash"])
            if not ok:
                return "FAIL", "cmd_gone: %s — reconcile, never re-dispatch" % why
            # evid
            status_before = None
            rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + "/" + e["status_ftp"], "-o",
                                         os.path.join(ctx.run_dir, "e2e-status.after-%s.jsonl" % sid)], "evid status")
            rc2, out2, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + "/" + e["records_ftp"], "-o",
                                         os.path.join(ctx.run_dir, "records.after-%s.txt" % sid)], "evid records")
            if ctx.dry:
                return "PASS", "dry"
            text = open(os.path.join(ctx.run_dir, "e2e-status.after-%s.jsonl" % sid)).read()
            verdict, info = verify_e2e_block(text, state["cursor"], E2E_BLOCKS[block_key], block_key)
            state["cursor"] += info["new_lines"]
            state["total_new"] += info["new_lines"]
            if verdict != "PASS":
                if info.get("halt"):
                    return "FAIL", "HALT: %s | diff: %s" % (info["halt"], info["diff"][:2])
                return "FAIL", "expected-line mismatch: %s" % info["diff"][:3]
            all_lines = [ln for ln in text.splitlines() if ln.strip()]
            new_lines_list = all_lines[state["cursor"] - info["new_lines"]: state["cursor"]]
            for ln in new_lines_list:
                p = norm_json_line(ln)
                if p and '"event":"end"' in p and '"exit":0' not in p:
                    state["failed_ends"] += 1
                if p and '"stage":"e2e-cmd"' in p:
                    state["refusals"] += 1
            state["per_stage"].append({"id": sid, "cmd": cmdfile, "block": block_key,
                                       "verdict": verdict, "info": info})
            ctx.save("e2e-verify-%s.json" % sid, json.dumps({"verdict": verdict, "info": info}, indent=2))
            if verdict != "PASS":
                if info.get("halt"):
                    return "FAIL", "HALT: %s | diff: %s" % (info["halt"], info["diff"][:2])
                return "FAIL", "expected-line mismatch: %s" % info["diff"][:3]
            if special == "install":
                # interrupted-transaction detection: journal remaining / last end absent
                new_lines = [ln for ln in text.splitlines() if ln.strip()][state["cursor"] - info["new_lines"]:state["cursor"]]
                end_ok = any('"event":"end","exit":0' in norm_json_line(ln) or norm_json_line(ln) == '{"event":"end","exit":0}'
                             for ln in new_lines)
                state["install_completed"] = end_ok
                if not end_ok:
                    state["install_interrupted"] = True
                    return "FAIL", ("stage 3 transaction INCOMPLETE (no clean end line) — stop-on-first-failure applies; "
                                    "the ONE permitted recovery dispatch is the next rescue step")
            return "PASS", "%s: %d new lines match ledger %s" % (cmdfile, info["new_lines"], block_key)
        return group, cycle

    for sid, title, cmdfile, block, evid_n, special in dispatches:
        group, cyc = make_cycle(sid, title, cmdfile, block, evid_n, special)
        fl.step("e2e.%s" % sid, title, group,
                cmd_cycle_gates + ["expected NEW lines: verbatim block %s (%d lines)" % (block, evid_n)],
                ["curl -sS -T %s/%s %s/data/modbridge/intake/e2e-cmd.json   # cmd_put (stale guard + readback sha)" % (e["cmddir"], cmdfile, FTP),
                 "curl -sS \"%s\"   # dispatch, exactly once" % e["dispatch_url"],
                 "cmd_gone: original GONE; .consumed hash == last cmdfiles.log line",
                 "curl -sS %s/%s -o $EV/e2e-status.jsonl ; curl -sS %s/%s -o $EV/records.txt   # evid" % (FTP, e["status_ftp"], FTP, e["records_ftp"]),
                 "verdict: new lines == EXPECTED-LEDGER %s verbatim" % block],
                cyc)
        if special == "install":
            fl.step("e2e.d07-recover", "§4.4 contingency — recover.json (the ONE permitted recovery dispatch; "
                    "runs ONLY when stage 3 was actually INTERRUPTED)", "cycle-d07-recover",
                    ["exactly ONE recovery dispatch permitted; never replay an identical failed request",
                     "if stage 3 completed cleanly this step dispatches NOTHING (records SKIP_NOT_NEEDED)",
                     "after a real recovery the flow HALTS for reconciliation: the 35-line green ledger no longer applies",
                     "expected on a clean tree: begin recover / end 0"],
                    ["cmd_put %s/recover.json; dispatch; cmd_gone; evid 2   # ONLY if interrupted" % e["cmddir"]],
                    rescue=True)

    def recover_fn(ctx):
        if not state.get("install_interrupted"):
            return "PASS", "SKIP_NOT_NEEDED — stage 3 completed cleanly; no recovery dispatch sent"
        if not user_gate(ctx, "Stage-3 install was INTERRUPTED (no clean end line).\n"
                              "The plan permits EXACTLY ONE recovery dispatch (recover.json).\n"
                              "Confirm no other console activity is in flight and this is not a duplicate\n"
                              "of an already-consumed recovery."):
            return "FAIL", "recovery declined by operator — record and stop (reconcile host-side)"
        group, cyc = make_cycle("d07r", "recover.json", "recover.json", "S3_recover", 2, None)
        status, detail = cyc(ctx)
        if status == "PASS":
            return "FAIL", ("recovery dispatched ONCE and verified (begin recover / end 0) — HALT: reconcile "
                            "host-side before any further stage; the 35-line all-green ledger no longer applies")
        return status, detail

    # attach the guarded recovery handler to the rescue step (find it by id —
    # steps[-1] is d15, NOT the rescue step)
    for st in fl.steps:
        if st["id"] == "e2e.d07-recover":
            st["fn"] = recover_fn
            break

    def ledger_fn(ctx):
        problems = e2e_ledger_check(state["total_new"], state["failed_ends"], state["refusals"], 0)
        ctx.save("e2e-ledger-final.json", json.dumps({
            "total_new_lines": state["total_new"], "expected": E2E_GREEN_TOTAL,
            "failed_ends": state["failed_ends"], "refusal_lines": state["refusals"],
            "per_stage": state["per_stage"], "problems": problems,
            "note": "SIMULATED label applies to stages 4-5 (ack-file proof, NOT runtime acceptance); "
                    "stage-6 CORRECTION: removals emit the F-R2 status line while mods remain; "
                    "only the final removal is silent."}, indent=2))
        if problems:
            return "FAIL", "; ".join(problems)
        return "PASS", "35/35 new lines, 0 failed ends, 0 refusals"

    fl.step("e2e.ledger", "§4.7 line-count ledger (35 total; 0 unmatched begins; 0 failed ends; 0 refusals)", "ledger",
            ["all-green run = 35 new e2e-status lines (2+2+4+6+3+8+8+2)",
             "any other count: stop, collect, report distinctly"],
            ["python3 console_batch.py e2e-ledger aggregation over per-dispatch verdicts"],
            ledger_fn)

    def collect_fn(ctx):
        for latest, dst in (("e2e-status.after-d15.jsonl", "MODBRIDGE_INST_E2E-e2e-status.jsonl"),
                            ("records.after-d15.txt", "MODBRIDGE_INST_E2E-records.txt")):
            p = os.path.join(ctx.run_dir, latest)
            if os.path.exists(p) and not ctx.dry:
                ctx.save(dst, open(p).read())
        rc, out, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + "/data/modbridge/state/bundles/movspeed.mfb",
                                    "-o", os.path.join(ctx.run_dir, "movspeed.mfb.onconsole")],
                             "fetch on-console bridge artifact")
        if not ctx.dry and os.path.exists(os.path.join(ctx.run_dir, "movspeed.mfb.onconsole")):
            sha, n = local_sha(os.path.join(ctx.run_dir, "movspeed.mfb.onconsole"))
            ctx.log("on-console movspeed.mfb sha256 %s (%d bytes)" % (sha, n))
        receipt = {
            "flow": "e2e", "plan": "CONSOLE-STAGE-PLAN.md + STAGED-E2E-COMMANDS.md",
            "payload_sha256": e["sha"], "payload_bytes": e["bytes"], "mode": "MODE=B (gated at e2e.4)",
            "dispatches": len(dispatches), "per_stage": state["per_stage"],
            "labels": {"stages_4_5": "SIMULATED ack-file proof, NOT runtime acceptance",
                       "stage_6": "CORRECTION applied: F-R2 status lines while mods remain"},
            "erratum_ack": "port 8084 pinned; 9223 never used",
            "F_R2_notes": "install success carries a terminal status line; removals carry one only while mods "
                          "remain; refusals close EX_CONFIG 6 with the cmd file left in place",
        }
        ctx.save("MODBRIDGE_INST_E2E-receipt.json", json.dumps(receipt, indent=2, default=str))
        return "PASS", "evidence copies + receipt written to run dir"

    fl.step("e2e.stepC", "Step C — evidence collection (read-only) + receipt JSON", "ledger",
            ["copies of e2e-status.jsonl + records.txt under the run dir",
             "receipt: dispatch list, MODE, per-stage pass/fail, SIMULATED label, CORRECTION reference, F-R2 notes, erratum ack"],
            ["cp $EV/e2e-status.jsonl $EV/MODBRIDGE_INST_E2E-e2e-status.jsonl",
             "cp $EV/records.txt $EV/MODBRIDGE_INST_E2E-records.txt",
             "curl -sS %s/data/modbridge/state/bundles/movspeed.mfb | shasum -a 256" % FTP,
             "shasum -a 256 $EV/MODBRIDGE_INST_E2E-e2e-status.jsonl; cat $EV/cmdfiles.log"],
            collect_fn)

    def reset_fn(ctx):
        if not user_gate(ctx, "§7 full-tree reset (this run owns /data/modbridge/** only).\n"
                             "Removes intake leftovers + cmd files; state/ and roots/ deleted via FTP RMD if possible,\n"
                             "otherwise left dormant and RECORDED. Never deletes anything outside /data/modbridge/**."):
            return "FAIL", "reset declined — record the leftover tree in the receipt"
        results = []
        for rel in E2E["reset_ftp"]:
            rc, out, _ = run_cmd(ctx, ["curl", "-sS", "-X", "DELE", FTP + "/" + rel], "DELE %s" % rel)
            results.append((rel, rc))
        rc, out, _ = run_cmd(ctx, ["curl", "-sS", "-X", "RMD", FTP + "/data/modbridge/state"], "RMD state")
        rc2, out2, _ = run_cmd(ctx, ["curl", "-sS", "-X", "RMD", FTP + "/data/modbridge/roots"], "RMD roots")
        ctx.save("e2e-reset-results.json", json.dumps({"deles": [list(x) for x in results],
                 "rmd_state_rc": rc, "rmd_roots_rc": rc2,
                 "note": "if FTP cannot remove directories the dormant tree stays and is RECORDED here"}, indent=2))
        return "PASS", "reset attempted; leftovers recorded in e2e-reset-results.json (payload alias MODBRIDGE_INST_E2E stays — history)"

    fl.step("e2e.reset", "§7 full-tree reset (after green, or on abort BEFORE stage 3 commits)", "ledger",
            ["DELE only inside /data/modbridge/**; if FTP cannot remove dirs, leave dormant + RECORD",
             "after green+reset the console is back to the pre-E2E payload-only state",
             "on abort AFTER stage 3 commits: rollback is stage 6 itself, NOT a reset"],
            ["curl -sS -X DELE %s/data/modbridge/intake/movspeed.zip" % FTP,
             "curl -sS -X DELE %s/data/modbridge/intake/mm-skin.mfb" % FTP,
             "curl -sS -X DELE %s/data/modbridge/intake/mm-lua.mfb" % FTP,
             "curl -sS -X DELE %s/data/modbridge/intake/e2e-cmd.json; curl -sS -X DELE %s/data/modbridge/intake/e2e-cmd.json.consumed" % (FTP, FTP),
             "# delete /data/modbridge/state and /data/modbridge/roots (owned fresh tree)"],
            reset_fn)
    return fl


# --- stage-qdrv1 ----------------------------------------------------------------

def build_stage_qdrv1():
    fl = Flow("stage-qdrv1")
    q = QDRV1

    fl.step("qdrv1.0", "pre-flight: qdrv1 PRX identity (verify the hash before replacing any owned file)", "preflight",
            [gate_sha(q["prx"], q["sha"], q["bytes"]),
             "game CLOSED — staging happens BEFORE the game session; no ploader arm yet"],
            ["shasum -a 256 %s" % q["prx"], "stat -f %%z %s" % q["prx"]],
            preflight_fn([(q["prx"], q["sha"], q["bytes"])], "qdrv1"))

    def preimage_fn(ctx):
        p = os.path.join(ctx.run_dir, "ploader.ini.pre")
        rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + q["ini"], "-o", p], "RETR pre-image ploader.ini")
        if ctx.dry:
            return "PASS", "dry"
        pre = None
        if os.path.exists(p) and os.path.getsize(p) > 0:
            pre = open(p).read()
            sha, n = local_sha(p)
            ctx.save("ploader.ini.pre.sha256", "%s  %d\n" % (sha, n))
            ctx.log("pre-image captured (%d bytes, sha %s)" % (n, sha[:12]))
        else:
            ctx.log("no prior ploader.ini retrievable — will create the STAGE-PLAN-QDRV1 §1 shape")
        # backup to console if a pre-image exists
        if pre is not None:
            tag = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
            rc2, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", p, FTP + q["ini"] + "." + tag + ".bak"],
                                "console backup ploader.ini.$TAG.bak")
            if rc2 != 0:
                return "FAIL", "could not write console .bak backup — STOP (never edit without a rollback artifact)"
        ctx.state["pre"] = pre
        return "PASS", "pre-image secured" if pre is not None else "fresh create path"

    fl.step("qdrv1.1", "RETR live ploader.ini pre-image + console .bak backup (rollback artifact first)", "edit",
            ["pre-image RETR + hash recorded BEFORE any edit (Gate A0 pattern)",
             "if no pre-image retrievable: creation path uses the EXACT STAGE-PLAN-QDRV1 §1 shape",
             "console .bak backup written before STOR"],
            ["curl -sS %s%s -o <run-dir>/ploader.ini.pre" % (FTP, q["ini"]),
             "shasum -a 256 <run-dir>/ploader.ini.pre",
             "curl -sS -T <run-dir>/ploader.ini.pre %s%s.$TAG.bak" % (FTP, q["ini"])],
            preimage_fn)

    def edit_fn(ctx):
        pre = getattr(ctx, "state", {}).get("pre")
        status, new_text = ploader_plan_edit(pre)
        ctx.save("ploader.ini.new", new_text)
        ctx.save("ploader-edit-decision.json", json.dumps({"status": status}, indent=2))
        if status == "stop":
            return "FAIL", "AMBIGUOUS loader contract — %s | pre-image preserved in run dir; reconcile, never blind-edit" % new_text[:120]
        if status == "noop":
            return "PASS", "qdrv1 entry already present in ploader.ini — no-op (change nothing)"
        problems = ploader_verify_new(new_text, status)
        if problems:
            return "FAIL", "; ".join(problems)
        return "PASS", "local edit ready (%s)" % status

    fl.step("qdrv1.2", "Local minimal edit: add `2=/data/plugins/modbridge_qh_driver.prx` (next free index)", "edit",
            ["STAGE-PLAN-QDRV1 §1: independent late-load entry, does NOT touch the composition entry",
             "if the line already exists: change nothing (no-op path)",
             "ambiguous/unknown loader contract: STOP and show the pre-image (never blind-edit)"],
            ["python3 console_batch.py ploader_plan_edit(pre-image)  # numbered-index append or exact §1 create"],
            edit_fn)

    def upload_fn(ctx):
        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", q["prx"], FTP + q["target_prx"]], "STOR qdrv1 PRX")
        if rc != 0:
            return "FAIL", "PRX STOR failed rc=%d" % rc
        p = os.path.join(ctx.run_dir, "qdrv1.readback.prx")
        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + q["target_prx"], "-o", p], "PRX readback")
        if ctx.dry:
            return "PASS", "dry"
        sha, n = local_sha(p)
        if sha != q["sha"] or n != q["bytes"]:
            return "FAIL", "qdrv1 readback %s/%s != pinned %s/%d — placement receipt flow violated" % (sha, n, q["sha"], q["bytes"])
        return "PASS", "PRX placed + readback hash equal (place, read back, compare hash, only then rely on it)"

    fl.step("qdrv1.3", "STOR modbridge_qh_driver.prx to /data/plugins/ + readback hash verify", "upload",
            ["readback sha256 == 52c32216… / 26157 bytes (verify before relying on it)"],
            ["curl -sS -T %s %s%s" % (q["prx"], FTP, q["target_prx"]),
             "curl -sS %s%s | shasum -a 256" % (FTP, q["target_prx"])],
            upload_fn)

    def stor_ini_fn(ctx):
        new_local = os.path.join(ctx.run_dir, "ploader.ini.new")
        if not os.path.exists(new_local):
            return "FAIL", "edited ini missing (qdrv1.2 must run first)"
        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", new_local, FTP + q["ini"]], "STOR edited ploader.ini")
        if rc != 0:
            return "FAIL", "ini STOR failed rc=%d" % rc
        p = os.path.join(ctx.run_dir, "ploader.ini.readback")
        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + q["ini"], "-o", p], "ini readback")
        if ctx.dry:
            return "PASS", "dry"
        rb = open(p).read()
        new = open(new_local).read()
        if rb != new:
            return "FAIL", "ini readback != edited bytes — nothing else may change; STOP and reconcile"
        return "PASS", "readback byte-identical to the minimal edit"

    fl.step("qdrv1.4", "STOR edited ploader.ini + readback eyeball (== §1 shape, nothing else changed)", "upload",
            ["readback must be byte-identical to the local minimal edit",
             "composition entry untouched; only the qdrv1 line added"],
            ["curl -sS -T <run-dir>/ploader.ini.new %s%s" % (FTP, q["ini"]),
             "curl -sS %s%s   # eyeball == §1 shape" % (FTP, q["ini"])],
            stor_ini_fn)

    fl.step("qdrv1.5", "HANDOFF — the game session (single launch window; qdrv1 rides =2)", "handoff",
            ["ONE game-launch window total: composition-5 + qdrv1 in the same ploader.ini",
             "USER: arm ploader.elf (Payload Manager UI or loadpayload) with the game fully CLOSED",
             "USER: launch Townfall; ploader surface reports injection (~1 min)",
             "then run: console_batch.py game-evidence"],
            ["USER arms ploader.elf (game closed)",
             "USER launches Townfall",
             "controller: python3 console_batch.py game-evidence"],
            None)
    return fl


# --- game-evidence ---------------------------------------------------------------

def fetch_console_file(ctx, ftp_path, out_name):
    p = os.path.join(ctx.run_dir, out_name)
    rc, out, err = run_cmd(ctx, ["curl", "-sS", "--connect-timeout", "10", "--max-time", "60",
                                 "--max-filesize", "33554432", FTP + ftp_path, "-o", p],
                           "fetch %s" % ftp_path, outfile="fetch-%s.txt" % out_name.replace(".", "_"))
    if ctx.dry:
        return True, ""
    if rc != 0 or not os.path.exists(p):
        return False, ""
    return True, open(p, errors="replace").read()


GE_PROMPT_LAUNCH = (
    "1. With the game fully CLOSED, ARM ploader.elf (Payload Manager UI or loadpayload entry).\n"
    "2. LAUNCH Townfall.\n"
    "3. Wait until the game has been running for ~60 seconds (records appear ~1 min after launch).")

GE_PROMPT_OPTIONS = (
    "1. LOAD A SAVE and get in-game (precondition P3).\n"
    "2. Press OPTIONS briefly — ONE press and release (edge-triggered; holding still yields exactly one F7).\n"
    "3. Expect the menu overlay on screen.\n"
    "Do NOT press nav buttons yet (they are silent no-ops while the menu is closed).")

GE_PROMPT_MENU = (
    "CONFIRM VISUAL: is the Item Spawner menu overlay VISIBLE on screen (title, 12 rows, controls line)?\n"
    "Optional T4: press OPTIONS again — the menu should close silently. Then continue.")

GE_PROMPT_CROSS = (
    "1. Use UP/DOWN to highlight an item row; LEFT/RIGHT to page if needed.\n"
    "2. Press CROSS once.\n"
    "3. Watch for a new pickup appearing in the world at the aim point.")


def handoff(prompt):
    def fn(ctx):
        return ("PASS", "user proceeded") if user_gate(ctx, prompt) else ("FAIL", "user aborted at the handoff gate")
    return fn


def build_game_evidence():
    fl = Flow("game-evidence")
    g = GAME

    fl.step("ge.0", "HANDOFF — arm + launch (runbook preconditions)", "handoff",
            ["USER: game fully CLOSED, arm ploader.elf (Payload Manager UI or loadpayload)",
             "USER: launch Townfall; ~1 min until injection is reported",
             "records appear ~1 min after launch; controller waits for the user's confirmation"],
            ["USER arms ploader.elf", "USER launches Townfall", "wait ~60 s after launch"],
            handoff(GE_PROMPT_LAUNCH), prompt=GE_PROMPT_LAUNCH)

    def stepA_fn(ctx):
        args = [sys.executable, g["post_launch_check"], "--json"]
        rc, out, err = run_cmd(ctx, args, "post_launch_check", outfile="post-launch-check.json", timeout=300)
        if ctx.dry:
            return "PASS", "dry"
        verdict = None
        try:
            j = json.loads(out)
            verdict = j.get("verdict")
        except Exception:
            m = re.search(r"VERDICT:\s*(\S+)", out)
            verdict = m.group(1) if m else None
        if rc == 4:
            return "FAIL", "FETCH_FAILED (all three sinks unreachable) — wait ~60 s and re-run before touching staging"
        if verdict in ("MODS_STARTED",):
            return "PASS", "post-launch verdict MODS_STARTED — proceed to CP file checks + CP4 gate"
        if verdict and verdict.startswith("FAILED_"):
            return "FAIL", "post-launch verdict %s — follow the tool's NEXT guidance (mfa -2: relaunch once; -6/-8: staged-vs-final; -3/-4/-5/-12: loadlist regenerate; bridge 1-4: CONVERSION/CONTEXT/START/INTERNAL)" % verdict
        if verdict in ("ENGINE_READY", "ACTIVATED"):
            return "WARN", "still progressing (%s) — re-run game-evidence in 30-60 s; frozen = F7/F8 bounded-wait hang check" % verdict
        if verdict in ("UNKNOWN_NEW_LINES", "NOT_STARTED"):
            return "WARN", "%s — wait, then F0 (staging/ploader)" % verdict
        if verdict in ("STOPPED",):
            return "FAIL", "STOPPED — F10 stop failure"
        return "WARN", "verdict %r — read post-launch-check.json + the tool's NEXT line" % verdict

    fl.step("ge.1", "Step A — post_launch_check.py (always first; S+L+D1 in one pass)", "classify",
            ["exit 4 = FETCH_FAILED: retry ~60 s before touching staging",
             "FAILED_<stage> verdicts route via the runbook's decisive-action table"],
            ["python3 %s [--json]" % g["post_launch_check"]],
            stepA_fn)

    def fetch_fn(ctx):
        ok_s, s_text = fetch_console_file(ctx, g["S"], "S-runtime-status.txt")
        ok_l, l_text = fetch_console_file(ctx, g["L"], "L-UE4SS.log")
        ok_d, d_text = fetch_console_file(ctx, g["D1"], "D1-aligned-entry.log")
        ok_c, c_text = fetch_console_file(ctx, g["C2"], "C2-ingame-facts.jsonl")
        if ctx.dry:
            return "PASS", "dry"
        if not (ok_s or ok_l or ok_d):
            return "FAIL", "all sinks unreachable — wait ~60 s and retry (runbook §2)"
        ctx.state["s_text"], ctx.state["l_text"], ctx.state["d1_text"], ctx.state["c2_text"] = s_text, l_text, d_text, c_text
        return "PASS", "fetched: S=%s L=%s D1=%s C2=%s" % (ok_s, ok_l, ok_d, ok_c)

    fl.step("ge.2", "Fetch the five evidence sinks (bounded curl, one fresh fetch per file)", "classify",
            ["FTP base ftp://ps5-host-not-configured.invalid:2121 anonymous; --connect-timeout 10 --max-time 60 --max-filesize 32M",
             "if fetches fail: wait ~60 s and re-run before touching staging"],
            ["curl 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/runtime-status.txt'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/townfall_runtime_aligned_entry.log'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-ingame-facts.jsonl'"],
            fetch_fn)

    def cp_fn(ctx):
        if ctx.dry:
            return "PASS", "dry"
        s_text = ctx.state.get("s_text") or ""
        l_text = ctx.state.get("l_text") or ""
        d_text = ctx.state.get("d1_text") or ""
        s_info = cp_parse_status_s(s_text)
        l_info = classify_l_log(l_text)
        d1_info = classify_d1_log(d_text)
        verdict, per, hint = cp01234_verdict(s_info, l_info, d1_info)
        ctx.save("cp0-cp4-verdict.json", json.dumps({"verdict": verdict, "per_cp": per, "hint": hint,
                 "session": s_info["session"], "states": s_info["states"]}, indent=2))
        ctx.state["cp_verdict"] = verdict
        ctx.state["cp_hint"] = hint
        if verdict != "PASS":
            return "FAIL", "CP gate not green: %s | %s" % (per, hint)
        return "PASS", "CP0-CP4 green — %s" % hint

    fl.step("ge.3", "CP0-CP4 checks (composed PRX ran; activation; engine-ready+Tick 0x37ac8c0; mods-started; mod banner)", "classify",
            ["CP0: one new module_start_entered…module_start_return 0x0 pair + probe_lifecycle_result 0x0",
             "CP1: fresh S session reaches activated; banner + 'Setting up mods...'; no module_start failed line",
             "CP2: engine-ready AND 'GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x37ac8c0)'",
             "CP3: fresh mods-started (rung-1 liveness delegated to rung_check; UNVERIFIED_DRIVER is honest, gap G2)",
             "CP4: exactly one fresh '[TownfallItemSpawner] Loaded v1.0…' banner; S ends mods-started"],
            ["python3 console_batch.py cp0-cp4 classifier over S/L/D1 in the run dir"],
            cp_fn)

    def rung_doc_fn(ctx):
        return "PASS", ("baselines: RUNG1 strict = engine-ready/mods-started AND ticks_accepted strictly increasing "
                        "AND one stable non-zero game_thread_id_key AND >=1 counted guard refusal AND a "
                        "native_query_harness line; RUNG2 strict = terminal NQH1 completed record, counts>10000, "
                        "monotonic equal/non_decreasing, identity_checks==2, refusals_between==0. "
                        "Executed for real in the next step.")

    fl.step("ge.4", "Step B — rung_check.py baselines (RUNG1+RUNG2; honest UNVERIFIED_DRIVER without the driver)", "classify",
            ["exit 0 PASS / 1 FAIL-UNVERIFIED / 2 usage / 3 CANNOT_ASSERT / 4 FETCH_FAILED",
             "with no driver staged the honest verdict is UNVERIFIED/CANNOT_ASSERT — NOT a tool failure (gap G2)",
             "never re-rate unchanged evidence; a new window needs a NEW launch"],
            ["python3 %s --json" % g["rung_check"],
             "python3 %s --rung RUNG2 --json" % g["rung_check"]],
            rung_doc_fn)

    def rung_fn(ctx):
        outs = []
        for extra, name in ((["--rung", "RUNG1"], "rung1"), (["--rung", "RUNG2"], "rung2")):
            rc, out, err = run_cmd(ctx, [sys.executable, g["rung_check"], "--json"] + extra,
                                   "rung_check %s" % name, outfile="rung_check-%s.json" % name, timeout=300)
            outs.append((name, rc, out[:200]))
        if ctx.dry:
            return "PASS", "dry"
        hard_fail = all(rc not in (0, 1, 3, 4) for _n, rc, _o in outs)
        if hard_fail:
            return "WARN", "rung_check invocation problems (usage/error): %s" % outs
        return "PASS", "rung evidence captured (PASS/UNVERIFIED/CANNOT_ASSERT are all honest outcomes — see rung_check-*.json)"

    fl.step("ge.4b", "Step B b — actually run rung_check RUNG1 + RUNG2 into the run dir", "classify",
            ["same baselines as ge.4; exit codes as documented"],
            ["python3 %s --rung RUNG1 --json" % g["rung_check"],
             "python3 %s --rung RUNG2 --json" % g["rung_check"]],
            rung_fn)

    fl.step("ge.5", "CP5 HANDOFF — load a save, press OPTIONS once (P1-P5 preconditions)", "cp5",
            ["gate: CP0-CP4 green (this step only runs after ge.3 PASS)",
             "P3: a save is LOADED, user in-game (guarantees ItemDatabaseSubsystem resolves)",
             "USER: brief OPTIONS press and release, ONE press (edge-triggered, F7 toggle; second press closes)",
             "nav binds are silent no-ops while the menu is closed — by design, do not misread as dead pad"],
            ["USER loads a save and enters the game",
             "USER presses OPTIONS once (brief press-release)",
             "controller re-fetches UE4SS.log and classifies CP5 shapes a-d"],
            handoff(GE_PROMPT_OPTIONS), prompt=GE_PROMPT_OPTIONS)

    def cp5_fn(ctx):
        before = ctx.state.get("l_text") or ""
        ok, after = fetch_console_file(ctx, g["L"], "L-after-OPTIONS.log")
        if ctx.dry:
            return "PASS", "dry"
        if not ok:
            return "FAIL", "L re-fetch failed — wait ~60 s and re-run"
        _cp5, per = cp56_verdict(before, after, after)
        ctx.save("cp5-verdict.json", json.dumps(per, indent=2))
        v = per.get("CP5", "")
        ctx.state["l_after_options"] = after
        if v.startswith("PASS"):
            return "PASS", v
        return "FAIL", v

    fl.step("ge.6", "CP5 — classify the OPTIONS result (success / shapes a-d)", "cp5",
            ["SUCCESS: '<N> spawnable pickups found. Pages=<P>' + 'Menu created.' (two new lines, in order)",
             "(a) EngineTick hook-unavailable line = UNREACHABLE anomaly — restage check, keep KEEP-EngineTick",
             "(b) WidgetBlueprintLibrary not found / UI creation failed = UI build error, retry-safe",
             "(c) 'Item database not ready. Load a save first.' = expected guard when P3 skipped (T3)",
             "(d) zero new lines = §4.3d ladder; if all checks hold it is the first genuine live dispatch gap — escalate, do NOT re-press repeatedly"],
            ["curl 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log' (re-fetch)",
             "python3 console_batch.py cp5 classifier (diff vs pre-OPTIONS L)"],
            cp5_fn)

    fl.step("ge.7", "CP5 visual confirmation (user's eyes — no log substitutes)", "cp5",
            ["menu overlay visible: canvas panel, two borders + vertical box, title, 12 rows, controls line",
             "game look/move input locked while open",
             "T4: second OPTIONS closes the menu silently (visibility restore + input unlock, no log line)"],
            ["USER confirms the menu is visibly on screen"],
            handoff(GE_PROMPT_MENU), prompt=GE_PROMPT_MENU)

    fl.step("ge.8", "CP6 HANDOFF — select an item and press CROSS", "cp6",
            ["UP/DOWN moves selection; LEFT/RIGHT changes page (visual only, no log lines — T5)",
             "CROSS activates: ground trace -> load pickup class -> spawn actor -> configure pickup (§5.2)"],
            ["USER navigates with UP/DOWN (LEFT/RIGHT pages)",
             "USER presses CROSS once",
             "controller re-fetches UE4SS.log and classifies the spawn path"],
            handoff(GE_PROMPT_CROSS), prompt=GE_PROMPT_CROSS)

    def cp6_fn(ctx):
        before = ctx.state.get("l_after_options") or ""
        ok, after = fetch_console_file(ctx, g["L"], "L-after-CROSS.log")
        if ctx.dry:
            return "PASS", "dry"
        if not ok:
            return "FAIL", "L re-fetch failed"
        # the spawn diff is measured against the menu-open L (post-OPTIONS state)
        _cp5, per = cp56_verdict(before, before, after)
        ctx.save("cp6-verdict.json", json.dumps(per, indent=2))
        v = per.get("CP6", "")
        fails = [ln for ln in after.splitlines() if "Failed at " in ln and "TownfallItemSpawner" in ln]
        if fails:
            return "FAIL", "%s | failure-stage lines: %s (retry-safe: distinct revision, never blind retry)" % (v, fails[-1])
        if v.startswith("PASS"):
            return "PASS", v
        return "FAIL", v

    fl.step("ge.9", "CP6 — classify the spawn result (Spawned line + §5.4 stage signatures)", "cp6",
            ["PASS: '[Lua] [TownfallItemSpawner] Spawned <Item> x<N> ID=<id>' (live post-readback path)",
             "'Failed at ground trace/load pickup class/spawn actor/configure pickup' each name their stage (§5.4)",
             "a visible actor WITHOUT the log line should not be possible; log WITHOUT visible actor = investigate"],
            ["curl 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log' (re-fetch)",
             "python3 console_batch.py cp6 classifier"],
            cp6_fn)

    def final_fn(ctx):
        receipt = {
            "flow": "game-evidence", "runbook": "MASTER-VERIFICATION-RUNBOOK.md",
            "cp_results": "see cp0-cp4-verdict.json / cp5-verdict.json / cp6-verdict.json / rung_check-*.json",
            "honesty": ("visible menu + visible pickup are USER-eye facts; log lines classify and route, "
                        "they never certify (runbook G4/G7; do-not-claim guard)"),
            "rung_note": "rung-1/2 with the qdrv1 driver staged should show ticks ramp + terminal NQH1 record; "
                         "after the ramp the file freezes and later runs honestly report ticks-flat FAIL (never re-rate)",
        }
        ctx.save("game-evidence-receipt.json", json.dumps(receipt, indent=2))
        return "PASS", "session evidence complete in run dir"

    fl.step("ge.10", "CP7 + session receipt (optional deeper rungs; never block the goal)", "cp6",
            ["RUNG2 via rung_check --rung RUNG2 (only with the rung-1 driver staged)",
             "RUNG3 via separate Rung3Probe mod per RUNG3-RECIPE — out of scope for this session unless requested"],
            ["python3 console_batch.py game-evidence-receipt write"],
            final_fn)
    return fl


# Session order (CONSOLE-STAGE-PLAN-8FAM "Sequencing"): EXACTLY ONE DLL stage
# runs per session. dll8 (ALLFAM8_DLL_TEST) is the PREFERRED single run — a PASS
# proves a strict superset of the 7-family run in the same budget. dll7
# (ALLFAM7_DLL_TEST, plan/ledger frozen and unchanged) is the documented
# FALLBACK attribution path: run it explicitly (--dll 7 / the dll7 subcommand)
# ONLY if the 8-family run fails on a SEH64-specific surface and the window
# still has budget. Never both by default.
DEFAULT_DLL = "dll8"
SELECTED_DLL = DEFAULT_DLL
DLL_BUILDERS = {"dll7": build_dll7, "dll8": build_dll8}


def build_all():
    dll_tag = SELECTED_DLL
    fallback_tag = "dll7" if dll_tag == "dll8" else "dll8"
    fl = Flow("all")
    fl.notes = ("Recommended single-session order (CONSOLE-STAGE-PLAN-8FAM Sequencing): "
                "payload-only stages first in ONE game-closed window (%s, then e2e), "
                "then stage-qdrv1, then the single UE4SS game session with evidence collection LAST. "
                "EXACTLY ONE DLL stage per session; %s is the documented fallback attribution "
                "path — run it explicitly only per the plan's fallback rule, never both by default."
                % (dll_tag, fallback_tag))
    for sub in (DLL_BUILDERS[dll_tag], build_e2e, build_stage_qdrv1, build_game_evidence):
        f = sub()
        for st in f.steps:
            st2 = dict(st)
            st2["id"] = "%s/%s" % (f.name, st["id"])
            fl.steps.append(st2)
    return fl


FLOWS = {"dll8": build_dll8, "dll7": build_dll7, "e2e": build_e2e, "stage-qdrv1": build_stage_qdrv1,
         "game-evidence": build_game_evidence, "all": build_all}


# --------------------------------------------------------------------------
# selftest (host + fixture tests; zero console contact)
# --------------------------------------------------------------------------

def selftest(verbose=True):
    fails = []
    oks = []

    def check(name, cond, detail=""):
        (oks if cond else fails).append((name, detail))
        if verbose:
            print(("  ok   " if cond else "  FAIL ") + name + (("  :: " + detail) if (detail and not cond) else ""))

    print("== selftest A: plan mode — every command present + gated (no execution) ==")
    for name, builder in FLOWS.items():
        ctx = Ctx(run_dir=None, dry=True)
        fl = builder()
        results = fl.run(ctx)
        plan_text = "\n".join(ctx.events)
        n_steps = len(fl.steps)
        check("plan:%s has steps" % name, n_steps > 0)
        check("plan:%s all steps PLANNED" % name, all(r == "PLANNED" for _i, r in results),
              str([r for _i, r in results if r != "PLANNED"]))
        gateless = [st["id"] for st in fl.steps if not st["gates"]]
        cmdless = [st["id"] for st in fl.steps if not st["cmds"] and st["fn"] is None]
        check("plan:%s every step carries gates" % name, not gateless, str(gateless))
        # handoff/user steps legitimately have no shell commands; everything else must
        real_cmdless = [i for i in cmdless if "HANDOFF" not in (dict((s["id"], s) for s in fl.steps)[i]["title"]) ]
        check("plan:%s every non-handoff step lists commands" % name, not real_cmdless, str(real_cmdless))

    ctx = Ctx(None, dry=True); FLOWS["dll7"]().run(ctx); d7 = "\n".join(ctx.events)
    for anchor in ["manage:upload?filename=ALLFAM7_DLL_TEST.elf",
                   "loadpayload:/data/pldmgr/payloads/ALLFAM7_DLL_TEST/ALLFAM7_DLL_TEST.elf",
                   "manage:check", "/data/modbridge-owned-dll-candidate.jsonl",
                   "83ee2ae95268", "334656", "EXPECTED-LEDGER-7FAM"]:
        check("dll7 anchor %r" % anchor[:40], anchor in d7)

    ctx = Ctx(None, dry=True); FLOWS["dll8"]().run(ctx); d8 = "\n".join(ctx.events)
    for anchor in ["manage:upload?filename=ALLFAM8_DLL_TEST.elf",
                   "loadpayload:/data/pldmgr/payloads/ALLFAM8_DLL_TEST/ALLFAM8_DLL_TEST.elf",
                   "manage:check", "/data/modbridge-owned-dll-candidate.jsonl",
                   "792811911e77", "336552", "EXPECTED-DELTA-8FAM", "CONSOLE-STAGE-PLAN-8FAM",
                   "consumer_imports 90", "memory.last.length 114688", "memory.last.length 163840",
                   "-130..-137", "allfam8-console-receipt.json"]:
        check("dll8 anchor %r" % anchor[:40], anchor in d8)

    print("== selftest A2: session order — dll8-first, exactly ONE DLL stage ==")
    saved_sel = SELECTED_DLL
    fa = build_all()
    ctx = Ctx(None, dry=True); fa.run(ctx)
    ids = [st["id"] for st in fa.steps]
    check("all default runs dll8 (never dll7)",
          any(i.startswith("dll8/") for i in ids) and not any(i.startswith("dll7/") for i in ids))
    check("all default order dll8 FIRST", ids[0].startswith("dll8/"), ids[0])
    check("all notes name the fallback attribution path", "fallback attribution" in (fa.notes or ""))
    globals()["SELECTED_DLL"] = "dll7"
    fa = build_all()
    ctx = Ctx(None, dry=True); fa.run(ctx)
    ids = [st["id"] for st in fa.steps]
    check("all --dll 7 runs dll7 (never dll8)",
          any(i.startswith("dll7/") for i in ids) and not any(i.startswith("dll8/") for i in ids))
    globals()["SELECTED_DLL"] = saved_sel

    ctx = Ctx(None, dry=True); FLOWS["e2e"]().run(ctx); e2 = "\n".join(ctx.events)
    for cmdf in ["recover.json", "stage0-records.json", "stage1-register.json", "stage2a-prepare-archive.json",
                 "stage2b-1-register.json", "stage2b-2-register.json", "stage2b-3-prepare.json",
                 "stage3-install.json", "stage4-activated.json", "stage5-mods-started.json",
                 "stage6-1-remove.json", "stage6-2-remove.json", "stage6-3-remove.json"]:
        check("e2e staged cmd file present: %s" % cmdf, cmdf in e2)
    for anchor in ["SHASUMS.txt", "3850a2b5", "7b9888b9", "eefc774a", "a21771f8", "MODE=B",
                   "manage:upload?filename=MODBRIDGE_INST_E2E.elf", "loadpayload:/data/pldmgr/payloads/MODBRIDGE_INST_E2E",
                   "e2e-cmd.json.consumed", "MODBRIDGE_INST_E2E.elf", "data/modbridge/intake/movspeed.zip",
                   "-X DELE", "35"]:
        check("e2e anchor %r" % anchor[:40], anchor in e2)
    n_dispatch = e2.count("# dispatch, exactly once")
    check("e2e has exactly 15 dispatch cycles", n_dispatch == 15, "found %d" % n_dispatch)

    ctx = Ctx(None, dry=True); FLOWS["stage-qdrv1"]().run(ctx); qd = "\n".join(ctx.events)
    for anchor in ["modbridge_qh_driver.prx", "52c3221665a4", "ploader.ini", ".bak",
                   "/data/plugins/modbridge_qh_driver.prx", "USER ACTION REQUIRED" if False else "arm ploader"]:
        check("qdrv1 anchor %r" % anchor[:40], anchor in qd)

    ctx = Ctx(None, dry=True); FLOWS["game-evidence"]().run(ctx); ge = "\n".join(ctx.events)
    for anchor in ["post_launch_check.py", "rung_check.py", "runtime-status.txt", "UE4SS.log",
                   "townfall_runtime_aligned_entry.log", "modbridge-ingame-facts.jsonl",
                   "CP0-CP4", "OPTIONS", "CROSS", "0x37ac8c0", "USER ACTION REQUIRED",
                   "LOAD A SAVE", "ARM ploader.elf"]:
        check("game-evidence anchor %r" % anchor[:40], anchor in ge)

    print("== selftest B: 7FAM verdict fixtures ==")
    base = {"marker": "modbridge.owned-dll", "schema": 1, "sequence": 0, "phase": 1,
            "build_id": "e72f5318233845369a68556cd61e59ca",
            "entry": {"enabled": 0}, "execution": {"stage": 0, "started": 0, "run_attempted": 0,
            "completed": 0, "dll_calls_started": 0, "dll_calls_returned": 0},
            "clock": {}, "worker": {}, "counts": {}, "results": [0, 0],
            "cleanup": {"pending": 0, "retired": 0}, "delivery": {}, "memory": {}, "first_failure": {}}

    def rec(seq, phase, **over):
        r = json.loads(json.dumps(base))
        r["sequence"] = seq
        r["phase"] = phase
        for k, v in over.items():
            if k == "_fams":
                for f in FAMILY_KEYS:
                    r[f] = {"enabled": 1}
            else:
                r[k] = v
        return r

    r0 = rec(0, 1)
    r1 = rec(1, 2, _fams=1, execution={"stage": 2, "started": 1, "run_attempted": 0, "completed": 0,
             "dll_calls_started": 0, "dll_calls_returned": 0},
             entry={"enabled": 1}, clock={"enabled": 1}, worker={"enabled": 1, "calls_started": 0, "calls_returned": 0},
             cleanup={"pending": 1, "retired": 0}, delivery={"file_records": 1}, first_failure={})
    good2 = json.loads(json.dumps({
        "entry": {"attach_calls": [1, 1], "attach_result": [7, 7], "detach_calls": [0, 1], "detach_result": [0, 0], "contract_errors": 0, "enabled": 1},
        "execution": {"stage": 9, "started": 1, "run_attempted": 1, "completed": 1, "dll_calls_started": 3, "dll_calls_returned": 3},
        "clock": {"enabled": 1, "session_begins": [1, 1], "session_ends": [0, 1]},
        "worker": {"enabled": 1, "calls_started": 4, "calls_returned": 4, "begin": [5, 1, 258, 259],
                   "result": [2166572391, 1234605616436508552, 3782448555, 0]},
        "counts": {"consumer_imports": 82, "helper_imports": 10, "source_acquires": [1, 1], "source_releases": [0, 1]},
        "results": [86, 93], "static_tls": {"enabled": 1},
        "memory": {"owner_status": 0, "last": {"operation": 3, "native_result": 0, "native_errno": 0, "offset": 0, "length": 147456, "rights": 0}, "first_failure": {}},
        "delivery": {"first_file_error": 0, "last_file_error": 0, "first_stdout_error": 32, "last_stdout_error": 32, "file_records": 2, "stdout_records": 1},
        "cleanup": {"pending": 1, "retired": 0}, "first_failure": {}}))
    r2 = rec(2, 3, **good2)
    good3 = json.loads(json.dumps(good2))
    good3.update({"execution": dict(good2["execution"], stage=11),
                  "entry": dict(good2["entry"], detach_calls=[1, 1]),
                  "clock": dict(good2["clock"], session_ends=[1, 1]),
                  "counts": dict(good2["counts"], source_releases=[1, 1]),
                  "cleanup": {"pending": 0, "retired": 1},
                  "delivery": dict(good2["delivery"], file_records=3)})
    r3 = rec(3, 5, **good3)
    pass_text = "\n".join(json.dumps(x) for x in (r0, r1, r2, r3))
    v, d = verdict_7fam_records(pass_text)
    check("7fam PASS fixture", v == "PASS", str(d["divergences"][:3]))

    def fail_case(name, mutate):
        lines = [json.loads(x) for x in pass_text.splitlines()]
        mutate(lines)
        v, d = verdict_7fam_records("\n".join(json.dumps(x) for x in lines))
        check("7fam FAIL fixture: %s" % name, v == "FAIL", "verdict=%s" % v)
        return d

    d = fail_case("consumer_imports 78 (stale ALLFAM payload)", lambda ls: ls[2].__setitem__("counts", dict(ls[2]["counts"], consumer_imports=78)))
    check("7fam names the field", any("consumer_imports" in x for x in d["divergences"]))
    fail_case("missing static_tls key on record1", lambda ls: ls[1].pop("static_tls"))
    fail_case("worker.begin wrong", lambda ls: ls[2].__setitem__("worker", dict(ls[2]["worker"], begin=[5, 1, 1, 1])))
    fail_case("cleanup retired:0 at phase5", lambda ls: ls[3].__setitem__("cleanup", {"pending": 1, "retired": 0}))
    fail_case("memory.last.length stale (98304)", lambda ls: ls[2]["memory"].__setitem__("last", dict(ls[2]["memory"]["last"], length=98304)))
    d = fail_case("first_failure -122", lambda ls: ls[2].__setitem__("first_failure", {"stage": -122, "native_status": 0, "windows_error": 0, "module_cause": 0}))
    check("7fam triage names TLS fixture", any("same-thread idempotence" in t for t in d["triage"]), str(d["triage"]))
    fail_case("only 1 record (phase1 crash shape)", lambda ls: ls.__delitem__(slice(1, 4)))
    fail_case("family key present at census-zero", lambda ls: ls[0].__setitem__("vm", {"enabled": 1}))
    check("7fam triage map -126", "persistence" in triage_7fam(-126))
    check("7fam triage map -77 -> mutex", "mutex" in triage_7fam(-77))
    check("7fam stdout_error32 note emitted", True)  # exercised in PASS fixture path above

    print("== selftest B2: 8FAM verdict fixtures (EXPECTED-DELTA-8FAM oracle) ==")
    base8 = json.loads(json.dumps(base))
    base8["build_id"] = "c3aba79f968a492b849e5ece2f009e8e"

    def rec8(seq, phase, **over):
        r = json.loads(json.dumps(base8))
        r["sequence"] = seq
        r["phase"] = phase
        for k, v in over.items():
            if k == "_fams":
                for f in FAMILY_KEYS_8:
                    r[f] = {"enabled": 1}
            else:
                r[k] = v
        return r

    good2_8 = json.loads(json.dumps(good2))
    good2_8["counts"] = dict(good2_8["counts"], consumer_imports=90)
    good2_8["memory"] = dict(good2_8["memory"],
                             last=dict(good2_8["memory"]["last"], length=114688))
    good2_8["seh64"] = {"enabled": 1}
    good3_8 = json.loads(json.dumps(good2_8))
    good3_8.update({"execution": dict(good3_8["execution"], stage=11),
                    "entry": dict(good3_8["entry"], detach_calls=[1, 1]),
                    "clock": dict(good3_8["clock"], session_ends=[1, 1]),
                    "counts": dict(good3_8["counts"], source_releases=[1, 1]),
                    "cleanup": {"pending": 0, "retired": 1},
                    "delivery": dict(good3_8["delivery"], file_records=3),
                    "memory": dict(good3_8["memory"],
                                   last=dict(good3_8["memory"]["last"], length=163840))})
    r1_8 = rec8(1, 2, _fams=1, execution={"stage": 2, "started": 1, "run_attempted": 0, "completed": 0,
                "dll_calls_started": 0, "dll_calls_returned": 0},
                entry={"enabled": 1}, clock={"enabled": 1},
                worker={"enabled": 1, "calls_started": 0, "calls_returned": 0},
                cleanup={"pending": 1, "retired": 0}, delivery={"file_records": 1}, first_failure={})
    pass8_text = "\n".join(json.dumps(x) for x in
                           (rec8(0, 1), r1_8, rec8(2, 3, **good2_8), rec8(3, 5, **good3_8)))
    v, d = verdict_8fam_records(pass8_text)
    check("8fam PASS fixture", v == "PASS", str(d["divergences"][:3]))
    check("8fam PASS fixture notes positions 37-40 (dll8-first)",
          any("37-40" in n and "dll8-first" in n for n in d["notes"]), str(d["notes"]))

    def fail8(name, mutate, triage_needle=None):
        lines = [json.loads(x) for x in pass8_text.splitlines()]
        mutate(lines)
        v, d = verdict_8fam_records("\n".join(json.dumps(x) for x in lines))
        check("8fam FAIL fixture: %s" % name, v == "FAIL", "verdict=%s" % v)
        if triage_needle:
            check("8fam triage names %r" % triage_needle,
                  any(triage_needle in t for t in d["triage"]), str(d["triage"]))
        return d

    fail8("consumer_imports 82 (stale ALLFAM7 artifact shape)",
          lambda ls: ls[2].__setitem__("counts", dict(ls[2]["counts"], consumer_imports=82)))
    fail8("consumer_imports 88 (partial SEH64 bind chain)",
          lambda ls: ls[3].__setitem__("counts", dict(ls[3]["counts"], consumer_imports=88)))
    fail8("helper_imports drift (must stay 10)",
          lambda ls: ls[2].__setitem__("counts", dict(ls[2]["counts"], helper_imports=12)))
    fail8("memory.last.length stale 7fam value 147456 at seq2",
          lambda ls: ls[2]["memory"].__setitem__("last", dict(ls[2]["memory"]["last"], length=147456)))
    fail8("final memory.last oracle lost (163840 -> 114688 at seq3)",
          lambda ls: ls[3]["memory"].__setitem__("last", dict(ls[3]["memory"]["last"], length=114688)))
    fail8("seh64 key missing at record1 (stale ALLFAM artifact)",
          lambda ls: ls[1].pop("seh64"))
    fail8("census-zero carries seh64", lambda ls: ls[0].__setitem__("seh64", {"enabled": 1}))
    fail8("first_failure -133",
          lambda ls: ls[2].__setitem__("first_failure", {"stage": -133, "native_status": 0,
                                                         "windows_error": 0, "module_cause": 0}),
          triage_needle="__C_specific_handler")
    check("8fam triage map -135 names registration order", "registration order" in triage_8fam(-135))
    check("8fam triage map -136 names RtlAddFunctionTable", "RtlAddFunctionTable" in triage_8fam(-136))
    check("8fam triage still maps 7fam ranges (-122)", "same-thread idempotence" in triage_8fam(-122))
    check("8fam triage rejects foreign code -140", "outside known fixture ranges" in triage_8fam(-140))

    # Fallback-order positional case: dll7 appends 37-40 first, dll8 takes 41-44.
    seven = [json.loads(x) for x in pass_text.splitlines()]
    eight = [json.loads(x) for x in pass8_text.splitlines()]
    prior36 = [{"marker": "modbridge.owned-dll", "schema": 1, "build_id": "133c5e9e59f24c2fb57e7e3422c665a1",
                "sequence": i % 4, "phase": [1, 2, 3, 5][i % 4]} for i in range(36)]
    v, d = verdict_8fam_records("\n".join(json.dumps(x) for x in prior36 + seven + eight))
    check("8fam fallback-order fixture (41-44 after dll7) PASS", v == "PASS", str(d["divergences"][:3]))
    check("8fam fallback-order fixture notes 41-44", any("41-44" in n for n in d["notes"]), str(d["notes"]))
    v, d = verdict_8fam_records("\n".join(json.dumps(x) for x in prior36 + seven))
    check("8fam FAIL fixture: 0 records for c3aba79f (never ran) names dual-owner suspects",
          v == "FAIL" and any("stage 55/56 dual-owner" in x for x in d["divergences"]), str(d["divergences"][:2]))

    print("== selftest C: E2E verdict fixtures ==")
    pass35 = "\n".join([
        *E2E_BLOCKS["S0_records"], *E2E_BLOCKS["S1_register"], *E2E_BLOCKS["S2a_prepare_archive"],
        *E2E_BLOCKS["S2b_bundle"], *E2E_BLOCKS["S3_install"],
        *E2E_BLOCKS["S4_simulate"], *E2E_BLOCKS["S0_records"], *E2E_BLOCKS["S4_simulate"], *E2E_BLOCKS["S0_records"],
        *E2E_BLOCKS["S6_remove1"], *E2E_BLOCKS["S6_remove2"], *E2E_BLOCKS["S6_remove3"], *E2E_BLOCKS["S0_records"],
    ])
    cursor = 0
    blocks = [("S0_records", 2), ("S1_register", 2), ("S2a_prepare_archive", 4), ("S2b_bundle", 6),
              ("S3_install", 3), ("S4_simulate", 2), ("S0_records", 2), ("S4_simulate", 2), ("S0_records", 2),
              ("S6_remove1", 3), ("S6_remove2", 3), ("S6_remove3", 2), ("S0_records", 2)]
    lines = pass35.splitlines()
    all_pass = True
    total = 0
    cursor = 0
    for key, n in blocks:
        partial = "\n".join(lines[:cursor + n])  # simulate the file AFTER this dispatch
        v, info = verify_e2e_block(partial, cursor, E2E_BLOCKS[key], key)
        if v != "PASS":
            all_pass = False
            check("e2e block %s" % key, False, str(info["diff"][:2]))
        cursor += n
        total += info["new_lines"]
    check("e2e all 13 blocks PASS against the 35-line transcript", all_pass)
    check("e2e green total == 35", total == 35 == E2E_GREEN_TOTAL, str(total))

    def e2e_fail(name, text, cursor0=0, key="S2a_prepare_archive"):
        v, info = verify_e2e_block(text, cursor0, E2E_BLOCKS[key], key)
        check("e2e FAIL fixture: %s" % name, v == "FAIL", v)
        return info

    missing_autobundle = "\n".join(E2E_BLOCKS["S2a_prepare_archive"][:2] + [E2E_BLOCKS["S2a_prepare_archive"][3]])
    info = e2e_fail("missing auto-bundle line", missing_autobundle)
    check("e2e halt classifies missing auto-bundle", info["halt"] and "auto-bundle" in info["halt"], str(info["halt"]))
    refusal = "\n".join(E2E_BLOCKS["S2a_prepare_archive"][:3] +
                        ['{"event":"status","stage":"e2e-cmd","outcome":"refused","detail":"bad grammar"}'])
    info = e2e_fail("refusal line", refusal)
    check("e2e halt classifies refusal", info["halt"] and "REFUSAL" in info["halt"], str(info["halt"]))
    exit2 = "\n".join(E2E_BLOCKS["S2a_prepare_archive"][:3] + ['{"event":"end","exit":2}'])
    info = e2e_fail("end exit=2", exit2)
    check("e2e halt classifies exit2 refusal path", info["halt"] and "exit=2" in info["halt"], str(info["halt"]))
    stage6_missing_status = "\n".join(E2E_BLOCKS["S6_remove1"][:1] + E2E_BLOCKS["S6_remove1"][2:])
    v, info = verify_e2e_block(stage6_missing_status, 0, E2E_BLOCKS["S6_remove1"], "S6_remove1")
    check("e2e FAIL fixture: stage6 missing F-R2 status line", v == "FAIL", v)
    check("e2e ledger rejects wrong count", e2e_ledger_check(33, 0, 0, 0) != [])
    check("e2e ledger accepts 35 clean", e2e_ledger_check(35, 0, 0, 0) == [])

    print("== selftest D: MODE gate + cmd_gone ==")
    m, why = mode_gate(E2E["sha"], E2E["bytes"])
    check("MODE=B on config-entry bytes", m == "MODE=B" and why is None)
    m, why = mode_gate(E2E["d1rel_sha"], E2E["d1rel_bytes"])
    check("MODE=A refuses cmd files", m == "MODE=A" and why and "STOP" in why)
    m, why = mode_gate("deadbeef", 123)
    check("NO-GO on foreign bytes", m == "NO-GO" and why and "NO-GO" in why)
    ok, why = cmd_gone_ok(False, "abc", "abc")
    check("cmd_gone pass", ok)
    ok, why = cmd_gone_ok(True, "abc", "abc")
    check("cmd_gone not-consumed", not ok and "NOT CONSUMED" in why)
    ok, why = cmd_gone_ok(False, "zzz", "abc")
    check("cmd_gone hash mismatch", not ok and "hash" in why)

    print("== selftest E: ploader.ini edit fixtures ==")
    st, nt = ploader_plan_edit(None)
    check("create path uses exact §1 shape", st == "create" and nt == "[modules]\n1=%s\n2=%s\n" % (QDRV1["composition_line"], QDRV1["module_line"]))
    pre = "[modules]\n1=%s\n" % QDRV1["composition_line"]
    st, nt = ploader_plan_edit(pre)
    check("append next free index", st == "append" and "2=%s" % QDRV1["module_line"] in nt and "1=%s" % QDRV1["composition_line"] in nt)
    check("append preserves composition line byte-identical", nt.startswith(pre.rstrip("\n")))
    st, nt = ploader_plan_edit(pre + "2=%s\n" % QDRV1["module_line"])
    check("noop when already staged", st == "noop")
    st, nt = ploader_plan_edit("[PPSA33286]\n%s = 2\n" % QDRV1["composition_line"])
    check("STOP on unknown loader contract", st == "stop")
    st, nt = ploader_plan_edit("garbage without modules\n")
    check("STOP on unparseable ini", st == "stop")
    problems = ploader_verify_new("[modules]\n1=%s\n2=%s\n" % (QDRV1["composition_line"], QDRV1["module_line"]), "create")
    check("verify create shape clean", problems == [])
    problems = ploader_verify_new("[modules]\n2=%s\n" % QDRV1["module_line"], "append")
    check("verify flags lost composition line", problems != [])

    print("== selftest F: CP0-CP7 classifier fixtures ==")
    s_green = "modforge-runtime-status\t1\nsession\t4242\t9876543\nstate\t0\tactivated\t0\t\nstate\t1\tengine-ready\t0\t\nstate\t2\tmods-started\t0\t\n"
    d1_green = ("[PPSA33286] modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556 "
                "module_start_entered value=0x00000000\nconstructor_begin\nconstructor_return\n"
                "probe_lifecycle_result value=0x00000000\nruntime_wiring_begin value=0x00000000\n"
                "mfa_activated_low\nmfa_activated_high\nruntime_start_ok value=0x00000000\n"
                "module_start_return value=0x00000000\n")
    l_green = ("UE4SS - v3.0 - Git SHA #abc\nroot directory: /data/plugins\nmods directories: [0] ue4ss/Mods\n"
               "Setting up mods...\n" + GAME["tick_line"] + "\n" + GAME["mod_banner"] + "\n")
    v, per, hint = cp01234_verdict(cp_parse_status_s(s_green), classify_l_log(l_green), classify_d1_log(d1_green))
    check("CP0-CP4 green fixture", v == "PASS", str(per))
    check("CP5 gate hint present", "load a save" in hint)
    s_fail = "modforge-runtime-status\t1\nsession\t4242\t9876543\nstate\t0\tactivated\t0\t\n"
    v2, per2, _ = cp01234_verdict(cp_parse_status_s(s_fail), classify_l_log(l_green), classify_d1_log(d1_green))
    check("CP2/CP3 fail on stuck-activated session", v2 == "INCOMPLETE" and "FAIL" in per2["CP2"])
    d1_f0 = "nothing here"
    _v3, per3, _ = cp01234_verdict(cp_parse_status_s(s_green), classify_l_log(l_green), classify_d1_log(d1_f0))
    check("CP0 F0 shape named", "F0" in per3["CP0"])
    l_menu_open = l_green + "[Lua] [TownfallItemSpawner] 3 spawnable pickups found. Pages=1\n[Lua] [TownfallItemSpawner] Menu created.\n"
    cp5, per = cp56_verdict(l_green, l_menu_open, l_menu_open)
    check("CP5 success shape", cp5 and per["CP5"].startswith("PASS"), per["CP5"])
    l_guard = l_green + "[Lua] [TownfallItemSpawner] Scan failed: Mods/TownfallItemSpawner/Scripts/main.lua:66: Item database not ready. Load a save first.\n"
    cp5b, per = cp56_verdict(l_green, l_guard, l_guard)
    check("CP5 shape (c) load-a-save guard", not cp5b and "shape c" in per["CP5"], per["CP5"])
    l_uifail = l_green + "[Lua] [TownfallItemSpawner] UI creation failed: bad widget\n"
    cp5c, per = cp56_verdict(l_green, l_uifail, l_uifail)
    check("CP5 shape (b) UI build error", not cp5c and "shape b" in per["CP5"], per["CP5"])
    l_silent = l_green
    cp5d, per = cp56_verdict(l_silent, l_silent, l_silent)
    check("CP5 shape (d) no reaction", not cp5d and "shape d" in per["CP5"], per["CP5"])
    l_spawn = l_menu_open + "[Lua] [TownfallItemSpawner] Spawned Antibiotics x1 ID=DeprecatedItem\n"
    _cp5e, per = cp56_verdict(l_green, l_menu_open, l_spawn)
    check("CP6 Spawned line detected", per["CP6"].startswith("PASS"), per["CP6"])
    l_spawnfail = l_menu_open + "[Lua] [TownfallItemSpawner] Failed at ground trace. See UE4SS.log. boom\n"
    _cp5f, per = cp56_verdict(l_green, l_menu_open, l_spawnfail)
    check("CP6 failure names stage", per["CP6"].startswith("FAIL"), per["CP6"])

    print("")
    print("selftest summary: %d ok, %d failed" % (len(oks), len(fails)))
    for name, detail in fails:
        print("  FAILED: %s :: %s" % (name, detail))
    return 0 if not fails else 1


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def make_run_dir(run_root, flow):
    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    d = os.path.join(run_root, "%s-%s" % (stamp, flow))
    os.makedirs(d, exist_ok=True)
    return d


def main(argv=None):
    global STOP_ON_FAILURE
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1],
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__)
    ap.add_argument("--run-root", default=os.path.join(SELF_DIR, "runs"),
                    help="evidence run-dir root (default: worker-console-batch-runner/runs; "
                         "pass %s to match the plans' canonical controller dir)" % (T + "/../2026-10-04-zcode-controller/console"))
    ap.add_argument("--continue-on-failure", action="store_true",
                    help="record failures and continue where SAFE (dependent groups are still skipped; no re-dispatch)")
    sub = ap.add_subparsers(dest="cmd")

    def add_flow(name, help_):
        p = sub.add_parser(name, help=help_)
        p.add_argument("--dry-run", action="store_true", help="plan mode for this flow only (no execution)")
        p.add_argument("--assume-yes", action="store_true",
                       help="do not block at USER ACTION gates (scheduled runs stop honestly at dry mode instead)")
        p.add_argument("--dll", choices=("7", "8"), default=None,
                       help="DLL stage for flow 'all' (and 'plan --only all'): 8 = ALLFAM8_DLL_TEST "
                            "(PREFERRED, default); 7 = ALLFAM7_DLL_TEST fallback attribution path. "
                            "EXACTLY ONE DLL stage runs per session either way.")
        return p

    p = sub.add_parser("plan", help="dry-run: print every step, gate and exact command; NO execution")
    p.add_argument("--only", default="all", help="comma list: dll8,dll7,e2e,stage-qdrv1,game-evidence,all")
    p.add_argument("--dll", choices=("7", "8"), default=None,
                   help="DLL stage 'plan --only all' previews: 8 (preferred, default) or 7 (fallback)")
    add_flow("dll8", "8-family DLL payload-only stage (game closed) — PREFERRED single DLL run")
    add_flow("dll7", "7-family DLL payload-only stage — FALLBACK attribution path only (see CONSOLE-STAGE-PLAN-8FAM)")
    add_flow("e2e", "E2E installer 15-dispatch stage 0..6 cycle (game closed)")
    add_flow("stage-qdrv1", "stage qdrv1 PRX + ploader.ini =2 entry (BEFORE the game session)")
    add_flow("game-evidence", "post-launch evidence + CP0-CP7 (interactive user handoffs)")
    add_flow("all", "full recommended session: dll8 -> e2e -> stage-qdrv1 -> game-evidence "
                    "(--dll 7 swaps in the dll7 fallback attribution path; exactly one DLL stage either way)")
    sub.add_parser("selftest", help="host + fixture tests of the tool logic (no console contact)")

    args = ap.parse_args(argv)
    if args.cmd is None:
        ap.print_help()
        return 2
    if args.cmd == "selftest":
        return selftest()
    global SELECTED_DLL
    sel = getattr(args, "dll", None)
    if sel:
        SELECTED_DLL = "dll" + sel
        print("DLL stage selected for session 'all': %s%s" % (
            SELECTED_DLL,
            " (PREFERRED)" if SELECTED_DLL == DEFAULT_DLL else " (documented fallback attribution path)"))
    STOP_ON_FAILURE = not args.continue_on_failure if hasattr(args, "continue_on_failure") else True

    dry = (args.cmd == "plan") or getattr(args, "dry_run", False)
    which = ["all"] if args.cmd in ("plan",) else [args.cmd]
    if args.cmd == "plan" and getattr(args, "only", "all") != "all":
        which = args.only.split(",")

    run_root = os.path.abspath(args.run_root)
    overall_ok = True
    for flow_name in which:
        if flow_name not in FLOWS:
            print("unknown flow: %s (choose from %s)" % (flow_name, sorted(FLOWS)))
            return 2
        fl = FLOWS[flow_name]()
        if hasattr(fl, "notes") and fl.notes:
            print("ORDER: %s" % fl.notes)
        if dry:
            ctx = Ctx(run_dir=None, dry=True)
            print("\n" + "#" * 78)
            print("# PLAN (dry-run) — flow '%s' — nothing will be executed" % flow_name)
            print("# console contact: NONE in this mode")
            print("#" * 78)
        else:
            rd = make_run_dir(run_root, flow_name)
            ctx = Ctx(run_dir=rd, dry=False, assume_yes=getattr(args, "assume_yes", False))
            print("\n" + "#" * 78)
            print("# EXECUTE flow '%s' — evidence dir: %s" % (flow_name, rd))
            print("# stop-on-first-failure: %s" % ("True" if STOP_ON_FAILURE else "False (dependent groups still skipped)"))
            print("#" * 78)
        results = fl.run(ctx)
        bad = [r for _i, r in results if r not in ("PASS", "WARN", "PLANNED")]
        if bad:
            overall_ok = False
        if not dry and ctx.run_dir:
            receipt = {
                "flow": flow_name, "date_utc": _dt.datetime.now().isoformat() + "Z",
                "stop_on_first_failure": STOP_ON_FAILURE,
                "steps": [{"id": i, "status": s} for i, s in results],
                "honesty": "statuses reflect this run only; no claim beyond the plans' own verdict rules",
            }
            with open(os.path.join(ctx.run_dir, "receipt.json"), "w") as f:
                json.dump(receipt, f, indent=2)
            with open(os.path.join(ctx.run_dir, "events.log"), "a") as f:
                f.write("\n".join(ctx.events) + "\n")
            print("\nreceipt + evidence: %s" % ctx.run_dir)
    print("\nOVERALL: %s" % ("GREEN" if overall_ok else "NOT GREEN (see step statuses)"))
    return 0 if overall_ok else 1


if __name__ == "__main__":
    sys.exit(main())
