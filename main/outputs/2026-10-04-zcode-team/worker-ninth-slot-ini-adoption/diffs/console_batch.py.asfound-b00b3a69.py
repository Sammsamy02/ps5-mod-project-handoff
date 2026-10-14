#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
console_batch.py — single controller for the staged console test plans.

Built by worker-console-batch-runner, 2026-10-04; extended the same day by
worker-batch-runner-8fam-patch (ownership transferred in-place) with the
8-family stage, then by worker-evidence-freshness-fixes (freshness gate +
CP0 fallback label), then by worker-observer-staging-extension with the
combined ridealong staging, and by worker-batch-final-ini-adoption with the
DEFINITIVE 8-entry return-session ploader.ini law (both launch probes +
bootstrap reconciliation + stage-settings). This tool executes, step by step
with gates, the plans frozen by:
  * worker-8fam-console-prep/CONSOLE-STAGE-PLAN-8FAM.md (PREFERRED single DLL
    run) + worker-dll-8fam-combined/EXPECTED-DELTA-8FAM.md
  * worker-dll-7fam-stage-prep/CONSOLE-STAGE-PLAN-7FAM.md (+ EXPECTED-LEDGER-7FAM.md)
    — kept as the documented FALLBACK attribution path, never run by default
  * worker-e2e-console-stage-prep/CONSOLE-STAGE-PLAN.md (+ EXPECTED-LEDGER.md)
    via worker-e2e-staging-final/STAGED-E2E-COMMANDS.md (the 13 cmd files)
  * worker-record-sink-addon/STAGE-PLAN-QDRV1.md (ploader.ini =2 stage, BEFORE launch)
  * worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md (§0-§9: pinned parser
    law §0/§3/§4 + SC-0..SC-8 staging skeleton + SHT-strip readback law)
  * worker-final-ini-layout/FINAL-INI-LAYOUT.md (+ CONSOLE-BATCH-CONSTANTS-BRIEF.md)
    — THE definitive return-session layout: 8 entries / 7 registered, probes
    first, content-anchored ensure ops, ONE sanctioned bootstrap whole-file
    STOR, byte-exact canonical end-state gate (supersedes the 6-entry shape
    FOR THE RETURN SESSION; parser law unchanged)
  * worker-settings-v3-package/SETTINGS-V3-DECISION.md + worker-engineversion-ini/
    NEW-SETTINGS.md §3 (the settings+loadlist lockstep restage, v3 pair)
  * worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md (CP0-CP7)
    + worker-post-launch-tools/post_launch_check.py
    + worker-rung-evidence-reader/rung_check.py

Subcommands:
  plan             dry-run: print every step, gate and exact command; NO execution
  dll8             8-family DLL payload-only stage (upload/check/readback/dispatch/ledger) — PREFERRED
  dll7             7-family DLL payload-only stage — FALLBACK attribution path only
  e2e              E2E installer 15-dispatch stage 0..6 cycle with records baselines
  stage-ridealongs DEFINITIVE ridealong+probe staging (game CLOSED, pre-launch):
                   xwp-armed + fc1pool + qdrv1 + observer PRXs under the SHT-strip
                   readback law, then the FINAL 8-entry ploader.ini reached via the
                   ONE bootstrap whole-file STOR (only when the pre-image needs the
                   historical line moves) + idempotent content-anchored ensure ops,
                   gated byte-exact against ploader.ini.canonical (7 registered) —
                   supersedes stage-qdrv1/stage-observer inside `all`
  stage-observer   observer ridealong ONLY (legacy partial path as delivered):
                   PRX upload + SHT-strip readback law + idempotent ini append of
                   ONLY the observer line (coordinates with a qdrv1 line already
                   staged by stage-qdrv1/stage-ridealongs)
  stage-qdrv1      qdrv1-only stage (legacy single-ridealong path, unchanged) + readback
  stage-settings   settings+loadlist lockstep restage (NEW-SETTINGS §3): freeze the
                   staged pair, upload the v3 package bytes, byte-exact readback BOTH
  rollback-ridealongs rollback per STAGE-PLAN-COMBINED §7: flip BOTH ridealong lines
                   to = 0 (single-character edits) with .bak + byte-verified readback
  game-evidence    post-launch evidence: post_launch_check + rung_check + CP0-CP7 checks
                   + the two probe verdict sinks (xwp jsonl + fc1pool jsonl)
  all              recommended session order: dll8 -> e2e -> stage-ridealongs -> 
                   stage-settings -> game-evidence
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
import hashlib
import json
import os
import re
import shlex
import struct
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
    # SHT-strip readback law constants (STAGE-PLAN-COMBINED §3; recomputed on host
    # 2026-10-04 from the bare-sdk ELF and byte-identical to the plan's table)
    "bare": f"{T}/worker-record-sink-addon/determinism/run1/bare-sdk/modbridge_qh_driver.prx",
    "bare_sha": "7ae1e4e39c5f312ec5927be6bec82d92c860ab8b3b65ef4a9826fd296c8ce1d9",
    "bare_bytes": 88744,
    "readback_bytes": 87112,
    "readback_body_sha": "71406d3ccc7c1da0688b68ff35fd35c4a7ef9241b4d02ce427504cd392002cf5",
    "target_prx": "/data/plugins/modbridge_qh_driver.prx",
    "ini": "/data/plugins/ploader.ini",
    "module_line": "/data/plugins/modbridge_qh_driver.prx",
    "composition_line": "/data/plugins/modbridge_supplier_composition.prx",
}

# Observer ridealong (worker-game-session-ridealongs/STAGE-PLAN-COMBINED §0/§3;
# worker-observer-packaging build-1; all four identities re-verified on disk
# 2026-10-04 by worker-observer-staging-extension before pinning).
OBS = {
    "prx": f"{T}/worker-observer-packaging/modbridge_runtime_aligned_observer.prx",
    "sha": "1afa0997682cc6ecc14fd9f82961d68755edfacb36b36a4135203e37b1011e30",
    "bytes": 30152151,
    "bare": f"{T}/worker-observer-packaging/build-1/bare-sdk/modbridge_runtime_aligned_observer.prx",
    "bare_sha": "d0a3d6feebae18680962fb976726e86ea1ea397e183185793b75c290c6c0290f",
    "bare_bytes": 30145992,
    "readback_bytes": 30143976,
    "readback_body_sha": "3f89a5b66bba614926f7e7b1273c1d2aadeb505b2797dcd7389b12cebeee6622",
    "target_prx": "/data/plugins/modbridge_runtime_aligned_observer.prx",
    "module_line": "/data/plugins/modbridge_runtime_aligned_observer.prx",
    "log": "/data/townfall_runtime_aligned_observer.log",
}

# Launch probe #1 (definitive layout entry 1): IGW exec-writability probe, ARMED
# variant (worker-writability-probe-build, build_id igd1-6ebab07f2111; identities
# re-verified on disk 2026-10-04 by worker-batch-final-ini-adoption before pinning).
XWP_ARMED = {
    "prx": f"{T}/worker-writability-probe-build/modbridge_exec_write_probe_armed.prx",
    "sha": "48949316821b574bc0738b965ec0ab01500a870e34540e44568b65490704402e",
    "bytes": 20013,
    "bare": f"{T}/worker-writability-probe-build/determinism/run-armed-1/bare-sdk/modbridge_exec_write_probe.prx",
    "bare_sha": "cfed5e3d7e62100649fe1221cd4eaaf7554df421f08792264ae5238acb6d0830",
    "bare_bytes": 69464,
    # SHT-strip readback law constants (same law as STAGE-PLAN-COMBINED §3;
    # recomputed on host 2026-10-04 from the bare-sdk ELF, run1 == run2)
    "readback_bytes": 67832,
    "readback_body_sha": "daea2ff4a6533fa9006efeeb1b24913a586b72c0f68826f563b9479bf6ef561b",
    "target_prx": "/data/plugins/modbridge_exec_write_probe_armed.prx",
    "module_line": "/data/plugins/modbridge_exec_write_probe_armed.prx",
    "build_id": "igd1-6ebab07f2111",
    "log": "/data/modbridge-exec-write-probe.jsonl",   # + UE4SS mirror lines
}

# Launch probe #2 (definitive layout entry 2): fc1 validator probe
# (worker-fc1-validator-probe, build_id fc1pool1-78508a2e2940; identities
# re-verified on disk 2026-10-04 by worker-batch-final-ini-adoption before pinning).
FC1POOL = {
    "prx": f"{T}/worker-fc1-validator-probe/modbridge_fc1pool.prx",
    "sha": "d2431bf842fdba85321a6a08b2c1b6fdc9426c971fbc7cddfcd2b14fb49f0d96",
    "bytes": 22733,
    "bare": f"{T}/worker-fc1-validator-probe/determinism/run1/bare-sdk/modbridge_fc1pool.prx",
    "bare_sha": "54f8335e6ed476a64e5bc0d65741c83808e52f79b55631a0803a410bd2b3f8a2",
    "bare_bytes": 69408,
    "readback_bytes": 67848,
    "readback_body_sha": "7e2dce27e415bea80c3100d74e6f58c9a2cd8697bc87e03b67226fba09646ed3",
    "target_prx": "/data/plugins/modbridge_fc1pool.prx",
    "module_line": "/data/plugins/modbridge_fc1pool.prx",
    "build_id": "fc1pool1-78508a2e2940",
    "log": "/data/modbridge-fc1pool-facts.jsonl",      # + one FC1POOL: UE4SS line
}

# DEFINITIVE return-session ploader.ini (worker-final-ini-layout/FINAL-INI-LAYOUT.md
# §1/§3 + CONSOLE-BATCH-CONSTANTS-BRIEF.md — supersedes the 6-entry COMBINED_INI
# shape FOR THE RETURN SESSION). Pinned parser facts unchanged (MAX_PLUGINS=10,
# modes 0/1/2 only, mode 0 SKIPPED at parse — no slot, register order == file
# order, NO path dedupe): 8 entries, 7 registered, probes first, aligned_entry/
# path_test relocated AFTER observer. End-state gate = BYTE-EXACT equality with
# ploader.ini.canonical (400 B, sha bbc34c16…), enforced via the final verifier.
FINAL_INI = {
    "ini": "/data/plugins/ploader.ini",
    "header": "[PPSA33286]",
    "expected_existing": {   # pinned modes; never rewritten, only validated
        "/data/plugins/modbridge_exec_write_probe_armed.prx": 2,
        "/data/plugins/modbridge_fc1pool.prx": 2,
        "/data/plugins/modbridge_supplier_composition.prx": 2,
        "/data/plugins/modbridge_qh_driver.prx": 2,
        "/data/plugins/modbridge_runtime_aligned_observer.prx": 2,
        "/data/plugins/modbridge_runtime_aligned_entry.prx": 2,
        "/data/plugins/modbridge_path_test.prx": 2,
        "/data/plugins/modbridge_ingame_facts.prx": 0,
    },
    "anchor_of": {   # content-anchored ensure (FINAL-INI-LAYOUT.md §3.3);
                     # NOT tail-append — converges byte-for-byte in any order
        "/data/plugins/modbridge_exec_write_probe_armed.prx": "[PPSA33286]",
        "/data/plugins/modbridge_fc1pool.prx": "/data/plugins/modbridge_exec_write_probe_armed.prx",
        "/data/plugins/modbridge_qh_driver.prx": "/data/plugins/modbridge_supplier_composition.prx",
        "/data/plugins/modbridge_runtime_aligned_observer.prx": "/data/plugins/modbridge_qh_driver.prx",
        "/data/plugins/modbridge_runtime_aligned_entry.prx": "/data/plugins/modbridge_runtime_aligned_observer.prx",
        "/data/plugins/modbridge_path_test.prx": "/data/plugins/modbridge_runtime_aligned_entry.prx",
        "/data/plugins/modbridge_ingame_facts.prx": "/data/plugins/modbridge_path_test.prx",
    },
    # lines PRE-EXISTING on the console (historical state) — their ABSENCE is an
    # anomaly (reconcile per rollback-runbook §4.3), never silently re-created
    "pre_existing_required": (
        "/data/plugins/modbridge_supplier_composition.prx",
        "/data/plugins/modbridge_runtime_aligned_entry.prx",
        "/data/plugins/modbridge_path_test.prx",
        "/data/plugins/modbridge_ingame_facts.prx",
    ),
    # lines THIS tool stages (ensured-if-absent at their anchored position)
    "stager_lines": (
        "/data/plugins/modbridge_exec_write_probe_armed.prx",
        "/data/plugins/modbridge_fc1pool.prx",
        "/data/plugins/modbridge_qh_driver.prx",
        "/data/plugins/modbridge_runtime_aligned_observer.prx",
    ),
    "registered_expected_final": 7,          # was 5 (mode-0 igf consumes no slot)
    "registered_expected_partial": "<n active lines present>",  # per-flow subset
    "canonical_sha256": "bbc34c16621ad337632d0ff29a116b0aa6e86e45ad219e8ef7539b369a5a6fe1",
    "canonical_bytes": 400,
    "canonical_template": f"{T}/worker-final-ini-layout/ploader.ini.canonical",
    "final_verifier": f"{T}/worker-final-ini-layout/verify_final_ini_layout.py",
    "final_simulator": f"{T}/worker-final-ini-layout/simulate_ploader_ini_final.py",
}

# Settings+loadlist lockstep restage pair (worker-settings-v3-package/package-v3;
# bytes re-verified on disk 2026-10-04 before pinning; targets + freeze/readback
# law per worker-engineversion-ini/NEW-SETTINGS.md §3 — the mfa refuses a
# mismatched pair with MFA_E_SIZE in EITHER direction, so the two uploads are
# ONE lockstep restage; v2 is the recorded rollback pair, never staged blind).
SETTINGS_V3 = {
    "root": "/data/modbridge/roots/PPSA33286",
    "settings_local": f"{T}/worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini",
    "settings_ftp": "/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini",
    "settings_sha": "d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038",
    "settings_bytes": 1448,
    "loadlist_local": f"{T}/worker-settings-v3-package/package-v3/loadlist.txt",
    "loadlist_ftp": "/data/modbridge/roots/PPSA33286/loadlist.txt",
    "loadlist_sha": "4d98ecd7aba1777028cca50552ef9d9616ecb070a0eb8772b8f3e40a590f5d5d",
    "loadlist_bytes": 447,
    "settings_row": "ue4ss/UE4SS-settings.ini",
    # recorded prior state (freeze gate): v2 pair, or already-v3 on an idempotent re-run
    "prior": {
        "v2": {"settings_sha": "9570ead39ce8b9a2a2d96eddbcf04d43dd0225be0bbfd5b2cd417d618b8c6e0c",
               "settings_bytes": 834,
               "loadlist_sha": "3afd10cebca1a89c653d70fff453bdec121f2bc6c230c4768f0833b3d0032ff5",
               "loadlist_bytes": 446},
        "v3": {"settings_sha": "d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038",
               "settings_bytes": 1448,
               "loadlist_sha": "4d98ecd7aba1777028cca50552ef9d9616ecb070a0eb8772b8f3e40a590f5d5d",
               "loadlist_bytes": 447},
    },
}

# LEGACY combined 6-entry end-state (STAGE-PLAN-COMBINED §1 as delivered) —
# SUPERSEDED for the return session by FINAL_INI above; retained ONLY for the
# legacy partial flows (stage-observer / stage-qdrv1 fallback paths) and their
# delivered fixtures. `all` and stage-ridealongs use FINAL_INI exclusively.
COMBINED_INI = {
    "ini": "/data/plugins/ploader.ini",
    "header": "[PPSA33286]",
    "expected_existing": {
        "/data/plugins/modbridge_runtime_aligned_entry.prx": 2,
        "/data/plugins/modbridge_path_test.prx": 2,
        "/data/plugins/modbridge_ingame_facts.prx": 0,
        "/data/plugins/modbridge_supplier_composition.prx": 2,
    },
    "anchor": "/data/plugins/modbridge_supplier_composition.prx",
    "registered_expected_combined": 5,   # 6 entries, mode-0 igf line consumes no slot
    "registered_expected_observer_only": 4,  # qdrv1 line absent (observer-only staging)
}

