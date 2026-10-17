#!/usr/bin/env python3
"""harvest.py -- per-launch PS5 UE4SS evidence harvest + marker analysis (l25 launch tests).

Written 2026-10-08 by agent-Y for the l25 prep harvest tooling. MAC-ONLY,
HOST-TESTED (--dry-run only). This script is RUN LATER BY ROOT after each
console launch; it never contacts the console during development (all local
testing is --dry-run, which performs zero network calls and zero file writes).

WHAT IT DOES (one command per launch):
  1. Pulls evidence files over read-only FTP (subprocess curl, --max-time 20
     per item, continue on per-item failure). NO SITE SELF is used anywhere
     because no executables are fetched (text logs only -- the ftpsrv
     decrypt-on-download rule cannot bite; l24 HARVEST-PLAN section 3 note).
  2. Picks up the local klog TCP capture (written in place by the separate
     klog tap at ps5-host-not-configured.invalid:3232) from ranked directories, newest first.
  3. Runs marker analysis (markers.json, same directory) over the pulled
     UE4SS.log + klog (+ diag/aligned/runtime-status where a marker says so).
  4. Writes HARVEST-<label>.md: timestamp, per-file pull status+size+sha256,
     session boundary analysis, every marker hit with line numbers +/- 1
     surrounding line, and an auto-filled VERDICT section (init-passed-:528,
     mod-started, crash-signature, admission, plus the L25 delete-listener
     fields -- listener registered / refused (code) / variant-B skip /
     engine called listener -- per agent-O-listener-instrumentation.md;
     marker catalog completed 2026-10-08 by agent-Z14 from the landed
     agent-O/agent-P/agent-Q reports. Mod-side TownfallItemSpawner verdict
     fields added 2026-10-08 by agent-AJ from LAUNCH-PLAN.md v3.3 +
     agent-AI-spawn-procedure.md: mod menu opened ('Menu created.' OR
     'spawnable pickups found'), item spawned ('Spawned ' marker; item
     name/qty/ID parsed), menu failure mode (UI creation failed /
     WidgetBlueprintLibrary not found. / Scan failed: / none). All additions
     are backward compatible: pre-mod logs read NO / none, never a false
     positive. zzdiag diagnostic-mod verdict added 2026-10-08 by agent-CK
     (MOD MENU UP acceptance package): a 'diag-zz' section summarizes
     '[Lua] [zz] ' lines when present (resident diag version, F7 presses,
     per-press A/B/C scan-chain state, E/F 30-char error chunks
     reassembled per run, truncation/gaps reported); absent lines read
     ABSENT - never a false positive.
     l26 fix-wave update 2026-10-08 by agent-EH (backward compatible):
     (a) diag-zz is now VERSION-AWARE - the resident version is read from
     the Z0 vN load banner; zzdiag v8 auto-probe tokens 'T<n> db=v/n/i/e
     pc=v/n/i/e' yield a 'database-live' verdict field (YES only when any
     db=v/valid observed) plus an 'auto-probe cycles' count (T-token
     lines, with per-cycle db/pc values and a went-live flag); v7 tokens
     (Z1/S1r/S1e/S2r/S2e/S3r/S3e/U1/U2r/U2e/U3r/F1r/F2r/F3r/A1r) are
     recognized with per-token meanings and CX-matrix press states; v4
     (D1 db=/D2/S1/S2/S3) and v6 (N-count) tokens are classified too;
     v7 24-char error tails (two-leading-space continuation lines) are
     captured and attributed to their token; E/F chunk reassembly now
     applies only to v2/v3 (v7's F<n>r tokens are FindFirstOf probes,
     NOT chunks). (b) an ACCEPTANCE summary line wires the three
     acceptance-ladder log rungs into one verdict: 'ACCEPTANCE: MOD MENU
     UP' only when scan-succeeded ('spawnable pickups found') AND menu
     created AND item spawned all hit (ACCEPTANCE-LADDER.md rungs 1-3;
     rung 4 klog-clean remains the separate crash-signature verdict).
     (c) --local-ue4ss PATH runs analysis-only regression over a saved
         log (no network, no file writes, report to stdout).
     DECODE-FIRST (agent GZ, 2026-10-08, Claude's NUL-interleave finding):
     every session-log read now goes through decode_session_log.py (same
     directory) BEFORE any marker/verdict analysis -- the deployed log
     writer is a dual-path writer (narrow UTF-8 literals + wide-doubled
     converted args: byte,NUL pairs; race interleaves messages mid-line),
     and plain substring scans over the raw bytes silently miss markers
     spanning wide runs (the agent-GO row-3 lesson: '[EU-VMT] arming
     refused: non-text-ent' was invisible to plain grep). Raw pulled
     files stay UNTOUCHED on disk (decode is in-memory only); per-file
     decode stats (nuls/wide-pairs/orphans) are recorded in the report.
     --raw restores the previous raw-scan behavior for diffing.

USAGE:
    python3 harvest.py --label NAME [--skip-klog] [--console-ip ps5-host-not-configured.invalid]
    python3 harvest.py --label NAME --local-ue4ss /tmp/<saved-log>   # regression:
                                                  # analyze a local UE4SS.log only,
                                                  # no pulls, no file writes, stdout report
    python3 harvest.py --label NAME --dry-run     # prints commands + marker
                                                  # table + report skeleton;
                                                  # no network, no file writes
    python3 harvest.py --label NAME --raw         # old raw-scan behavior (no
                                                  # decode) for diffing
    python3 harvest.py --help

EXIT CODES: 0 = report produced (read it for meaning, even with pull
failures); 2 = usage error (bad/missing --label); 3 = markers.json missing
or invalid.
"""

import argparse
import datetime
import glob
import hashlib
import json
import os
import re
import subprocess
import sys

# Decode-first wiring (agent GZ 2026-10-08): decode_session_log.py lives
# next to this file. RAW_MODE=True restores the pre-GZ raw-scan behavior
# (--raw) for diffing old vs new analysis output.
try:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import decode_session_log as _session_decode
except Exception:  # pragma: no cover - decoder missing = hard misbehavior
    _session_decode = None

RAW_MODE = False
DECODE_STATS = {}  # path -> stats dict (reported in Provenance)

# ---------------------------------------------------------------------------
# Encoded remote paths. Every constant cites its source (per the l25-prep
# task contract). All paths verified against saved provenance on 2026-10-08.
# ---------------------------------------------------------------------------

DEFAULT_CONSOLE_IP = "ps5-host-not-configured.invalid"
FTP_PORT = 2121  # ftpsrv on the console; every prior harvest used ftp://IP:2121
                 # (l16 harvest-first.log header "FTP=ftp://ps5-host-not-configured.invalid:2121";
                 #  l24 HARVEST-PLAN.md section 3).

# UE4SS main log.
# Source: l16 console-test SESSION.json harvest chain (harvest-first.log
# "fetched /data/plugins/UE4SS.log -> UE4SS.log (9253 bytes)") and l24
# HARVEST-PLAN.md section 3 item 1 (line 119).
UE4SS_REMOTE = "/data/plugins/UE4SS.log"

# modbridge runtime status (state 1 activated / state 2 failed).
# Source: task contract; l16 harvest-first.log "fetched
# /data/modbridge/roots/PPSA33286/runtime-status.txt -> runtime-status.txt
# (206 bytes)"; l24 HARVEST-PLAN.md section 3 item 2 (line 120).
# PPSA33286 = Silent Hill: Townfall title id (probe_config.h PROBE_TITLE).
RUNTIME_STATUS_REMOTE = "/data/modbridge/roots/PPSA33286/runtime-status.txt"

# l24-era supplier DIAG STREAM (JSONL, marker modbridge.supplier-diag/v1;
# pad tags, copyin/pool tags, r3_marshal_pumped heartbeat land here).
# Source: l16 console-test SESSION.json references the local harvest copy
# "supplier-diag-this-launch.jsonl"; the exact remote path is proven by the
# same session's harvest-first.log "fetched /data/modbridge-supplier-diag.jsonl
# -> modbridge-supplier-diag.jsonl (3266259 bytes)" and restated in l24
# HARVEST-PLAN.md section 3 item 3 (line 121: "/data/modbridge-supplier-diag.jsonl
# -> supplier-diag.jsonl"). Local l24-era copy verified: /tmp/l24-diag.jsonl
# line 1 marker "modbridge.supplier-diag/v1" (agent-F-crash-forensics.md).
DIAG_STREAM_REMOTE = "/data/modbridge-supplier-diag.jsonl"

# l24-era ALIGNED-ENTRY log (text sink of the diagnostic_event channel,
# marker modbridge-runtime-aligned-entry/v1 build=f6530c79...).
# Source: PROBE_LOG_PATH in
#   .superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/
#   generated/probe_config.h  ("#define PROBE_LOG_PATH
#   "/data/townfall_runtime_aligned_entry.log"")
# corroborated by the harvested local copy /tmp/l24-aligned.log whose first
# line carries exactly that PROBE_MARKER (agent-F-crash-forensics.md evidence
# list; l24 REVIEW-PACKET.md section 8 "diagnostic_event aligned-entry sink
# + probe_config.h"). The l16 SESSION.json provenance itself does not name
# this path -- the spike header + matching harvested marker is the citation.
ALIGNED_ENTRY_REMOTE = "/data/townfall_runtime_aligned_entry.log"

# klog: NOT an FTP pull. The klog stream is a raw TCP tap (ps5-host-not-configured.invalid:3232)
# that must be started BEFORE launch (l16 klog-process.log line 1:
# "[klog-capture] host=ps5-host-not-configured.invalid:3232 window=7200s file=..."; l24
# HARVEST-PLAN.md section 4). The harvest therefore picks up existing local
# capture files. agent-T-klog-capture.md (the authoritative ranked-path
# procedure) LANDED 2026-10-08 14:26 and the list below was updated to match
# it (agent-BE 2026-10-08, from agent-BD rehearsal section 7.2): agent-T's
# recipe captures with klog_capture.py --out-dir into per-test dirs
# (/tmp/l25-vanilla, /tmp/l25-variant-B, /tmp/l25-variant-A), and every
# l13/l14/l15/l16/l18/l22/l23-era tap landed beside its console-test session
# dir (outputs/<dated dir>/console-test-*/), so those are searched too
# (glob - cheap: <20 files as of 2026-10-08).
# RESOLUTION: the NEWEST klog-*.log (mtime) across ALL ranked dirs wins.
# (Was: first ranked dir with hits; changed because the agent-T family
# leaves stale captures in sibling out-dirs - e.g. an earlier /tmp/l25-vanilla
# klog must never shadow a newer variant-A/B tap. Tap files are create-only
# and never truncated, so mtime = how recently the tap wrote.)
#   1. this script's own directory (root may drop the tap output beside it)
#   2. outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-{A,B}/ --
#      DURABLE default since 2026-10-10 (agent-HA evidence-retention law:
#      prep_modded_launch.py now writes taps/monitors/readbacks there, never
#      /tmp; the search globs klog-*.log per dir, hence the explicit
#      per-variant entries)
#   3. /tmp/l25-harvest  (l24 tap OUT_DIR naming convention)
#   4. /tmp/l25-vanilla, /tmp/l25-variant-B, /tmp/l25-variant-A
#      (agent-T section 2 per-test out-dirs; LEGACY pre-2026-10-10 sessions)
#   5. /tmp/l24-harvest  (l24 tap OUT_DIR precedent, klog_capture.py:44)
#   6. outputs/*/console-test-*/ session dirs (KLOG_CONSOLE_TEST_GLOB)
# PATH-CONSTANT-ONLY CHANGE by agent-HA 2026-10-10: touches ONLY the ranked
# dirs tuple (log-parsing regions of this file are concurrently edited by
# agent-GZ; do not entangle).
KLOG_RANKED_DIRS = (
    os.path.dirname(os.path.abspath(__file__)),
    os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                 "evidence", "runtime", "l25-variant-B"),
    os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                 "evidence", "runtime", "l25-variant-A"),
    "/tmp/l25-harvest",
    "/tmp/l25-vanilla",
    "/tmp/l25-variant-B",
    "/tmp/l25-variant-A",
    "/tmp/l24-harvest",
)
# script is at outputs/<dated>/harvest/ -> three dirnames up is outputs/
KLOG_CONSOLE_TEST_GLOB = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "*", "console-test-*")
KLOG_TAP_NOTE = ("local TCP tap files klog-*.log (host ps5-host-not-configured.invalid:3232; "
                 "klog_capture.py --out-dir recipe per agent-T-klog-capture.md "
                 "section 2, landed 2026-10-08); newest file across ranked dirs wins")

CURL_MAX_TIME = "20"  # per-item curl cap (task contract)