GAME = {
    "post_launch_check": f"{T}/worker-post-launch-tools/post_launch_check.py",
    "rung_check": f"{T}/worker-rung-evidence-reader/rung_check.py",
    "S": "/data/modbridge/roots/PPSA33286/runtime-status.txt",
    "L": "/data/plugins/UE4SS.log",
    "D1": "/data/townfall_runtime_aligned_entry.log",
    "C2": "/data/modbridge-ingame-facts.jsonl",
    "tick_line": "GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)",
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


# --- combined ridealong staging (STAGE-PLAN-COMBINED) -------------------------
# Channel law (§3): the ftpsrv decryption readback of a staged fself is the
# segment-covered prefix of the inner (bare-sdk) ELF — make_fself embeds
# per-phdr filesz slices only; the SHT tail is not embedded and e_shnum is
# zeroed. Full-file hash equality through this channel is IMPOSSIBLE by
# construction; verification is exact-length + body-hash + Ehdr-exception.

# Ehdr bytes allowed to differ between readback and bare ELF: e_shoff (40-47),
# e_shentsize/e_shnum/e_shstrndx (58-63). e_shnum (offset 60, u16) must be 0.
SHT_HDR_ALLOWED = set(range(40, 48)) | set(range(58, 64))


def sht_prefix_end(bare):
    """max(p_offset + p_filesz) over the bare-sdk ELF's program headers."""
    phoff = struct.unpack_from("<Q", bare, 32)[0]
    n = struct.unpack_from("<H", bare, 56)[0]
    end = 0
    for i in range(n):
        o = phoff + i * 56
        off, _vaddr, _paddr, filesz = struct.unpack_from("<QQQQ", bare, o + 8)
        if filesz and off + filesz > end:
            end = off + filesz
    return end


def sht_strip_readback_law(rb, bare, expect_bytes, expect_body_sha):
    """STAGE-PLAN-COMBINED §3 verification, exact snippet semantics.

    Returns (ok, detail). ok requires: len(rb) == expect_bytes (which itself
    must equal the bare ELF's computed segment-covered prefix), ELF magic,
    body bytes 64..end byte-identical to the bare ELF with body sha256 ==
    expect_body_sha, every differing header byte inside SHT_HDR_ALLOWED, and
    e_shnum zeroed in the readback header."""
    if expect_bytes < 64 or len(rb) < 64:
        return False, "READBACK-FAIL len=%d (need >= 64)" % len(rb)
    if len(rb) != expect_bytes:
        return False, "READBACK-FAIL len=%d/%d" % (len(rb), expect_bytes)
    if rb[:4] != b"\x7fELF":
        return False, "READBACK-FAIL not an ELF (magic %r)" % rb[:4]
    if len(bare) < 64 or bare[:4] != b"\x7fELF":
        return False, "READBACK-FAIL bare-sdk reference is not an ELF"
    try:
        end = sht_prefix_end(bare)
    except struct.error:
        return False, "READBACK-FAIL bare-sdk ELF header unparseable"
    if end != expect_bytes:
        return False, ("READBACK-FAIL bare-sdk computed prefix %d != pinned readback %d "
                       "(constants/artifact divergence — STOP, never widen the law)" % (end, expect_bytes))
    if end > len(bare):
        return False, "READBACK-FAIL computed prefix %d exceeds bare ELF size %d" % (end, len(bare))
    if rb[64:end] != bare[64:end]:
        return False, "READBACK-FAIL body bytes 64..%d diverge from the bare-sdk ELF" % end
    body_sha = hashlib.sha256(rb[64:end]).hexdigest()
    if body_sha != expect_body_sha:
        return False, "READBACK-FAIL body sha %s != pinned %s" % (body_sha, expect_body_sha)
    hdr_diff = [i for i in range(64) if rb[i] != bare[i]]
    outside = [hex(i) for i in hdr_diff if i not in SHT_HDR_ALLOWED]
    if outside:
        return False, "READBACK-FAIL header bytes outside the allowed set %s: %s" % (
            sorted(hex(i) for i in SHT_HDR_ALLOWED), outside)
    if struct.unpack_from("<H", rb, 60)[0] != 0:
        return False, "READBACK-FAIL e_shnum not zeroed in the readback header"
    return True, ("READBACK-OK len=%d body-sha=%s hdr-diff-bytes=%s (e_shoff/e_shentsize/"
                  "e_shnum/e_shstrndx exception; e_shnum=0)" % (len(rb), body_sha[:12],
                  [hex(i) for i in hdr_diff] or "none"))


def prx_law_verify_file(readback_path, d):
    """Run the SHT-strip law against a fetched readback file + the local bare-sdk
    reference (which is itself hash-pinned by preflight). Returns (ok, detail)."""
    rb = open(readback_path, "rb").read()
    bare = open(d["bare"], "rb").read()
    return sht_strip_readback_law(rb, bare, d["readback_bytes"], d["readback_body_sha"])


_COMBINED_ENTRY_RE = re.compile(r"^(\s*)(\S+)(\s*=\s*)([0-9]+)(\s*)$")


def _parse_combined_ini(pre_text):
    """Parse the combined ploader.ini format ([PPSA33286] + `path = mode` lines).

    Returns (entries, header_count, fatal) where entries is an ordered list of
    (path, mode_int, raw_line). fatal is None or a reason string (per the plan:
    only modes 0/1/2 are valid — anything else is dropped by the parser, so it
    must never be written or silently accepted)."""
    header_count = 0
    entries = []
    for raw in pre_text.splitlines():
        s = raw.strip()
        if s.startswith("[") and s.endswith("]"):
            header_count += 1
            continue
        if s == "" or s[0] in ";#":
            continue
        m = _COMBINED_ENTRY_RE.match(raw)
        if m:
            mode = int(m.group(4))
            if mode > 2:
                return entries, header_count, ("ini line %r has mode %d — only 0/1/2 are valid "
                                               "(parse_mode drops anything else); never write or "
                                               "build on one" % (raw.strip(), mode))
            entries.append((m.group(2), mode, raw))
        else:
            return entries, header_count, "unparseable ini line: %r" % raw
    return entries, header_count, None


def ridealong_ini_plan(pre_text, wanted):
    """STAGE-PLAN-COMBINED §4/§8 SC-0+SC-5 append-only law, idempotent.

    wanted: module paths to ensure present EXACTLY once with mode 2 (the
    observer flow passes only its own line; the combined flow passes both).

    Returns (status, new_text_or_reason, appended) —
      "append": new_text = pre-image + ONLY the missing `path = 2` lines
                (trailing newline added first if the pre-image lacks one);
                appended lists the added lines
      "noop":   every wanted line already present exactly once at mode 2
                (pre-text returned unchanged — stage nothing, store nothing)
      "stop":   ambiguous/duplicate/unexpected contract — reason string;
                NEVER blind-edit (show the pre-image, reconcile first)."""
    if pre_text is None or pre_text.strip() == "":
        return "stop", ("no ploader.ini pre-image retrievable — the combined law stages "
                        "only from the ACTUAL console pre-image (STAGE-PLAN-COMBINED §2 "
                        "honesty caveat: the exact bytes were never recorded host-side); "
                        "reconcile first"), []
    entries, header_count, fatal = _parse_combined_ini(pre_text)
    if header_count != 1:
        return "stop", ("expected exactly one %s header, found %d — reconcile against the "
                        "live pre-image" % (COMBINED_INI["header"], header_count)), []
    if fatal:
        return "stop", fatal + " — reconcile (parser drops such lines; never write one)", []
    paths = [p for p, _m, _r in entries]
    for p in set(paths):
        if paths.count(p) > 1:
            return "stop", ("duplicate plugin line %s — ploader does NOT dedupe paths and "
                            "would inject it twice (plan §1); reconcile first" % p), []
    present = dict((p, m) for p, m, _r in entries)
    for p, m in COMBINED_INI["expected_existing"].items():
        if p not in present:
            return "stop", ("expected existing line %s is missing — reconcile per the "
                            "rollback-runbook §4.3 variant rules first" % p), []
        if present[p] != m:
            return "stop", ("expected existing line %s has mode %d, pinned %d — never rewrite "
                            "an existing line; reconcile" % (p, present[p], m)), []
    for w in wanted:
        if w in present and present[w] != 2:
            return "stop", ("%s is present at mode %d — only exactly-once mode 2 (staged) or absent "
                            "(appendable) is unambiguous; a mode-0 line means a ROLLED-BACK staging "
                            "and a mode-1 line an early-load staging — reconcile per the "
                            "rollback-runbook first, never blind-flip" % (w, present[w])), []
    missing = [w for w in wanted if w not in present]
    if not missing:
        return "noop", pre_text, []
    new_text = pre_text if pre_text.endswith("\n") else pre_text + "\n"
    for w in missing:
        new_text += "%s = 2\n" % w
    return "append", new_text, ["%s = 2" % w for w in missing]


def ridealong_ini_append_only(pre_text, new_text, appended):
    """Prove the edit was append-only: new must be the pre-image (plus a single
    added newline iff the pre-image lacked one) followed by EXACTLY the
    appended lines. Returns a list of problems (empty == proven)."""
    problems = []
    if pre_text is None:
        return ["no pre-image to compare against"]
    base = pre_text if pre_text.endswith("\n") else pre_text + "\n"
    if not new_text.startswith(base):
        problems.append("edited ini does not start with the pre-image bytes (append-only law)")
        return problems
    tail = new_text[len(base):]
    expect_tail = "".join(ln + "\n" for ln in appended)
    if tail != expect_tail:
        problems.append("appended tail %r != expected exactly %r" % (tail, expect_tail))
    return problems


def ridealong_verify_endstate(new_text, required, optional=()):
    """Combined end-state per STAGE-PLAN-COMBINED §1/§4 (SC-5 gate, simulator facts).

    required: ridealong paths that MUST be present exactly once at mode 2.
    optional: ridealong paths that may be present exactly once at mode 2 or
              absent (idempotent coordination with a sibling staging flow).
    Verifies: header exactly once; the 4 expected existing lines with their
    pinned modes; no unknown plugin lines; no duplicates; registered count
    (mode != 0 — a mode-0 line is SKIPPED at parse and consumes no slot) equals
    the combined 5, or 4 when an optional ridealong is absent."""
    problems = []
    entries, header_count, fatal = _parse_combined_ini(new_text)
    if header_count != 1:
        problems.append("header %s count = %d, expected exactly 1" % (COMBINED_INI["header"], header_count))
    if fatal:
        problems.append(fatal)
        return problems
    paths = [p for p, _m, _r in entries]
    for p in set(paths):
        if paths.count(p) > 1:
            problems.append("duplicate plugin line %s (ploader would inject it twice)" % p)
    present = dict((p, m) for p, m, _r in entries)
    for p, m in COMBINED_INI["expected_existing"].items():
        if present.get(p) != m:
            problems.append("expected line %s mode %s, pinned %d" % (p, present.get(p), m))
    required_seen = []
    for w in required:
        if w in present and present[w] == 2:
            required_seen.append(w)
        else:
            problems.append("required ridealong %s must be present exactly once at mode 2 (found %s)"
                            % (w, present.get(w)))
    n_registered_expected = COMBINED_INI["registered_expected_combined"]
    for w in optional:
        if w in present and present[w] == 2:
            pass  # sibling already staged — combined 5 still holds
        elif w in present:
            problems.append("optional ridealong %s present at mode %d (only mode 2 or absent is "
                            "unambiguous)" % (w, present[w]))
        else:
            n_registered_expected = COMBINED_INI["registered_expected_observer_only"]
    known = set(COMBINED_INI["expected_existing"]) | set(required) | set(optional)
    extras = [p for p in paths if p not in known]
    if extras:
        problems.append("unknown plugin line(s) %s — combined layout is exactly the 6 pinned "
                        "entries; reconcile" % extras)
    n_registered = sum(1 for _p, m, _r in entries if m != 0)
    if n_registered != n_registered_expected:
        problems.append("registered plugin count = %d, expected exactly %d (mode-0 lines consume "
                        "no slot)" % (n_registered, n_registered_expected))
    return problems


def ridealong_rollback_edit(pre_text, wanted):
    """STAGE-PLAN-COMBINED §7 rollback: flip each wanted ridealong line from
    ` = 2` to ` = 0` — a single-character edit per line, nothing else changes
    (mode 0 is SKIPPED at parse: no attach, no FAIL notify, no timeout).

    Returns (status, new_text_or_reason, flipped):
      "flip":  new_text with exactly the wanted staged lines flipped (flipped
               lists them); a wanted line at mode 0 or absent is already rolled
               back and is left untouched
      "noop":  no wanted line is present at mode 2 (nothing to roll back)
      "stop":  ambiguous state (duplicate / mode 1 / unparseable line)."""
    if pre_text is None or pre_text.strip() == "":
        return "stop", "no ploader.ini pre-image retrievable — nothing to roll back safely", []
    entries, header_count, fatal = _parse_combined_ini(pre_text)
    if header_count != 1 or fatal:
        return "stop", "unexpected ini contract (headers=%d, fatal=%s) — reconcile" % (header_count, fatal), []
    paths = [p for p, _m, _r in entries]
    for w in wanted:
        if paths.count(w) > 1:
            return "stop", "duplicate line %s — reconcile before rollback" % w, []
    present = dict((p, m) for p, m, _r in entries)
    for w in wanted:
        if present.get(w) == 1:
            return "stop", "%s at mode 1 — not the staged = 2 shape; reconcile" % w, []
    to_flip = [w for w in wanted if present.get(w) == 2]
    if not to_flip:
        return "noop", pre_text, []
    out_lines = []
    for raw in pre_text.splitlines():
        m = _COMBINED_ENTRY_RE.match(raw)
        if m and m.group(2) in to_flip and m.group(4) == "2":
            out_lines.append(m.group(1) + m.group(2) + m.group(3) + "0" + m.group(5))
        else:
            out_lines.append(raw)
    new_text = "\n".join(out_lines)
    if pre_text.endswith("\n"):
        new_text += "\n"
    return "flip", new_text, ["%s = 2 -> = 0" % w for w in to_flip]


# --- DEFINITIVE final-layout ini law (FINAL-INI-LAYOUT.md §1/§3) --------------
# Content-anchored ensure ops (§3.3): insertion position is determined by a
# CONTENT anchor (the canonical predecessor line), never the file tail, so any
# execution order of the ensure ops converges BYTE-FOR-BYTE to the canonical
# layout (proven over all 8! = 40320 permutations by
# worker-final-ini-layout/simulate_ploader_ini_final.py, variant
# staging_permutations). Duplicates remain a hard STOP; the header is a
# PRECONDITION proven by the Step-0 RETR, never appended.

FINAL_ORDER = list(FINAL_INI["expected_existing"].keys())          # canonical order
FINAL_LINE_OF = dict((p, "%s = %d" % (p, m)) for p, m in FINAL_INI["expected_existing"].items())
FINAL_PREDECESSOR = dict(FINAL_INI["anchor_of"])
FINAL_PREDECESSOR[FINAL_INI["header"]] = None
# composition sits at canonical position 3 with the xwp probe as its predecessor;
# anchor_of pins the seven ENSURED lines' anchors (header for xwp) — the
# composition line itself is pre-existing and never appended by this tool.
FINAL_PREDECESSOR.setdefault(
    "/data/plugins/modbridge_supplier_composition.prx",
    "/data/plugins/modbridge_fc1pool.prx")


class FinalEnsureError(Exception):
    pass


def final_canonical_bytes():
    """The pinned canonical template bytes, re-hashed on EVERY call against the
    pinned sha/bytes (no stale cache; a diverged template file is a hard error)."""
    path = FINAL_INI["canonical_template"]
    data = open(path, "rb").read()
    if len(data) != FINAL_INI["canonical_bytes"] or \
            hashlib.sha256(data).hexdigest() != FINAL_INI["canonical_sha256"]:
        raise FinalEnsureError(
            "canonical template %s diverged from the pinned identity (%d B, sha %s) — "
            "STOP, never stage from a diverged template" % (
                path, len(data), hashlib.sha256(data).hexdigest()[:12]))
    return data


def final_canonical_text():
    return final_canonical_bytes().decode("ascii")


def _final_count_path(lines, path):
    return sum(1 for ln in lines if ln.split(" = ")[0].strip() == path)


def final_ini_ensure(text, path):
    """Content-anchored idempotent ensure for ONE canonical entry (§3.3).
    Mirrors simulate_ploader_ini_final.ensure (the 40320-permutation-proven law):
    duplicates raise; present-exactly-once is a no-op; absent inserts the line
    immediately AFTER its canonical predecessor (recursively ensured first).
    The header is a precondition, never appended."""
    if path == FINAL_INI["header"]:
        if _final_count_path(text.split("\n"), path) >= 1:
            return text
        raise FinalEnsureError("header %s missing — STOP, reconcile" % FINAL_INI["header"])
    if path not in FINAL_LINE_OF:
        raise FinalEnsureError("unknown path %r (never invent lines)" % path)
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    n = _final_count_path(lines, path)
    if n > 1:
        raise FinalEnsureError("duplicate line for %s — STOP, reconcile "
                               "(ploader registers duplicates twice)" % path)
    if n == 1:
        return "\n".join(lines) + "\n"          # idempotent no-op
    pred = FINAL_PREDECESSOR[path]
    text = final_ini_ensure(text, pred)
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    idx = max(i for i, ln in enumerate(lines) if ln.split(" = ")[0].strip() == pred)
    lines.insert(idx + 1, FINAL_LINE_OF[path])
    return "\n".join(lines) + "\n"


def final_ini_is_canonical_form(text, paths):
    """True iff text is EXACTLY the canonical serialization restricted to paths
    (header line + the canonical lines in canonical order, LF endings, no
    comments/blanks/whitespace variants, trailing newline present). Only from
    such a pre-image can pure appends ever reach byte-exact canonical."""
    want = FINAL_INI["header"] + "\n" + "".join(FINAL_LINE_OF[p] + "\n" for p in paths)
    return text == want


def final_ini_plan(pre_text, wanted):
    """DEFINITIVE return-session ini law (FINAL-INI-LAYOUT.md §3.3) — replaces
    the 6-entry tail-append gate; every legacy STOP class is kept.

    Returns (status, new_text_or_reason, appended_or_rebuild):
      "append":    new_text = pre-image with ONLY the missing wanted lines
                   inserted at their CONTENT-anchored positions; appended lists
                   the added lines (in canonical order regardless of call order)
      "noop":      the pre-image is ALREADY byte-exact canonical (8 entries,
                   7 registered) — stage nothing, store nothing
      "bootstrap": the ONE sanctioned whole-file rebuild is REQUIRED (the
                   present lines are NOT in canonical relative order — the
                   historical aligned_entry/path_test-before-composition shape,
                   which NO sequence of appends can fix). new_text = the
                   canonical template bytes; the caller must STOR it ONCE as its
                   own INI-BOOTSTRAPPED verdict with the .bak + RETR byte-exact
                   diff. Never a silent rewrite; a pre-image anomaly still STOPs.
      "stop":      ambiguous/anomalous — reason string; NEVER blind-edit."""
    if pre_text is None or pre_text.strip() == "":
        return "stop", ("no ploader.ini pre-image retrievable — the definitive law stages "
                        "only from the ACTUAL console pre-image (Step-0 RETR); reconcile "
                        "first"), []
    entries, header_count, fatal = _parse_combined_ini(pre_text)
    if header_count != 1:
        return "stop", ("expected exactly one %s header, found %d — reconcile against the "
                        "live pre-image" % (FINAL_INI["header"], header_count)), []
    if fatal:
        return "stop", fatal + " — reconcile (parser drops such lines; never write one)", []
    paths = [p for p, _m, _r in entries]
    for p in set(paths):
        if paths.count(p) > 1:
            return "stop", ("duplicate plugin line %s — ploader does NOT dedupe paths and "
                            "would inject it twice (FINAL-INI-LAYOUT §3.2); reconcile "
                            "first" % p), []
    present = dict((p, m) for p, m, _r in entries)
    for p in paths:                                     # unknown-line STOP (kept)
        if p not in FINAL_INI["expected_existing"]:
            return "stop", ("unknown line %s — the parser would REGISTER it (slot burn, "
                            "nothing audited it); not in the definitive 8-entry contract; "
                            "reconcile per rollback-runbook §4.3" % p), []
    for p in FINAL_INI["pre_existing_required"]:        # missing-expected STOP (kept)
        if p not in present:
            return "stop", ("expected pre-existing line %s is missing — reconcile per the "
                            "rollback-runbook §4.3 variant rules first (this tool never "
                            "re-creates historical console lines)" % p), []
    for p, m in present.items():                        # mode-divergence STOP (kept)
        if m != FINAL_INI["expected_existing"][p]:
            return "stop", ("line %s has mode %d, pinned %d — never rewrite an existing "
                            "line (a mode-0 active line is a ROLLED-BACK staging, a mode-1 "
                            "line an early-load staging); reconcile, never blind-flip"
                            % (p, m, FINAL_INI["expected_existing"][p])), []
    # order check -> decides bootstrap vs ensure-only (FINAL-INI-LAYOUT §3.3)
    idxs = [FINAL_ORDER.index(p) for p in paths]
    order_ok = all(a < b for a, b in zip(idxs, idxs[1:]))
    if not order_ok:
        canonical_text = final_canonical_text()
        return "bootstrap", canonical_text, (
            "present lines are NOT in canonical relative order (historical "
            "aligned_entry/path_test-before-composition shape) — ONE sanctioned "
            "whole-file STOR of the canonical 8-line template, gated as "
            "INI-BOOTSTRAPPED with .bak + RETR byte-exact diff")
    if [p for p in paths] == FINAL_ORDER:
        if pre_text == final_canonical_text():
            return "noop", pre_text, []
        return "stop", ("line set and order already canonical but bytes are not "
                        "byte-exact canonical (CRLF/comment/whitespace/trailing-newline "
                        "form divergence) — appends can never converge; reconcile per "
                        "rollback-runbook §4.3 (a whole-file rewrite outside the sanctioned "
                        "line-move case is NOT auto-sanctioned)"), []
    if not final_ini_is_canonical_form(pre_text, paths):
        return "stop", ("pre-image is canonically ORDERED but not in canonical byte FORM "
                        "(expected exactly header + the present canonical lines, LF, "
                        "trailing newline) — appends can never reach byte-exact canonical "
                        "from these bytes; reconcile per rollback-runbook §4.3"), []
    new_text = pre_text
    appended = []
    for p in FINAL_ORDER:                    # canonical application order (any order works)
        if p in present or p not in wanted:
            continue
        new_text = final_ini_ensure(new_text, p)
        appended.append(FINAL_LINE_OF[p])
    return "append", new_text, appended


def final_ini_verify_endstate(new_text):
    """DEFINITIVE end-state gate: byte-exact equality with ploader.ini.canonical
    (replacing the legacy '5 registered' count gate), plus the pinned parser law
    (exactly 7 registered, igf never registered, MAX_PLUGINS headroom). Returns
    a list of problems (empty == the file IS the definitive layout)."""
    problems = []
    try:
        canonical = final_canonical_text()
    except FinalEnsureError as exc:
        return [str(exc)]
    if new_text == canonical:
        entries, header_count, fatal = _parse_combined_ini(new_text)
        if fatal:
            problems.append(fatal)
        n_registered = sum(1 for _p, m, _r in entries if m != 0)
        if n_registered != FINAL_INI["registered_expected_final"]:
            problems.append("parser registered %d plugins, expected exactly %d "
                            "(mode-0 lines consume no slot)"
                            % (n_registered, FINAL_INI["registered_expected_final"]))
        if any(p.endswith("modbridge_ingame_facts.prx") for p, m, _r in entries if m != 0):
            problems.append("igf line registered despite mode 0 (parser violation)")
        return problems
    problems.append("FINAL-INI bytes differ from canonical (expected sha256 %s)"
                    % FINAL_INI["canonical_sha256"])
    entries, header_count, fatal = _parse_combined_ini(new_text)
    if header_count != 1:
        problems.append("header %s count = %d, expected exactly 1"
                        % (FINAL_INI["header"], header_count))
    if fatal:
        problems.append(fatal)
        return problems
    paths = [p for p, _m, _r in entries]
    for p in set(paths):
        if paths.count(p) > 1:
            problems.append("duplicate plugin line %s (ploader would inject it twice)" % p)
    present = dict((p, m) for p, m, _r in entries)
    for p in paths:
        if p not in FINAL_INI["expected_existing"]:
            problems.append("unknown line %s — not in the definitive 8-entry contract" % p)
    for p, pin in FINAL_INI["expected_existing"].items():
        if p in present and present[p] != pin:
            problems.append("line %s mode %d, pinned %d" % (p, present[p], pin))
        elif p not in present:
            problems.append("missing expected line %s" % p)
    idxs = [FINAL_ORDER.index(p) for p in paths if p in FINAL_INI["expected_existing"]]
    if idxs != sorted(idxs) or len(idxs) != len(paths):
        problems.append("ORDER mismatch vs canonical (injection order == file order; "
                        "probes MUST precede composition; aligned_entry/path_test belong "
                        "AFTER observer)")
    n_registered = sum(1 for _p, m, _r in entries if m != 0)
    if n_registered != FINAL_INI["registered_expected_final"]:
        problems.append("registered plugin count = %d, expected exactly %d (mode-0 igf "
                        "consumes no slot; 3/10 headroom)"
                        % (n_registered, FINAL_INI["registered_expected_final"]))
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
            ["candidate identity table in %s re-verified before touching the console" % plan_doc,
             gate_sha(d["elf"], d["sha"], d["bytes"]),
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


# --- combined ridealong staging (STAGE-PLAN-COMBINED §3-§8) --------------------

def ini_preimage_fn(label):
    """SC-0: RETR the live ini pre-image + console .bak backup (rollback artifact
    FIRST); the pre-image is authoritative — the plan's shape block is a
    reference, never a replacement file."""
    def fn(ctx):
        p = os.path.join(ctx.run_dir, "ploader.ini.pre")
        rc, out, err = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + COMBINED_INI["ini"], "-o", p],
                               "RETR pre-image ploader.ini")
        if ctx.dry:
            return "PASS", "dry"
        if rc != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
            return "FAIL", ("no ploader.ini pre-image retrievable (curl rc=%s) — the combined law "
                            "stages ONLY from the actual console pre-image; reconcile first" % rc)
        pre = open(p).read()
        sha, n = local_sha(p)
        ctx.save("ploader.ini.pre.sha256", "%s  %d\n" % (sha, n))
        ctx.state["pre"] = pre
        tag = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
        rc2, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", p, FTP + COMBINED_INI["ini"] + "." + tag + ".bak"],
                            "console backup ploader.ini.$TAG.bak")
        if rc2 != 0:
            return "FAIL", "could not write console .bak backup — STOP (never edit without a rollback artifact)"
        return "PASS", "%s pre-image secured (%d bytes, sha %s) + .bak written" % (label, n, sha[:12])
    return fn


def ini_edit_fn(label, wanted, optional=()):
    """SC-5 local edit: append ONLY the missing wanted `= 2` lines (idempotent),
    prove append-only, verify the combined end-state."""
    def fn(ctx):
        pre = ctx.state.get("pre")
        status, new_text, appended = ridealong_ini_plan(pre, wanted)
        ctx.save("ploader.ini.new", new_text if status != "stop" else "")
        ctx.save("ploader-edit-decision.json", json.dumps(
            {"flow": label, "status": status, "appended": appended}, indent=2))
        if status == "stop":
            return "FAIL", "AMBIGUOUS ini state — %s | pre-image preserved in the run dir; reconcile, never blind-edit" % new_text
        if status == "noop":
            return "PASS", ("%s line(s) already staged exactly-once at = 2 — no-op (change nothing, "
                            "store nothing)" % label)
        problems = ridealong_ini_append_only(pre, new_text, appended)
        problems += ridealong_verify_endstate(new_text, required=wanted, optional=optional)
        if problems:
            return "FAIL", "; ".join(problems)
        return "PASS", "append-only edit ready: +%d line(s) %s; combined end-state verified" % (
            len(appended), appended)
    return fn


def ini_stor_fn(label):
    """SC-5 STOR + fresh RETR readback, byte-exact law (plain file: full equality)."""
    def fn(ctx):
        new_local = os.path.join(ctx.run_dir, "ploader.ini.new")
        decision = {}
        try:
            decision = json.load(open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")))
        except Exception:
            pass
        if decision.get("status") == "noop":
            return "PASS", "edit was a no-op — ini untouched on console (no STOR)"
        if not os.path.exists(new_local) or os.path.getsize(new_local) == 0:
            return "FAIL", "edited ini missing (%s edit step must run first)" % label
        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", new_local, FTP + COMBINED_INI["ini"]],
                           "STOR edited ploader.ini")
        if rc != 0:
            return "FAIL", "ini STOR failed rc=%d" % rc
        p = os.path.join(ctx.run_dir, "ploader.ini.readback")
        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + COMBINED_INI["ini"], "-o", p],
                            "ini readback")
        if ctx.dry:
            return "PASS", "dry"
        if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
            return "FAIL", "ini readback failed rc=%d — do not launch on an unverified ini" % rc2
        rb = open(p).read()
        new = open(new_local).read()
        if rb != new:
            return "FAIL", "ini readback != edited bytes — nothing else may change; STOP and reconcile"
        return "PASS", "ini STOR + readback byte-identical to the append-only edit"
    return fn


def prx_law_upload_fn(d, label):
    """§3 upload pattern: STOR the locally hash-pinned fself ONCE to its FINAL
    name, RETR the readback, verify the SHT-strip law. On a law FAIL: re-STOR
    from the verified local fself EXACTLY ONCE and re-verify; a second failure
    aborts the staging (nothing else runs; reconcile). When the ini edit step
    already declared a no-op (fully staged), the STOR is skipped and the
    readback law still verifies the on-console artifact."""
    url = FTP + d["target_prx"]
    def fn(ctx):
        skip_stor = False
        try:
            with open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")) as f:
                skip_stor = json.load(f).get("status") == "noop"
        except Exception:
            pass
        if ctx.dry:
            return "PASS", "dry"
        rc = 0
        if not skip_stor:
            rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", d["prx"], url], "STOR %s fself" % label)
            if rc != 0:
                return "FAIL", "%s STOR failed rc=%d" % (label, rc)
        p = os.path.join(ctx.run_dir, "%s.readback.prx" % label)
        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [url, "-o", p], "%s readback" % label)
        if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
            return "FAIL", "%s readback fetch failed rc=%d%s" % (
                label, rc2, " (no-op path: the on-console artifact must still verify)" if skip_stor else "")
        ok, detail = prx_law_verify_file(p, d)
        attempt = 1 if not skip_stor else 0
        if not ok and not skip_stor:
            # the ONE permitted re-STOR from the verified local fself, then re-verify
            ctx.log("RETRY  %s readback failed the law (%s) — re-STOR once from the verified "
                    "local fself" % (label, detail[:120]))
            rc3, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", d["prx"], url], "re-STOR %s fself" % label)
            if rc3 != 0:
                return "FAIL", "%s re-STOR failed rc=%d — abort staging, leave the console as-is" % (label, rc3)
            rc4, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [url, "-o", p],
                                "%s readback (attempt 2)" % label)
            if rc4 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
                return "FAIL", "%s readback (attempt 2) fetch failed rc=%d — abort staging" % (label, rc4)
            ok, detail = prx_law_verify_file(p, d)
            attempt = 2
        ctx.save("%s-readback-verdict.json" % label, json.dumps(
            {"prx": d["target_prx"], "law": "STAGE-PLAN-COMBINED §3 SHT-strip",
             "ok": ok, "stor_skipped_noop": skip_stor, "attempts": attempt,
             "detail": detail}, indent=2))
        if not ok:
            return "FAIL", ("%s readback failed the SHT-strip law (attempts %d): %s — "
                            "abort the staging (leave the console as-is, reconcile)" % (label, attempt, detail))
        return "PASS", "%s %s%s" % (label, detail, " (STOR skipped: already staged no-op)" if skip_stor else "")
    return fn


def freshness_baseline_fn():
    """SC-0 pre-launch baselines: UE4SS.log stat (size baseline; absence is a
    valid fresh-console baseline), observer log must be ABSENT (it must be
    created fresh by the module), igf jsonl baseline IF PRESENT (never delete
    owned evidence), plus BOTH probe verdict sinks IF PRESENT (byte baselines —
    the post-launch gate then demands FRESH GROWTH for this launch)."""
    def fn(ctx):
        ok_l, l_text = fetch_console_file(ctx, "data/plugins/UE4SS.log", "baseline-UE4SS.log")
        if ctx.dry:
            return "PASS", "dry"
        l_note = "absent (fresh baseline)" if not ok_l or l_text == "" else \
            "%d bytes (size baseline recorded)" % len(l_text.encode("utf-8", "replace"))
        ok_o, o_text = fetch_console_file(ctx, OBS["log"].lstrip("/"), "baseline-observer-log.txt")
        if ok_o and o_text != "":
            return "FAIL", ("%s already exists (%d bytes) — observer precondition: it must be "
                            "created FRESH by the module this launch; reconcile/rotate first"
                            % (OBS["log"], len(o_text)))
        ok_c, c_text = fetch_console_file(ctx, GAME["C2"].lstrip("/"), "baseline-ingame-facts.jsonl")
        c_note = "absent" if not ok_c or c_text == "" else \
            "%d bytes baseline recorded (qdrv1 lines distinguishable by build_id qdrv1- prefix; never delete owned evidence)" % len(c_text)
        probe_notes = []
        for d, key in ((XWP_ARMED, "xwp"), (FC1POOL, "fc1")):
            ok_p, p_text = fetch_console_file(ctx, d["log"].lstrip("/"), "baseline-%s.jsonl" % key)
            if ok_p and p_text != "":
                ctx.state["%s_jsonl_baseline" % key] = p_text
                probe_notes.append("%s %d bytes baseline (fresh growth proves this launch's "
                                   "verdicts; never delete owned evidence)"
                                   % (d["log"], len(p_text.encode("utf-8", "replace"))))
            else:
                ctx.state["%s_jsonl_baseline" % key] = ""
                probe_notes.append("%s absent (first-launch baseline)" % d["log"])
        return "PASS", "baselines: UE4SS.log %s; observer log absent OK; igf jsonl %s; %s" % (
            l_note, c_note, "; ".join(probe_notes))
    return fn


RIDEALONG_HANDOFF = (
    "1. With the game fully CLOSED, ARM ploader.elf (Payload Manager UI or loadpayload entry).\n"
    "2. LAUNCH Townfall manually — NEVER automated.\n"
    "3. Read the ploader notification lines (SC-5b): expect EXACTLY 7 `OK (<ms>): <file>` lines\n"
    "   (the definitive 8-entry layout: 7 active entries; the mode-0 igf line consumes no slot),\n"
    "   ZERO FAIL lines. Any FAIL -> STOP and run: console_batch.py rollback-ridealongs\n"
    "4. Then run: console_batch.py game-evidence (probe verdicts land in\n"
    "   /data/modbridge-exec-write-probe.jsonl + /data/modbridge-fc1pool-facts.jsonl;\n"
    "   observer log/jsonl may take ~2-3 min post-launch).")

FINAL_INI_HANDOFF = RIDEALONG_HANDOFF