# Session-boundary helpers (formats proven by the l24 evidence in
# agent-J-log-digest.md / agent-F-crash-forensics.md):
UE4SS_TS_RE = re.compile(r"^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]")  # UE4SS line prefix
# One banner = one UE4SS session. Verified format in /tmp/l24-ue4ss-4.log:2:
#   "[2026-10-08 13:47:23] UE4SS - v3.0.1 - Git SHA #e3ba101..."
UE4SS_BANNER_RE = re.compile(r"UE4SS\s*-?\s*v\d")
MODBRIDGE_LINE_RE = re.compile(r"\[t=\d+ms pid=\d+\]")                    # modbridge-format appended line
MFA_RE = re.compile(r"\bmfa_\w+")                                         # admission failure family
SHUTDOWN_LISTENER = "Adding shutdown listener"
MOD_STARTED = "[l22] Lua mod started"
CRASH_PATTERNS = ("copyin", "nonsleeping")

# --- L25 delete-listener instrumentation (agent-O-listener-instrumentation.md
# section 5; strings grepped from typescan-r4/candidate sources 2026-10-08) ---
L25_DL = "[L25][DeleteListener]"
# Either registration-returned post-marker = the AddUObjectDeleteListener call
# returned (shutdown family prints AFTER the last 'Adding shutdown listener').
REG_RETURNED_MARKERS = (
    "shutdown-listener registration returned in",
    "class-delete-listener registration returned in",
)
# 'REFUSED (<code>) : <reason>'; codes UObjectArray.cpp:436-442.
L25_REFUSED_RE = re.compile(re.escape(L25_DL) + r" REFUSED \((\d+)\)")
L25_REFUSAL_CODES = {
    "1": "null GUObjectArray",
    "2": "exception/layout-missing",
    "3": "insane header",
    "4": "duplicate",
    "5": "GMalloc unbound for growth",
}
L25_VARIANT_B = "delete-listener registration SKIPPED (variant B"
# G3 UObjectArray-cache suppression under variant B (agent-AO section 3 exact
# string) - like L25_VARIANT_B it proves the :528 REGION was reached, never
# that the registration call returned (no call is made under variant B).
L25_VARIANT_B_SUPPRESSION = "UObjectArray cache block suppressed under variant B"
# Any 'engine called F*DeleteListener::...' marker = direct engine-dispatch proof.
L25_ENGINE_CALLED_PREFIX = L25_DL + " engine called"
ENGINE_CALLED_LISTENERS = (
    "engine called FClassDeleteListener::NotifyUObjectDeleted",
    "engine called FShutdownDeleteListener::NotifyUObjectDeleted",
    "engine called FClassDeleteListener::OnUObjectArrayShutdown",
    "engine called FShutdownDeleteListener::OnUObjectArrayShutdown",
)

# --- Mod-side TownfallItemSpawner markers (added by agent-AJ 2026-10-08 from
# LAUNCH-PLAN.md v3.3 + agent-AI-spawn-procedure.md section 3; every string
# re-verified against the provenance main.lua copy 21415 B at
# outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/
# harvest-20261008T174239Z-25929d/main.lua; log() = main.lua:5-6 MOD_TAG
# prefix, rendered '[Lua] [TownfallItemSpawner] ...' per fx04 fixture) ---
MOD_TAG = "[TownfallItemSpawner]"
MOD_LOADED = MOD_TAG + " Loaded v1.0"          # :746 - mod script executed
MOD_SCAN_OK = "spawnable pickups found"        # :189 - first-open scan success
                                               # (NOT tag-prefixed: the rendered
                                               # line has the count between tag
                                               # and phrase - fx04 fixture)
MOD_SCAN_FAIL = "Scan failed:"                 # :194 - scan failure (mod context)
MOD_MENU_CREATED = MOD_TAG + " Menu created."  # :643 - UI built
MOD_UI_FAIL = "UI creation failed:"            # :636 - GATE-1 FName-pool suspect
MOD_WIDGETBP_MISSING = "WidgetBlueprintLibrary not found."  # :501 - dependency missing
MOD_SPAWNED = MOD_TAG + " Spawned "            # :335 - SPAWN PROOF (trailing space)
MOD_SPAWN_FAILED = MOD_TAG + " Failed at "     # :342 - spawn failure, stage in msg
# '[TownfallItemSpawner] Spawned <name> x<qty> ID=<id>' (fixture sample:
# 'Spawned Antibiotics x1 ID=DeprecatedItem'); name may contain spaces.
MOD_SPAWN_RE = re.compile(re.escape(MOD_TAG) +
                          r" Spawned (.+?) x(\d+) ID=(\S+)")
# '[TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>' with stage
# in {player, ground trace, load pickup class, spawn actor, configure pickup}.
MOD_SPAWN_FAILED_RE = re.compile(re.escape(MOD_TAG) + r" Failed at (.*?)\. ")
# Menu-failure modes in verdict precedence order (task contract:
# UI creation failed / WidgetBlueprintLibrary / Scan failed / none).
MENU_FAILURE_MODES = (
    ("ui-creation-failed", MOD_UI_FAIL),
    ("widgetbp-missing", MOD_WIDGETBP_MISSING),
    ("scan-failed", MOD_SCAN_FAIL),
)

# --- zzdiag diagnostic-mod summary (added by agent-CK 2026-10-08 for the
# MOD MENU UP acceptance package, outputs/2026-10-08-l25-prep/). Sources:
# console-resident v2 /tmp/zzdiag/Mods/zzdiag/Scripts/main.lua 1107 B
# (loadlist row 'ue4ss/Mods/zzdiag/Scripts/main.lua 1107', verified
# post-reboot per evidence/UFUNCTION-NULLPTR-FINDING.md); v3 =
# outputs/2026-10-08-l25-prep/zzdiag-v3/main.lua 2314 B; session evidence
# evidence/zzdiag-v2-session.log + /tmp/zzdiag2.log (2026-10-08 19:09).
# Rendered format '[Lua] [zz] <payload>' with payload '<series><n> <text>':
#   Z = script load ('Z0 v2' / 'Z0 v3' identifies the resident diag version)
#   K = keybind edge (K1 f7 press, K2 q queued)
#   A = FindFirstOf(ItemDatabaseSubsystem)  (A3 ok / A3 db-nil)
#   B = db:GetItemIDFromDeprecatedEnum      (B1 ok/no, B2 id=<short>,
#                                            B3 none = id ''/'None')
#   C = db:GetInteractablePickupIDActorPath (C1 ok/no, C2 <path>)
#   E/F = 30-char error-tail chunks for the B/C call errors (v2: E only;
#         v3 adds F). Chunk index resets to 0 per press.
# The console's wide-char Lua log writer collides with concurrent [l22]
# lines and drops tails MID-LINE (proven in the v2 session), so payloads
# are cut at the first interleave marker and truncated chunks are counted,
# never hidden; wide-char-written lines carry NUL bytes, so both the raw
# and the NUL-stripped line are matched. v3 contains NO GetClass/
# FindFunction probes (v2's crasher) - no other series can appear. ---
ZZ_RENDER_PREFIX = "[Lua] [zz] "
# Segment head parser. Digits optional: writer collisions can truncate a
# payload MID-SERIES (v2 session proof: '[zz] K1 f7' survived only as
# '[zz] K', same for A/B). Payloads are extracted by SPLITTING the line
# at the render prefix (a greedy per-line regex would swallow the later
# payloads that the writer collision merged into one physical line).
ZZ_HEAD_RE = re.compile(r"\A([A-Z])(\d*)[ ]?(.*)", re.DOTALL)
ZZ_INTERLEAVE_MARKERS = ("[2026-", "[l22]", "[Lua]", "[L25]", "[R3]",
                         "[l25]", "[l23]")
ZZ_CHUNK_SERIES = ("E", "F")
# Furthest-first chain states (used for the overall diag-zz verdict).
ZZ_CHAIN_RANK = ("CHAIN-OK", "C-FAIL", "B-FAIL", "A-OK-ONLY",
                 "A-FAIL", "IDLE")
ZZ_STATE_MEANINGS = {
    "CHAIN-OK": "FindFirstOf + both UFunction calls returned - the fixed "
                "scan chain works end-to-end",
    "C-FAIL": "B-call ok (id returned) but GetInteractablePickupIDActorPath "
              "failed - marshaling/second-resolution suspect (F chunks)",
    "B-FAIL": "GetItemIDFromDeprecatedEnum failed - UFunction resolution "
              "STILL NULL family (E chunks; the 2026-10-08 nullptr blocker)",
    "A-OK-ONLY": "FindFirstOf ok but the probe never reached B - closure "
                 "drained early or errored before B",
    "A-FAIL": "FindFirstOf returned nil - no save loaded / subsystem absent",
    "IDLE": "keybind fired but the closure never ran - game-thread pump "
            "suspect (same owner as a silent menu)",
}