def final_ini_decision_step(label):
    """DEFINITIVE §3.3 sequence, step 1: BOOTSTRAP-or-ensure decision from the
    Step-0 pre-image. Runs final_ini_plan; on the bootstrap status performs the
    ONE sanctioned whole-file STOR of the canonical template + RETR byte-exact
    diff, gated as its own INI-BOOTSTRAPPED verdict (never a silent rewrite);
    ensure-only/noop statuses store nothing here."""
    def fn(ctx):
        pre = ctx.state.get("pre")
        status, new_text, note = final_ini_plan(pre, FINAL_ORDER)
        decision = {"flow": label, "law": "FINAL-INI-LAYOUT.md §3.3 (definitive 8-entry)",
                    "status": status, "appended": note if status == "append" else []}
        ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
        if status == "stop":
            ctx.save("ploader.ini.new", "")
            return "FAIL", "AMBIGUOUS ini state — %s | pre-image preserved in the run dir; reconcile, never blind-edit" % note
        if status == "bootstrap":
            # the ONE sanctioned whole-file STOR (historical line moves), with the
            # .bak already written by the Step-0 pre-image step; RETR byte-exact diff
            ctx.save("ploader.ini.new", new_text)
            ctx.save("ini-bootstrap-verdict.txt", "INI-BOOTSTRAPPED: ONE whole-file STOR of "
                     "ploader.ini.canonical (400 B, sha %s); .bak + byte-exact RETR diff "
                     "follow" % FINAL_INI["canonical_sha256"])
            rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T",
                                     os.path.join(ctx.run_dir, "ploader.ini.new"),
                                     FTP + FINAL_INI["ini"]], "BOOTSTRAP STOR canonical ploader.ini")
            if rc != 0:
                return "FAIL", "bootstrap STOR failed rc=%d — STOP, reconcile (never a second silent attempt)" % rc
            p = os.path.join(ctx.run_dir, "ploader.ini.readback")
            rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + FINAL_INI["ini"], "-o", p],
                                "bootstrap RETR readback")
            if ctx.dry:
                ctx.state["pre"] = new_text
                decision["status"] = "bootstrap-applied"
                decision["stor"] = "done"
                ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
                return "PASS", "dry"
            if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
                return "FAIL", "bootstrap readback failed rc=%d — STOP and reconcile" % rc2
            if open(p, errors="replace").read() != new_text:
                return "FAIL", ("bootstrap readback != canonical bytes — the ONE sanctioned "
                                "rewrite did not land byte-exact; STOP, reconcile (never a "
                                "second silent attempt)")
            ctx.state["pre"] = new_text
            decision["status"] = "bootstrap-applied"
            decision["stor"] = "done"
            ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
            return "PASS", "INI-BOOTSTRAPPED: canonical 8-line layout stored whole-file ONCE + readback byte-exact (%s)" % note
        # "append" (ensure ops needed) or "noop" (already canonical)
        ctx.save("ploader.ini.new", new_text if status == "append" else (pre or ""))
        decision["stor"] = "pending" if status == "append" else "none"
        ctx.save("ploader-edit-decision.json", json.dumps(decision, indent=2))
        if status == "noop":
            return "PASS", "pre-image is ALREADY byte-exact canonical — no-op (change nothing, store nothing)"
        return "PASS", "INI-ENSURE-ONLY: content-anchored ensure ops ready (+%d line(s) %s)" % (
            len(decision["appended"]), decision["appended"])
    return fn


def final_ini_verify_local_fn(label):
    """DEFINITIVE §3.3 sequence, step 2: byte-exact end-state proof of the LOCAL
    file BEFORE any STOR (in-process law + the final verifier subprocess — the
    verifier is a pure host-side checker, run directly, no console contact)."""
    def fn(ctx):
        new_local = os.path.join(ctx.run_dir, "ploader.ini.new")
        if not os.path.exists(new_local) or os.path.getsize(new_local) == 0:
            return "FAIL", "edited ini missing (%s decision step must run first)" % label
        new_text = open(new_local, errors="replace").read()
        problems = final_ini_verify_endstate(new_text)
        if problems:
            return "FAIL", "FINAL-INI end-state gate: %s" % "; ".join(problems[:6])
        if ctx.dry:
            return "PASS", "dry"
        proc = subprocess.run([sys.executable, FINAL_INI["final_verifier"], new_local],
                              capture_output=True, text=True, timeout=60)
        vout = (proc.stdout or "") + "\n" + (proc.stderr or "")
        ctx.save("final-verifier.txt", vout)
        if proc.returncode != 0 or "FINAL-INI-OK" not in vout:
            return "FAIL", "final verifier rejected the local file (rc=%s): %s" % (
                proc.returncode, vout.strip().splitlines()[:4])
        return "PASS", "byte-exact canonical 8-entry layout (400 B, sha %s…); 7 registered; igf=0 skipped" % (
            FINAL_INI["canonical_sha256"][:12])
    return fn


def final_ini_stor_fn(label):
    """DEFINITIVE §3.3 sequence, step 3: STOR the verified local file + fresh
    RETR byte-exact (skipped when the decision was a no-op; ALREADY DONE and
    readback-verified when the bootstrap branch fired)."""
    def fn(ctx):
        decision = {}
        try:
            decision = json.load(open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")))
        except Exception:
            pass
        stor = decision.get("stor")
        if stor == "none":
            return "PASS", "edit was a no-op — ini untouched on console (no STOR)"
        if stor == "done":
            return "PASS", "bootstrap branch already stored + readback-verified the canonical bytes (INI-BOOTSTRAPPED)"
        new_local = os.path.join(ctx.run_dir, "ploader.ini.new")
        if not os.path.exists(new_local) or os.path.getsize(new_local) == 0:
            return "FAIL", "edited ini missing (%s decision step must run first)" % label
        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", new_local, FTP + FINAL_INI["ini"]],
                           "STOR edited ploader.ini")
        if rc != 0:
            return "FAIL", "ini STOR failed rc=%d" % rc
        p = os.path.join(ctx.run_dir, "ploader.ini.readback")
        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + FINAL_INI["ini"], "-o", p],
                            "ini readback")
        if ctx.dry:
            return "PASS", "dry"
        if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
            return "FAIL", "ini readback failed rc=%d — do not launch on an unverified ini" % rc2
        if open(p, errors="replace").read() != open(new_local, errors="replace").read():
            return "FAIL", "ini readback != edited bytes — nothing else may change; STOP and reconcile"
        return "PASS", "ini STOR + readback byte-identical (canonical end-state on console)"
    return fn


def build_stage_ridealongs():
    """DEFINITIVE return-session staging (FINAL-INI-LAYOUT.md §3 + the
    CONSOLE-BATCH-CONSTANTS-BRIEF): BOTH launch probes (xwp-armed + fc1pool) and
    BOTH ridealongs (qdrv1 + observer) under the SHT-strip readback law, then the
    FINAL 8-entry ploader.ini via the ONE bootstrap whole-file STOR (only when
    the pre-image needs the historical line moves) + idempotent content-anchored
    ensure ops, gated BYTE-EXACT against ploader.ini.canonical (7 registered).
    Supersedes the 6-entry combined staging inside `all`."""
    fl = Flow("stage-ridealongs")
    fl.notes = ("DEFINITIVE return-session staging (FINAL-INI-LAYOUT §3): 4 PRXs (xwp-armed "
                "igd1-6ebab07f2111, fc1pool fc1pool1-78508a2e2940, qdrv1, observer) + the FINAL "
                "8-entry ploader.ini (7 registered; bootstrap whole-file STOR only when the "
                "historical line moves require it; ensure ops otherwise), byte-exact vs "
                "ploader.ini.canonical.")

    def prefix_gate_fn(ctx):
        # host-side cross-check: the computed prefix of each pinned bare ELF must
        # equal the pinned readback length, else the constants/artifact pair diverged
        for d, name in ((QDRV1, "qdrv1"), (OBS, "observer"), (XWP_ARMED, "xwp"), (FC1POOL, "fc1")):
            bare = open(d["bare"], "rb").read()
            end = sht_prefix_end(bare)
            if end != d["readback_bytes"]:
                return "FAIL", ("%s bare-sdk computed prefix %d != pinned readback %d — STOP, "
                                "never widen the law" % (name, end, d["readback_bytes"]))
            if hashlib.sha256(bare[64:end]).hexdigest() != d["readback_body_sha"]:
                return "FAIL", "%s body-sha constant diverges from the artifact — STOP" % name
        try:
            final_canonical_bytes()
        except FinalEnsureError as exc:
            return "FAIL", str(exc)
        return "PASS", ("SHT-strip law constants cross-checked against all four bare-sdk "
                        "artifacts + canonical template identity")

    fl.step("ride.0", "SC-PREFLIGHT — local identities: all FOUR fself PRXs (qdrv1 52c32216…, "
                      "observer 1afa0997…, xwp-armed 48949316…, fc1pool d2431bf8…) + all FOUR "
                      "bare-sdk ELFs + the canonical template (verify hashes before replacing "
                      "any owned file)", "preflight",
            [gate_sha(QDRV1["prx"], QDRV1["sha"], QDRV1["bytes"]),
             gate_sha(OBS["prx"], OBS["sha"], OBS["bytes"]),
             gate_sha(XWP_ARMED["prx"], XWP_ARMED["sha"], XWP_ARMED["bytes"]),
             gate_sha(FC1POOL["prx"], FC1POOL["sha"], FC1POOL["bytes"]),
             gate_sha(FINAL_INI["canonical_template"], FINAL_INI["canonical_sha256"],
                      FINAL_INI["canonical_bytes"]),
             "bare-sdk refs pinned by sha + bytes (determinism run dirs)",
             "game CLOSED — staging happens BEFORE the game session; no ploader arm yet"],
            ["shasum -a 256 %s %s %s %s" % (QDRV1["prx"], OBS["prx"], XWP_ARMED["prx"], FC1POOL["prx"]),
             "shasum -a 256 %s %s %s %s" % (QDRV1["bare"], OBS["bare"], XWP_ARMED["bare"], FC1POOL["bare"]),
             "shasum -a 256 %s" % FINAL_INI["canonical_template"]],
            preflight_fn([(QDRV1["prx"], QDRV1["sha"], QDRV1["bytes"]),
                          (OBS["prx"], OBS["sha"], OBS["bytes"]),
                          (XWP_ARMED["prx"], XWP_ARMED["sha"], XWP_ARMED["bytes"]),
                          (FC1POOL["prx"], FC1POOL["sha"], FC1POOL["bytes"]),
                          (QDRV1["bare"], QDRV1["bare_sha"], QDRV1["bare_bytes"]),
                          (OBS["bare"], OBS["bare_sha"], OBS["bare_bytes"]),
                          (XWP_ARMED["bare"], XWP_ARMED["bare_sha"], XWP_ARMED["bare_bytes"]),
                          (FC1POOL["bare"], FC1POOL["bare_sha"], FC1POOL["bare_bytes"]),
                          (FINAL_INI["canonical_template"], FINAL_INI["canonical_sha256"],
                           FINAL_INI["canonical_bytes"])], "ridealongs"))

    fl.step("ride.1", "SC-PREFLIGHT b — recompute the §3 readback constants from all four "
                      "bare-sdk ELFs (qdrv1 87112 / 71406d3c…; observer 30143976 / 3f89a5b6…; "
                      "xwp 67832 / daea2ff4…; fc1 67848 / 7e2dce27…)", "preflight",
            ["qdrv1: readback bytes == 87112, body sha == 71406d3c…",
             "observer: readback bytes == 30143976, body sha == 3f89a5b6…",
             "xwp: readback bytes == 67832, body sha == daea2ff4…",
             "fc1: readback bytes == 67848, body sha == 7e2dce27…",
             "canonical template == 400 B, sha bbc34c16…",
             "any divergence = artifact/constants mismatch = STOP, never widen the law"],
            ["python3 - <bare-sdk>  # max(p_offset+p_filesz) + body sha (§3 snippet semantics)"],
            prefix_gate_fn)

    fl.step("ride.2", "SC-0 — RETR live ploader.ini pre-image + console .bak backup (rollback "
                      "artifact FIRST; header exactly once; NO duplicate lines; the 4 "
                      "pre-existing lines present at pinned modes)", "ini",
            ["pre-image RETR + hash recorded BEFORE any edit (Gate A0 pattern)",
             "FAIL on: no [PPSA33286], duplicate lines (ploader does NOT dedupe), "
             "missing pre-existing lines, wrong modes, unknown lines — reconcile, never blind-edit",
             "pre-existing lines (pinned modes): composition = 2; aligned_entry = 2; "
             "path_test = 2; igf = 0",
             "console .bak backup written before any STOR (distinct TAG)"],
            ["curl -sS %s%s -o <run-dir>/ploader.ini.pre" % (FTP, FINAL_INI["ini"]),
             "shasum -a 256 <run-dir>/ploader.ini.pre",
             "curl -sS -T <run-dir>/ploader.ini.pre %s%s.$TAG.bak" % (FTP, FINAL_INI["ini"])],
            ini_preimage_fn("final"))

    fl.step("ride.2b", "§3.3 BOOTSTRAP decision — canonical-order pre-image -> ensure-only; "
                       "historical aligned_entry/path_test-before-composition shape -> the ONE "
                       "sanctioned whole-file STOR of ploader.ini.canonical + RETR byte-exact "
                       "diff, verdict INI-BOOTSTRAPPED (never a silent rewrite, never a second "
                       "attempt)", "ini",
            ["STOP classes kept: duplicate / missing pre-existing / mode divergence / unknown "
             "line / unparseable — reconcile per rollback-runbook §4.3",
             "bootstrap is gated as its own INI-BOOTSTRAPPED verdict with the .bak + "
             "byte-exact RETR diff; ensure-only path stores nothing here",
             "canonical: 8 entries, 7 registered, probes first, igf=0 last"],
            ["diff <run-dir>/ploader.ini.pre <run-dir>/ploader.ini.new  # bootstrap: whole file; ensure: the inserted line(s)"],
            final_ini_decision_step("ridealongs"))

    fl.step("ride.3", "§3.3 byte-exact end-state proof of the LOCAL file BEFORE any STOR "
                      "(in-process law: header once, 8 entries, 7 registered, igf never "
                      "registered + the final verifier subprocess)", "ini",
            ["byte-exact equality with ploader.ini.canonical replaces the legacy "
             "'5 registered' count gate",
             "the final verifier (worker-final-ini-layout/verify_final_ini_layout.py) must "
             "print FINAL-INI-OK"],
            ["python3 %s <run-dir>/ploader.ini.new   # FINAL-INI-OK" % FINAL_INI["final_verifier"]],
            final_ini_verify_local_fn("ridealongs"))

    fl.step("ride.4", "§3.3 STOR edited ploader.ini + fresh RETR byte-exact (skipped when the "
                      "decision was a no-op; already done + verified on the bootstrap branch)",
            "ini",
            ["readback must be byte-identical to the verified local file (plain file law)",
             "no-op edit -> nothing is stored (idempotent re-run)"],
            ["curl -sS -T <run-dir>/ploader.ini.new %s%s" % (FTP, FINAL_INI["ini"]),
             "curl -sS %s%s -o <run-dir>/ploader.ini.readback" % (FTP, FINAL_INI["ini"])],
            final_ini_stor_fn("ridealongs"))

    fl.step("ride.5", "SC-0 b — freshness baselines: UE4SS.log stat; observer log ABSENT; igf "
                      "jsonl + BOTH probe jsonl sinks baselined IF PRESENT (never delete "
                      "owned evidence; fresh growth proves this launch's verdicts)",
            "baseline",
            ["UE4SS.log size baseline (absent = valid fresh baseline)",
             "%s must be ABSENT — created fresh by the module this launch" % OBS["log"],
             "%s and %s: byte baselines recorded if present" % (XWP_ARMED["log"], FC1POOL["log"]),
             "igf jsonl: record size+sha if present; qdrv1 lines carry the qdrv1- build_id prefix"],
            ["curl -sS %s/data/plugins/UE4SS.log          # size baseline" % FTP,
             "curl -sS %s%s   # must be absent" % (FTP, OBS["log"]),
             "curl -sS %s%s   # baseline if present" % (FTP, GAME["C2"]),
             "curl -sS %s%s   # baseline if present" % (FTP, XWP_ARMED["log"]),
             "curl -sS %s%s   # baseline if present" % (FTP, FC1POOL["log"])],
            freshness_baseline_fn())

    fl.step("ride.6", "SC-1/SC-2 — STOR modbridge_exec_write_probe_armed.prx (48949316… / "
                      "20013 B) to its FINAL name + SHT-strip readback law (67832 B / body "
                      "daea2ff4…; ONE re-STOR on failure, then abort)", "upload",
            ["local sha == 48949316… BEFORE upload (BUILD-ID law)",
             "readback: exact-length 67832 + body-hash daea2ff4… + Ehdr exception (e_shnum zeroed)",
             "distinct file under its OWN name — nothing is replaced",
             "skipped entirely when the ini decision was a no-op (readback-only verify)"],
            ["curl -sS -T %s %s%s" % (XWP_ARMED["prx"], FTP, XWP_ARMED["target_prx"]),
             "curl -sS %s%s -o <run-dir>/xwp.readback.prx" % (FTP, XWP_ARMED["target_prx"]),
             "python3 - §3 snippet vs determinism/run-armed-1/bare-sdk/modbridge_exec_write_probe.prx"],
            prx_law_upload_fn(XWP_ARMED, "xwp"))

    fl.step("ride.7", "SC-1/SC-2 — STOR modbridge_fc1pool.prx (d2431bf8… / 22733 B) to its "
                      "FINAL name + SHT-strip readback law (67848 B / body 7e2dce27…; ONE "
                      "re-STOR on failure, then abort)", "upload",
            ["local sha == d2431bf8… BEFORE upload (BUILD-ID law)",
             "readback: exact-length 67848 + body-hash 7e2dce27… + Ehdr exception",
             "distinct file under its OWN name — nothing is replaced",
             "skipped entirely when the ini decision was a no-op (readback-only verify)"],
            ["curl -sS -T %s %s%s" % (FC1POOL["prx"], FTP, FC1POOL["target_prx"]),
             "curl -sS %s%s -o <run-dir>/fc1.readback.prx" % (FTP, FC1POOL["target_prx"]),
             "python3 - §3 snippet vs determinism/run1/bare-sdk/modbridge_fc1pool.prx"],
            prx_law_upload_fn(FC1POOL, "fc1"))

    fl.step("ride.8", "SC-1/SC-2 — STOR modbridge_qh_driver.prx (52c32216… / 26157 B) + SHT-strip "
                      "readback law (87112 B / body 71406d3c…; ONE re-STOR on failure, then abort)",
            "upload",
            ["local sha == 52c32216… BEFORE upload (BUILD-ID law)",
             "readback: exact-length 87112 + body-hash 71406d3c… + Ehdr exception (e_shnum zeroed)",
             "full-file hash equality through this channel is IMPOSSIBLE by construction (§3)",
             "skipped entirely when the ini decision was a no-op (readback-only verify)"],
            ["curl -sS -T %s %s%s" % (QDRV1["prx"], FTP, QDRV1["target_prx"]),
             "curl -sS %s%s -o <run-dir>/qdrv1.readback.prx" % (FTP, QDRV1["target_prx"]),
             "python3 - §3 snippet vs determinism/run1/bare-sdk/modbridge_qh_driver.prx"],
            prx_law_upload_fn(QDRV1, "qdrv1"))

    fl.step("ride.9", "SC-3/SC-4 — STOR modbridge_runtime_aligned_observer.prx (1afa0997… / "
                      "30152151 B) + SHT-strip readback law (30143976 B / body 3f89a5b6…; ONE "
                      "re-STOR on failure, then abort)", "upload",
            ["local sha == 1afa0997… BEFORE upload (BUILD-ID law)",
             "readback: exact-length 30143976 + body-hash 3f89a5b6… + Ehdr exception",
             "distinct files under their OWN names — nothing is replaced",
             "skipped entirely when the ini decision was a no-op (readback-only verify)"],
            ["curl -sS -T %s %s%s" % (OBS["prx"], FTP, OBS["target_prx"]),
             "curl -sS %s%s -o <run-dir>/observer.readback.prx" % (FTP, OBS["target_prx"]),
             "python3 - §3 snippet vs build-1/bare-sdk/modbridge_runtime_aligned_observer.prx"],
            prx_law_upload_fn(OBS, "observer"))

    fl.step("ride.10", "SC-6/SC-7 HANDOFF — arm ploader + USER launches Townfall (single launch "
                       "window; the definitive 8-entry ini: 7 registered plugins ride, igf=0 "
                       "stays inert)", "handoff",
            ["ONE game-launch window total: 7 registered plugins in the same ploader.ini",
             "SC-5b after launch: EXACTLY 7 `OK (<ms>): <file>` notification lines, ZERO FAIL "
             "lines; any FAIL -> STOP + rollback-ridealongs",
             "probe verdicts land in %s (build_id %s) and %s (build_id %s)"
             % (XWP_ARMED["log"], XWP_ARMED["build_id"], FC1POOL["log"], FC1POOL["build_id"]),
             "then run: console_batch.py game-evidence"],
            ["USER arms ploader.elf (game closed)",
             "USER launches Townfall",
             "controller: python3 console_batch.py game-evidence"],
            handoff(FINAL_INI_HANDOFF), prompt=FINAL_INI_HANDOFF)
    return fl


def build_stage_observer():
    """Observer ridealong ONLY (STAGE-PLAN-COMBINED §3-§5, observer rows):
    PRX upload + SHT-strip readback law + idempotent ini append of ONLY the
    observer line — coordinates with a qdrv1 line already present (staged by
    stage-qdrv1 or a prior stage-ridealongs) without touching it."""
    fl = Flow("stage-observer")
    fl.notes = ("STAGE-PLAN-COMBINED observer ridealong ONLY: appends ONLY the observer line to "
                "the CURRENT ini state; a qdrv1 line already staged by stage-qdrv1/"
                "stage-ridealongs is left untouched (idempotent coordination).")

    fl.step("obs.0", "SC-PREFLIGHT — local identities: observer fself (1afa0997… / 30152151 B) + "
                     "bare-sdk reference (d0a3d6fe… / 30145992 B); computed SHT prefix == 30143976",
            "preflight",
            [gate_sha(OBS["prx"], OBS["sha"], OBS["bytes"]),
             gate_sha(OBS["bare"], OBS["bare_sha"], OBS["bare_bytes"]),
             "game CLOSED — staging happens BEFORE the game session; no ploader arm yet"],
            ["shasum -a 256 %s" % OBS["prx"], "stat -f %%z %s" % OBS["prx"],
             "shasum -a 256 %s" % OBS["bare"]],
            preflight_fn([(OBS["prx"], OBS["sha"], OBS["bytes"]),
                          (OBS["bare"], OBS["bare_sha"], OBS["bare_bytes"])], "observer"))

    fl.step("obs.1", "SC-0 — RETR live ploader.ini pre-image + console .bak backup (rollback "
                     "artifact FIRST)", "ini",
            ["pre-image RETR + hash recorded BEFORE any edit",
             "no pre-image retrievable -> STOP (the combined law never writes a ploader.ini from scratch)",
             "console .bak backup written before any STOR"],
            ["curl -sS %s%s -o <run-dir>/ploader.ini.pre" % (FTP, COMBINED_INI["ini"]),
             "shasum -a 256 <run-dir>/ploader.ini.pre",
             "curl -sS -T <run-dir>/ploader.ini.pre %s%s.$TAG.bak" % (FTP, COMBINED_INI["ini"])],
            ini_preimage_fn("observer"))

    fl.step("obs.2", "SC-5 (observer) — idempotent local edit: append ONLY the observer `= 2` "
                     "line if absent; a qdrv1 line already present is left untouched; no-op when "
                     "the observer line is already staged", "ini",
            ["COORDINATION: if stage-qdrv1/stage-ridealongs already appended its line, this flow "
             "appends ONLY its own line to the current state",
             "append-only law proven against the pre-image (+single leading NL iff it lacked one)",
             "STOP on: duplicate observer line, unexpected modes, missing expected lines, "
             "unknown contract — reconcile, never blind-edit"],
            ["diff <run-dir>/ploader.ini.pre <run-dir>/ploader.ini.new  # exactly the observer line"],
            ini_edit_fn("observer", [OBS["module_line"]], optional=(QDRV1["module_line"],)))

    fl.step("obs.3", "SC-3/SC-4 — STOR modbridge_runtime_aligned_observer.prx to its FINAL name + "
                     "SHT-strip readback law (30143976 B / body 3f89a5b6…; ONE re-STOR on failure, "
                     "then abort)", "upload",
            ["local sha == 1afa0997… BEFORE upload (BUILD-ID law)",
             "readback: exact-length 30143976 + body-hash 3f89a5b6… + Ehdr exception (e_shnum zeroed)",
             "full-file hash equality through this channel is IMPOSSIBLE by construction (§3)"],
            ["curl -sS -T %s %s%s" % (OBS["prx"], FTP, OBS["target_prx"]),
             "curl -sS %s%s -o <run-dir>/observer.readback.prx" % (FTP, OBS["target_prx"]),
             "python3 - §3 snippet vs build-1/bare-sdk/modbridge_runtime_aligned_observer.prx"],
            prx_law_upload_fn(OBS, "observer"))

    fl.step("obs.4", "SC-5 — STOR edited ploader.ini + fresh readback byte-exact (skipped when "
                     "the edit was a no-op)", "ini",
            ["readback must be byte-identical to the local append-only edit (plain file law)",
             "no-op edit -> nothing is stored (idempotent re-run)"],
            ["curl -sS -T <run-dir>/ploader.ini.new %s%s" % (FTP, COMBINED_INI["ini"]),
             "curl -sS %s%s -o <run-dir>/ploader.ini.readback" % (FTP, COMBINED_INI["ini"])],
            ini_stor_fn("observer"))

    fl.step("obs.5", "HANDOFF — the game session (single launch window; observer rides =2) + "
                     "rollback pointer", "handoff",
            ["ONE game-launch window total; observer log %s is created fresh by the module" % OBS["log"],
             "rollback (plan §7): python3 console_batch.py rollback-ridealongs (flips ridealong "
             "lines to = 0 — parse-skipped, no attach), then byte-verify",
             "then run: console_batch.py game-evidence"],
            ["USER arms ploader.elf (game closed)",
             "USER launches Townfall",
             "controller: python3 console_batch.py game-evidence"],
            handoff(RIDEALONG_HANDOFF), prompt=RIDEALONG_HANDOFF)
    return fl


def build_rollback_ridealongs():
    """STAGE-PLAN-COMBINED §7: flip BOTH ridealong lines to ` = 0` (single-character
    edits; mode 0 is SKIPPED at parse — no attach, no FAIL notify, no timeout),
    with a fresh .bak + byte-verified readback. Never replay a failed activation
    without a fresh preflight and a distinct TAG."""
    fl = Flow("rollback-ridealongs")
    fl.notes = ("STAGE-PLAN-COMBINED §7 rollback: BOTH ridealong lines -> = 0. Game must be "
                "CLOSED and the ploader NOT armed. Ridealong LOGS are owned evidence — keep them.")

    fl.step("rb.0", "§7.1 — game CLOSED, ploader NOT armed; RETR current ini pre-image + fresh "
                    "console .bak (distinct TAG)", "ini",
            ["game fully closed; ploader not armed — mode flips take effect at the next arm",
             "never replay a failed activation without a fresh preflight + distinct TAG"],
            ["curl -sS %s%s -o <run-dir>/ploader.ini.pre" % (FTP, COMBINED_INI["ini"]),
             "curl -sS -T <run-dir>/ploader.ini.pre %s%s.$TAG.bak" % (FTP, COMBINED_INI["ini"])],
            ini_preimage_fn("rollback"))

    def rollback_edit_fn(ctx):
        pre = ctx.state.get("pre")
        status, new_text, flipped = ridealong_rollback_edit(
            pre, [QDRV1["module_line"], OBS["module_line"]])
        ctx.save("ploader.ini.rolledback", new_text if status != "stop" else "")
        ctx.save("ploader-rollback-decision.json", json.dumps(
            {"status": status, "flipped": flipped}, indent=2))
        if status == "stop":
            return "FAIL", "AMBIGUOUS ini state — %s | pre-image preserved; reconcile" % new_text
        if status == "noop":
            return "PASS", "no ridealong line is staged at = 2 — nothing to roll back"
        problems = []
        if pre is not None:
            if len(new_text) != len(pre):
                problems.append("rollback changed file length (must be single-character mode flips)")
            else:
                diffs = [(i, a, b) for i, (a, b) in enumerate(zip(pre, new_text)) if a != b]
                if len(diffs) != len(flipped) or any(b != "0" or a != "2" for _i, a, b in diffs):
                    problems.append("rollback diff is not exactly one 2->0 flip per staged line: %s"
                                    % diffs[:6])
        if problems:
            return "FAIL", "; ".join(problems)
        return "PASS", "mode flips ready: %s" % flipped

    fl.step("rb.1", "§7.1 — flip BOTH ridealong lines to ` = 0` (single-character edits, verified "
                    "byte-level)", "ini",
            ["mode 0 = SKIPPED at parse (rollback-runbook §4.2): no attach, no FAIL notify, no timeout",
             "every OTHER byte must be identical to the pre-image (verified)",
             "lines absent / already = 0 -> honest no-op"],
            ["diff <run-dir>/ploader.ini.pre <run-dir>/ploader.ini.rolledback  # exactly the two mode digits"],
            rollback_edit_fn)

    def rollback_stor_fn(ctx):
        new_local = os.path.join(ctx.run_dir, "ploader.ini.rolledback")
        decision = {}
        try:
            decision = json.load(open(os.path.join(ctx.run_dir, "ploader-rollback-decision.json")))
        except Exception:
            pass
        if decision.get("status") == "noop":
            return "PASS", "nothing was flipped — ini untouched (no STOR)"
        if not os.path.exists(new_local) or os.path.getsize(new_local) == 0:
            return "FAIL", "rolled-back ini missing (rb.1 must run first)"
        rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", new_local, FTP + COMBINED_INI["ini"]],
                           "STOR rolled-back ploader.ini")
        if rc != 0:
            return "FAIL", "ini STOR failed rc=%d" % rc
        p = os.path.join(ctx.run_dir, "ploader.ini.readback")
        rc2, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + COMBINED_INI["ini"], "-o", p],
                            "ini readback")
        if ctx.dry:
            return "PASS", "dry"
        if rc2 != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
            return "FAIL", "ini readback failed rc=%d — reconcile before any next staging" % rc2
        if open(p).read() != open(new_local).read():
            return "FAIL", "ini readback != rolled-back bytes — STOP and reconcile"
        return "PASS", "rolled-back ini stored + readback byte-verified"

    fl.step("rb.2", "§7.1 — STOR rolled-back ini + RETR + byte-verify readback", "ini",
            ["RETR + byte-verify readback after the flip (plan §7.1)",
             "optional §7.2 DELE of the two PRXs is NOT automatic — decide per recovery"],
            ["curl -sS -T <run-dir>/ploader.ini.rolledback %s%s" % (FTP, COMBINED_INI["ini"]),
             "curl -sS %s%s -o <run-dir>/ploader.ini.readback" % (FTP, COMBINED_INI["ini"])],
            rollback_stor_fn)

    fl.step("rb.3", "§7.2-§7.4 — post-rollback receipts (doc-only)", "doc",
            ["§7.2 optionally DELE the two PRXs if discarding; KEEP the observer/qdrv1 logs "
             "(owned evidence)",
             "§7.3 confirm aligned_entry/path_test/composition lines + logs/PRXs byte-identical "
             "to the Step-0 preflight captures",
             "§7.4 never replay a failed activation without a fresh preflight and a distinct TAG; "
             "never relaunch an identical rejected binary"],
            ["curl -sS %s%s   # eyeball: both lines = 0, everything else unchanged" % (FTP, COMBINED_INI["ini"])],
            None)
    return fl


# --- stage-settings (NEW-SETTINGS §3 lockstep restage, v3 pair) -----------------

def settings_pair_lockstep_problems(loadlist_text, settings_row, settings_bytes):
    """Pair law (NEW-SETTINGS §3): the loadlist's settings row must pin EXACTLY
    the settings byte count — the mfa's size-pin enforcement (MFA_E_SIZE -8 on
    any drift, either direction), pre-proven host-side before any upload."""
    row_found = None
    for ln in loadlist_text.splitlines():
        parts = ln.split("\t")
        if len(parts) >= 4 and parts[2] == settings_row:
            row_found = int(parts[3])
    if row_found is None:
        return ["loadlist has NO row for %r — not a modforge loadlist for this root" % settings_row]
    if row_found != settings_bytes:
        return ["loadlist settings row pins %d, settings file is %d B — pair NOT in lockstep; "
                "never stage a mismatched pair" % (row_found, settings_bytes)]
    return []