# --- zzdiag v4/v6/v7/v8 recognition (added by agent-EH 2026-10-08, l26 fix
# wave; backward compatible - v2/v3 handling above is unchanged). Sources:
# v8 = task directive 2026-10-08 (auto-probe tokens 'T<n> db=v/n/i/e
# pc=v/n/i/e'; not yet deployed when written); v7 = agent-CX-v7-probe.md
# token table + interpretation matrix (staged outputs/.../zzdiag-v7/, run
# captured in /tmp/now3.log 2026-10-08 22:47); v4/v6 = session-proven token
# bodies (SESSION-L26V2-ANALYSIS.md sections 2/5/6, logs /tmp/l26v2.log +
# /tmp/v6-run.log; v4 = v3 chain + D/S tokens, v6 = N-count tokens). ---
# Resident version is read from the Z0 load banner ('Z0 v7' -> 7).
ZZ_VERSION_RE = re.compile(r"\bv(\d+)\b")
# db=/db- probe field (v8 'T<n> db=v pc=n', v4 'D1 db=invalid', v3 'A3
# db-nil'); pc= field (v8 PlayerController twin). Values canonicalized:
# v/valid = live instance; n/nil/none = no result; i/invalid = wrapper
# IsValid() false; e/err/error = pcall caught a throw.
ZZ_DB_FIELD_RE = re.compile(r"\bdb[=-]([A-Za-z]+)")
ZZ_PC_FIELD_RE = re.compile(r"\bpc[=-]([A-Za-z]+)")
ZZ_DB_VALUE_CANON = {
    "v": "valid", "valid": "valid",
    "n": "nil", "nil": "nil", "none": "nil",
    "i": "invalid", "invalid": "invalid",
    "e": "error", "err": "error", "error": "error",
}
ZZ_V7_TOKEN_MEANINGS = {
    "Z0": "script load banner (resident diag version)",
    "K1": "F7 keybind edge fired",
    "K2": "probe closure queued (ExecuteInGameThread)",
    "Z1": "IsKeyBindRegistered(Key.F7) - native-bridge sanity, no UObject/FName",
    "S1r": "StaticFindObject('/Script/Engine.PlayerController'):IsValid - y/n",
    "S2r": "StaticFindObject('/Script/Engine.GameEngine'):IsValid - y/n",
    "S3r": "StaticFindObject('/Script/Engine.KismetStringLibrary'):IsValid - y/n",
    "S1e": "S1 pcall ERROR - native throw on the static path (24-char tail follows)",
    "S2e": "S2 pcall ERROR",
    "S3e": "S3 pcall ERROR",
    "U1": "require('UEHelpers') + version - 'y v3' / 'n'",
    "U2r": "UEHelpers.GetKismetStringLibrary():IsValid - CDO path + shared-var bridge",
    "U2e": "U2 pcall ERROR",
    "U3r": "UEHelpers.GetEngine():IsValid - FindFirstOf('Engine') via an independent caller",
    "F1r": "FindFirstOf('PlayerController'):IsValid - y/n",
    "F2r": "FindFirstOf('GameEngine'):IsValid - y/n",
    "F3r": "FindFirstOf('KismetStringLibrary'):IsValid - n EXPECTED even when healthy (CDO excluded)",
    "A1r": "FindAllOf('PlayerController') ~= nil - real-nil contract (scan-vs-wrapper discriminator)",
    "T": "v8 auto-probe cycle: db=<ItemDatabaseSubsystem> pc=<PlayerController> validity verdicts (v/n/i/e)",
}
ZZ_V7_STATE_MEANINGS = {
    "RESOLUTION-HEALTHY": "S/F1/F2/U3/A1 all y (F3r n expected) - resolution "
                          "globally healthy at probe time; spawner failure is "
                          "timing/instance-specific (CX row 10)",
    "WRAPPER-BUG-SUSPECT": "A1r y but F1r n - the scan finds instances; the "
                           "F1 empty-wrapper result is a wrapper-construction "
                           "bug (CX row 8)",
    "SCAN-PATH-ONLY-BROKEN": "S y but F1/F2 n - path lookup fine, FindFirstOf "
                             "class-scan path specifically broken (Equals "
                             "compare / FindRootClassesByName / "
                             "IsValidObjectForFindXOf over-excluding) (CX row 2)",
    "FILTER-NOT-EXCLUDING": "F3r y - CDO returned: exclusion filter not "
                            "excluding / compare over-matching (CX row 7)",
    "S-PATH-ERRORS": "pcall caught a native throw on the StaticFindObject "
                     "path (S*e/U2e) - read the 24-char tail; the "
                     "GetPackageNameFromLongName family = the long-name parse "
                     "path throwing (CX row 9)",
    "COMMON-LAYER-BROKEN": "all S n + all F n - common layer broken: global "
                           "object cache / FUObjectHashTables / FName pool "
                           "admission (CX row 3)",
    "V7-PARTIAL": "v7 tokens present but no CX matrix row matched cleanly - "
                  "inspect raw tokens",
    "IDLE": "keybind fired but no probe token drained - game-thread pump "
            "suspect (CX row 1)",
}
ZZ_V4_TOKEN_MEANINGS = {
    "Z0": "script load banner (resident diag version)",
    "K1": "F7 keybind edge fired",
    "K2": "probe closure queued (ExecuteInGameThread)",
    "D1": "FindFirstOf('ItemDatabaseSubsystem') wrapper validity - db=valid/invalid "
          "(db=invalid is the CA-predicted MENU-STATE outcome: only CDOs live, "
          "excluded by IsValidObjectForFindXOf - CZ section 6)",
    "D2": "db:GetItemIDFromDeprecatedEnum probe - 'no' + pipe-chunked error text "
          "= the CN-hardened null-wrapper standard error (instance is the null side)",
    "S1": "KismetStringLibrary probe - ks=valid/invalid",
    "S2": "Conv_NameToString reachability probe - conv=yes/no",
    "S3": "S2 error text - pipe-separated 24-char chunks",
}
ZZ_V4_STATE_MEANINGS = {
    "DB-LIVE": "db probe returned valid - ItemDatabaseSubsystem live at probe "
               "time (save loaded)",
    "DB-NOT-LIVE": "every db= probe non-valid (v4/v8) - at menu state this is "
                   "the PREDICTED outcome (CDO-only, excluded); in-game it "
                   "would be a real resolution failure (CZ section 6)",
    "D2-FAIL": "db wrapper present but the GetItemIDFromDeprecatedEnum call "
               "failed (null-wrapper error text)",
    "IDLE": "keybind fired but no probe token drained - game-thread pump suspect",
}
ZZ_V6_STATE_MEANINGS = {
    "FINDALLOF-PARTIAL": "v6 N-count probes: at least one FindAllOf returned a "
                         "table (non-nil)",
    "ALL-FINDALLOF-NIL": "v6 N1pc/N2db/N3en/N4ks all -1 - all four FindAllOf "
                         "probes nil (menu-state predicted; CZ section 5)",
    "IDLE": "keybind fired but no probe token drained - game-thread pump suspect",
}
# Furthest-first rank per resident version (v2/v3 tuple reused for unknown).
ZZ_CHAIN_RANKS = {
    None: ("CHAIN-OK", "C-FAIL", "B-FAIL", "A-OK-ONLY", "A-FAIL", "IDLE"),
    4: ("DB-LIVE", "DB-NOT-LIVE", "D2-FAIL", "IDLE"),
    6: ("FINDALLOF-PARTIAL", "ALL-FINDALLOF-NIL", "IDLE"),
    7: ("RESOLUTION-HEALTHY", "WRAPPER-BUG-SUSPECT", "SCAN-PATH-ONLY-BROKEN",
        "FILTER-NOT-EXCLUDING", "S-PATH-ERRORS", "COMMON-LAYER-BROKEN",
        "V7-PARTIAL", "IDLE"),
    8: ("DB-LIVE", "DB-NOT-LIVE", "IDLE"),
}
ZZ_STATE_MEANINGS_BY_VERSION = {
    None: ZZ_STATE_MEANINGS, 4: ZZ_V4_STATE_MEANINGS,
    6: ZZ_V6_STATE_MEANINGS, 7: ZZ_V7_STATE_MEANINGS,
    8: ZZ_V4_STATE_MEANINGS,  # v8 shares the DB-LIVE/DB-NOT-LIVE vocabulary
}
ZZ_TOKEN_MEANINGS_BY_VERSION = {
    4: ZZ_V4_TOKEN_MEANINGS, 7: ZZ_V7_TOKEN_MEANINGS, 8: ZZ_V7_TOKEN_MEANINGS,
}

MAX_DISPLAYED_HITS = 60   # per marker per file; surplus summarised as "+N more"
CONTEXT_CHARS = 240       # trim long lines (klog diag rows) in the report


def now_utc() -> str:
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_of(path: str):
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 16), b""):
                h.update(chunk)
        return h.hexdigest()
    except OSError:
        return None