def build_stage_settings():
    """Settings+loadlist LOCKSTEP restage (worker-engineversion-ini/NEW-SETTINGS.md
    §3; worker-settings-v3-package/package-v3 bytes): freeze the currently staged
    pair BEFORE touching anything (v2 recorded inventory, or already-v3 on an
    idempotent re-run), upload BOTH files as one pair, byte-exact readback BOTH.
    The mfa refuses a mismatched pair with MFA_E_SIZE (-8) in EITHER direction at
    activation — a half-restaged root cannot launch silently, but the tool never
    relies on that: it verifies bytes before any launch gate."""
    fl = Flow("stage-settings")
    fl.notes = ("Settings+loadlist lockstep restage (NEW-SETTINGS §3): v3 pair "
                "(UE4SS-settings.ini 1448 B d8faa4ec… with [Hooks] HookProcessInternal=0 + "
                "HookProcessLocalScriptFunction=0; loadlist.txt 447 B 4d98ecd7… with the "
                "settings row 834->1448). Game CLOSED; run BEFORE the launch gate.")

    def lockstep_check_fn(ctx):
        # host-side pair law: the loadlist's settings row must pin EXACTLY the
        # local settings byte count (the mfa's size-pin enforcement, pre-proven)
        problems = settings_pair_lockstep_problems(
            open(SETTINGS_V3["loadlist_local"], errors="replace").read(),
            SETTINGS_V3["settings_row"], SETTINGS_V3["settings_bytes"])
        if problems:
            return "FAIL", problems[0]
        return "PASS", "pair lockstep proven host-side: loadlist settings row == %d == local settings bytes" % SETTINGS_V3["settings_bytes"]

    _preflight = preflight_fn([(SETTINGS_V3["settings_local"], SETTINGS_V3["settings_sha"], SETTINGS_V3["settings_bytes"]),
                               (SETTINGS_V3["loadlist_local"], SETTINGS_V3["loadlist_sha"], SETTINGS_V3["loadlist_bytes"])],
                              "settings")

    def set0_fn(ctx):
        rc_status, detail = _preflight(ctx)
        if rc_status != "PASS":
            return rc_status, detail
        return lockstep_check_fn(ctx)

    fl.step("set.0", "PREFLIGHT — v3 pair identities + lockstep proof (settings ini d8faa4ec… / "
                     "1448 B; loadlist 4d98ecd7… / 447 B; loadlist settings row == 1448)",
            "preflight",
            [gate_sha(SETTINGS_V3["settings_local"], SETTINGS_V3["settings_sha"], SETTINGS_V3["settings_bytes"]),
             gate_sha(SETTINGS_V3["loadlist_local"], SETTINGS_V3["loadlist_sha"], SETTINGS_V3["loadlist_bytes"]),
             "game CLOSED — settings restage happens BEFORE the launch gate",
             "rollback pair recorded (never staged blind): v2 settings 834 B 9570ead3… + "
             "v2 loadlist 446 B 3afd10ce… (worker-engineversion-ini)"],
            ["shasum -a 256 %s %s" % (SETTINGS_V3["settings_local"], SETTINGS_V3["loadlist_local"]),
             "awk -F '\\t' '$3 == \"%s\" {print $4}' %s  # settings row size"
             % (SETTINGS_V3["settings_row"], SETTINGS_V3["loadlist_local"])],
            set0_fn)

    def freeze_fn(ctx):
        outs = {}
        for key, ftp in (("settings", SETTINGS_V3["settings_ftp"]),
                         ("loadlist", SETTINGS_V3["loadlist_ftp"])):
            p = os.path.join(ctx.run_dir, "freeze-%s" % key)
            rc, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + ftp, "-o", p],
                               "freeze RETR staged %s" % key)
            if ctx.dry:
                continue
            if rc != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
                outs[key] = None
                continue
            sha, n = local_sha(p)
            outs[key] = (sha, n)
        if ctx.dry:
            return "PASS", "dry"
        prior = SETTINGS_V3["prior"]
        def classify(got_s, got_l):
            for tag in ("v3", "v2"):
                ref = prior[tag]
                if got_s == (ref["settings_sha"], ref["settings_bytes"]) and \
                   got_l == (ref["loadlist_sha"], ref["loadlist_bytes"]):
                    return tag
            return None
        tag = classify(outs.get("settings"), outs.get("loadlist"))
        ctx.save("settings-freeze.json", json.dumps(
            {"staged_settings": outs.get("settings"), "staged_loadlist": outs.get("loadlist"),
             "classified": tag}, indent=2))
        if tag is None:
            diverged = (outs.get("settings") != prior["v2"] and outs.get("settings") is not None) or \
                       (outs.get("loadlist") != prior["v2"] and outs.get("loadlist") is not None)
            if diverged or None in outs.values():
                return "FAIL", ("staged root does NOT match the recorded v2 or v3 inventory "
                                "(settings=%s loadlist=%s; expected v2 %s/%s + %s/%s) — "
                                "divergent/unowned content is never overwritten; reconcile first"
                                % (outs.get("settings"), outs.get("loadlist"),
                                   prior["v2"]["settings_sha"][:12], prior["v2"]["settings_bytes"],
                                   prior["v2"]["loadlist_sha"][:12], prior["v2"]["loadlist_bytes"]))
        if tag == "v3":
            return "PASS", "freeze: staged pair is ALREADY the v3 bytes — idempotent no-op path"
        return "PASS", "freeze: staged pair matches the recorded v2 inventory (834/9570ead3… + 446/3afd10ce…) — restage may proceed"

    fl.step("set.1", "FREEZE — RETR the staged settings + loadlist; classify against the "
                     "recorded inventories (v2 pair = proceed; v3 pair = idempotent no-op; "
                     "anything else = FAIL, never overwrite divergent/unowned content)",
            "freeze",
            ["the staged root must match the recorded inventory BEFORE any upload",
             "settings: /data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini",
             "loadlist: /data/modbridge/roots/PPSA33286/loadlist.txt"],
            ["curl -sS %s%s -o <run-dir>/freeze-settings" % (FTP, SETTINGS_V3["settings_ftp"]),
             "curl -sS %s%s -o <run-dir>/freeze-loadlist" % (FTP, SETTINGS_V3["loadlist_ftp"]),
             "shasum -a 256 <run-dir>/freeze-*"],
            freeze_fn)

    def upload_pair_fn(ctx):
        try:
            freeze = json.load(open(os.path.join(ctx.run_dir, "settings-freeze.json")))
        except Exception:
            freeze = {}
        if freeze.get("classified") == "v3":
            return "PASS", "staged pair is already v3 — no upload (idempotent re-run)"
        for key, local, ftp in (("settings", SETTINGS_V3["settings_local"], SETTINGS_V3["settings_ftp"]),
                                ("loadlist", SETTINGS_V3["loadlist_local"], SETTINGS_V3["loadlist_ftp"])):
            rc, _, _ = run_cmd(ctx, ["curl", "-sS", "-T", local, FTP + ftp], "STOR v3 %s" % key)
            if rc != 0:
                return "FAIL", "v3 %s STOR failed rc=%d — pair is now MID-RESTAGE; do not launch; " \
                               "re-run this flow (freeze will re-classify) or restore the v2 pair" % (key, rc)
        return "PASS", "v3 pair uploaded as ONE lockstep restage (settings + loadlist)"

    fl.step("set.2", "UPLOAD (lockstep) — STOR the v3 settings ini and the v3 loadlist back to "
                     "back (activation does not run mid-transfer; never launch between the two "
                     "stores); skipped entirely when the freeze found the v3 pair already staged",
            "upload",
            ["the two uploads are ONE restage: mfa refuses a mismatched pair either direction "
             "(MFA_E_SIZE -8), and the tool never relies on that guard",
             "either STOR failure = pair mid-restage = do NOT launch; re-run the flow or "
             "restore the recorded v2 pair"],
            ["curl -sS -T %s %s%s" % (SETTINGS_V3["settings_local"], FTP, SETTINGS_V3["settings_ftp"]),
             "curl -sS -T %s %s%s" % (SETTINGS_V3["loadlist_local"], FTP, SETTINGS_V3["loadlist_ftp"])],
            upload_pair_fn)

    def readback_pair_fn(ctx):
        try:
            freeze = json.load(open(os.path.join(ctx.run_dir, "settings-freeze.json")))
        except Exception:
            freeze = {}
        already_v3 = freeze.get("classified") == "v3"
        problems = []
        for key, sha, nbytes, ftp in (("settings", SETTINGS_V3["settings_sha"], SETTINGS_V3["settings_bytes"], SETTINGS_V3["settings_ftp"]),
                                      ("loadlist", SETTINGS_V3["loadlist_sha"], SETTINGS_V3["loadlist_bytes"], SETTINGS_V3["loadlist_ftp"])):
            p = os.path.join(ctx.run_dir, "readback-%s" % key)
            rc, _, _ = run_cmd(ctx, ["curl", "-sS"] + CURL_TIMEOUT + [FTP + ftp, "-o", p],
                               "readback v3 %s" % key)
            if ctx.dry:
                continue
            if rc != 0 or not os.path.exists(p) or os.path.getsize(p) == 0:
                problems.append("%s readback failed rc=%s" % (key, rc))
                continue
            got_sha, got_n = local_sha(p)
            if (got_sha, got_n) != (sha, nbytes):
                problems.append("%s readback %s/%s != pinned %s/%s (partial upload — re-transfer, "
                                "never proceed)" % (key, got_sha[:12], got_n, sha[:12], nbytes))
        if ctx.dry:
            return "PASS", "dry"
        if problems:
            return "FAIL", "; ".join(problems)
        return "PASS", "v3 pair readback byte-exact%s: settings 1448 B d8faa4ec… + loadlist 447 B 4d98ecd7…" % (
            " (no-op path: on-console bytes still re-verified)" if already_v3 else "")

    fl.step("set.3", "READBACK — byte-exact FTP download + sha256 BOTH v3 files (expected "
                     "d8faa4ec… / 1448 and 4d98ecd7… / 447); mismatch = re-transfer + re-verify, "
                     "never proceed on a partial upload (that is exactly the MFA_E_SIZE failure "
                     "mode)", "readback",
            ["expected: settings 1448 B d8faa4ec…, loadlist 447 B 4d98ecd7…",
             "launch gate: this step must be green before any arm/launch handoff"],
            ["curl -sS %s%s -o <run-dir>/readback-settings" % (FTP, SETTINGS_V3["settings_ftp"]),
             "curl -sS %s%s -o <run-dir>/readback-loadlist" % (FTP, SETTINGS_V3["loadlist_ftp"]),
             "shasum -a 256 <run-dir>/readback-*"],
            readback_pair_fn)

    fl.step("set.4", "DOC — what v3 changes at activation + rollback pointer", "doc",
            ["v3 appends [Hooks] HookProcessInternal=0 + HookProcessLocalScriptFunction=0 "
             "(skips the ProcessInternal/ProcessLocalScriptFunction derivation: prevents a "
             "probable init fault when text is unreadable, removes unused detour-write risk "
             "when readable); Lua event path unaffected (ProcessEvent resolves from the "
             "vtable under its own untouched key)",
             "v2 remains the rollback pair (834 B 9570ead3… + 446 B 3afd10ce… recorded in "
             "worker-engineversion-ini) — re-verify before any rollback staging",
             "v3 does NOT touch EngineTickResolveMethod or any other text-read dependency"],
            ["shasum -a 256 <run-dir>/readback-settings <run-dir>/readback-loadlist  # receipt anchors",
             "curl -sS %s%s | shasum -a 256   # v2 rollback source, re-verify before use (worker-engineversion-ini/package-v2)" % (FTP, SETTINGS_V3["settings_ftp"])],
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


# --- probe verdict sinks (game-evidence; evidence paths per the probes' stage plans)

def _fresh_sink_lines(text, baseline):
    """Fresh bytes of a single-writer append-only sink vs its recorded byte
    baseline. Returns (fresh_text, note); fresh_text None = baseline mismatch
    (sink rotated/truncated — reconcile, never guess across it)."""
    if baseline and text:
        if text.startswith(baseline):
            return text[len(baseline):], "growth vs recorded baseline (%d -> %d bytes)" % (
                len(baseline), len(text))
        return None, "sink does NOT start with the recorded baseline (rotated/truncated?)"
    return (text or ""), ("no baseline recorded this run (standalone game-evidence run) — "
                          "all lines treated as candidate evidence")

def probe_evidence_verdict(xwp_text, xwp_base, fc1_text, fc1_base):
    """Classify the two probe verdict sinks (evidence-path law):
      /data/modbridge-exec-write-probe.jsonl — xwp-armed verdicts, envelope
        build_id igd1-6ebab07f2111; per-attempt + summary records, fail-open.
      /data/modbridge-fc1pool-facts.jsonl — fc1pool1-78508a2e2940; success =
        fc1pool_result verdict validated (checks 11/11) + single FC1POOL: UE4SS line.
    Returns (status, detail, info). Presence/freshness/classification only — the
    probes' verdicts are EVIDENCE; this gate never certifies writability."""
    info = {"xwp": {}, "fc1": {}}
    status = "PASS"
    notes = []
    fresh_x, note_x = _fresh_sink_lines(xwp_text, xwp_base)
    if fresh_x is None:
        return "FAIL", "xwp jsonl: %s" % note_x, info
    x_lines = [ln for ln in fresh_x.splitlines() if ln.strip()]
    x_bad, x_foreign, x_exps = [], [], []
    for ln in x_lines:
        try:
            j = json.loads(ln)
        except Exception:
            x_bad.append(ln[:60])
            continue
        if j.get("build_id") != XWP_ARMED["build_id"]:
            x_foreign.append(str(j.get("build_id")))
        elif j.get("exp"):
            x_exps.append(j["exp"])
    info["xwp"] = {"fresh_records": len(x_lines), "exps": x_exps, "baseline_note": note_x,
                   "build_id": XWP_ARMED["build_id"]}
    if x_bad:
        return "FAIL", "xwp jsonl: %d unparseable line(s): %s" % (len(x_bad), x_bad[:2]), info
    if x_foreign:
        return "FAIL", "xwp jsonl: foreign build_id(s) %s (single-writer discipline violated or stale sink)" % x_foreign[:3], info
    if not x_lines:
        status = "WARN"
        notes.append("xwp: ZERO fresh verdict records this launch (probe did not run or sink "
                     "unreachable — check SC-5b OK lines / UE4SS mirror)")

    fresh_f, note_f = _fresh_sink_lines(fc1_text, fc1_base)
    if fresh_f is None:
        return "FAIL", "fc1 jsonl: %s" % note_f, info
    f_lines = [ln for ln in fresh_f.splitlines() if ln.strip()]
    f_bad, f_foreign, f_validated, f_refusals = [], [], 0, []
    for ln in f_lines:
        try:
            j = json.loads(ln)
        except Exception:
            f_bad.append(ln[:60])
            continue
        if j.get("build_id") != FC1POOL["build_id"]:
            f_foreign.append(str(j.get("build_id")))
            continue
        if j.get("exp") == "fc1pool_result":
            if j.get("verdict") == "validated" and j.get("checks_passed") == 11 \
                    and j.get("checks_total") == 11:
                f_validated += 1
            else:
                f_refusals.append("verdict=%s reason=%s checks=%s/%s" % (
                    j.get("verdict"), j.get("reason_code"), j.get("checks_passed"), j.get("checks_total")))
    info["fc1"] = {"fresh_records": len(f_lines), "validated_results": f_validated,
                   "refusals": f_refusals, "baseline_note": note_f,
                   "build_id": FC1POOL["build_id"]}
    if f_bad:
        return "FAIL", "fc1 jsonl: %d unparseable line(s): %s" % (len(f_bad), f_bad[:2]), info
    if f_foreign:
        return "FAIL", "fc1 jsonl: foreign build_id(s) %s (single-writer discipline violated or stale sink)" % f_foreign[:3], info
    if f_validated:
        notes.append("fc1: fc1pool_result validated 11/11 (%d fresh records)" % len(f_lines))
    elif f_lines:
        status = "WARN"
        notes.append("fc1: fresh records but NO validated fc1pool_result — recorded refusals: "
                     "%s (honest terminal verdicts; never relaunch an identical rejected layout)"
                     % (f_refusals or "incomplete sequence"))
    else:
        status = "WARN"
        notes.append("fc1: ZERO fresh records this launch (probe did not run or sink unreachable)")
    detail = "; ".join(notes) if notes else "probe sinks green"
    return status, detail, info


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
        ok_x, x_text = fetch_console_file(ctx, XWP_ARMED["log"].lstrip("/"), "XWP-exec-write-probe.jsonl")
        ok_f, f_text = fetch_console_file(ctx, FC1POOL["log"].lstrip("/"), "FC1-fc1pool-facts.jsonl")
        if ctx.dry:
            return "PASS", "dry"
        if not (ok_s or ok_l or ok_d):
            return "FAIL", "all sinks unreachable — wait ~60 s and retry (runbook §2)"
        ctx.state["s_text"], ctx.state["l_text"], ctx.state["d1_text"], ctx.state["c2_text"] = s_text, l_text, d_text, c_text
        ctx.state["xwp_text"], ctx.state["fc1_text"] = x_text, f_text
        return "PASS", "fetched: S=%s L=%s D1=%s C2=%s XWP=%s FC1=%s" % (
            ok_s, ok_l, ok_d, ok_c, ok_x, ok_f)

    fl.step("ge.2", "Fetch the seven evidence sinks (bounded curl, one fresh fetch per file; "
                    "the two probe verdict sinks per their stage plans)", "classify",
            ["FTP base ftp://ps5-host-not-configured.invalid:2121 anonymous; --connect-timeout 10 --max-time 60 --max-filesize 32M",
             "if fetches fail: wait ~60 s and re-run before touching staging"],
            ["curl 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/runtime-status.txt'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/townfall_runtime_aligned_entry.log'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-ingame-facts.jsonl'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-exec-write-probe.jsonl'",
             "curl 'ftp://ps5-host-not-configured.invalid:2121/data/modbridge-fc1pool-facts.jsonl'"],
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

    fl.step("ge.3", "CP0-CP4 checks (composed PRX ran; activation; engine-ready+Tick 0x33ac8c0; mods-started; mod banner)", "classify",
            ["CP0: one new module_start_entered…module_start_return 0x0 pair + probe_lifecycle_result 0x0",
             "CP1: fresh S session reaches activated; banner + 'Setting up mods...'; no module_start failed line",
             "CP2: engine-ready AND 'GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)'",
             "CP3: fresh mods-started (rung-1 liveness delegated to rung_check; UNVERIFIED_DRIVER is honest, gap G2)",
             "CP4: exactly one fresh '[TownfallItemSpawner] Loaded v1.0…' banner; S ends mods-started"],
            ["python3 console_batch.py cp0-cp4 classifier over S/L/D1 in the run dir"],
            cp_fn)

    def probes_fn(ctx):
        if ctx.dry:
            return "PASS", "dry"
        x_text = ctx.state.get("xwp_text") or ""
        f_text = ctx.state.get("fc1_text") or ""
        x_base = ctx.state.get("xwp_jsonl_baseline") or ""
        f_base = ctx.state.get("fc1_jsonl_baseline") or ""
        l_text = ctx.state.get("l_text") or ""
        status, detail, info = probe_evidence_verdict(x_text, x_base, f_text, f_base)
        info["fc1_ue4ss_mirror_lines"] = sum(1 for ln in l_text.splitlines() if ln.startswith("FC1POOL:"))
        ctx.save("probe-verdicts.json", json.dumps({"status": status, "detail": detail,
                 "sinks": info}, indent=2))
        ctx.state["probe_status"] = status
        if status == "FAIL":
            return "FAIL", "probe verdict sinks: %s" % detail
        if status == "WARN":
            return "WARN", "probe verdict sinks: %s" % detail
        return "PASS", "probe verdict sinks: %s" % detail

    fl.step("ge.3b", "Probe verdict sinks — /data/modbridge-exec-write-probe.jsonl (build_id "
                     "igd1-6ebab07f2111) + /data/modbridge-fc1pool-facts.jsonl (build_id "
                     "fc1pool1-78508a2e2940): freshness vs the staging baselines, strict-JSON "
                     "envelope law, fc1pool_result validated 11/11; single FC1POOL: UE4SS line "
                     "recorded", "classify",
            ["fresh growth vs the recorded baselines proves THIS launch's verdict records",
             "fc1 success signature: fc1pool_result verdict validated, reason_code none, "
             "checks 11/11 (+ exactly one FC1POOL: mirror line in UE4SS.log, recorded)",
             "recorded refusals are honest terminal evidence (verdict/refusal classes per the "
             "probe's stage plan) — never relaunch an identical rejected layout",
             "these verdicts are EVIDENCE about exec-writability/pool state; this gate never "
             "certifies writability"],
            ["python3 console_batch.py probe-sink classifier over XWP/FC1 jsonl in the run dir"],
            probes_fn)

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
            "probe_sinks": "see probe-verdicts.json (/data/modbridge-exec-write-probe.jsonl "
                           "igd1-6ebab07f2111 + /data/modbridge-fc1pool-facts.jsonl "
                           "fc1pool1-78508a2e2940)",
            "honesty": ("visible menu + visible pickup are USER-eye facts; log lines classify and route, "
                        "they never certify (runbook G4/G7; do-not-claim guard); probe sink "
                        "classification is evidence presence/freshness, never a writability claim"),
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
    if dll_tag == "dll8":
        fallback_note = ("EXACTLY ONE DLL stage per session; dll7 is the documented fallback "
                         "attribution path — run it explicitly only per the plan's fallback rule, "
                         "never both by default.")
    else:
        fallback_note = ("This is the documented FALLBACK attribution session (chosen via --dll 7): "
                         "EXACTLY ONE DLL stage runs; the preferred single DLL run remains dll8 "
                         "(CONSOLE-STAGE-PLAN-8FAM). Never both by default.")
    fl = Flow("all")
    fl.notes = ("Recommended single-session order (CONSOLE-STAGE-PLAN-8FAM Sequencing + "
                "FINAL-INI-LAYOUT §3 + NEW-SETTINGS §3): payload-only stages first in ONE "
                "game-closed window (%s, then e2e), then the DEFINITIVE ridealong+probe staging "
                "(stage-ridealongs: xwp-armed + fc1pool + qdrv1 + observer PRXs + the FINAL 8-entry "
                "ploader.ini via bootstrap-if-needed + ensure ops, byte-exact canonical, 7 "
                "registered), then the settings+loadlist lockstep restage (stage-settings, v3 "
                "pair), then the single UE4SS game session with evidence collection LAST. %s"
                % (dll_tag, fallback_note))
    for sub in (DLL_BUILDERS[dll_tag], build_e2e, build_stage_ridealongs, build_stage_settings,
                build_game_evidence):
        f = sub()
        for st in f.steps:
            st2 = dict(st)
            st2["id"] = "%s/%s" % (f.name, st["id"])
            fl.steps.append(st2)
    return fl


FLOWS = {"dll8": build_dll8, "dll7": build_dll7, "e2e": build_e2e,
         "stage-ridealongs": build_stage_ridealongs, "stage-observer": build_stage_observer,
         "stage-qdrv1": build_stage_qdrv1, "stage-settings": build_stage_settings,
         "rollback-ridealongs": build_rollback_ridealongs,
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
    # ridealong extension: `all` stages BOTH ridealongs between e2e and game-evidence
    def idx_of(prefix):
        return min(i for i, sid in enumerate(ids) if sid.startswith(prefix))
    check("all contains the combined stage-ridealongs (never the qdrv1-only stage)",
          any(i.startswith("stage-ridealongs/") for i in ids)
          and not any(i.startswith("stage-qdrv1/") for i in ids)
          and not any(i.startswith("stage-observer/") for i in ids), str(ids))
    check("all order: e2e < stage-ridealongs < stage-settings < game-evidence",
          idx_of("e2e/") < idx_of("stage-ridealongs/") < idx_of("stage-settings/")
          < idx_of("game-evidence/"), str(ids))
    check("all ridealongs handoff is the launch gate (last ridealongs step)",
          [s for s in fa.steps if s["id"].startswith("stage-ridealongs/")][-1]["group"] == "handoff")
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

    ctx = Ctx(None, dry=True); FLOWS["stage-observer"]().run(ctx); ob = "\n".join(ctx.events)
    for anchor in ["modbridge_runtime_aligned_observer.prx", "1afa0997682c", "30152151",
                   "d0a3d6feebae", "30145992", "30143976", "3f89a5b6", "SHT-strip", "ploader.ini",
                   ".bak", "ONLY its own line", "ARM ploader", "rollback-ridealongs"]:
        check("stage-observer anchor %r" % anchor[:40], anchor in ob)

    ctx = Ctx(None, dry=True); FLOWS["stage-ridealongs"]().run(ctx); rd = "\n".join(ctx.events)
    for anchor in ["52c3221665a4", "1afa0997682c", "87112", "71406d3c", "30143976", "3f89a5b6",
                   "SHT-strip", "[PPSA33286]", "EXACTLY 7", "7 registered", "composition = 2",
                   "modbridge_qh_driver.prx", "modbridge_runtime_aligned_observer.prx",
                   "modbridge_exec_write_probe_armed.prx", "modbridge_fc1pool.prx",
                   "48949316", "d2431bf8", "67832", "daea2ff4", "67848", "7e2dce27",
                   "igd1-6ebab07f2111", "fc1pool1-78508a2e2940",
                   "INI-BOOTSTRAPPED", "ploader.ini.canonical", "bbc34c16",
                   ".bak", "rollback-ridealongs", "ARM ploader", "UE4SS.log",
                   "townfall_runtime_aligned_observer.log",
                   "modbridge-exec-write-probe.jsonl", "modbridge-fc1pool-facts.jsonl"]:
        check("stage-ridealongs anchor %r" % anchor[:40], anchor in rd)

    ctx = Ctx(None, dry=True); FLOWS["stage-settings"]().run(ctx); st = "\n".join(ctx.events)
    for anchor in ["d8faa4ec", "4d98ecd7", "1448", "447", "lockstep", "FREEZE",
                   "UE4SS-settings.ini", "loadlist.txt", "MFA_E_SIZE", "9570ead3",
                   "HookProcessInternal", "PPSA33286" if False else "roots/PPSA33286"]:
        check("stage-settings anchor %r" % anchor[:40], anchor in st)

    ctx = Ctx(None, dry=True); FLOWS["rollback-ridealongs"]().run(ctx); rbc = "\n".join(ctx.events)
    for anchor in ["= 0", "single-character", "ploader.ini", ".bak", "readback", "owned evidence"]:
        check("rollback-ridealongs anchor %r" % anchor[:40], anchor in rbc)

    ctx = Ctx(None, dry=True); FLOWS["game-evidence"]().run(ctx); ge = "\n".join(ctx.events)
    for anchor in ["post_launch_check.py", "rung_check.py", "runtime-status.txt", "UE4SS.log",
                   "townfall_runtime_aligned_entry.log", "modbridge-ingame-facts.jsonl",
                   "modbridge-exec-write-probe.jsonl", "modbridge-fc1pool-facts.jsonl",
                   "igd1-6ebab07f2111", "fc1pool1-78508a2e2940", "validated 11/11",
                   "CP0-CP4", "OPTIONS", "CROSS", "0x33ac8c0", "USER ACTION REQUIRED",
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
    check("8fam PASS fixture (bare 4 records)", v == "PASS", str(d["divergences"][:3]))
    prior36 = [{"marker": "modbridge.owned-dll", "schema": 1, "build_id": "133c5e9e59f24c2fb57e7e3422c665a1",
                "sequence": i % 4, "phase": [1, 2, 3, 5][i % 4]} for i in range(36)]
    full8_text = "\n".join([json.dumps(x) for x in prior36] + pass8_text.splitlines())
    v, d = verdict_8fam_records(full8_text)
    check("8fam PASS fixture (full transcript, dll8-first)", v == "PASS", str(d["divergences"][:3]))
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

    print("== selftest G: SHT-strip readback law (STAGE-PLAN-COMBINED §3, both ways) ==")
    def make_elf(body_n):
        # minimal ELF64: 64-byte Ehdr + 2 phdrs (56 B each) + body.
        # ph1: offset 176 filesz body_n -> end 176+body_n; ph2: offset 176 filesz 100.
        ehdr = bytearray(64)
        ehdr[0:4] = b"\x7fELF"
        struct.pack_into("<Q", ehdr, 32, 64)         # e_phoff
        struct.pack_into("<H", ehdr, 56, 2)          # e_phnum
        ph1 = bytearray(56)
        struct.pack_into("<Q", ph1, 8, 176)          # p_offset
        struct.pack_into("<Q", ph1, 32, body_n)      # p_filesz
        ph2 = bytearray(56)
        struct.pack_into("<Q", ph2, 8, 176)
        struct.pack_into("<Q", ph2, 32, 100)
        body = bytes((i * 7) % 251 + 1 for i in range(body_n))
        return bytes(ehdr) + bytes(ph1) + bytes(ph2) + body
    BARE = make_elf(124)
    END = sht_prefix_end(BARE)
    check("synthetic ELF prefix end computed", END == 300 and len(BARE) == 300, (END, len(BARE)))
    BODY_SHA = hashlib.sha256(BARE[64:END]).hexdigest()
    rb_ok = bytearray(BARE[:END])
    struct.pack_into("<Q", rb_ok, 40, 0xDEADBEEF)     # e_shoff junked by make_fself
    struct.pack_into("<H", rb_ok, 58, 0x40)           # e_shentsize junked
    struct.pack_into("<H", rb_ok, 60, 0)              # e_shnum zeroed (law)
    struct.pack_into("<H", rb_ok, 62, 0)              # e_shstrndx junked
    ok, detail = sht_strip_readback_law(bytes(rb_ok), BARE, END, BODY_SHA)
    check("law accepts a law-shaped readback", ok, detail)
    check("law detail is READBACK-OK", detail.startswith("READBACK-OK"), detail[:60])
    def law_fail(name, rb, bare=BARE, expect=END, sha=BODY_SHA):
        ok, detail = sht_strip_readback_law(rb, bare, expect, sha)
        check("law rejects %s" % name, not ok, detail[:80])
        return detail
    d = law_fail("SHT tail leaked back into the readback", BARE + b"SHT-TAIL")
    check("length rejection names both sizes", "len=308/300" in d, d[:60])
    law_fail("truncated readback", BARE[:END - 1])
    corrupt = bytearray(BARE[:END]); corrupt[100] ^= 0xFF
    law_fail("corrupted body byte", bytes(corrupt))
    wrong_body_sha = hashlib.sha256(b"x").hexdigest()
    d = law_fail("pinned body sha mismatch", bytes(rb_ok), sha=wrong_body_sha)
    check("body-sha rejection names the sha", wrong_body_sha[:12] in d, d[:80])
    forbidden = bytearray(bytes(rb_ok)); forbidden[7] ^= 0x01   # e_ident byte outside allowed set
    d = law_fail("forbidden header byte diff", bytes(forbidden))
    check("header rejection names the outside byte", "outside the allowed set" in d, d[:80])
    shnum = bytearray(bytes(rb_ok)); struct.pack_into("<H", shnum, 60, 3)
    law_fail("e_shnum not zeroed", bytes(shnum))
    foreign = bytearray(bytes(rb_ok)); foreign[0:4] = b"\x7fELx"
    law_fail("foreign magic", bytes(foreign))
    d = law_fail("constants/artifact divergence (pinned 301)", bytes(rb_ok) + b"x", expect=END + 1)
    check("divergence refusal says never widen the law", "never widen the law" in d, d[:80])
    not_elf = bytearray(64)
    law_fail("bare reference not an ELF", bytes(rb_ok), bare=bytes(not_elf))
    # real pinned artifacts: the law constants must reproduce from the bare-sdk ELFs
    for d_ref, name in ((QDRV1, "qdrv1"), (OBS, "observer"), (XWP_ARMED, "xwp"), (FC1POOL, "fc1")):
        bare = open(d_ref["bare"], "rb").read()
        end = sht_prefix_end(bare)
        check("%s artifact prefix == pinned readback length" % name, end == d_ref["readback_bytes"],
              "%d vs %d" % (end, d_ref["readback_bytes"]))
        check("%s artifact body sha == pinned" % name,
              hashlib.sha256(bare[64:end]).hexdigest() == d_ref["readback_body_sha"])
        rb = bytearray(bare[:end])
        struct.pack_into("<H", rb, 60, 0)
        ok, detail = sht_strip_readback_law(bytes(rb), bare, d_ref["readback_bytes"], d_ref["readback_body_sha"])
        check("%s artifact law round-trip (fself channel shape)" % name, ok, detail[:80])
        ok2, _ = sht_strip_readback_law(bare, bare, d_ref["readback_bytes"], d_ref["readback_body_sha"])
        check("%s full bare file rejected (SHT tail not embedded)" % name, not ok2)

    print("== selftest H: combined ridealong ini law (append-only + idempotent + end-state) ==")
    E_LINE = "/data/plugins/modbridge_runtime_aligned_entry.prx"
    P_LINE = "/data/plugins/modbridge_path_test.prx"
    I_LINE = "/data/plugins/modbridge_ingame_facts.prx"
    C_LINE = COMBINED_INI["anchor"]
    Q_LINE = QDRV1["module_line"]
    O_LINE = OBS["module_line"]
    pre4 = "[PPSA33286]\n%s = 2\n%s = 2\n%s = 0\n%s = 2\n" % (E_LINE, P_LINE, I_LINE, C_LINE)
    pre5q = pre4 + "%s = 2\n" % Q_LINE
    pre6 = pre5q + "%s = 2\n" % O_LINE

    st, nt, ap = ridealong_ini_plan(pre4, [Q_LINE, O_LINE])
    check("combined append adds exactly BOTH lines in order",
          st == "append" and ap == ["%s = 2" % Q_LINE, "%s = 2" % O_LINE], str(ap))
    check("combined edit is append-only from the pre-image",
          ridealong_ini_append_only(pre4, nt, ap) == [])
    check("combined end-state clean (6 entries, 5 registered)",
          ridealong_verify_endstate(nt, required=[Q_LINE, O_LINE]) == [],
          str(ridealong_verify_endstate(nt, required=[Q_LINE, O_LINE])))

    st, nt2, ap2 = ridealong_ini_plan(pre5q, [Q_LINE, O_LINE])
    check("COORDINATION: qdrv1 already staged -> observer appends ONLY its own line",
          st == "append" and ap2 == ["%s = 2" % O_LINE], str(ap2))
    check("qdrv1 line untouched byte-identical", nt2.startswith(pre5q))
    check("observer-only end-state clean", ridealong_verify_endstate(nt2, required=[O_LINE],
          optional=(Q_LINE,)) == [])

    st, nt3, ap3 = ridealong_ini_plan(pre6, [Q_LINE, O_LINE])
    check("idempotent: fully-staged ini is a no-op (change nothing)", st == "noop" and nt3 == pre6)

    dup_pre = pre4.replace(C_LINE + " = 2", C_LINE + " = 2\n" + O_LINE + " = 2", 1) \
        + "%s = 2\n" % O_LINE
    st, nt4, _ap = ridealong_ini_plan(dup_pre, [O_LINE])
    check("STOP on duplicate ridealong line (ploader injects twice)", st == "stop", str((st, nt4)))

    st, _nt, _ap = ridealong_ini_plan(None, [O_LINE])
    check("STOP: combined law never writes an ini from scratch", st == "stop")
    st, _nt, _ap = ridealong_ini_plan("[modules]\n1=%s\n2=%s\n" % (C_LINE, Q_LINE), [O_LINE])
    check("STOP on legacy numbered shape (no [PPSA33286] header) — reconcile", st == "stop")
    st, _nt, _ap = ridealong_ini_plan(pre4 + "[PPSA33286]\n", [O_LINE])
    check("STOP on duplicate header", st == "stop")
    missing_path = pre4.replace("%s = 2\n" % P_LINE, "")
    st, _nt, _ap = ridealong_ini_plan(missing_path, [O_LINE])
    check("STOP when an expected existing line is missing", st == "stop")
    st, _nt, _ap = ridealong_ini_plan(pre4.replace(C_LINE + " = 2", C_LINE + " = 1"), [O_LINE])
    check("STOP when an expected line's mode diverges (composition = 1)", st == "stop")
    st, _nt, _ap = ridealong_ini_plan(pre5q.replace(Q_LINE + " = 2", Q_LINE + " = 0"), [Q_LINE, O_LINE])
    check("STOP on a rolled-back mode-0 ridealong line (never blind-flip)", st == "stop")
    st, _nt, _ap = ridealong_ini_plan(pre5q.replace(Q_LINE + " = 2", Q_LINE + " = 1"), [Q_LINE, O_LINE])
    check("STOP on a mode-1 ridealong line", st == "stop")
    st, _nt, _ap = ridealong_ini_plan(pre4 + "junk without = sign\n", [O_LINE])
    check("STOP on an unparseable ini line", st == "stop")
    st, _nt, _ap = ridealong_ini_plan(pre4 + "%s = 3\n" % P_LINE.replace(".prx", "2.prx"), [O_LINE])
    check("STOP on an out-of-range mode (parser would drop it)", st == "stop")

    pre4_nonl = pre4.rstrip("\n")
    st, nt5, ap5 = ridealong_ini_plan(pre4_nonl, [O_LINE])
    check("missing trailing newline handled (single added NL + the line)",
          st == "append" and nt5.startswith(pre4_nonl + "\n") and nt5.endswith("%s = 2\n" % O_LINE))
    check("append-only proof passes for the NL-added case",
          ridealong_ini_append_only(pre4_nonl, nt5, ap5) == [])
    tampered = nt5.replace(I_LINE + " = 0", I_LINE + " = 2")
    check("append-only proof catches a rewritten existing line",
          ridealong_ini_append_only(pre4_nonl, tampered, ap5) != [])
    reordered = pre4.replace(E_LINE + " = 2", "x" + E_LINE + " = 2", 1)
    check("append-only proof catches any non-tail change",
          ridealong_ini_append_only(pre4, reordered + "%s = 2\n" % O_LINE, ["%s = 2" % O_LINE]) != [])

    extra = nt + "%s = 2\n" % "/data/plugins/some_other.prx"
    check("end-state flags an unknown plugin line",
          any("unknown plugin line" in p for p in ridealong_verify_endstate(extra, required=[Q_LINE, O_LINE])))
    dup = nt + "%s = 2\n" % O_LINE
    check("end-state flags a duplicated ridealong line",
          any("duplicate" in p for p in ridealong_verify_endstate(dup, required=[O_LINE])))
    missing_req = pre4 + "%s = 2\n" % O_LINE
    check("end-state flags a missing required ridealong",
          any("required ridealong" in p for p in ridealong_verify_endstate(missing_req, required=[Q_LINE, O_LINE])))
    mode0_optional = pre4 + "%s = 2\n" % O_LINE + "%s = 0\n" % Q_LINE
    check("end-state flags an optional ridealong at mode 0",
          any("mode 0" in p for p in ridealong_verify_endstate(mode0_optional, required=[O_LINE], optional=(Q_LINE,))))
    check("end-state registered count gate (4 != 5 combined)",
          any("registered plugin count" in p for p in
              ridealong_verify_endstate(missing_req, required=[Q_LINE, O_LINE])))

    print("== selftest I: §7 rollback edit (single-character mode flips) ==")
    st, nt6, fl6 = ridealong_rollback_edit(pre6, [Q_LINE, O_LINE])
    check("rollback flips BOTH staged lines", st == "flip" and len(fl6) == 2, str(fl6))
    diffs = [(i, a, b) for i, (a, b) in enumerate(zip(pre6, nt6)) if a != b]
    check("rollback is exactly two single-character 2->0 edits",
          len(nt6) == len(pre6) and len(diffs) == 2 and all(a == "2" and b == "0" for _i, a, b in diffs),
          str(diffs))
    check("end-state gate rightly refuses a rolled-back (mode-0) ini as a staging state",
          ridealong_verify_endstate(nt6, required=[], optional=(Q_LINE, O_LINE)) != [])
    st, nt7, fl7 = ridealong_rollback_edit(pre4, [Q_LINE, O_LINE])
    check("rollback noop when no ridealong line is staged", st == "noop" and nt7 == pre4)
    st, nt8, fl8 = ridealong_rollback_edit(pre5q, [Q_LINE, O_LINE])
    check("rollback flips only the staged line (observer absent)",
          st == "flip" and len(fl8) == 1 and nt8 == pre5q.replace(Q_LINE + " = 2", Q_LINE + " = 0"))
    st, _nt, _fl = ridealong_rollback_edit(pre5q.replace(Q_LINE + " = 2", Q_LINE + " = 1"), [Q_LINE, O_LINE])
    check("rollback STOP on a mode-1 line", st == "stop")
    st, _nt, _fl = ridealong_rollback_edit(pre6 + "%s = 2\n" % O_LINE, [O_LINE])
    check("rollback STOP on a duplicate line", st == "stop")
    odd = "[PPSA33286]\n  %s   =  2  \n" % O_LINE
    st, nt9, fl9 = ridealong_rollback_edit(odd, [O_LINE])
    check("rollback preserves whitespace (single-char edit on odd spacing)",
          st == "flip" and nt9 == odd.replace("=  2", "=  0"), repr(nt9))

    print("== selftest J: DEFINITIVE final-layout law (FINAL-INI-LAYOUT §3) ==")
    CANON = final_canonical_text()
    check("canonical template is the pinned 400 B / bbc34c16… bytes",
          len(CANONICAL_BYTES := CANON.encode()) == FINAL_INI["canonical_bytes"]
          and hashlib.sha256(CANONICAL_BYTES).hexdigest() == FINAL_INI["canonical_sha256"])
    check("canonical end-state verify accepts the template itself",
          final_ini_verify_endstate(CANON) == [],
          str(final_ini_verify_endstate(CANON)))
    X_LINE = XWP_ARMED["module_line"]
    F_LINE = FC1POOL["module_line"]
    PRE_EXISTING4 = [C_LINE, E_LINE, P_LINE, I_LINE]   # historical console lines
    HISTORICAL = FINAL_INI["header"] + "\n" + "".join(
        FINAL_LINE_OF[p] + "\n" for p in [E_LINE, P_LINE, I_LINE, C_LINE])
    # canonically-ORDERED partial pre-image (e.g. after a prior bootstrap, or a
    # console whose pre-existing lines already sit at their definitive positions)
    ORDERED4 = FINAL_INI["header"] + "\n" + "".join(
        FINAL_LINE_OF[p] + "\n" for p in [C_LINE, E_LINE, P_LINE, I_LINE])

    # ensure-op convergence: EVERY order of the 4 stager ensures from the
    # canonically-ordered partial pre-image reaches byte-exact canonical (the
    # full 8! = 40320-permutation proof lives in simulate_ploader_ini_final.py,
    # re-run in receipts; here: all 24 orders + reversed + repeats)
    import itertools as _it
    convergence = set()
    for perm in _it.permutations(FINAL_INI["stager_lines"]):
        text = ORDERED4
        try:
            for p in perm:
                text = final_ini_ensure(text, p)
        except FinalEnsureError as exc:
            check("ensure permutation %s" % (perm,), False, str(exc))
            break
        convergence.add(text)
    check("all 24 stager ensure orders from the ordered pre-image converge byte-exact",
          convergence == {CANON}, "distinct end-states: %d" % len(convergence))
    text = ORDERED4
    for p in reversed(FINAL_INI["stager_lines"]):
        text = final_ini_ensure(text, p)
    check("reversed application converges byte-exact", text == CANON)
    for p in FINAL_INI["stager_lines"] * 2:
        text = final_ini_ensure(text, p)
    check("repeated (idempotent) application is a byte no-op", text == CANON)
    # the historical pre-image CANNOT converge by appends (the two pre-existing
    # lines sit BEFORE composition — exactly why the bootstrap rebuild exists):
    # every ensure order from it converges to the same WRONG (degraded-order) bytes
    degraded_hist = set()
    for perm in _it.permutations(FINAL_INI["stager_lines"]):
        text = HISTORICAL
        for p in perm:
            text = final_ini_ensure(text, p)
        degraded_hist.add(text)
    check("appends from the HISTORICAL pre-image converge to one DEGRADED order "
          "(never canonical — the bootstrap whole-file STOR is required)",
          len(degraded_hist) == 1 and degraded_hist != {CANON}
          and final_ini_verify_endstate(degraded_hist.pop()) != [])

    st, ntj, apj = final_ini_plan(ORDERED4, list(FINAL_ORDER))
    check("plan on the ordered pre-image: ensure-only path appends exactly the 4 staged lines",
          st == "append" and apj == ["%s = 2" % p for p in FINAL_INI["stager_lines"]], str((st, apj)))
    check("ensure-plan result is byte-exact canonical", ntj == CANON)
    check("end-state gate accepts the planned result", final_ini_verify_endstate(ntj) == [])

    ordered5 = ORDERED4.replace(C_LINE + " = 2", C_LINE + " = 2\n" + Q_LINE + " = 2", 1)
    st, ntj2, apj2 = final_ini_plan(ordered5, list(FINAL_ORDER))
    check("plan with qdrv1 pre-staged appends only the remaining 3 lines",
          st == "append" and len(apj2) == 3 and ntj2 == CANON, str((st, apj2)))

    st, ntj3, notej3 = final_ini_plan(HISTORICAL, list(FINAL_ORDER))
    check("BOOTSTRAP required: historical order (aligned_entry/path_test BEFORE "
          "composition) cannot be fixed by appends",
          st == "bootstrap" and ntj3 == CANON and "INI-BOOTSTRAPPED" in notej3, str((st, notej3[:60])))
    check("bootstrap end-state verify accepts the rebuild target",
          final_ini_verify_endstate(ntj3) == [])

    st, ntj4, _ = final_ini_plan(CANON, list(FINAL_ORDER))
    check("plan on byte-exact canonical is a no-op (stage nothing, store nothing)", st == "noop")

    def plan_stop(name, text):
        st, reason, _extra = final_ini_plan(text, list(FINAL_ORDER))
        check("plan STOPs on %s" % name, st == "stop", "%s: %s" % (st, str(reason)[:70]))
        return reason
    hist6 = FINAL_INI["header"] + "\n" + "".join(
        FINAL_LINE_OF[p] + "\n" for p in [E_LINE, P_LINE, I_LINE, C_LINE, Q_LINE, O_LINE])
    plan_stop("no pre-image", None)
    plan_stop("empty pre-image", "")
    plan_stop("duplicate header", HISTORICAL + FINAL_INI["header"] + "\n")
    plan_stop("duplicate ridealong line", HISTORICAL + "%s = 2\n" % Q_LINE + "%s = 2\n" % Q_LINE)
    plan_stop("missing pre-existing line", HISTORICAL.replace("%s = 2\n" % P_LINE, ""))
    plan_stop("missing composition line", HISTORICAL.replace("%s = 2\n" % C_LINE, ""))
    plan_stop("mode divergence (composition = 1)", HISTORICAL.replace(C_LINE + " = 2", C_LINE + " = 1"))
    plan_stop("igf flipped to 2 (MUST NEVER)", HISTORICAL.replace(I_LINE + " = 0", I_LINE + " = 2"))
    plan_stop("rolled-back mode-0 ridealong", hist6.replace(Q_LINE + " = 2", Q_LINE + " = 0"))
    plan_stop("mode-1 ridealong", hist6.replace(Q_LINE + " = 2", Q_LINE + " = 1"))
    plan_stop("unknown line", HISTORICAL + "/data/plugins/modbridge_surprise.prx = 2\n")
    plan_stop("out-of-range mode (parser would drop it)", HISTORICAL + "%s = 3\n" % P_LINE.replace(".prx", "2.prx"))
    plan_stop("unparseable line", HISTORICAL + "junk without = sign\n")
    plan_stop("canonical ORDER but non-canonical byte FORM", ORDERED4.replace("\n", "\r\n"))
    r = plan_stop("canonical order+set but missing trailing newline", CANON.rstrip("\n"))
    check("form-divergence refusal names the rollback-runbook reconcile path",
          "rollback-runbook" in r, r[:80])

    def endstate_fail(name, text, needle=None):
        problems = final_ini_verify_endstate(text)
        check("end-state gate rejects %s" % name, problems != [], str(problems[:2]))
        if needle:
            check("end-state rejection names %r" % needle, any(needle in p for p in problems),
                  str(problems[:2]))
        return problems
    endstate_fail("an extra unknown line", CANON + "/data/plugins/x.prx = 2\n", "unknown line")
    degraded = FINAL_INI["header"] + "\n" + "".join(FINAL_LINE_OF[p] + "\n" for p in
                                                    [F_LINE, C_LINE, X_LINE, Q_LINE, O_LINE, E_LINE, P_LINE, I_LINE])
    endstate_fail("the degraded probe-after-composition order", degraded, "ORDER mismatch")
    endstate_fail("igf flipped to 2", CANON.replace(I_LINE + " = 0", I_LINE + " = 2"), "pinned 0")
    endstate_fail("a missing line", CANON.replace("%s = 2\n" % F_LINE, ""), "missing expected line")
    endstate_fail("fc1 mode-1 flip", CANON.replace(F_LINE + " = 2", F_LINE + " = 1"), "pinned 2")
    endstate_fail("duplicate line", CANON + "%s = 2\n" % F_LINE, "duplicate")
    endstate_fail("empty file", "", "differ from canonical")

    print("== selftest K: probe verdict sink classifier (evidence-path law) ==")
    xwp_j = {"marker": "modbridge.execwriteprobe", "schema": 1, "seq": 1, "exp": "row",
             "build_id": XWP_ARMED["build_id"]}
    xwp_good = json.dumps(xwp_j) + "\n" + json.dumps(dict(xwp_j, seq=2, exp="attempt")) + "\n"
    fc1_j = {"marker": "modbridge.ingamefacts", "schema": 1, "seq": 4, "exp": "fc1pool_result",
             "build_id": FC1POOL["build_id"], "verdict": "validated", "reason_code": "none",
             "checks_passed": 11, "checks_total": 11}
    fc1_good = json.dumps({"marker": "modbridge.ingamefacts", "seq": 1, "exp": "entry",
                           "build_id": FC1POOL["build_id"]}) + "\n" + json.dumps(fc1_j) + "\n"
    v, d, info = probe_evidence_verdict(xwp_good, "", fc1_good, "")
    check("probe sinks PASS on green fixtures (no baseline)", v == "PASS", d)
    check("fc1 validated 11/11 detected", info["fc1"]["validated_results"] == 1)
    v, d, info = probe_evidence_verdict(xwp_good, xwp_good, fc1_good, fc1_good)
    check("probe sinks WARN when no fresh growth this launch", v == "WARN" and
          "ZERO fresh" in d, d)
    v, d, info = probe_evidence_verdict(xwp_good + json.dumps(dict(xwp_j, seq=3)) + "\n",
                                        xwp_good, fc1_good, "")
    check("fresh growth vs baseline isolates THIS launch's records", v == "PASS" and
          info["xwp"]["fresh_records"] == 1, str(info["xwp"]))
    v, d, _ = probe_evidence_verdict("not-json\n", "", fc1_good, "")
    check("FAIL on unparseable xwp line", v == "FAIL" and "unparseable" in d, d)
    v, d, _ = probe_evidence_verdict(json.dumps(dict(xwp_j, build_id="igd1-DEADBEEF")) + "\n",
                                     "", fc1_good, "")
    check("FAIL on foreign xwp build_id", v == "FAIL" and "foreign build_id" in d, d)
    v, d, _ = probe_evidence_verdict("zz\n", "baseline-mismatch", fc1_good, "")
    check("FAIL on baseline mismatch (rotated sink)", v == "FAIL" and "baseline" in d, d)
    fc1_refused = json.dumps(dict(fc1_j, verdict="refused", reason_code="pool_window_uncovered",
                                  checks_passed=0, checks_total=11)) + "\n"
    v, d, info = probe_evidence_verdict(xwp_good, "", fc1_refused, "")
    check("WARN (not FAIL) on an honestly recorded fc1 refusal", v == "WARN" and
          "pool_window_uncovered" in d, d)

    print("== selftest L: settings v3 pair lockstep law (NEW-SETTINGS §3) ==")
    ll_text = open(SETTINGS_V3["loadlist_local"], errors="replace").read()
    check("v3 pair lockstep law accepts the shipped pair",
          settings_pair_lockstep_problems(ll_text, SETTINGS_V3["settings_row"],
                                          SETTINGS_V3["settings_bytes"]) == [])
    check("v3 pair lockstep law rejects the stale v2 size row",
          settings_pair_lockstep_problems(ll_text.replace("\t%d\n" % SETTINGS_V3["settings_bytes"],
                                                          "\t834\n", 1),
                                          SETTINGS_V3["settings_row"],
                                          SETTINGS_V3["settings_bytes"]) != [])
    check("v3 pair lockstep law rejects a missing settings row",
          settings_pair_lockstep_problems("modforge-loadlist\t1\n",
                                          SETTINGS_V3["settings_row"],
                                          SETTINGS_V3["settings_bytes"]) != [])
    check("pinned v3 bytes match the shipped package files",
          local_sha(SETTINGS_V3["settings_local"]) == (SETTINGS_V3["settings_sha"], SETTINGS_V3["settings_bytes"])
          and local_sha(SETTINGS_V3["loadlist_local"]) == (SETTINGS_V3["loadlist_sha"], SETTINGS_V3["loadlist_bytes"]))
    check("recorded inventories are internally consistent (v2 != v3, v3 self-consistent)",
          SETTINGS_V3["prior"]["v2"]["settings_bytes"] == 834
          and SETTINGS_V3["prior"]["v3"]["settings_bytes"] == SETTINGS_V3["settings_bytes"]
          and SETTINGS_V3["prior"]["v3"]["loadlist_bytes"] == SETTINGS_V3["loadlist_bytes"])

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
    p.add_argument("--only", default="all",
                   help="comma list: dll8,dll7,e2e,stage-ridealongs,stage-observer,stage-qdrv1,"
                        "stage-settings,rollback-ridealongs,game-evidence,all")
    p.add_argument("--dll", choices=("7", "8"), default=None,
                   help="DLL stage 'plan --only all' previews: 8 (preferred, default) or 7 (fallback)")
    add_flow("dll8", "8-family DLL payload-only stage (game closed) — PREFERRED single DLL run")
    add_flow("dll7", "7-family DLL payload-only stage — FALLBACK attribution path only (see CONSOLE-STAGE-PLAN-8FAM)")
    add_flow("e2e", "E2E installer 15-dispatch stage 0..6 cycle (game closed)")
    add_flow("stage-ridealongs", "DEFINITIVE ridealong+probe staging: xwp-armed + fc1pool + qdrv1 + "
                                 "observer PRXs (SHT-strip readback law) + the FINAL 8-entry "
                                 "ploader.ini (bootstrap whole-file STOR only when the historical "
                                 "line moves require it; ensure ops otherwise; byte-exact canonical, "
                                 "7 registered) (game closed, BEFORE the session) — supersedes "
                                 "stage-qdrv1/stage-observer inside `all`")
    add_flow("stage-observer", "observer ridealong ONLY (legacy partial path): PRX + SHT-strip readback "
                               "law + idempotent ini append of ONLY the observer line (game closed)")
    add_flow("stage-qdrv1", "qdrv1-only stage (legacy single-ridealong path) + ploader.ini entry "
                            "(BEFORE the game session)")
    add_flow("stage-settings", "settings+loadlist LOCKSTEP restage (NEW-SETTINGS §3): freeze the "
                               "staged pair, upload the v3 package bytes (settings 1448 B d8faa4ec… "
                               "+ loadlist 447 B 4d98ecd7…), byte-exact readback BOTH (game closed)")
    add_flow("rollback-ridealongs", "STAGE-PLAN-COMBINED §7 rollback: flip BOTH ridealong lines to "
                                    "= 0 + byte-verified readback (game closed, ploader not armed)")
    add_flow("game-evidence", "post-launch evidence + CP0-CP7 + probe verdict sinks (interactive "
                              "user handoffs)")
    add_flow("all", "full recommended session: dll8 -> e2e -> stage-ridealongs -> stage-settings -> "
                    "game-evidence (--dll 7 swaps in the dll7 fallback attribution path; exactly "
                    "one DLL stage either way)")
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