def read_lines(path: str):
    """DECODE-THEN-ANALYZE (agent GZ 2026-10-08): the file's raw bytes are
    decoded per the dual-path log-writer grammar (decode_session_log.py)
    BEFORE line splitting -- plain reads silently miss markers spanning
    wide-doubled runs. The on-disk file is NEVER modified (decode is
    in-memory); decode stats are recorded for the report. --raw restores
    the previous behavior for diffing. A file with zero NULs decodes to
    itself (clean host logs and klog taps are a strict no-op)."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    if not RAW_MODE and _session_decode is not None:
        if data.startswith(_session_decode.UTF8_BOM):
            data = data[len(_session_decode.UTF8_BOM):]
        decoded, stats = _session_decode.decode_bytes(data)
        stats.update(_session_decode.line_stats(data, decoded))
        DECODE_STATS[path] = stats
        text = decoded.decode("utf-8", errors="replace")
    else:
        DECODE_STATS[path] = {"mode": "raw", "bytes_in": len(data),
                              "nul_count": data.count(b"\x00")}
        text = data.decode("utf-8", errors="replace")
    return text.splitlines()


def load_markers(script_dir: str):
    path = os.path.join(script_dir, "markers.json")
    if not os.path.isfile(path):
        sys.stderr.write("ERROR: markers.json not found next to harvest.py: %s\n" % path)
        sys.exit(3)
    try:
        with open(path, "r", encoding="utf-8") as f:
            table = json.load(f)
    except (OSError, ValueError) as e:
        sys.stderr.write("ERROR: markers.json unreadable/invalid: %s\n" % e)
        sys.exit(3)
    markers = table.get("markers")
    if not isinstance(markers, list) or not markers:
        sys.stderr.write("ERROR: markers.json has no non-empty 'markers' list\n")
        sys.exit(3)
    return markers, path


# ---------------------------------------------------------------------------
# Pull plan
# ---------------------------------------------------------------------------

def build_pull_plan(console_ip: str):
    """Returns list of dicts: key, remote path, local filename template."""
    return [
        {"key": "ue4ss", "remote": UE4SS_REMOTE,
         "local": "harvest-{label}-ue4ss.log"},
        {"key": "runtime-status", "remote": RUNTIME_STATUS_REMOTE,
         "local": "harvest-{label}-runtime-status.txt"},
        {"key": "diag", "remote": DIAG_STREAM_REMOTE,
         "local": "harvest-{label}-supplier-diag.jsonl"},
        {"key": "aligned", "remote": ALIGNED_ENTRY_REMOTE,
         "local": "harvest-{label}-aligned-entry.log"},
    ]


def curl_cmd(console_ip: str, remote: str, dest: str):
    return ["curl", "-sS", "--max-time", CURL_MAX_TIME, "-o", dest,
            "ftp://%s:%d%s" % (console_ip, FTP_PORT, remote)]


def do_pull(cmd, dest):
    """Runs one curl pull. Continue-on-failure is the caller's job.
    dest is unlinked FIRST (fixed 2026-10-08, agent-BE, from agent-BD
    rehearsal section 7.3): a refused connection creates no file, but a
    MID-TRANSFER failure (timeout/partial) or a same-label re-run after an
    earlier success would otherwise leave a stale/partial dest that the
    analysis layer must never see as evidence."""
    try:
        if os.path.lexists(dest):
            os.unlink(dest)
    except OSError:
        pass
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=40)
        rc, err = proc.returncode, (proc.stderr or "").strip()
    except subprocess.TimeoutExpired:
        rc, err = -1, "local subprocess timeout"
    except OSError as e:  # curl missing, etc.
        rc, err = -1, "cannot execute curl: %s" % e
    exists = os.path.isfile(dest)
    size = os.path.getsize(dest) if exists else 0
    note = ""
    if rc == 0 and exists and size > 0:
        status = "ok"
    elif rc == 78 and exists is False:
        status, note = "absent", "file not present on console (curl rc 78)"
    elif exists and size == 0 and rc == 0:
        status, note = "empty", "0 bytes fetched (recorded, l23 autotune precedent)"
    else:
        status = "failed"
        note = "curl rc=%s %s" % (rc, err[:160])
    return {"status": status, "rc": rc, "size": size,
            "sha256": sha256_of(dest) if exists else None, "note": note}


# ---------------------------------------------------------------------------
# klog pickup (local files only -- never a network action)
# ---------------------------------------------------------------------------

def klog_search_dirs():
    """Ranked klog search dirs: the literal KLOG_RANKED_DIRS entries plus the
    console-test session dirs matched by KLOG_CONSOLE_TEST_GLOB (sorted for a
    stable display order; freshness is handled by newest-file resolution)."""
    return list(KLOG_RANKED_DIRS) + sorted(glob.glob(KLOG_CONSOLE_TEST_GLOB))


def resolve_klog(skip: bool):
    if skip:
        return {"status": "skipped", "path": None, "size": None, "sha256": None,
                "note": "--skip-klog given"}
    candidates = []  # (mtime, path)
    tried = []
    for d in klog_search_dirs():
        cand = glob.glob(os.path.join(d, "klog-*.log"))
        tried.append("%s (%d file(s))" % (d, len(cand)))
        for p in cand:
            try:
                candidates.append((os.path.getmtime(p), p))
            except OSError:
                pass  # tap rotated the file away between glob and stat
    if candidates:
        candidates.sort()
        path = candidates[-1][1]  # newest klog-*.log across ALL ranked dirs
        return {"status": "ok", "path": path,
                "size": os.path.getsize(path), "sha256": sha256_of(path),
                "note": "newest klog-*.log (mtime) across ranked dirs (agent-T "
                        "procedure landed 2026-10-08; a stale capture in a "
                        "higher-ranked dir can no longer shadow a newer tap); "
                        "dirs scanned in order: %s" % "; ".join(tried)}
    return {"status": "not-found", "path": None, "size": None, "sha256": None,
            "note": "no klog-*.log in any ranked dir (dirs scanned in order: %s). If a tap ran elsewhere, copy its file into one of the ranked dirs and re-run." % "; ".join(tried)}


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------

def analyze_sessions(lines):
    """Session boundary analysis over UE4SS.log lines."""
    if lines is None:
        return None
    last_ts = None
    last_ts_lineno = None
    banners = 0
    modbridge_lines = []
    for i, ln in enumerate(lines, 1):
        m = UE4SS_TS_RE.match(ln)
        if m:
            last_ts, last_ts_lineno = m.group(1), i
        if UE4SS_BANNER_RE.search(ln):
            banners += 1
        if MODBRIDGE_LINE_RE.search(ln):
            modbridge_lines.append(i)
    return {
        "last_timestamp": last_ts,
        "last_timestamp_lineno": last_ts_lineno,
        "ue4ss_banner_sessions": banners,
        "modbridge_appended_lines": modbridge_lines,
    }


def init_passed_528(lines, session_info):
    """Verdict input: is any expected marker present AFTER the last
    'Adding shutdown listener' line? (task contract; the :528 stall point is
    AddUObjectDeleteListener per agent-G-mod-start-gating.md section 5).
    Honest refinement: modbridge-format '[t=...ms pid=...]' lines that follow
    are APPENDED BY A LATER SESSION's failed module_start (agent-J section 5,
    l24 precedent) - they are reported but do NOT count as this session's
    init passing the stall point.
    L25 upgrade (agent-O, backward compatible): a registration-returned
    post-marker ('[L25][DeleteListener] shutdown-listener registration
    returned in ...ms') after the last 'Adding shutdown listener' proves the
    exact stall call RETURNED -> verdict YES even if '[l22] Lua mod started'
    is absent. Variant-B SKIPPED lines after it do NOT count (no call made).
    Z14 rule implemented 2026-10-08 (agent-BE, from agent-BD rehearsal
    section 6): lines matching L25_VARIANT_B (UnrealInitializer/Lua-mod
    'delete-listener registration SKIPPED (variant B') or the G3 suppression
    string L25_VARIANT_B_SUPPRESSION are bucketed into 'variant_b_skip' and
    EXCLUDED from the after-set: reaching the SKIPPED log only proves the
    :528 REGION was reached, not passed. They are still REPORTED (line
    numbers) in the verdict. Variant-A logs are unaffected: a
    registration-returned marker still lands in 'after' and still yields YES
    with the agent-O why."""
    if lines is None:
        return {"verdict": "UNKNOWN", "why": "UE4SS.log not available",
                "variant_b_skip": []}
    last_idx = None
    for i, ln in enumerate(lines):
        if SHUTDOWN_LISTENER in ln:
            last_idx = i
    if last_idx is None:
        return {"verdict": "NO",
                "why": "'%s' never appears - init did not reach PostInitialize end" % SHUTDOWN_LISTENER,
                "after": [], "appended": [], "variant_b_skip": []}
    after, appended, vb_skip = [], [], []
    for j, ln in enumerate(lines[last_idx + 1:], start=last_idx + 2):
        if not ln.strip():
            continue
        if MODBRIDGE_LINE_RE.search(ln):
            appended.append(ln.strip())
        elif L25_VARIANT_B in ln or L25_VARIANT_B_SUPPRESSION in ln:
            vb_skip.append((j, ln.strip()))
        else:
            after.append(ln.strip())
    if after:
        reg = [ln for ln in after if any(p in ln for p in REG_RETURNED_MARKERS)]
        if reg:
            return {"verdict": "YES",
                    "why": "L25 registration-returned marker after the last '%s' (line %d): "
                           "the AddUObjectDeleteListener stall call RETURNED - init passed :528 "
                           "even without '%s' (agent-O section 5 item 10)"
                           % (SHUTDOWN_LISTENER, last_idx + 1, MOD_STARTED),
                    "after": after[:10], "appended": appended[:5],
                    "variant_b_skip": vb_skip[:10]}
        return {"verdict": "YES",
                "why": "%d same-session line(s) after the last '%s' (line %d)" % (len(after), SHUTDOWN_LISTENER, last_idx + 1),
                "after": after[:10], "appended": appended[:5],
                "variant_b_skip": vb_skip[:10]}
    if vb_skip:
        return {"verdict": "NO",
                "why": "only the variant-B skip/suppression line(s) follow the last '%s' "
                       "(line %d) - the :528 region was REACHED, not passed: variant B makes "
                       "no registration call, and no 'Starting callback garbage collector!' "
                       "(:542) proof line follows (agent-O section 5; Z14 rule)"
                       % (SHUTDOWN_LISTENER, last_idx + 1),
                "after": [], "appended": appended[:5],
                "variant_b_skip": vb_skip[:10]}
    if appended:
        return {"verdict": "NO",
                "why": "the only line(s) after the last '%s' (line %d) are "
                       "modbridge-format [t=...ms pid=...] appends from a LATER "
                       "session's failed module_start - this session stalled at "
                       "the l24 signature point (agent-J section 5)" % (SHUTDOWN_LISTENER, last_idx + 1),
                "after": [], "appended": appended[:5], "variant_b_skip": []}
    return {"verdict": "NO",
            "why": "'%s' is the LAST non-empty line (line %d) - the l24 stall signature: nothing after init end" % (SHUTDOWN_LISTENER, last_idx + 1),
            "after": [], "appended": [], "variant_b_skip": []}


def scan_markers(markers, files):
    """files: {key: path or None}. Returns list of result dicts."""
    results = []
    cache = {}
    for m in markers:
        scan_keys = m.get("files", ["ue4ss", "klog"])
        entry = {"pattern": m["pattern"], "phase": m.get("phase", "?"),
                 "source": m.get("source", "?"), "meaning": m.get("meaning", ""),
                 "hits": {}, "total": 0}
        for key in scan_keys:
            path = files.get(key)
            if not path or not os.path.isfile(path):
                continue
            if key not in cache:
                cache[key] = read_lines(path)
            lines = cache[key]
            if lines is None:
                continue
            hit_lines = [i for i, ln in enumerate(lines, 1) if m["pattern"] in ln]
            if hit_lines:
                entry["hits"][key] = {
                    "path": path,
                    "count": len(hit_lines),
                    "shown": hit_lines[:MAX_DISPLAYED_HITS],
                    "surplus": max(0, len(hit_lines) - MAX_DISPLAYED_HITS),
                    # context[i] = (line before, hit line, line after)
                    "context": {i: (lines[i - 2] if i >= 2 else "",
                                    lines[i - 1],
                                    lines[i] if i < len(lines) else "")
                                for i in hit_lines[:MAX_DISPLAYED_HITS]},
                }
                entry["total"] += len(hit_lines)
        results.append(entry)
    return results


def listener_verdict(ue4ss_lines):
    """L25 delete-listener verdict fields (agent-O-listener-instrumentation.md
    section 5). Returns four sub-verdicts: registered (registration-returned
    post-markers), refused (REFUSED (code) lines, decoded), variant_b (SKIPPED
    line = MODBRIDGE_L25_SKIP_DELETE_LISTENER build), engine_called (any
    'engine called F*DeleteListener' marker = the engine dispatched our
    listener). All fields UNKNOWN when UE4SS.log is unavailable; on a pre-l25
    log (no L25 instrumentation) registered/engine_called read NO with an
    explicit 'pre-l25 or hung' why, never a false positive."""
    unknown = {"verdict": "UNKNOWN", "why": "UE4SS.log not available"}
    out = {"registered": dict(unknown, lines=[]),
           "refused": dict(unknown, codes=[]),
           "variant_b": dict(unknown, lines=[]),
           "engine_called": dict(unknown, listeners={})}
    if ue4ss_lines is None:
        return out
    reg = [(i, ln.strip()) for i, ln in enumerate(ue4ss_lines, 1)
           if any(p in ln for p in REG_RETURNED_MARKERS)]
    vb = [i for i, ln in enumerate(ue4ss_lines, 1) if L25_VARIANT_B in ln]
    # NB why-text fixed 2026-10-08 (agent-BE, from agent-BD rehearsal section
    # 7.1): when variant-B SKIPPED lines are present, registered=NO means
    # "no call was made BY DESIGN" - the pre-l25/hung/refused wording would
    # be misleading on a variant-B log.
    if reg:
        reg_why = ("L25 registration-returned marker(s) present - the "
                   "AddUObjectDeleteListener call(s) returned")
    elif vb:
        reg_why = ("no L25 registration-returned marker (pre-l25 build, "
                   "or variant-B build (registration skipped by design); "
                   "see 'variant-B skip detected' below)")
    else:
        reg_why = ("no L25 registration-returned marker (pre-l25 build, or "
                   "the call hung/refused at the stall point)")
    out["registered"] = {
        "verdict": "YES" if reg else "NO",
        "why": reg_why,
        "lines": reg[:10]}
    codes = []
    for i, ln in enumerate(ue4ss_lines, 1):
        m = L25_REFUSED_RE.search(ln)
        if m:
            codes.append({"line": i, "code": m.group(1),
                          "meaning": L25_REFUSAL_CODES.get(m.group(1), "unknown code")})
    out["refused"] = {
        "verdict": "YES" if codes else "NO",
        "why": ("clean refusal(s): registration skipped, init continues "
                "(codes below)" if codes else
                "no REFUSED line - no clean refusal recorded"),
        "codes": codes[:20]}
    out["variant_b"] = {
        "verdict": "YES" if vb else "NO",
        "why": ("delete-listener registration SKIPPED line(s) present - "
                "variant-B build (MODBRIDGE_L25_SKIP_DELETE_LISTENER): zero "
                "delete listeners registered" if vb else
                "no variant-B SKIPPED line"),
        "lines": vb}
    listeners = {}
    for name in ENGINE_CALLED_LISTENERS:
        hits = [i for i, ln in enumerate(ue4ss_lines, 1)
                if (L25_DL + " " + name) in ln]
        if hits:
            listeners[name] = hits
    out["engine_called"] = {
        "verdict": "YES" if listeners else "NO",
        "why": ("the engine dispatched into our listener(s) - direct "
                "engine-dispatch proof" if listeners else
                "no 'engine called' marker - engine has not (yet) dispatched "
                "into our listeners"),
        "listeners": listeners}
    return out


def mod_verdict(ue4ss_lines):
    """Mod-side (TownfallItemSpawner) verdict fields, added by agent-AJ
    2026-10-08 from LAUNCH-PLAN.md v3.3 + agent-AI-spawn-procedure.md
    section 3 (backward compatible: reads UNKNOWN without UE4SS.log and
    NO/none on pre-mod logs - no false positives). Returns three sub-verdicts:
      menu        - 'mod menu opened': '[TownfallItemSpawner] Menu created.'
                    OR '<N> spawnable pickups found' present (the F7 toggle
                    itself logs nothing; this pair is the only in-log
                    menu-open evidence).
      spawn       - 'item spawned': '[TownfallItemSpawner] Spawned ' marker
                    present; item name/qty/ID parsed when the line matches
                    'Spawned <name> x<qty> ID=<id>'. 'Failed at <stage>'
                    lines are collected alongside (spawn-pipeline failure
                    detail, they do not flip the verdict to YES).
      menu_failure- WHICH menu-failure marker hit, precedence
                    ui-creation-failed > widgetbp-missing > scan-failed,
                    'none' when no failure marker is present."""
    if ue4ss_lines is None:
        unknown = {"verdict": "UNKNOWN", "why": "UE4SS.log not available"}
        return {"menu": dict(unknown, lines=[]),
                "spawn": dict(unknown, lines=[], items=[], failed_at=[]),
                "menu_failure": dict(unknown, mode="unknown", hits={})}

    # mod menu opened: Menu created. OR spawnable-found present
    menu_lines = [i for i, ln in enumerate(ue4ss_lines, 1)
                  if (MOD_MENU_CREATED in ln or MOD_SCAN_OK in ln)]
    menu = {
        "verdict": "YES" if menu_lines else "NO",
        "why": ("menu-open marker(s) present ('%s' and/or '%s') - the F7 "
                "toggle's ExecuteInGameThread closure drained and the menu "
                "path executed" % (MOD_MENU_CREATED, MOD_SCAN_OK)
                if menu_lines else
                "neither '%s' nor '%s' - menu never opened (or the open "
                "never drained) in this log" % (MOD_MENU_CREATED, MOD_SCAN_OK)),
        "lines": menu_lines[:20],
    }

    # item spawned: Spawned marker present + parse name/qty/ID when possible
    spawned_lines = [i for i, ln in enumerate(ue4ss_lines, 1)
                     if MOD_SPAWNED in ln]
    items = []
    for i in spawned_lines:
        m = MOD_SPAWN_RE.search(ue4ss_lines[i - 1])
        if m:
            items.append({"line": i, "name": m.group(1),
                          "qty": m.group(2), "id": m.group(3)})
    failed_at = []
    for i, ln in enumerate(ue4ss_lines, 1):
        m = MOD_SPAWN_FAILED_RE.search(ln)
        if m:
            failed_at.append({"line": i, "stage": m.group(1)})
    if spawned_lines:
        why = ("'%s' present - spawn proof (name/qty/ID parsed for %d of "
               "%d line(s))" % (MOD_SPAWNED, len(items), len(spawned_lines)))
    elif failed_at:
        why = ("no '%s' line; %d 'Failed at <stage>' line(s) - spawn "
               "pipeline attempted and failed (stages below)"
               % (MOD_SPAWNED, len(failed_at)))
    else:
        why = ("'%s' absent - no spawn proof in this log" % MOD_SPAWNED)
    spawn = {"verdict": "YES" if spawned_lines else "NO",
             "why": why, "lines": spawned_lines[:20],
             "items": items[:20], "failed_at": failed_at[:20]}

    # menu failure mode: WHICH failure marker hit (precedence order), or none
    hits = {}
    for mode, pat in MENU_FAILURE_MODES:
        mode_lines = [i for i, ln in enumerate(ue4ss_lines, 1) if pat in ln]
        if mode_lines:
            hits[mode] = mode_lines
    if hits:
        mode = next(m for m, _ in MENU_FAILURE_MODES if m in hits)
        why = ("menu-failure marker(s) present - first by precedence "
               "(ui-creation-failed > widgetbp-missing > scan-failed): %s"
               % mode)
    else:
        mode = "none"
        why = ("no menu-failure marker ('%s' / '%s' / '%s') in this log"
               % (MOD_UI_FAIL, MOD_WIDGETBP_MISSING, MOD_SCAN_FAIL))
    menu_failure = {"verdict": mode.upper() if mode != "none" else "NONE",
                    "why": why, "mode": mode, "hits": hits}
    return {"menu": menu, "spawn": spawn, "menu_failure": menu_failure}


# --- Acceptance ladder summary (added by agent-EH 2026-10-08). The three
# log rungs already exist as mod-side verdicts; this wires them into the
# single 'ACCEPTANCE: MOD MENU UP' line per ACCEPTANCE-LADDER.md (rungs
# 1-3; rung 0 = admission/mod-start verdicts, rung 4 klog-clean = the
# separate crash-signature verdict). ---
MOD_LADDER_RUNGS = (
    ("scan-succeeded", MOD_SCAN_OK),      # rung 1: '<N> spawnable pickups found'
    ("menu created", MOD_MENU_CREATED),   # rung 2: 'Menu created.'
    ("item spawned", MOD_SPAWNED),        # rung 3: 'Spawned <name> x<qty> ID=<id>'
)


def acceptance_verdict(ue4ss_lines):
    """Three acceptance-ladder log rungs + the single summary line
    'ACCEPTANCE: MOD MENU UP' that fires only when ALL THREE hit
    (scan-succeeded AND menu created AND item spawned). Backward
    compatible: no UE4SS.log -> all rungs UNKNOWN, summary UNKNOWN;
    pre-mod/partial logs read NO per rung and 'NOT UP', never a false
    positive."""
    if ue4ss_lines is None:
        rungs = {name: {"verdict": "UNKNOWN",
                        "why": "UE4SS.log not available", "lines": []}
                 for name, _ in MOD_LADDER_RUNGS}
        return {"summary": "ACCEPTANCE: UNKNOWN (UE4SS.log not available)",
                "rungs": rungs,
                "note": "rungs 1-3 of ACCEPTANCE-LADDER.md only; rung 4 "
                        "(klog clean) is the separate crash-signature verdict"}
    rungs = {}
    for name, pattern in MOD_LADDER_RUNGS:
        hits = [i for i, ln in enumerate(ue4ss_lines, 1) if pattern in ln]
        rungs[name] = {
            "verdict": "YES" if hits else "NO",
            "why": ("'%s' present (%d hit(s))"
                    % (pattern, len(hits))) if hits else
                   ("'%s' absent - rung not hit in this log" % pattern),
            "lines": hits[:20]}
    states = ", ".join("%s=%s" % (name, rungs[name]["verdict"])
                       for name, _ in MOD_LADDER_RUNGS)
    if all(rungs[name]["verdict"] == "YES" for name, _ in MOD_LADDER_RUNGS):
        summary = "ACCEPTANCE: MOD MENU UP"
    else:
        summary = "ACCEPTANCE: MOD MENU NOT UP (%s)" % states
    return {"summary": summary, "rungs": rungs,
            "note": "log rungs 1-3 of ACCEPTANCE-LADDER.md; rung 0 is the "
                    "init/mod-started verdicts, rung 4 (klog clean) is the "
                    "separate crash-signature verdict"}


def _zz_cut(payload):
    """Cut one [zz] payload at the first interleaved foreign-line marker
    (the wide-char writer collision drops tails mid-line; v2 session
    proof: '[zz] E2 nullptr' + interleaved '[l22] ...' in one physical
    line). Returns (text, truncated?)."""
    cut = len(payload)
    for marker in ZZ_INTERLEAVE_MARKERS:
        pos = payload.find(marker)
        if pos != -1 and pos < cut:
            cut = pos
    return payload[:cut], cut < len(payload)


def _zz_canon_db(raw):
    return ZZ_DB_VALUE_CANON.get(raw.lower(), "other:%s" % raw.lower())


def _zz_token_id(e):
    """Stable token id for one [zz] event: series + index + status letter
    when the payload leads with 'r' or 'e' - the v7 result/error status
    convention ('S1r', 'S1e', 'F1r', 'A1r', 'U2e'). Payload words like
    K2's 'q', Z1's 'true' or U1's 'y v3' are NOT status letters, so K2/U1
    keep their bare ids. v4 'D1'/'D2', v8 'T3' and v2/v3 'A3'/'B1' are
    unchanged by this."""
    tok = e["series"] + (str(e["idx"]) if e["idx"] is not None else "")
    words = e["text"].split()
    if words and re.fullmatch(r"[re]", words[0]):
        tok += words[0]
    return tok


def _zz_token_value(e):
    """Payload value of one token: the word after the status letter when
    present ('S1r n' -> 'n'), else the payload's first word ('S1e' -> 'e')."""
    words = e["text"].split()
    if len(words) > 1:
        return words[1]
    if words:
        return words[0]
    return ""


def _zz_press_state(evts, version):
    """Chain state for one press's events, VERSION-AWARE (agent-EH 2026-10-08).
    v7: CX-matrix classification over S/U/F/A tokens (K1..K2 segment).
    v4/v8: DB-LIVE / DB-NOT-LIVE from the db= probe field (D1 / T tokens).
    v6: ALL-FINDALLOF-NIL / FINDALLOF-PARTIAL from N-count tokens.
    v2/v3 (+ unknown versions): the original A/B/C status-key logic with the
    E/F error chunks as durable fallback - byte-for-byte the previous
    behavior."""
    if version == 7:
        by_tok = {_zz_token_id(e): _zz_token_value(e) for e in evts}
        probed = any(t in by_tok for t in
                     ("Z1", "S1r", "S1e", "S2r", "S2e", "S3r", "S3e",
                      "U2r", "U2e", "U3r", "F1r", "F2r", "F3r", "A1r"))
        if not probed:
            return "IDLE"
        s_err = any(t in by_tok for t in ("S1e", "S2e", "S3e"))
        s_yes = any(by_tok.get(t) == "y" for t in ("S1r", "S2r", "S3r"))
        s_no = any(by_tok.get(t) == "n" for t in ("S1r", "S2r", "S3r"))
        f12_n = any(by_tok.get(t) == "n" for t in ("F1r", "F2r"))
        f3_y = by_tok.get("F3r") == "y"
        f1_n = by_tok.get("F1r") == "n"
        a1_y = by_tok.get("A1r") == "y"
        if s_err:
            return "S-PATH-ERRORS"
        if f3_y:
            return "FILTER-NOT-EXCLUDING"
        if a1_y and f1_n:
            return "WRAPPER-BUG-SUSPECT"
        if s_yes and f12_n:
            return "SCAN-PATH-ONLY-BROKEN"
        if s_no and f12_n:
            return "COMMON-LAYER-BROKEN"
        if s_yes and not f12_n and a1_y:
            return "RESOLUTION-HEALTHY"
        return "V7-PARTIAL"
    if version in (4, 8):
        db_raw = []
        for e in evts:
            if (version == 8 and e["series"] == "T") or \
               (version == 4 and e["series"] == "D" and e["idx"] == 1):
                m = ZZ_DB_FIELD_RE.search(e["text"])
                if m:
                    db_raw.append(m.group(1))
        d2 = any(e["series"] == "D" and e["idx"] == 2 for e in evts)
        if db_raw:
            return ("DB-LIVE" if any(_zz_canon_db(r) == "valid" for r in db_raw)
                    else "DB-NOT-LIVE")
        if d2:
            return "D2-FAIL"
        return "IDLE"
    if version == 6:
        vals = []
        for e in evts:
            if e["series"] == "N":
                m = re.search(r"=(-?\w+)", e["text"])
                if m:
                    vals.append(m.group(1))
        if vals:
            return ("FINDALLOF-PARTIAL"
                    if any(v not in ("-1", "nil", "n") for v in vals)
                    else "ALL-FINDALLOF-NIL")
        return "IDLE"
    # v2/v3 + unknown versions: original logic (unchanged).
    by_key = {}
    for e in evts:
        by_key.setdefault(e["series"] + str(e["idx"]), e["text"].strip())
    series_present = {e["series"] for e in evts}
    if "A3" in by_key and "db-nil" in by_key["A3"]:
        return "A-FAIL"
    if "C1" in by_key and "no" in by_key["C1"]:
        return "C-FAIL"
    if "C1" in by_key:  # C1 ok (path logged via C2)
        return "CHAIN-OK"
    if "B1" in by_key and "no" in by_key["B1"]:
        return "B-FAIL"
    if "B1" in by_key:  # B ok but no C lines
        return "A-OK-ONLY"
    if "F" in series_present:
        return "C-FAIL"
    if "E" in series_present:
        return "B-FAIL"
    if "A3" in by_key:  # A ok but no B lines
        return "A-OK-ONLY"
    return "IDLE"


def diag_zz_verdict(ue4ss_lines):
    """zzdiag verdict section (agent-CK 2026-10-08, MOD MENU UP acceptance
    package; version-aware extension by agent-EH 2026-10-08 for the l26 fix
    wave). Summarizes '[Lua] [zz] ...' lines when present: resident diag
    version (Z0 vN banner + v8 auto-probe), F7 presses (K1 count),
    per-press chain state (v2/v3 A/B/C ladder, v7 CX-matrix states, v4/v8
    db states, v6 N-count states), the E/F 30-char error chunks REASSEMBLED
    per run (v2/v3 ONLY - v7's F<n>r tokens are FindFirstOf probes, not
    chunks), v7 24-char error tails captured and attributed, a
    'database-live' verdict (YES only when any db=v/valid probe value is
    observed; v8 'T<n> db=...' cycles, v4 'D1 db=...', v3 'A3 db-nil' all
    feed it) and the v8 'auto-probe cycles' count with per-cycle db/pc
    values. Backward compatible: UE4SS.log absent -> UNKNOWN; no [zz]
    lines -> ABSENT (pre-zzdiag logs read ABSENT, never a false
    positive)."""
    _zero = {"version": [], "presses": 0, "press_states": [], "chunks": [],
             "resident_version": None, "database_live": None,
             "auto_probe": None, "token_meanings": {}, "tails": []}
    if ue4ss_lines is None:
        return dict({"verdict": "UNKNOWN",
                     "why": "UE4SS.log not available"}, **_zero)
    events = []
    for i, ln in enumerate(ue4ss_lines, 1):
        # NUL-strip is lossless for plain text and revives wide-char-written
        # [Lua] lines; split on the render prefix because writer collisions
        # MERGE several [zz] prints into one physical line (v2 session proof).
        view = ln.replace("\x00", "")
        if ZZ_RENDER_PREFIX not in view:
            continue
        for seg in view.split(ZZ_RENDER_PREFIX)[1:]:
            m = ZZ_HEAD_RE.match(seg)
            if m:
                text, truncated = _zz_cut(m.group(3))
                events.append({"line": i, "series": m.group(1),
                               "idx": int(m.group(2)) if m.group(2) else None,
                               "text": text, "truncated": truncated})
            elif seg[:1].isspace() and events:
                # v7 error-tail continuation (two leading spaces + first 24
                # chars of the pcall error, printed right after its S*e/U2e
                # token): attribute to the preceding token, never dropped.
                prev = events[-1]
                prev["tail"] = ((prev.get("tail") or "") + " " +
                                seg.strip()).strip()
            # else: interleave wrote over the payload head - skip (as before)
    if not events:
        return dict({"verdict": "ABSENT",
                     "why": "no '%s' lines - zzdiag not admitted/loaded in this "
                            "log (or a pre-zzdiag session)" % ZZ_RENDER_PREFIX},
                    **_zero)

    # Resident version from the Z0 banner ('Z0 v7' -> 7; max if several).
    # Z events with idx >= 1 are NOT load banners (v7's Z1 = keybind
    # sanity probe 'Z1 true'); only idx 0 / writer-truncated (idx None)
    # banners count.
    resident_version = None
    for e in events:
        if e["series"] == "Z" and e["idx"] in (None, 0):
            m = ZZ_VERSION_RE.search(e["text"])
            if m:
                v = int(m.group(1))
                resident_version = v if resident_version is None else max(resident_version, v)
    versions = [(e["line"], e["text"].strip()) for e in events
                if e["series"] == "Z" and e["idx"] in (None, 0)]
    # split into presses at each K1; events BEFORE the first K1 (the Z
    # load line etc.) attach to no press - they are reported via 'version'
    presses, cur = [], None
    for e in events:
        if e["series"] == "K" and e["idx"] == 1:
            if cur:
                presses.append(cur)
            cur = [e]
        elif cur is not None:
            cur.append(e)
    if cur:
        presses.append(cur)
    if not presses:
        # K1 boundary lost to a writer collision ('K1 f7' truncated to 'K',
        # v2 session proof): fall back to one press from all non-Z events.
        non_z = [e for e in events if e["series"] != "Z"]
        if non_z:
            presses = [non_z]
    press_states = [{"press": n + 1,
                     "state": _zz_press_state(p, resident_version),
                     "lines": (p[0]["line"], p[-1]["line"])}
                    for n, p in enumerate(presses)]

    # E/F chunk runs: v2/v3 ONLY (v2: E; v3: E+F). v4 uses inline
    # pipe-separated chunks; v6/v7/v8 have no 30-char chunk series at all
    # (v7's F<n>r are FindFirstOf probes - misreading them as chunks was
    # the pre-EH behavior this gate fixes). A new run starts when the
    # series changes or the chunk index resets to 0; an unknown index
    # (writer-truncated series letter) never resets.
    chunk_series = (ZZ_CHUNK_SERIES
                    if resident_version in (None, 2, 3) else ())
    chunks = []
    for e in events:
        if e["series"] not in chunk_series:
            continue
        if (not chunks
                or chunks[-1]["series"] != e["series"]
                or e["idx"] == 0):
            chunks.append({"series": e["series"], "first_line": e["line"],
                           "last_line": e["line"], "texts": [],
                           "indices": [], "truncated": 0})
        run = chunks[-1]
        run["last_line"] = e["line"]
        run["texts"].append(e["text"])
        if e["idx"] is not None:
            run["indices"].append(e["idx"])
        if e["truncated"]:
            run["truncated"] += 1
    for run in chunks:
        run["n"] = len(run["texts"])
        if run["indices"]:
            expected = list(range(max(run["indices"]) + 1))
            run["gaps"] = [i for i in expected if i not in run["indices"]]
        else:
            run["gaps"] = []
        run["reassembled"] = "".join(run["texts"])

    # database-live (agent-EH): YES only when some db=/db- probe value
    # canonicalizes to 'valid' (v8 'T<n> db=v', v4 'D1 db=invalid' style,
    # v3 'A3 db-nil'). Probed-but-never-valid vs no-db-probe are distinct
    # why-texts; both read NO.
    db_obs = []
    for e in events:
        m = ZZ_DB_FIELD_RE.search(e["text"])
        if m:
            db_obs.append({"line": e["line"], "token": _zz_token_id(e),
                           "raw": m.group(1),
                           "canon": _zz_canon_db(m.group(1))})
    if db_obs:
        any_valid = any(o["canon"] == "valid" for o in db_obs)
        values_seen = sorted({o["canon"] for o in db_obs})
        if any_valid:
            why = ("%d db= probe(s) observed, values: %s - at least one "
                   "db=v/valid: the ItemDatabaseSubsystem was LIVE at probe "
                   "time" % (len(db_obs), ", ".join(values_seen)))
        else:
            why = ("%d db= probe(s) observed, values: %s - never db=v/valid: "
                   "the item database was NOT live at any probe (at menu "
                   "state this is the predicted CDO-excluded outcome - CZ "
                   "section 6; in-game it is a real resolution failure)"
                   % (len(db_obs), ", ".join(values_seen)))
        database_live = {"verdict": "YES" if any_valid else "NO",
                         "why": why, "values_seen": values_seen,
                         "observations": db_obs[:20],
                         "n_probes": len(db_obs)}
    else:
        database_live = {
            "verdict": "NO",
            "why": ("no db= probe token in any [zz] line - the resident diag "
                    "version (%s) issues no database probe in this log"
                    % ("v%s" % resident_version if resident_version else
                       "unknown")),
            "values_seen": [], "observations": [], "n_probes": 0}

    # v8 auto-probe cycles (task directive 2026-10-08): one T<n> token per
    # cycle with db=/pc= verdicts; count + per-cycle values + went-live.
    t_evts = [e for e in events if e["series"] == "T"]
    auto_cycles = []
    for e in t_evts:
        dbm = ZZ_DB_FIELD_RE.search(e["text"])
        pcm = ZZ_PC_FIELD_RE.search(e["text"])
        auto_cycles.append({"line": e["line"], "cycle": e["idx"],
                            "db": _zz_canon_db(dbm.group(1)) if dbm else None,
                            "pc": _zz_canon_db(pcm.group(1)) if pcm else None,
                            "raw": e["text"].strip()})
    if t_evts:
        note = ("v8 auto-probe T<n> tokens: %d cycle(s), distinct cycle ids "
                "%s; db went live: %s" % (
                    len(t_evts),
                    sorted({c["cycle"] for c in auto_cycles
                            if c["cycle"] is not None}),
                    "yes" if any(c["db"] == "valid" for c in auto_cycles)
                    else "no"))
    else:
        note = ("no T-token auto-probe cycles in this log (resident diag "
                "%s; v4/v7 probe on F7 press, v8 auto-probes)"
                % ("v%s" % resident_version if resident_version else "unknown"))
    auto_probe = {"cycles": len(t_evts), "note": note,
                  "went_live": any(c["db"] == "valid" for c in auto_cycles),
                  "per_cycle": auto_cycles[:40]}

    # Token inventory with meanings (v4/v7/v8 tables; v2/v3 keep their
    # chunk/chain machinery and get no token-meaning entries).
    token_meanings = {}
    meanings_table = ZZ_TOKEN_MEANINGS_BY_VERSION.get(resident_version, {})
    for e in events:
        if e["series"] == "Z":
            continue  # version banner reported via 'version'
        tok = _zz_token_id(e)
        if tok not in token_meanings:
            token_meanings[tok] = {"count": 0, "first_line": e["line"],
                                   "meaning": meanings_table.get(tok, "")}
        token_meanings[tok]["count"] += 1

    # v7-style error tails attributed to their tokens.
    tails = [{"line": e["line"], "token": _zz_token_id(e), "text": e["tail"]}
             for e in events if e.get("tail")]

    rank = ZZ_CHAIN_RANKS.get(resident_version, ZZ_CHAIN_RANKS[None])
    state_meanings = ZZ_STATE_MEANINGS_BY_VERSION.get(
        resident_version, ZZ_STATE_MEANINGS)
    best = None
    for st in rank:
        if any(p["state"] == st for p in press_states):
            best = st
            break
    if best:
        why = ("zzdiag v%s lines present: %d press(es); furthest chain state "
               "%s - %s (per-press states, db/auto-probe fields and "
               "reassembled chunks below)"
               % (resident_version if resident_version is not None else "?",
                  len(presses), best, state_meanings.get(best, best)))
        verdict = "PRESENT:%s" % best
    elif versions:
        why = ("only the Z load line(s) present - the diag admitted and "
               "loaded but no F7 press was captured in this log")
        verdict = "PRESENT:LOADED-ONLY"
    else:
        why = ("[zz] lines present but no K1 press and no Z load line - "
               "partial capture; inspect raw lines")
        verdict = "PRESENT:PARTIAL"
    return {"verdict": verdict, "why": why, "version": versions,
            "resident_version": resident_version,
            "presses": len(presses), "press_states": press_states,
            "chunks": chunks, "database_live": database_live,
            "auto_probe": auto_probe, "token_meanings": token_meanings,
            "tails": tails}


def verdict(ue4ss_lines, klog_lines):
    """Auto-fill the four VERDICT rows (task contract)."""
    # init-passed-:528
    init = init_passed_528(ue4ss_lines, None)

    # mod-started
    mod = {"verdict": "UNKNOWN", "why": "UE4SS.log not available", "lines": []}
    if ue4ss_lines is not None:
        hits = [i for i, ln in enumerate(ue4ss_lines, 1) if MOD_STARTED in ln]
        mod = {"verdict": "YES" if hits else "NO",
               "why": ("'%s' present" % MOD_STARTED) if hits else
                      "'%s' absent - no Lua mod start in this log" % MOD_STARTED,
               "lines": hits}

    # crash-signature (klog copyin/nonsleeping hits)
    crash = {"verdict": "UNKNOWN", "why": "klog capture not available",
             "hits": {}}
    if klog_lines is not None:
        for pat in CRASH_PATTERNS:
            hits = [i for i, ln in enumerate(klog_lines, 1) if pat in ln]
            if hits:
                crash["hits"][pat] = hits
        crash["verdict"] = "YES" if crash["hits"] else "NO"
        crash["why"] = ("kernel-side copyin/nonsleeping evidence present"
                        if crash["hits"] else
                        "no copyin/nonsleeping hits in the klog capture")

    # admission (mfa lines)
    adm = {"verdict": "UNKNOWN", "why": "no analyzable evidence files", "lines": {}}
    srcs = {"ue4ss": ue4ss_lines, "klog": klog_lines}
    found_any = False
    have_any = False
    for key, lines in srcs.items():
        if lines is None:
            continue
        have_any = True
        for i, ln in enumerate(lines, 1):
            if MFA_RE.search(ln):
                adm["lines"].setdefault(key, []).append((i, ln.strip()[:CONTEXT_CHARS]))
                found_any = True
    if have_any:
        adm["verdict"] = "FAILED" if found_any else "OK"
        adm["why"] = ("mfa_* failure lines present - file admission failed"
                      if found_any else
                      "no mfa_* failure lines in UE4SS.log/klog")
    return {"init": init, "mod": mod, "crash": crash, "admission": adm,
            "listener": listener_verdict(ue4ss_lines),
            "modside": mod_verdict(ue4ss_lines),
            "diagzz": diag_zz_verdict(ue4ss_lines),
            "acceptance": acceptance_verdict(ue4ss_lines)}


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

def trim(s):
    s = s.strip()
    return s if len(s) <= CONTEXT_CHARS else s[:CONTEXT_CHARS - 3] + "..."


def generate_report(label, console_ip, pull_results, klog, session_info,
                    marker_results, verd, dry, local_ue4ss=None):
    out = []
    a = out.append
    a("# HARVEST-%s" % label)
    a("")
    a("- generated (UTC): %s" % now_utc())
    a("- session-log decode: %s" % ("RAW (--raw; legacy plain-scan behavior "
                                    "kept for diffing)" if RAW_MODE else
                                   "DECODE-FIRST (dual-path grammar; agent GZ "
                                   "2026-10-08)"))
    if local_ue4ss:
        a("- mode: LOCAL ANALYSIS (--local-ue4ss %s; no pulls, no writes; "
          "sha256 %s)" % (local_ue4ss,
                          (sha256_of(local_ue4ss) or "-")[:16] + "..."))
    else:
        a("- console: %s:%d (read-only FTP pulls; no SITE SELF -- text logs only)" % (console_ip, FTP_PORT))
        a("- mode: %s" % ("DRY-RUN (no commands executed, no files written; report to stdout only)" if dry else "live"))
    a("- klog source: %s" % (klog["path"] or "n/a"))
    a("")
    if local_ue4ss:
        a("## Pull status")
        a("")
        a("- n/a: analysis-only regression mode over the local log above "
          "(no console contact; marker analysis + verdicts only).")
        a("")
    else:
        a("## Pull status")
        a("")
        a("| key | source | local file | status | bytes | sha256 | note |")
        a("|---|---|---|---|---|---|---|")
        for r in pull_results:
            a("| %s | %s | %s | %s | %s | %s | %s |" % (
                r["key"], r["remote"], os.path.basename(r["dest"]),
                ("WOULD RUN" if dry else r["result"]["status"]),
                ("-" if dry else r["result"]["size"]),
                ("-" if dry else (r["result"]["sha256"] or "-")[:16] + "..."),
                ("cmd: %s" % " ".join(r["cmd"]) if dry else (r["result"]["note"] or ""))))
        a("| klog | %s | %s | %s | %s | %s | %s |" % (
            KLOG_TAP_NOTE, (os.path.basename(klog["path"]) if klog["path"] else "-"),
            klog["status"], (klog["size"] if klog["size"] is not None else "-"),
            (klog["sha256"][:16] + "..." if klog["sha256"] else "-"), ""))
        a("")
    if dry:
        a("## Session boundary analysis")
        a("")
        a("- not performed (dry-run; no files pulled).")
        a("- live definition: last `[%s]`-prefixed timestamp in UE4SS.log = "
          "current session init end; session count = UE4SS banner count "
          "(`UE4SS v<n>`), with modbridge `[t=...ms pid=...]` appended lines "
          "counted separately (failed module_start appends to the previous "
          "session's log; l24 precedent: agent-J-log-digest.md section 5)." % "YYYY-MM-DD HH:MM:SS")
        a("")
        a("## Marker analysis")
        a("")
        a("- not performed (dry-run). Marker table that WOULD be used "
          "(from markers.json): %d entries." % len(marker_results))
        a("")
        a("| pattern | phase | files | total hits | source |")
        a("|---|---|---|---|---|")
        for e in marker_results:
            a("| `%s` | %s | %s | - | %s |" % (
                e["pattern"], e["phase"], ",".join(e.get("scan_keys", [])),
                ("PENDING agent report" if e["source"].startswith("PENDING") else "cited")))
        a("")
        a("## VERDICT (skeleton -- auto-fills on the live run)")
        a("")
        a("- init-passed-:528?: auto (L25 registration-returned marker after the last '%s', "
          "or any marker/line present after it; variant-B SKIPPED/suppression lines are "
          "reported but NOT counted - Z14 rule)" % SHUTDOWN_LISTENER)
        a("- mod-started?: auto ('%s' present)" % MOD_STARTED)
        a("- crash-signature?: auto (klog copyin/nonsleeping hits)")
        a("- admission?: auto (mfa_* lines)")
        a("- listener registered?: auto (L25 'registration returned in' post-markers, agent-O)")
        a("- listener refused (code)?: auto (L25 'REFUSED (N)' lines; codes 1-5 decoded)")
        a("- variant-B skip detected?: auto (L25 'delete-listener registration SKIPPED' line)")
        a("- engine called listener?: auto (L25 'engine called F*DeleteListener' markers)")
        a("- mod menu opened?: auto ('%s' OR '%s' present; agent-AJ)" % (MOD_MENU_CREATED, MOD_SCAN_OK))
        a("- item spawned?: auto ('%s' present; name/qty/ID parsed when "
          "the line matches 'Spawned <name> x<qty> ID=<id>'; agent-AJ)" % MOD_SPAWNED)
        a("- menu failure mode?: auto (which of '%s' / '%s' / '%s' hit; "
          "'none' when no failure marker; agent-AJ)" % (MOD_UI_FAIL, MOD_WIDGETBP_MISSING, MOD_SCAN_FAIL))
        a("- diag-zz?: auto ('%s' lines summarized when present: resident "
          "diag version, F7 presses, per-press A/B/C chain state, E/F "
          "30-char error chunks reassembled per run; absent lines read "
          "ABSENT, never a false positive; agent-CK)" % ZZ_RENDER_PREFIX)
        a("- database-live?: auto (YES only when any db=v/valid probe value "
          "observed - v8 'T<n> db=...' auto-probe cycles, v4 'D1 db=...', "
          "v3 'A3 db-nil' all feed it; agent-EH)")
        a("- auto-probe cycles?: auto (v8 T-token count with per-cycle "
          "db/pc values + went-live flag; 0 when the resident diag does "
          "not auto-probe; agent-EH)")
        a("- diag-zz version-aware tokens: v7 S/U/F/A tokens carry per-token "
          "meanings + CX-matrix press states; v7 24-char error tails "
          "captured; E/F chunk reassembly applies to v2/v3 only (agent-EH)")
        a("- ACCEPTANCE: auto - 'ACCEPTANCE: MOD MENU UP' fires only when "
          "scan-succeeded ('%s') AND menu created ('%s') AND item spawned "
          "('%s') all hit (ACCEPTANCE-LADDER.md log rungs 1-3; agent-EH)"
          % (MOD_SCAN_OK, MOD_MENU_CREATED, MOD_SPAWNED))
        a("")
        return "\n".join(out)

    # --- live sections ---
    a("## Session boundary analysis (UE4SS.log)")
    a("")
    if session_info:
        a("- last `[%s]` timestamp: **%s** (line %s) = current session init end"
          % ("YYYY-MM-DD HH:MM:SS", session_info["last_timestamp"] or "none found",
             session_info["last_timestamp_lineno"]))
        a("- sessions (UE4SS banner count `UE4SS v<n>`): **%d**"
          % session_info["ue4ss_banner_sessions"])
        mb = session_info["modbridge_appended_lines"]
        if mb:
            a("- modbridge-format `[t=...ms pid=...]` lines: %d at line(s) %s "
              "(module_start attempts that append to a previous session's log; "
              "l24 precedent: agent-J section 5)" % (len(mb), ", ".join(map(str, mb[:10]))))
        else:
            a("- modbridge-format `[t=...ms pid=...]` lines: 0")
    else:
        a("- UE4SS.log not available; no boundary analysis.")
    a("")

    a("## Marker analysis")
    a("")
    any_hits = False
    for e in marker_results:
        if not e["total"]:
            continue
        any_hits = True
        a("### `%s` -- %d hit(s) (phase: %s)" % (e["pattern"], e["total"], e["phase"]))
        a("")
        a("- meaning: %s" % e["meaning"])
        a("- source: %s" % e["source"])
        for key, h in e["hits"].items():
            a("- file `%s` (%s): %d hit(s)" % (key, os.path.basename(h["path"]), h["count"]))
            for i in h["shown"]:
                prev, hit, nxt = h["context"][i]
                a("  - line %d:" % i)
                if prev.strip():
                    a("    - ctx-1: %s" % trim(prev))
                a("    - HIT : %s" % trim(hit))
                if nxt.strip():
                    a("    - ctx+1: %s" % trim(nxt))
            if h["surplus"]:
                a("  - ... and %d more hit(s) not shown (cap %d/file)" % (h["surplus"], MAX_DISPLAYED_HITS))
        a("")
    if not any_hits:
        a("- no marker hits in any available file.")
        a("")

    a("## VERDICT")
    a("")
    ac = verd.get("acceptance")
    if ac:
        a("- **%s**" % ac["summary"])
        for name, _ in MOD_LADDER_RUNGS:
            r = ac["rungs"].get(name, {})
            a("  - %s?: %s -- %s%s" % (
                name, r.get("verdict", "n/a"), r.get("why", ""),
                (" (line(s) %s)" % ", ".join(map(str, r.get("lines", []))))
                if r.get("lines") else ""))
        a("  - note: %s" % ac.get("note", ""))
    ini = verd["init"]
    a("- **init-passed-:528?** %s -- %s" % (ini["verdict"], ini["why"]))
    for ln in ini.get("after", [])[:10]:
        a("  - after: %s" % trim(ln))
    for ln in ini.get("appended", [])[:5]:
        a("  - later-session append (not same-session progress): %s" % trim(ln))
    for lineno, ln in ini.get("variant_b_skip", [])[:10]:
        a("  - variant-B skip/suppression line (REPORTED, not counted as "
          "post-:528 progress; line %d): %s" % (lineno, trim(ln)))
    md = verd["mod"]
    a("- **mod-started?** %s -- %s%s" % (md["verdict"], md["why"],
                                         (" (line(s) %s)" % ", ".join(map(str, md["lines"]))) if md.get("lines") else ""))
    cr = verd["crash"]
    a("- **crash-signature?** %s -- %s" % (cr["verdict"], cr["why"]))
    for pat, hits in cr.get("hits", {}).items():
        a("  - klog `%s`: %d hit(s), first at line %d" % (pat, len(hits), hits[0]))
    ad = verd["admission"]
    a("- **admission?** %s -- %s" % (ad["verdict"], ad["why"]))
    for key, rows in ad.get("lines", {}).items():
        for i, ln in rows[:10]:
            a("  - %s line %d: %s" % (key, i, ln))
    lv = verd.get("listener")
    if lv:
        rg = lv["registered"]
        a("- **listener registered?** %s -- %s%s" % (
            rg["verdict"], rg["why"],
            (" (line(s) %s)" % ", ".join(str(i) for i, _ in rg["lines"])) if rg["lines"] else ""))
        rf = lv["refused"]
        a("- **listener refused (code)?** %s -- %s" % (rf["verdict"], rf["why"]))
        for c in rf["codes"][:10]:
            a("  - line %d: code %s (%s)" % (c["line"], c["code"], c["meaning"]))
        vb = lv["variant_b"]
        a("- **variant-B skip detected?** %s -- %s%s" % (
            vb["verdict"], vb["why"],
            (" (line(s) %s)" % ", ".join(map(str, vb["lines"]))) if vb["lines"] else ""))
        ec = lv["engine_called"]
        a("- **engine called listener?** %s -- %s" % (ec["verdict"], ec["why"]))
        for name, hits in ec["listeners"].items():
            a("  - `%s`: %d hit(s), first at line %d" % (name, len(hits), hits[0]))
    ms = verd.get("modside")
    if ms:
        mn = ms["menu"]
        a("- **mod menu opened?** %s -- %s%s" % (
            mn["verdict"], mn["why"],
            (" (line(s) %s)" % ", ".join(map(str, mn["lines"]))) if mn["lines"] else ""))
        sp = ms["spawn"]
        a("- **item spawned?** %s -- %s%s" % (
            sp["verdict"], sp["why"],
            (" (line(s) %s)" % ", ".join(map(str, sp["lines"]))) if sp["lines"] else ""))
        for it in sp["items"][:10]:
            a("  - spawned item (line %d): %s x%s ID=%s"
              % (it["line"], it["name"], it["qty"], it["id"]))
        for fa in sp["failed_at"][:10]:
            a("  - spawn failed at stage '%s' (line %d)" % (fa["stage"], fa["line"]))
        mf = ms["menu_failure"]
        a("- **menu failure mode?** %s -- %s" % (mf["verdict"], mf["why"]))
        for mode, hits in mf["hits"].items():
            a("  - `%s`: %d hit(s), first at line %d" % (mode, len(hits), hits[0]))
    zz = verd.get("diagzz")
    if zz:
        a("- **diag-zz?** %s -- %s" % (zz["verdict"], zz["why"]))
        if zz.get("resident_version"):
            a("  - resident diag version: v%d" % zz["resident_version"])
        for line, text in zz.get("version", [])[:5]:
            a("  - diag load line (line %d): %s" % (line, text))
        db = zz.get("database_live")
        if db:
            a("  - **database-live?** %s -- %s" % (db["verdict"], db["why"]))
            for o in db.get("observations", [])[:10]:
                a("    - line %d token `%s`: db=%s (%s)"
                  % (o["line"], o["token"], o["raw"], o["canon"]))
        ap = zz.get("auto_probe")
        if ap:
            a("  - auto-probe cycles: %d -- %s" % (ap["cycles"], ap["note"]))
            for c in ap.get("per_cycle", [])[:12]:
                a("    - line %d cycle %s: db=%s pc=%s (`%s`)"
                  % (c["line"], c["cycle"], c["db"], c["pc"],
                     trim(c["raw"])))
        a("  - F7 presses captured: %d" % zz.get("presses", 0))
        for p in zz.get("press_states", [])[:10]:
            a("  - press %d (lines %d-%d): %s" % (
                p["press"], p["lines"][0], p["lines"][1], p["state"]))
        meanings = zz.get("token_meanings", {})
        if meanings:
            a("  - token inventory (resident-version table; counts):")
            for tok in sorted(meanings, key=lambda t: -meanings[t]["count"]):
                info = meanings[tok]
                meaning = info["meaning"] or "(no meaning-table entry)"
                a("    - `%s` x%d (first line %d): %s"
                  % (tok, info["count"], info["first_line"], meaning))
        for t in zz.get("tails", [])[:10]:
            a("  - error tail after `%s` (line %d): %s"
              % (t["token"], t["line"], t["text"]))
        for run in zz.get("chunks", [])[:10]:
            note = []
            if run["truncated"]:
                note.append("%d truncated chunk(s)" % run["truncated"])
            if run["gaps"]:
                note.append("missing index %s" %
                            ",".join(map(str, run["gaps"])))
            a("  - reassembled %s-run (%d chunk(s), line %d): `%s`%s" % (
                run["series"], run["n"], run["first_line"],
                trim(run["reassembled"]),
                (" (%s)" % "; ".join(note)) if note else ""))
        if (zz.get("resident_version") not in (None, 2, 3)
                and not zz.get("chunks")):
            a("  - (E/F chunk reassembly applies to zzdiag v2/v3 only; "
              "resident v%d has no 30-char chunk series - v7 F<n>r tokens "
              "are FindFirstOf probes)" % zz["resident_version"])
    a("")
    a("## Provenance")
    a("")
    a("- Encoded remote paths cite: l16 console-test SESSION.json + "
      "harvest-first.log (UE4SS.log, runtime-status.txt, "
      "/data/modbridge-supplier-diag.jsonl); probe_config.h PROBE_LOG_PATH "
      "(/data/townfall_runtime_aligned_entry.log); l24 HARVEST-PLAN.md "
      "section 3. klog = local TCP tap files (ps5-host-not-configured.invalid:3232); ranked "
      "dirs per agent-T-klog-capture.md (landed 2026-10-08): script dir, "
      "/tmp/l25-harvest, agent-T out-dirs (/tmp/l25-vanilla, "
      "/tmp/l25-variant-B, /tmp/l25-variant-A), /tmp/l24-harvest, and "
      "outputs/*/console-test-*/ session dirs; newest file wins.")
    a("- l26 fix-wave additions (agent-EH 2026-10-08): markers.json carries "
      "the five l26 fix markers ([FFO] query-fallback / [FFO] add-fallback / "
      "[UF] nofn / [UF] miss / [FN] ctor unready; sources agent-DA / "
      "agent-CP / agent-CN); diag-zz is version-aware (v8 auto-probe "
      "T<n> db=/pc= tokens -> database-live + auto-probe cycles; v7 "
      "S/U/F/A tokens with meanings + CX-matrix press states per "
      "agent-CX-v7-probe.md; v4/v6 session-proven tokens per "
      "SESSION-L26V2-ANALYSIS.md); the ACCEPTANCE summary wires "
      "ACCEPTANCE-LADDER.md log rungs 1-3 into one 'ACCEPTANCE: MOD MENU "
      "UP' line.")
    a("- Raw pulled files are preserved unchanged; this report is additive.")
    a("- Decode-first (agent GZ 2026-10-08): every analyzed session log is")
    a("  decoded in-memory per the dual-path writer grammar (narrow UTF-8 +")
    a("  wide-doubled byte,NUL runs; decode_session_log.py, self-tested)")
    a("  BEFORE marker/verdict scanning -- plain-grep misses spanning wide")
    a("  runs are impossible now. Raw files on disk are never rewritten.")
    if DECODE_STATS:
        a("- Decode stats per analyzed file:")
        for path in sorted(DECODE_STATS):
            st = DECODE_STATS[path]
            if st.get("mode") == "raw" or "wide_pairs" not in st:
                a("  - %s: raw mode (nuls=%s)" % (
                    os.path.basename(path), st.get("nul_count", "?")))
            else:
                a("  - %s: %d->%d bytes, nuls=%d wide-pairs=%d wide-runs=%d "
                  "orphan-nuls=%d, lines=%d (narrow=%d wide=%d mixed=%d)%s" % (
                      os.path.basename(path), st["bytes_in"], st["bytes_out"],
                      st["nul_count"], st["wide_pairs"], st["wide_runs"],
                      st["orphan_nuls"], st["lines_total"], st["lines_narrow"],
                      st["lines_wide"], st["lines_mixed"],
                      " partial-final-line" if st.get("partial_final_line") else ""))
    return "\n".join(out)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args(argv):
    p = argparse.ArgumentParser(
        prog="harvest.py",
        description="Per-launch PS5 UE4SS evidence harvest + marker analysis. "
                    "Pulls evidence over read-only FTP, picks up the local klog "
                    "tap file, runs marker analysis from markers.json, writes "
                    "HARVEST-<label>.md. --dry-run prints the plan only.")
    p.add_argument("--label", required=True,
                   help="launch label, e.g. l25-launch-1 (used in every output filename)")
    p.add_argument("--skip-klog", action="store_true",
                   help="skip klog capture pickup and klog marker analysis")
    p.add_argument("--console-ip", default=DEFAULT_CONSOLE_IP,
                   help="console FTP host (default %(default)s)")
    p.add_argument("--local-ue4ss", metavar="PATH", default=None,
                   help="analysis-only regression mode (agent-EH): analyze a "
                        "saved local UE4SS.log instead of pulling from the "
                        "console; no network, no file writes, report printed "
                        "to stdout (klog/diag verdicts read from this file "
                        "only; combine with --skip-klog for a quiet run)")
    p.add_argument("--dry-run", action="store_true",
                   help="print every command it would run + the marker table + "
                        "report skeleton; no network, no file writes")
    p.add_argument("--raw", action="store_true",
                   help="old pre-GZ behavior: analyze the raw bytes WITHOUT "
                        "decoding the dual-path NUL-interleaved session logs "
                        "(for diffing decode-first vs legacy verdicts)")
    args = p.parse_args(argv)
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", args.label):
        p.error("--label must be alnum/._- (used in filenames): %r" % args.label)
    return args


def main(argv):
    args = parse_args(argv)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    markers, markers_path = load_markers(script_dir)

    # Decode-first wiring (agent GZ): --raw restores legacy raw scanning.
    global RAW_MODE
    RAW_MODE = bool(args.raw)
    if not RAW_MODE and _session_decode is None:
        sys.stderr.write("WARNING: decode_session_log.py not importable next "
                         "to harvest.py -- falling back to raw scans (the "
                         "plain-grep miss class returns!)\n")

    # ---- local analysis-only regression mode (agent-EH 2026-10-08): no
    # pulls, no writes; marker analysis + verdicts over one saved log,
    # report to stdout. Exit 0. ----
    if args.local_ue4ss:
        path = os.path.abspath(os.path.expanduser(args.local_ue4ss))
        if not os.path.isfile(path):
            sys.stderr.write("ERROR: --local-ue4ss file not found: %s\n" % path)
            return 2
        lines = read_lines(path)
        if lines is None:
            sys.stderr.write("ERROR: --local-ue4ss unreadable: %s\n" % path)
            return 2
        klog_local = {"status": "skipped" if args.skip_klog else "not-found",
                      "path": None, "size": None, "sha256": None,
                      "note": "local analysis-only mode (--local-ue4ss): no "
                              "klog pickup"}
        files = {"ue4ss": path, "klog": None}
        verd = verdict(lines, None)
        report = generate_report(args.label, args.console_ip, [],
                                 klog_local, analyze_sessions(lines),
                                 scan_markers(markers, files), verd, dry=False,
                                 local_ue4ss=path)
        print(report)
        zz = verd.get("diagzz", {})
        db = zz.get("database_live") or {}
        ap = zz.get("auto_probe") or {}
        ac = verd.get("acceptance") or {}
        acc_short = ("UP" if ac.get("summary", "").startswith(
                         "ACCEPTANCE: MOD MENU UP")
                     else "UNKNOWN" if "UNKNOWN" in ac.get("summary", "")
                     else "NOT-UP")
        print("[harvest] local analysis of %s done (no writes)" % path)
        print("[harvest] verdict: diag-zz=%s database-live=%s "
              "auto-probe-cycles=%s acceptance=%s"
              % (zz.get("verdict", "n/a"), db.get("verdict", "n/a"),
                 ap.get("cycles", 0), acc_short))
        return 0

    plan = build_pull_plan(args.console_ip)
    for item in plan:
        item["dest"] = os.path.join(script_dir, item["local"].format(label=args.label))
        item["cmd"] = curl_cmd(args.console_ip, item["remote"], item["dest"])

    klog = resolve_klog(args.skip_klog)

    # ---- dry-run: plan only, no execution, no writes ----
    if args.dry_run:
        print("=== DRY-RAN (no network, no file writes) ===")
        print("label=%s console=%s markers=%s" % (args.label, args.console_ip, markers_path))
        print("")
        print("--- commands that WOULD run (one per line; per-item --max-time %s, continue on failure) ---" % CURL_MAX_TIME)
        for item in plan:
            print(" ".join(item["cmd"]))
        if args.skip_klog:
            print("(klog pickup skipped: --skip-klog)")
        else:
            print("(klog: no curl -- local tap pickup from ranked dirs + console-test session dirs, in order:)")
            for d in klog_search_dirs():
                print("  %s  [%d klog-*.log file(s) present now]" % (d, len(glob.glob(os.path.join(d, "klog-*.log")))))
            print("  -> resolved now (newest file across all ranked dirs): %s" % (klog["path"] or "none (harvest would report not-found)"))
        print("")
        print("--- marker table (%d entries; source labels: cited / PENDING) ---" % len(markers))
        for m in markers:
            print("  [%s] files=%s source=%s" % (
                m["pattern"], ",".join(m.get("files", ["ue4ss", "klog"])),
                "PENDING agent report" if str(m.get("source", "")).startswith("PENDING") else "cited"))
            print("      phase=%s :: %s" % (m.get("phase", "?"), m["meaning"][:150]))
        print("")
        print("--- report skeleton (HARVEST-%s.md) ---" % args.label)
        mr = [{"pattern": m["pattern"], "phase": m.get("phase", "?"),
               "source": m.get("source", "?"),
               "get_scan_keys": None, "scan_keys": m.get("files", ["ue4ss", "klog"])}
              for m in markers]
        print(generate_report(args.label, args.console_ip,
                              [{"key": i["key"], "remote": i["remote"],
                                "dest": i["dest"], "cmd": i["cmd"], "result": None}
                               for i in plan],
                              klog, None, mr, None, dry=True))
        return 0

    # ---- live: pull, analyze, write ----
    print("[harvest] label=%s console=%s start=%s" % (args.label, args.console_ip, now_utc()))
    for item in plan:
        print("[harvest] pulling %s -> %s" % (item["remote"], os.path.basename(item["dest"])))
        item["result"] = do_pull(item["cmd"], item["dest"])
        print("[harvest]   %s (%s bytes)" % (item["result"]["status"], item["result"]["size"]))

    # Analysis file set built ONLY from pulls that succeeded (status ok, or
    # 'empty' = 0 bytes fetched, recorded per l23 autotune precedent) -- a
    # failed pull's dest is never analyzed even if a stale/partial file
    # survives on disk (fixed 2026-10-08, agent-BE, from agent-BD rehearsal
    # section 7.3; do_pull also unlinks dest before each attempt).
    files = {i["key"]: (i["dest"]
                        if i.get("result", {}).get("status") in ("ok", "empty")
                        and os.path.isfile(i["dest"])
                        else None)
             for i in plan}
    if klog["status"] == "ok":
        files["klog"] = klog["path"]
    else:
        files["klog"] = None

    ue4ss_lines = read_lines(files["ue4ss"]) if files["ue4ss"] else None
    klog_lines = read_lines(files["klog"]) if files["klog"] else None

    session_info = analyze_sessions(ue4ss_lines)
    marker_results = scan_markers(markers, files)
    # NB: fixed 2026-10-08 (agent-Z14) -- this call previously passed
    # (marker_results, ue4ss_lines, klog_lines) into the 2-parameter
    # verdict(); dry-run never reaches it, so the live path would have
    # raised TypeError before producing a report.
    verd = verdict(ue4ss_lines, klog_lines)

    report = generate_report(args.label, args.console_ip, plan, klog,
                             session_info, marker_results, verd, dry=False)
    report_path = os.path.join(script_dir, "HARVEST-%s.md" % args.label)
    with open(report_path, "w", encoding="utf-8") as f:
        f.write(report + "\n")
    print("[harvest] report: %s" % report_path)
    lv = verd.get("listener", {})
    ms = verd.get("modside", {})
    zz = verd.get("diagzz", {})
    db = zz.get("database_live") or {}
    ap = zz.get("auto_probe") or {}
    ac = verd.get("acceptance") or {}
    acc_short = ("UP" if ac.get("summary", "").startswith(
                     "ACCEPTANCE: MOD MENU UP")
                 else "UNKNOWN" if "UNKNOWN" in ac.get("summary", "")
                 else "NOT-UP")
    # l26 fix-marker roll-up (agent-EH): counts for the five new markers,
    # derived from marker_results so no extra file reads happen.
    fix_counts = {}
    for pat in ("[FFO] query-fallback", "[FFO] add-fallback", "[UF] nofn",
                "[UF] miss", "[FN] ctor unready"):
        fix_counts[pat] = sum(h["count"] for h in (
            next((e["hits"].values() for e in marker_results
                  if e["pattern"] == pat and e["hits"]), [])))
    print("[harvest] verdict: init=%s mod-started=%s crash=%s admission=%s "
          "listener-registered=%s listener-refused=%s variant-b=%s engine-called=%s "
          "menu-opened=%s item-spawned=%s menu-failure=%s diag-zz=%s "
          "database-live=%s auto-probe-cycles=%s acceptance=%s" % (
              verd["init"]["verdict"], verd["mod"]["verdict"],
              verd["crash"]["verdict"], verd["admission"]["verdict"],
              lv.get("registered", {}).get("verdict", "n/a"),
              lv.get("refused", {}).get("verdict", "n/a"),
              lv.get("variant_b", {}).get("verdict", "n/a"),
              lv.get("engine_called", {}).get("verdict", "n/a"),
              ms.get("menu", {}).get("verdict", "n/a"),
              ms.get("spawn", {}).get("verdict", "n/a"),
              ms.get("menu_failure", {}).get("mode", "n/a"),
              zz.get("verdict", "n/a"),
              db.get("verdict", "n/a"), ap.get("cycles", 0), acc_short))
    print("[harvest] l26-fix-markers: %s" % " ".join(
        "%s=%d" % (p.split("] ", 1)[-1].replace(" ", "-"), n)
        for p, n in fix_counts.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
