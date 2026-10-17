#!/usr/bin/env python3
"""prep_modded_launch.py -- one-command launch preparation for the PS5 UE4SS
l25 variant A/B tests (2026-10-08-l25-prep).

Written 2026-10-08 by agent-Z16. MAC-ONLY, HOST-TESTED (--dry-run only).
This script is EXECUTED LATER BY ROOT; during development it never contacts
the console (--dry-run performs zero network calls and writes nothing).

WHAT IT DOES (enforces LAUNCH-PLAN.md sections 1 + STEP 0.4 + SAFETY BOX,
INTEGRATION-CHECKLIST.md "Console preconditions", the agent-Z2 ploader
arming law, and the evidence/VANILLA-VERDICT.md post-hoc operator law --
each step VERIFIED before proceeding, ABORT on any mismatch):

  1. Preconditions: processes_list has kstuff/pldmgr/ftpsrv/klogsrv; NO
     eboot.bin running; ploader not resident mid-window (FTP RETR of
     /data/plugins/ploader.lock; curl rc 78 / RETR 550 = none; a LIVE lock
     pid = resident -> abort and tell the operator to wait <=120 s for the
     INJECTOR_TIMEOUT_S exit, per agent-Z2-ploader-timing.md).
  2. PRX safety gate (--prx-path): exists, sha256 recorded, magic must be a
     SIGNED self (4f153d1d / 5414f5ee). Plain ELF 7f454c46 -> STOP with
     'unsigned ELF -- run make_fself.py first'.
  3. Decrypt-OFF backup readback of the current resident PRX, upload the
     new PRX to /data/plugins/modbridge_supplier_composition.prx, then
     decrypt-OFF readback (-Q 'SITE SELF'): magic + FULL sha256 byte-exact.
     On mismatch: one verified restore-from-backup attempt, then ABORT --
     a broken executable is never left in place.
  4. Settings restore: the pinned l24-settings-backup.ini (durable-first
     resolution: ../evidence/ copy preferred over the legacy /tmp copy;
     MUST be exactly 1947
     bytes, sha256 b980cea7... -- verified BEFORE upload) ->
     /data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini; readback +
     cmp; abort on any drift (modforge MFA size-exact admission law).
  5. ploader.ini: restore the ENABLED state from the pinned
     ploader-ini-backup-20261008.ini (durable-first: ../evidence/ copy
     preferred over legacy /tmp; sha256 3eab9510...; verified
     before upload, =2 row for modbridge_supplier_composition.prx required)
     -> /data/plugins/ploader.ini; readback + cmp.
  5.5 LOADLIST ROW DEPLOY GATE (Claude round-8 item 6; the 2026-10-11 -6
     incident, CLAUDE-ROUND4-SPAWN-PROVEN.md: the loadlist declared
     ueselftest/enabled.txt which was never uploaded -> MFA refused the
     WHOLE module at boot and the launch was wasted). Runs the sibling
     harvest/prelaunch_rowcheck.py: fetch the console
     /data/modbridge/roots/PPSA33286/loadlist.txt, parse EVERY
     'file\t<role>\t<path>\t<size>' row, fetch each file, and verify it
     EXISTS at its EXACT declared size. Any missing/mismatched/malformed
     row -> ABORT with the exact row and the fix ('upload the missing
     rows or fix the loadlist') -- the user instruction sequence is NEVER
     handed off on a failing loadlist. Valid only with the game closed
     (enforced since STEP 1: no eboot.bin; FTP reads return empty under
     game load). This step is MANDATORY -- no skip flag exists.
  6. Arm ploader: ONE dispatch of
     .../loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf,
     then poll processes_list up to 10 s for a FRESH payload.elf pid;
     abort if none appears. (Ini-edit law: the dispatch is REQUIRED after
     any ini change -- the ini is read exactly once, at arm time.)
  7. Start the klog tap in the background (klog_capture.py, agent-T
     procedure, --out-dir <session out-dir, DURABLE by default:
     outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-<X>/>
     --window-seconds 3600;
     capture file path confirmed on disk) and the eboot monitor loop
     (15 s processes_list poll, transitions logged, bounded window) --
     both detached, both recorded.
  8. Emit LAUNCH-READY-<variant>-<timestamp>.md next to this script: every
     verification result, hashes, pids, capture paths, and the user
     instruction sequence (menu 90 s -> load save -> 30 s -> L3+R3 once ->
     clean exit; variant B notes the SKIPPED listener expectation).
  9. --dry-run prints every command + every check with its expected value;
     local read-only checks are really executed (no writes); exit 0.

NEVER: touches autoload.txt, kills any process (no /process_kill anywhere),
bypasses a failed verification, or duplicate-sends the loadpayload dispatch.

USAGE:
    python3 prep_modded_launch.py --variant B --prx-path /path/to/variant-B.prx
    python3 prep_modded_launch.py --variant A --prx-path /path/to/variant-A.prx
    python3 prep_modded_launch.py --variant A --prx-path P --dry-run   # plan only
    python3 prep_modded_launch.py --variant A --prx-path P --skip-klog # no tap
    python3 prep_modded_launch.py --help

EVIDENCE-RETENTION CHANGE (agent-HA, 2026-10-10; see
outputs/2026-10-08-l25-prep/EVIDENCE-RETENTION.md -- Claude's law: nothing
evidential lives in /tmp):
  * Session out-dir DEFAULT moved from /tmp/l25-variant-<X>/ to the durable
    tree outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-<X>/.
    Everything the run produces -- klog tap files, eboot-monitor logs,
    decrypt-off PRX readbacks, settings/ploader readbacks, arm responses --
    now lands durable BY DEFAULT. --out-dir still overrides (CLI contract
    unchanged; the internal --run-eboot-monitor mode always passes it).
  * SETTINGS_LOCAL / PLOADER_INI_LOCAL now resolve DURABLE-FIRST: the
    hash-identical copies in ../evidence/ (verified by GU 2026-10-10 and
    re-verified by HA 2026-10-10) are preferred; the legacy /tmp copies are
    only a last-resort fallback and are never the only copy the script
    depends on. The sha256+size gates below are UNCHANGED and remain the
    authority either way.

LAUNCH ORDER v3 (Claude review 2, 2026-10-08; LAUNCH-PLAN.md "Launch order
v3 -- B first"): variant B is prepared and launched FIRST (it skips the
registration entirely -- no TArray Append, no first live GMalloc call from
that path -- and is the path to the first real look at mod start, the pump,
the pad, and the menu); variant A runs SECOND, only to confirm the
mechanism, with the extended-A-logging build. Mac-only items 1-4 (offset
verification, GMalloc audit, container audit, extended A logging) must land
BEFORE any launch. Variant B stays diagnostic-only and unsafe long-term;
never leave B resident. This changes ORDER AND DOCS ONLY: the script's
gates, checks, commands, and exit codes are identical for either variant.

INTERNAL (spawned by step 7; do not call by hand):
    python3 prep_modded_launch.py --run-eboot-monitor --out-dir D \
        --poll-seconds 15 --window-seconds 3600

EXIT CODES: 0 = dry-run complete, or live run fully verified (LAUNCH-READY
written); 1 = verification ABORT (message says exactly which gate failed;
on abort a LAUNCH-PREP-FAILED-<variant>-<ts>.md receipt is written next to
this script so the failure is durable); 2 = usage error.
"""

import argparse
import datetime
import hashlib
import json
import os
import subprocess
import sys
import time

# ---------------------------------------------------------------------------
# Path constants + expected values. Every constant cites its source; style
# mirrors harvest.py in this directory (agent-Y) and the proven l22/l23
# console receipts. Verified against local evidence 2026-10-08.
# ---------------------------------------------------------------------------

DEFAULT_CONSOLE_IP = "ps5-host-not-configured.invalid"  # LAUNCH-PLAN.md header
HTTP_PORT = 8084                     # pldmgr HTTP (LAUNCH-PLAN "Console endpoints")
FTP_PORT = 2121                      # ftpsrv      (harvest.py FTP_PORT)
KLOG_PORT = 3232                     # klogsrv TCP tap (agent-T section 1)

PROCESSES_PATH = "/processes_list"   # LAUNCH-PLAN 1.1 (alias /api/processes)
ARM_URL_PATH = ("/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2F"
                "ploader%2Fploader.elf")  # LAUNCH-PLAN 1.3 -- dispatch ONCE

PRX_REMOTE = "/data/plugins/modbridge_supplier_composition.prx"  # task + l22 receipt
PLOADER_INI_REMOTE = "/data/plugins/ploader.ini"                 # Z2 L25 / T section 5
PLOADER_LOCK_REMOTE = "/data/plugins/ploader.lock"               # Z2 receipts (rc 78/550)
SETTINGS_REMOTE = ("/data/modbridge/roots/PPSA33286/ue4ss/"
                   "UE4SS-settings.ini")                         # LAUNCH-PLAN 1.2

# EVIDENCE-RETENTION (agent-HA 2026-10-10): settings/ploader pinned backups
# resolve DURABLE-FIRST. ../evidence/ copies are byte-identical to the /tmp
# originals (GU manifest verified 2026-10-10, re-verified by HA 2026-10-10);
# the sha256+size gates below stay authoritative whichever copy is used.
_PREP_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EVIDENCE_RUNTIME_ROOT = os.path.join(_PREP_ROOT, "evidence", "runtime")
SETTINGS_LOCAL_CANDIDATES = (
    os.path.join(_PREP_ROOT, "evidence", "l24-settings-backup.ini"),  # durable
    "/tmp/l24-settings-backup.ini",                                    # legacy
)
SETTINGS_LOCAL = next((c for c in SETTINGS_LOCAL_CANDIDATES
                       if os.path.isfile(c)), SETTINGS_LOCAL_CANDIDATES[0])
SETTINGS_SHA256 = ("b980cea75c46a390a2182b642e4991fcc830e7401a"
                   "fc740c086ca20c359d3ce2")              # LAUNCH-PLAN 1.2 (full)
SETTINGS_SIZE = 1947                                     # MFA size-exact law

PLOADER_INI_LOCAL_CANDIDATES = (
    os.path.join(_PREP_ROOT, "evidence", "ploader-ini-backup-20261008.ini"),
    "/tmp/ploader-ini-backup-20261008.ini",
)
PLOADER_INI_LOCAL = next((c for c in PLOADER_INI_LOCAL_CANDIDATES
                          if os.path.isfile(c)), PLOADER_INI_LOCAL_CANDIDATES[0])
PLOADER_INI_SHA256 = ("3eab951051082de57de3dc3708b3c7210ae8ba4d99ad"
                      "44b87e0d8c92e0cbb3d5")               # VANILLA-VERDICT.md (full)
PLOADER_INI_SIZE = 440   # observed byte size of the pinned backup, 2026-10-08
                         # (sha256 is the gate; size recorded as corroboration)
PLOADER_INI_INJECT_ROW = "/data/plugins/modbridge_supplier_composition.prx = 2"

# SELF magic (SAFETY BOX 1/4; l22 receipt "magic": "4f153d1d").
MAGIC_SIGNED = (bytes.fromhex("4f153d1d"), bytes.fromhex("5414f5ee"))
MAGIC_ELF = bytes.fromhex("7f454c46")  # plain ELF = unsigned download view
UNSIGNED_ELF_MESSAGE = ("unsigned ELF -- run make_fself.py first "
                        "(AGENTS.md PS5 rule 3: fake-sign before upload; "
                        "uploading it back unsigned bricks the game)")

# Daemon baseline (task step 1; names as observed in saved processes_list
# receipts, e.g. l22 processes.before.json: kstuff.elf/pldmgr.elf/
# ftpsrv.elf/klogsrv.elf). Match is on the name without the .elf suffix.
REQUIRED_DAEMONS = ("kstuff", "pldmgr", "ftpsrv", "klogsrv")
GAME_PROC_NAME = "eboot.bin"    # l23 20261008T173644Z-processes.json pid 102
PLOADER_PROC_NAME = "payload.elf"  # l22 PLOADER-ARMING.json process name

# klog tap: agent-T section 2 exact procedure (verified l18 script).
KLOG_CAPTURE = ("/WORKSPACE/.codex/worktrees/translation-core/what/"
                "outputs/2026-10-07-l18-full-build/"
                "console-test-20261007T205350Z-f1310cc9/klog_capture.py")
KLOG_WINDOW_S = 3600     # task step 7 (LAUNCH-PLAN 1.4 used 7200 for the
                         # longer vanilla window; l25 prep contract pins 3600)
MONITOR_POLL_S = 15      # task step 7: 15 s poll, log transitions
MONITOR_WINDOW_S = 3600  # bounded: monitor exits by itself

PLOADER_TIMEOUT_S = 120  # INJECTOR_TIMEOUT_S (Z2 mechanism table step 5)
ARM_POLL_INTERVAL_S = 2  # fresh-pid poll cadence
ARM_POLL_BUDGET_S = 10   # task step 6: up to 10 s
FTP_MAX_FILESIZE = 67108864  # 64 MiB decrypt-off cap (l22 backup argv)

# STEP 5.5 loadlist row deploy gate (Claude round-8 item 6, the -6
# incident): sibling tool, subprocess-only coupling -- its stdout --json
# contract is parsed, never its internals.
ROWCHECK_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "prelaunch_rowcheck.py")
ROWCHECK_LOADLIST_REMOTE = ("/data/modbridge/roots/PPSA33286/"
                            "loadlist.txt")  # rowcheck default root
ROWCHECK_SUBPROCESS_TIMEOUT = 900  # ~22 rows x curl --max-time 20 + parse


def default_out_dir(variant):
    """EVIDENCE-RETENTION (agent-HA 2026-10-10): session out-dir default is
    DURABLE (outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-<X>/),
    no longer /tmp/l25-variant-<X>. --out-dir overrides; legacy /tmp dirs
    remain readable by harvest.py's KLOG_RANKED_DIRS for old sessions."""
    return os.path.join(EVIDENCE_RUNTIME_ROOT, "l25-variant-%s" % variant)

VARIANT_DESC = {
    "A": ("Variant A -- l25 build, NORMAL AddUObjectDeleteListener "
          "registration kept (UnrealInitializer.cpp:528) + agent-O "
          "instrumentation + agent-P phase tags + agent-Q probe liveness. "
          "Launch order v3: A runs SECOND, only to confirm the mechanism, "
          "with the extended-A-logging build (Claude review 2)."),
    "B": ("Variant B -- l25 twin build, registration SKIPPED "
          "(MODBRIDGE_L25_SKIP_DELETE_LISTENER). DIAGNOSTIC-ONLY, unsafe "
          "long-term: stale object pointers are never cleared; NEVER leave "
          "B resident (LAUNCH-PLAN 4.2). Launch order v3 (Claude review 2): "
          "B launches FIRST -- skipping the registration avoids the "
          "allocator call entirely and is the path to the first real look "
          "at mod start, the pump, the pad, and the menu."),
}
# Variant-B harvest expectation (task step 8; strings are the landed
# agent-O vocabulary used by harvest.py/markers.json -- never invented).
VARIANT_B_SKIP_LINE = ("delete-listener registration SKIPPED (variant B; "
                       "unsafe long-term: stale object pointers not cleared)")
VARIANT_B_NO_REG_LINES = "[L25][DeleteListener]"

CURL_HTTP_MAX = "15"   # LAUNCH-PLAN 1.1 --max-time 15
CURL_INI_MAX = "30"    # small text uploads
CURL_PRX_MAX = "90"    # l22 upload argv --max-time 90
CURL_PRX_RB_MAX = "60" # l22 readback argv --max-time 60


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

class Abort(Exception):
    """A verification gate failed. Message is printed verbatim; exit 1."""


def now_utc():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def ts_slug():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y%m%dT%H%M%SZ")


def sha256_of(path):
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 16), b""):
                h.update(chunk)
        return h.hexdigest()
    except OSError:
        return None


def magic_of(path):
    """First 4 bytes as hex text, or None if unreadable/too short."""
    try:
        with open(path, "rb") as f:
            head = f.read(4)
        return head.hex() if len(head) == 4 else None
    except OSError:
        return None


def run_cmd(argv, timeout):
    try:
        p = subprocess.run(argv, capture_output=True, text=True,
                           timeout=timeout)
        return {"rc": p.returncode, "out": p.stdout or "",
                "err": p.stderr or "", "argv": argv}
    except subprocess.TimeoutExpired:
        return {"rc": -1, "out": "", "err": "local subprocess timeout",
                "argv": argv}
    except OSError as e:
        return {"rc": -1, "out": "", "err": "cannot execute: %s" % e,
                "argv": argv}


def http_get_cmd(console_ip, path, dest, max_time=CURL_HTTP_MAX):
    """LAUNCH-PLAN 1.1 curl shape (--noproxy '*' kept from the plan)."""
    return ["curl", "--silent", "--show-error", "--fail", "--noproxy", "*",
            "--connect-timeout", "5", "--max-time", max_time, "-o", dest,
            "http://%s:%d%s" % (console_ip, HTTP_PORT, path)]


def ftp_get_cmd(console_ip, remote, dest, max_time, self_mode=False,
                max_filesize=None):
    """harvest.py curl style; -Q 'SITE SELF' = decrypt-OFF (executables)."""
    argv = ["curl", "--silent", "--show-error", "--fail",
            "--connect-timeout", "5", "--max-time", max_time]
    if max_filesize:
        argv += ["--max-filesize", str(max_filesize)]
    if self_mode:
        argv += ["-Q", "SITE SELF"]
    argv += ["-o", dest, "ftp://%s:%d%s" % (console_ip, FTP_PORT, remote)]
    return argv


def ftp_put_cmd(console_ip, local, remote, max_time):
    """l22 UPLOAD-RECEIPT.json upload_argv shape (-T + SITE SELF toggle)."""
    return ["curl", "--silent", "--show-error", "--fail",
            "--connect-timeout", "5", "--max-time", max_time,
            "-Q", "SITE SELF", "--upload-file", local,
            "ftp://%s:%d%s" % (console_ip, FTP_PORT, remote)]


def load_processes(path):
    """Parse a saved processes_list JSON -> list of process dicts (or None)."""
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        procs = data.get("processes")
        return procs if isinstance(procs, list) else None
    except (OSError, ValueError):
        return None


def procs_named(procs, name):
    return [p for p in procs if str(p.get("name", "")) == name]


def daemon_pid(procs, base):
    """First pid whose name is <base>.elf (or exactly <base>)."""
    for p in procs:
        n = str(p.get("name", ""))
        if n == base or n == base + ".elf":
            return p.get("pid")
    return None


class Ledger:
    """Every check performed, with expected/actual -- becomes the report."""

    def __init__(self):
        self.rows = []

    def add(self, step, check, expected, actual, ok, note=""):
        self.rows.append({"step": step, "check": check, "expected": expected,
                          "actual": actual,
                          "status": "PASS" if ok else "FAIL", "note": note})
        return ok

    def add_would(self, step, check, expected, note=""):
        self.rows.append({"step": step, "check": check, "expected": expected,
                          "actual": "(dry-run: not executed)",
                          "status": "WOULD CHECK", "note": note})
        return True


def fmt_cmd(argv):
    return " ".join(argv)


# ---------------------------------------------------------------------------
# Report generation (task step 8)
# ---------------------------------------------------------------------------

def user_sequence_md(variant):
    lines = [
        "1. Launch the game (Townfall, PPSA33286) -- ONE launch; get explicit",
        "   user OK first (LAUNCH-PLAN section 2).",
        "2. Main menu: WAIT 90 s (also covers the PS5Pad >=10 s uptime needed",
        "   for the pad borrow).",
        "3. Load the SAME save used in the l24 crash run.",
        "4. WAIT 30 s past save-load.",
        "5. Press L3+R3 ONCE (PS5Pad rebind; OPTIONS stays the game's pause).",
    ]
    if variant == "B":
        lines += [
            "5b. IF THE MENU OPENS (launch order v3, Claude review 2): root",
            "    HARVESTS immediately (read-only pull; game keeps running),",
            "    then D-pad + one spawn IN THE SAME SESSION: DOWN x3 to the",
            "    first PICKUPS row, CROSS once (x1 by default), then root",
            "    harvests AGAIN -- the diff must show `[TownfallItemSpawner]",
            "    Spawned <name> x1 ID=<id>` (LAUNCH-PLAN v3.3).",
        ]
    lines += [
        "6. CLEAN EXIT the game via in-game quit back to the dashboard (never",
        "   a power event). If it crashes instead: note the code (expected",
        "   signature CE-108255-1) and WHEN it happened (menu / save-load /",
        "   L3+R3); root harvests immediately; a REBOOT is required before",
        "   any next launch (SAFETY BOX 5: no rapid-fire launch tests).",
    ]
    return lines


def variant_expectations_md(variant):
    if variant == "A":
        return [
            "- Variant A: NORMAL registration kept. At harvest EXPECT the",
            "  agent-O post-:528 markers (`[L25][DeleteListener] "
            "shutdown-listener registration returned in ...`),",
            "  `Starting callback garbage collector!` (first proof init",
            "  passed :528), the `[l25]` phase ladder, `[l22] Lua mod",
            "  started:`, and on L3+R3 the pad chain `pad_l3r3_edge` ->",
            "  `pad_l3r3_f7_dispatched`.",
        ]
    return [
        "- Variant B (LAUNCH ORDER v3 -- B FIRST, Claude review 2):",
        "  diagnostic-only, unsafe long-term -- the skipped",
        "  AddUObjectDeleteListener means stale object pointers are never",
        "  cleared. NEVER leave B resident; after B testing restore variant",
        "  A (or the l24 composition) as resident (LAUNCH-PLAN 4.2/4.4).",
        "- Variant B SKIPPED listener expectation at harvest: NO",
        "  `%s` registration lines -- ONLY the SKIPPED line:" %
        VARIANT_B_NO_REG_LINES,
        "  `%s`" % VARIANT_B_SKIP_LINE,
        "- IN-SESSION SPAWN TEST (only if the menu opens; LAUNCH-PLAN v3.3):",
        "  menu open (L3+R3 once) -> root harvests (read-only pull) -> user:",
        "  DOWN x3 to the first PICKUPS row, CROSS once (one spawn, x1 by",
        "  default) -> root harvests again; the diff must contain the proof",
        "  line `[TownfallItemSpawner] Spawned <name> x1 ID=<id>` (failure:",
        "  `[TownfallItemSpawner] Failed at <stage>. ...`).",
    ]


def generate_ready_md(variant, dry, console_ip, prx_path, prx_size,
                      prx_sha, prx_magic, out_dir, ledger, pids, jobs,
                      abort_reason=None):
    a = []
    title = "LAUNCH-PREP-FAILED-%s-%s" % (variant, ts_slug()) if abort_reason \
        else "LAUNCH-READY-%s-%s" % (variant, ts_slug())
    a.append("# %s" % title)
    a.append("")
    a.append("- generated (UTC): %s" % now_utc())
    a.append("- mode: %s" % ("DRY-RUN SKELETON (nothing executed, nothing "
                             "written to disk by the dry run)" if dry
                            else "live"))
    a.append("- console: %s (HTTP %d / FTP %d / klog TCP %d)"
             % (console_ip, HTTP_PORT, FTP_PORT, KLOG_PORT))
    a.append("- variant: %s" % VARIANT_DESC[variant])
    a.append("- session out-dir: %s" % out_dir)
    a.append("- PRX: %s" % prx_path)
    a.append("  - size: %s bytes" % prx_size)
    a.append("  - sha256: %s" % prx_sha)
    a.append("  - magic: %s (signed SELF: %s)"
             % (prx_magic, " or ".join(m.hex() for m in MAGIC_SIGNED)))
    a.append("")
    if abort_reason:
        a.append("## ABORT REASON")
        a.append("")
        a.append("**%s**" % abort_reason)
        a.append("")
    a.append("## Verification ledger")
    a.append("")
    a.append("| # | step | check | expected | actual | status |")
    a.append("|---|---|---|---|---|---|")
    for i, r in enumerate(ledger.rows, 1):
        a.append("| %d | %s | %s | %s | %s | %s |" % (
            i, r["step"], r["check"], r["expected"], r["actual"],
            r["status"]))
    a.append("")
    a.append("## Pids (from processes_list)")
    a.append("")
    for k, v in pids.items():
        a.append("- %s: %s" % (k, v))
    a.append("")
    a.append("## Background jobs (started before this report; bounded)")
    a.append("")
    for j in jobs:
        for k, v in j.items():
            a.append("- %s: %s" % (k, v))
        a.append("")
    a.append("## User instruction sequence (hand to the player)")
    a.append("")
    a.extend(user_sequence_md(variant))
    a.append("")
    a.append("## Variant expectations at harvest")
    a.append("")
    a.extend(variant_expectations_md(variant))
    a.append("")
    a.append("## Post-run")
    a.append("")
    a.append("- Harvest with the sibling tool: `python3 %s --label "
             "l25-%s-<n>` (agent-Y harvest.py; pulls UE4SS.log, "
             "runtime-status, diag, aligned-entry; picks up the klog file "
             "from the ranked dirs -- since 2026-10-10 taps land DURABLE at "
             "outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-%s/ "
             "and harvest.py searches that root first; legacy /tmp out-dirs "
             "are still searched for old sessions)." % (
                 os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "harvest.py"), variant.lower(), variant.lower()))
    a.append("- Standing rules: never touch autoload.txt; never kill "
             "processes; one launch per clean cycle; after any crash -> "
             "harvest -> reboot; never replay a failed configuration.")
    a.append("- Bounded background jobs end by themselves (klog tap and "
             "eboot monitor windows: %d s). Stopping them early by hand is "
             "allowed; they never write to the console." % KLOG_WINDOW_S)
    return "\n".join(a)


# ---------------------------------------------------------------------------
# Live step implementations. Each returns artifacts for the report and
# raises Abort on verification failure.
# ---------------------------------------------------------------------------

def step1_preconditions(console_ip, out_dir, ledger):
    """Daemons up, no eboot, ploader not resident (Z2 law)."""
    dest = os.path.join(out_dir, "processes-precond.json")
    cmd = http_get_cmd(console_ip, PROCESSES_PATH, dest)
    r = run_cmd(cmd, 30)
    if r["rc"] != 0 or not os.path.isfile(dest):
        raise Abort("STEP 1 precondition FAILED: cannot fetch processes_list "
                    "(curl rc=%s err=%s). Is pldmgr HTTP up on %s:%d? "
                    "Do not proceed." % (r["rc"], r["err"][:160], console_ip,
                                         HTTP_PORT))
    procs = load_processes(dest)
    if procs is None:
        raise Abort("STEP 1 precondition FAILED: processes_list at %s is not "
                    "parseable JSON with a 'processes' list." % dest)
    for base in REQUIRED_DAEMONS:
        pid = daemon_pid(procs, base)
        if not ledger.add("1", "daemon '%s' present" % base, "pid (any)",
                          "pid %s" % pid if pid else "ABSENT", bool(pid)):
            raise Abort("STEP 1 precondition FAILED: required daemon '%s' "
                        "missing from processes_list. Reboot/re-check the "
                        "console before any launch; do not proceed." % base)
    game = procs_named(procs, GAME_PROC_NAME)
    ledger.add("1", "no '%s' running" % GAME_PROC_NAME, "none",
               "%d proc(s) %s" % (len(game), [g.get("pid") for g in game])
               if game else "none", not game)
    if game:
        raise Abort("STEP 1 precondition FAILED: '%s' (pid %s) is ALREADY "
                    "RUNNING. One launch per clean cycle -- wait for a clean "
                    "exit (or reboot if it crashed), then re-run."
                    % (GAME_PROC_NAME, [g.get("pid") for g in game]))

    # ploader residency: lock file RETR (Z2 receipts: rc 78 / RETR 550 =
    # none). A live lock pid = resident holding the OLD frozen ini list.
    lock_dest = os.path.join(out_dir, "ploader-lock.check")
    lr = run_cmd(ftp_get_cmd(console_ip, PLOADER_LOCK_REMOTE, lock_dest,
                             "10"), 20)
    if lr["rc"] == 0 and os.path.isfile(lock_dest):
        raw = open(lock_dest, "r", encoding="utf-8",
                   errors="replace").read().strip()
        lock_pid = None
        try:
            lock_pid = int(raw)
        except ValueError:
            pass
        live = lock_pid is not None and any(
            p.get("pid") == lock_pid for p in procs)
        ledger.add("1", "ploader not resident (lock RETR)",
                   "curl rc 78 (RETR 550) = none",
                   "rc 0, lock pid %s (%s)" % (raw, "LIVE" if live
                                               else "stale/dead"), False)
        if live:
            raise Abort("STEP 1 precondition FAILED: ploader IS RESIDENT "
                        "(lock pid %s, mid-window). Per the agent-Z2 law it "
                        "holds the OLD in-memory ini list and cannot be "
                        "refreshed. WAIT <=%d s from its arm for the "
                        "'Timeout, shutting down.' exit (lock RETR turns "
                        "550), then RE-RUN this script. Do NOT dispatch "
                        "loadpayload now and do NOT launch yet."
                        % (lock_pid, PLOADER_TIMEOUT_S))
        # Stale lock: owner dead -- auto-unlinked at the next arm (Z2
        # L704); recorded, not fatal.
        ledger.rows[-1]["status"] = "PASS"
        ledger.rows[-1]["note"] = ("stale lock (pid %s not in processes_list)"
                                   " -- auto-unlinked at next arm, Z2 L704"
                                   % lock_pid)
    elif lr["rc"] == 78 or "550" in lr["err"]:
        ledger.add("1", "ploader not resident (lock RETR)",
                   "curl rc 78 (RETR 550) = none",
                   "rc 78 / RETR 550", True)
    else:
        ledger.add("1", "ploader not resident (lock RETR)",
                   "curl rc 78 (RETR 550) = none",
                   "rc %s %s" % (lr["rc"], lr["err"][:80]), False)
        raise Abort("STEP 1 precondition FAILED: cannot determine ploader "
                    "residency (lock RETR rc=%s err=%s). Never launch on an "
                    "unknown state." % (lr["rc"], lr["err"][:160]))
    return procs


def step2_prx_gate(prx_path, ledger):
    """Local PRX safety gate -- signed SELF magic, sha recorded."""
    if not os.path.isfile(prx_path):
        raise Abort("STEP 2 PRX gate FAILED: --prx-path does not exist: %s"
                    % prx_path)
    size = os.path.getsize(prx_path)
    sha = sha256_of(prx_path)
    magic = magic_of(prx_path)
    ledger.add("2", "PRX file exists", "regular file",
               "%s (%d bytes)" % (prx_path, size), True)
    ledger.add("2", "PRX sha256 recorded", "(recorded for readback match)",
               sha or "UNREADABLE", sha is not None)
    if magic in (m.hex() for m in MAGIC_SIGNED):
        ledger.add("2", "PRX magic is a signed SELF",
                   " or ".join(m.hex() for m in MAGIC_SIGNED), magic, True)
    elif magic == MAGIC_ELF.hex():
        ledger.add("2", "PRX magic is a signed SELF",
                   " or ".join(m.hex() for m in MAGIC_SIGNED),
                   "%s (plain ELF)" % magic, False)
        raise Abort("STEP 2 PRX gate STOP: %s is an %s. Fake-sign it first: "
                    "python3 make_fself.py <in.elf> <out.self> "
                    "(AGENTS.md PS5 rule 3; uploading it unsigned bricks the "
                    "game)." % (prx_path, UNSIGNED_ELF_MESSAGE))
    else:
        ledger.add("2", "PRX magic is a signed SELF",
                   " or ".join(m.hex() for m in MAGIC_SIGNED),
                   str(magic), False)
        raise Abort("STEP 2 PRX gate FAILED: %s has unrecognized magic %r "
                    "(expected a signed SELF: %s). Refusing to upload."
                    % (prx_path, magic,
                       " or ".join(m.hex() for m in MAGIC_SIGNED)))
    return size, sha, magic


def verify_readback_prx(path, label, ledger, step):
    magic = magic_of(path)
    ok = magic in (m.hex() for m in MAGIC_SIGNED)
    ledger.add(step, "%s magic (decrypt-OFF)" % label,
               " or ".join(m.hex() for m in MAGIC_SIGNED), str(magic), ok)
    return ok


def step3_upload_prx(console_ip, out_dir, prx_path, prx_sha, prx_size,
                     ledger):
    """Backup resident (decrypt-OFF) -> upload -> decrypt-OFF readback."""
    backup = os.path.join(out_dir, "backup-resident.decrypt-off.prx")
    br = run_cmd(ftp_get_cmd(console_ip, PRX_REMOTE, backup,
                             CURL_PRX_RB_MAX, self_mode=True,
                             max_filesize=FTP_MAX_FILESIZE), 120)
    backup_info = None
    if br["rc"] == 0 and os.path.isfile(backup):
        bsha, bsize, bmagic = (sha256_of(backup), os.path.getsize(backup),
                               magic_of(backup))
        ok = verify_readback_prx(backup, "resident backup", ledger, "3")
        ledger.add("3", "resident backup recorded",
                   "size + sha256 recorded",
                   "%d bytes, %s" % (bsize, bsha), True)
        backup_info = {"path": backup, "size": bsize, "sha256": bsha,
                       "magic": bmagic}
        if not ok:
            raise Abort("STEP 3 FAILED: the CURRENT resident %s does not "
                        "read back as a signed SELF (magic %r). Refusing to "
                        "overwrite it -- reconcile the console state first "
                        "(SAFETY BOX 4)." % (PRX_REMOTE, bmagic))
    elif br["rc"] == 78 or "550" in br["err"]:
        ledger.add("3", "resident backup readback", "signed SELF or absent",
                   "absent on console (rc 78 / RETR 550) -- first install",
                   True, note="no previous resident to preserve")
    else:
        ledger.add("3", "resident backup readback", "signed SELF or absent",
                   "rc %s %s" % (br["rc"], br["err"][:80]), False)
        raise Abort("STEP 3 FAILED: cannot back up the current resident %s "
                    "(curl rc=%s err=%s). LAUNCH-PLAN STEP 0.4 forbids "
                    "overwriting it without a decrypt-OFF backup."
                    % (PRX_REMOTE, br["rc"], br["err"][:160]))

    ur = run_cmd(ftp_put_cmd(console_ip, prx_path, PRX_REMOTE,
                             CURL_PRX_MAX), 180)
    ledger.add("3", "PRX upload (curl -T)",
               "curl rc 0", "rc %s" % ur["rc"], ur["rc"] == 0)
    if ur["rc"] != 0:
        raise Abort("STEP 3 FAILED: PRX upload rc=%s err=%s -- nothing "
                    "verified, do not launch." % (ur["rc"], ur["err"][:160]))

    readback = os.path.join(out_dir, "prx-readback.decrypt-off.prx")
    rr = run_cmd(ftp_get_cmd(console_ip, PRX_REMOTE, readback,
                             CURL_PRX_RB_MAX, self_mode=True,
                             max_filesize=FTP_MAX_FILESIZE), 120)
    if rr["rc"] != 0 or not os.path.isfile(readback):
        raise Abort("STEP 3 FAILED: decrypt-OFF readback failed (curl "
                    "rc=%s err=%s). Console state unverified -- do NOT "
                    "launch; reconcile now." % (rr["rc"], rr["err"][:160]))
    rsha, rsize = sha256_of(readback), os.path.getsize(readback)
    ok_magic = verify_readback_prx(readback, "uploaded PRX readback",
                                   ledger, "3")
    ok_sha = ledger.add("3", "uploaded PRX readback sha256 byte-exact",
                        prx_sha, rsha, rsha == prx_sha)
    ledger.add("3", "uploaded PRX readback size", "%d bytes" % prx_size,
               "%d bytes" % rsize, rsize == prx_size)
    if not (ok_magic and ok_sha):
        # Never leave a broken executable in place: ONE verified
        # restore-from-backup attempt (only from this run's verified
        # backup), then abort regardless.
        restored = False
        if backup_info and backup_info["magic"] in (
                m.hex() for m in MAGIC_SIGNED):
            x = run_cmd(ftp_put_cmd(console_ip, backup_info["path"],
                                    PRX_REMOTE, CURL_PRX_MAX), 180)
            check = os.path.join(out_dir, "restore-readback.decrypt-off.prx")
            xr = run_cmd(ftp_get_cmd(console_ip, PRX_REMOTE, check,
                                     CURL_PRX_RB_MAX, self_mode=True,
                                     max_filesize=FTP_MAX_FILESIZE), 120)
            if (x["rc"] == 0 and xr["rc"] == 0
                    and sha256_of(check) == backup_info["sha256"]
                    and magic_of(check) in (m.hex() for m in MAGIC_SIGNED)):
                restored = True
                ledger.add("3", "restore previous resident after mismatch",
                           "backup re-uploaded + verified",
                           "restored %s (sha %s)"
                           % (backup_info["path"],
                              backup_info["sha256"][:16] + "..."), True)
        if not restored:
            ledger.add("3", "restore previous resident after mismatch",
                       "backup re-uploaded + verified",
                       "NOT restored (no verified backup this run)", False)
        raise Abort("STEP 3 VERIFICATION MISMATCH: uploaded %s did not read "
                    "back byte-identical (magic %s, sha %s vs expected %s). "
                    "%s A broken executable must not stay in place -- "
                    "reconcile before ANY launch."
                    % (PRX_REMOTE, magic_of(readback), rsha, prx_sha,
                       "Previous resident was restored from the verified "
                       "backup." if restored else
                       "RESTORE FAILED -- console resident may be broken; "
                       "fix manually from a known-good signed artifact."))
    return backup_info, {"path": readback, "size": rsize, "sha256": rsha}


def verify_local_ini(path, size, sha, label, ledger, step):
    if not os.path.isfile(path):
        raise Abort("STEP %s FAILED: %s backup %s does not exist on this "
                    "Mac. Restore it first (LAUNCH-PLAN 1.2 / "
                    "VANILLA-VERDICT.md); never upload from an unverified "
                    "source." % (step, label, path))
    lsize, lsha = os.path.getsize(path), sha256_of(path)
    ledger.add(step, "%s local backup exists" % label, path, path, True)
    if not ledger.add(step, "%s local size" % label, "%d bytes" % size,
                      "%d bytes" % lsize, lsize == size):
        raise Abort("STEP %s FAILED: %s is %d bytes, expected exactly %d. "
                    "SIZE DRIFT = ABORT (the upload would break the "
                    "documented contract). Find the correct backup; never "
                    "trim or pad a file to force a pass."
                    % (step, path, lsize, size))
    if not ledger.add(step, "%s local sha256" % label, sha, str(lsha),
                      lsha == sha):
        raise Abort("STEP %s FAILED: %s sha256 %s != pinned %s. Content "
                    "drift -- abort before upload."
                    % (step, path, lsha, sha))
    return lsize, lsha


def upload_ini_and_verify(console_ip, out_dir, local, remote, label, step,
                          ledger, max_time=CURL_INI_MAX):
    ur = run_cmd(ftp_put_cmd(console_ip, local, remote, max_time), 90)
    ledger.add(step, "%s upload (curl -T)" % label, "curl rc 0",
               "rc %s" % ur["rc"], ur["rc"] == 0)
    if ur["rc"] != 0:
        raise Abort("STEP %s FAILED: %s upload rc=%s err=%s."
                    % (step, label, ur["rc"], ur["err"][:160]))
    rb = os.path.join(out_dir, os.path.basename(remote) + ".readback")
    rr = run_cmd(ftp_get_cmd(console_ip, remote, rb, max_time), 60)
    if rr["rc"] != 0 or not os.path.isfile(rb):
        raise Abort("STEP %s FAILED: %s readback failed (rc=%s err=%s) -- "
                    "state unverified, do not launch."
                    % (step, label, rr["rc"], rr["err"][:160]))
    same = open(local, "rb").read() == open(rb, "rb").read()
    ledger.add(step, "%s readback cmp (byte-exact)" % label,
               "identical to local backup",
               "%d bytes, sha256 %s, cmp %s"
               % (os.path.getsize(rb), sha256_of(rb),
                  "identical" if same else "DIFFERENT"), same)
    if not same:
        raise Abort("STEP %s VERIFICATION MISMATCH: %s readback differs "
                    "from the local backup. Console state unverified -- do "
                    "not launch; reconcile." % (step, remote))
    return rb


def _rowcheck_verdict(doc, rc):
    """rowcheck --json document + subprocess rc -> (ok, summary, failing).

    Pure helper (unit-testable offline from saved receipts): the rowcheck
    CLI contract is rc 0 = all rows pass, 1 = gate failure, 2 = usage."""
    if not isinstance(doc, dict):
        return False, "no parseable rowcheck document", []
    counts = doc.get("counts") or {}
    def g(k):
        v = counts.get(k, 0)
        return int(v) if isinstance(v, (int, float)) else 0
    summary = ("%d rows: %d pass, %d missing, %d size-mismatch, "
               "%d sha-mismatch, %d malformed, %d fetch-error"
               % (g("rows"), g("pass"), g("missing"), g("size_mismatch"),
                  g("sha_mismatch"), g("malformed"), g("fetch_error")))
    ok = bool(doc.get("ok")) and rc == 0
    failing = [r for r in (doc.get("rows") or [])
               if isinstance(r, dict) and r.get("status") != "PASS"]
    return ok, summary, failing


def step5_5_rowcheck(console_ip, out_dir, ledger):
    """STEP 5.5 -- loadlist row deploy gate (Claude round-8 item 6; the
    2026-10-11 -6 incident: a loadlist row declared a file that was never
    uploaded -> MFA refused the WHOLE module at boot -> wasted launch).

    After every upload (steps 3-5) and BEFORE arming (step 6) or handing
    off the launch sequence: run harvest/prelaunch_rowcheck.py, which
    fetches the console loadlist.txt, parses every 'file\\t<role>\\t<path>
    \\t<size>' row, fetches each file, and verifies it exists at its
    EXACT declared size. Game-closed law holds (STEP 1 refused while
    eboot.bin runs; nothing launched since). Returns the parsed doc."""
    if not os.path.isfile(ROWCHECK_SCRIPT):
        raise Abort("STEP 5.5 FAILED: row-check gate script missing: %s. "
                    "The deploy gate is MANDATORY (the -6 incident law: "
                    "every loadlist row is verified BEFORE launch) -- "
                    "restore the script before any launch."
                    % ROWCHECK_SCRIPT)
    rc_out = os.path.join(out_dir, "rowcheck")
    argv = [sys.executable, ROWCHECK_SCRIPT,
            "--console-ip", console_ip, "--out-dir", rc_out, "--json"]
    r = run_cmd(argv, ROWCHECK_SUBPROCESS_TIMEOUT)
    doc = None
    if (r["out"] or "").strip():
        try:
            doc = json.loads(r["out"])
        except ValueError:
            doc = None
    if r["rc"] not in (0, 1) or not isinstance(doc, dict):
        raise Abort("STEP 5.5 FAILED: the row-check tool did not produce "
                    "its verdict document (rc=%s err=%s out%.200r). The "
                    "loadlist is UNVERIFIED -- do not launch; re-run from "
                    "STEP 1 after reconciling."
                    % (r["rc"], (r["err"] or "")[:160], r["out"]))
    ok, summary, failing = _rowcheck_verdict(doc, r["rc"])
    receipt = doc.get("receipt")
    ledger.add("5.5", "loadlist row deploy gate (%s; every 'file' row "
               "exists at its EXACT declared size)" % ROWCHECK_LOADLIST_REMOTE,
               "all rows PASS", "%s [%s]" % (summary, receipt), ok)
    if not ok:
        # every failing row, with its EXACT row text, in the ledger
        for row in failing[:50]:
            ledger.add("5.5", "row line %s: %s"
                       % (row.get("lineno"), row.get("path", "?")),
                       "present at declared size %s"
                       % row.get("declared_size", "?"),
                       "%s (%s) row: %r" % (row.get("status", "?"),
                                            row.get("detail", ""),
                                            row.get("raw", "")), False)
        if len(failing) > 50:
            ledger.add("5.5", "failing rows", "all listed",
                       "(%d more -- see the rowcheck receipt %s)"
                       % (len(failing) - 50, receipt), False)
        refusal = doc.get("refusal")
        why = ("REFUSAL: %s" % refusal) if refusal else summary
        raise Abort("STEP 5.5 LOADLIST ROW GATE FAILED -- %s. FIX: upload "
                    "the missing rows or fix the loadlist (re-pin each row "
                    "size-exactly to the actual bytes, then re-run this "
                    "script from STEP 1). Do NOT launch: MFA admission is "
                    "all-or-nothing and would refuse the whole module at "
                    "boot (the 2026-10-11 mfa_activate_failed -6 incident "
                    "cost exactly this relaunch). Full receipt: %s"
                    % (why, receipt))
    return doc



# [settle 2026-10-13] Twice today the console dropped right after a fresh PRX upload + immediate ploader dispatch, and
# after one of those power events the resident PRX read back as neither the old nor the new build (the immediate
# readback had matched, i.e. the write was not yet persisted). Before arming: wait, then read the PRX back again
# decrypt-OFF and require the exact sha once more. The klog tap is also started BEFORE the arm (main flow).
SETTLE_S = int(os.environ.get("PREP_SETTLE_S", "75"))


def step5_6_settle_readback(console_ip, out_dir, prx_sha, prx_size, ledger):
    time.sleep(SETTLE_S)
    rb = os.path.join(out_dir, "prx-readback-settled.decrypt-off.prx")
    r = run_cmd(ftp_get_cmd(console_ip, PRX_REMOTE, rb, CURL_PRX_RB_MAX, self_mode=True,
                            max_filesize=FTP_MAX_FILESIZE), 180)
    got = sha256_of(rb) if r["rc"] == 0 and os.path.isfile(rb) else None
    size = os.path.getsize(rb) if got else -1
    ok = got == prx_sha and size == prx_size
    ledger.add("5.6", "PRX settled readback after %d s (decrypt-OFF, sha + size)" % SETTLE_S,
               "%s / %d bytes" % (prx_sha, prx_size), "%s / %d bytes" % (got, size), ok)
    if not ok:
        raise Abort("STEP 5.6 FAILED: the PRX read back after %d s does not match the upload (sha %s, %d bytes). "
                    "Do NOT arm or launch; re-upload and re-run from STEP 1." % (SETTLE_S, got, size))

def step6_arm_ploader(console_ip, out_dir, baseline_procs, ledger):
    """ONE dispatch + fresh payload.elf pid poll (ini-edit law)."""
    arm_dest = os.path.join(out_dir, "arm.response")
    cmd = http_get_cmd(console_ip, ARM_URL_PATH, arm_dest)
    ar = run_cmd(cmd, 30)
    body = ""
    if os.path.isfile(arm_dest):
        body = open(arm_dest, "r", encoding="utf-8",
                    errors="replace").read().strip()
    ledger.add("6", "loadpayload dispatched ONCE", "curl rc 0, body OK",
               "rc %s, body %r" % (ar["rc"], body[:60]), ar["rc"] == 0)
    if ar["rc"] != 0:
        raise Abort("STEP 6 FAILED: loadpayload dispatch rc=%s err=%s. Do "
                    "NOT re-send blindly (read the receipt first); "
                    "reconcile pldmgr, then re-run this script from STEP 1."
                    % (ar["rc"], ar["err"][:160]))
    baseline_payload_pids = set(p.get("pid") for p in
                                procs_named(baseline_procs, PLOADER_PROC_NAME))
    fresh_dest = os.path.join(out_dir, "processes-armed.json")
    fresh_pid, procs_final = None, None
    deadline = time.time() + ARM_POLL_BUDGET_S
    while time.time() < deadline:
        time.sleep(ARM_POLL_INTERVAL_S)
        r = run_cmd(http_get_cmd(console_ip, PROCESSES_PATH, fresh_dest),
                    30)
        if r["rc"] != 0:
            continue
        procs_final = load_processes(fresh_dest)
        if not procs_final:
            continue
        new = [p.get("pid") for p in procs_named(procs_final,
                                                 PLOADER_PROC_NAME)
               if p.get("pid") not in baseline_payload_pids]
        if new:
            fresh_pid = sorted(new)[-1]
            break
    ledger.add("6", "fresh '%s' pid appears" % PLOADER_PROC_NAME,
               "new pid within %d s (not in pre-arm set %s)"
               % (ARM_POLL_BUDGET_S, sorted(baseline_payload_pids)),
               "pid %s" % fresh_pid if fresh_pid else "NONE within budget",
               fresh_pid is not None)
    if fresh_pid is None:
        raise Abort("STEP 6 FAILED: no fresh '%s' pid appeared within %d s "
                    "of the (single) loadpayload dispatch. ploader is NOT "
                    "armed -- do NOT launch. Reconcile (dispatch is "
                    "one-shot; never duplicate-send: re-run this script "
                    "from STEP 1 only after understanding the refusal)."
                    % (PLOADER_PROC_NAME, ARM_POLL_BUDGET_S))
    # Injection row still in place after arming (LAUNCH-PLAN 1.3 last line):
    # the STEP 5 readback already proved byte-exact content; we assert the
    # row from that same verified backup for the record.
    row_ok = PLOADER_INI_INJECT_ROW in open(
        PLOADER_INI_LOCAL, "r", encoding="utf-8",
        errors="replace").read()
    ledger.add("6", "inject row for the variant PRX armed (=2)",
               PLOADER_INI_INJECT_ROW,
               "present in the verified backup/readback" if row_ok
               else "MISSING", row_ok)
    if not row_ok:
        raise Abort("STEP 6 FAILED: the enabled-state backup lacks the "
                    "'%s' row -- ploader would inject nothing. Abort."
                    % PLOADER_INI_INJECT_ROW)
    return fresh_pid, baseline_payload_pids, procs_final


def spawn_and_confirm(argv, log_path, ready_probe, ledger, step, name,
                      timeout_s=15.0, poll_s=0.5):
    """Spawn detached, wait for ready_probe(log_path) True. Abort on fail."""
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    with open(log_path, "wb") as lf:
        proc = subprocess.Popen(argv, stdout=lf, stderr=lf,
                                stdin=subprocess.DEVNULL,
                                start_new_session=True)
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if proc.poll() is not None:
            break
        if ready_probe is not None and ready_probe():
            break
        time.sleep(poll_s)
    ok = proc.poll() is None and (ready_probe is None or ready_probe())
    ledger.add(step, "%s running" % name, "detached, alive, ready",
               "pid %s (%s)" % (proc.pid,
                                "alive" if proc.poll() is None
                                else "EXITED rc=%s" % proc.returncode), ok)
    if not ok:
        tail = ""
        try:
            with open(log_path, "r", encoding="utf-8",
                      errors="replace") as f:
                tail = f.read()[-300:]
        except OSError:
            pass
        raise Abort("STEP %s FAILED: %s did not start (log tail: %r). The "
                    "launch must wait for a verified capture setup."
                    % (step, name, tail))
    return proc


def step7_background(console_ip, out_dir, variant, ledger, skip_klog):
    jobs = []
    if not skip_klog:
        if not os.path.isfile(KLOG_CAPTURE):
            raise Abort("STEP 7 FAILED: klog tap script missing: %s "
                        "(agent-T section 2 pinned path). Restore it before "
                        "any klog-covered launch." % KLOG_CAPTURE)
        klog_stderr = os.path.join(out_dir, "klog-process-%s.log" % ts_slug())
        klog_argv = ["python3", KLOG_CAPTURE,
                     "--out-dir", out_dir,
                     "--window-seconds", str(KLOG_WINDOW_S)]
        state = {"capture": None}

        def klog_ready():
            if state["capture"]:
                return True
            try:
                with open(klog_stderr, "r", encoding="utf-8",
                          errors="replace") as f:
                    for ln in f:
                        if "file=" in ln and "klog-" in ln:
                            state["capture"] = ln.split("file=", 1)[
                                1].strip()
                            return True
            except OSError:
                pass
            return False

        proc = spawn_and_confirm(klog_argv, klog_stderr, klog_ready, ledger,
                                 "7", "klog tap")
        capture = state["capture"]
        ok = bool(capture and os.path.isfile(capture)
                  and os.path.getsize(capture) > 0)
        ledger.add("7", "klog capture file created (new-file rule)",
                   "path from '[klog-capture] ... file=' line, non-empty",
                   "%s (%d bytes)" % (capture,
                                      os.path.getsize(capture)
                                      if capture and
                                      os.path.isfile(capture) else -1),
                   ok)
        if not ok:
            raise Abort("STEP 7 FAILED: klog capture file not confirmed on "
                        "disk. No klog evidence would exist for the launch "
                        "-- do not launch (--skip-klog exists only for an "
                        "explicit operator decision).")
        jobs.append({"job": "klog tap (agent-T procedure; TCP %s:%d)"
                     % (console_ip, KLOG_PORT),
                     "pid": proc.pid, "command": fmt_cmd(klog_argv),
                     "stderr/log": klog_stderr,
                     "capture file": capture,
                     "window": "%d s (ends by itself)" % KLOG_WINDOW_S})
    else:
        ledger.add("7", "klog tap", "started before launch",
                   "SKIPPED by --skip-klog", True,
                   note="operator explicitly waived klog coverage")
        jobs.append({"job": "klog tap", "pid": "skipped",
                     "note": "--skip-klog: NO klog evidence will exist for "
                             "this launch (recorded as a gap, never rated)"})

    mon_log = os.path.join(out_dir, "eboot-monitor-%s.log" % ts_slug())
    mon_argv = ["python3", os.path.abspath(__file__), "--run-eboot-monitor",
                "--out-dir", out_dir, "--poll-seconds", str(MONITOR_POLL_S),
                "--window-seconds", str(MONITOR_WINDOW_S),
                "--console-ip", console_ip, "--log-path", mon_log]
    mon = spawn_and_confirm(
        mon_argv, mon_log,
        lambda: os.path.isfile(mon_log) and
        os.path.getsize(mon_log) > 0,
        ledger, "7", "eboot monitor")
    jobs.append({"job": "eboot monitor (15 s processes_list poll; logs "
                        "transitions; read-only HTTP)",
                 "pid": mon.pid, "command": fmt_cmd(mon_argv),
                 "log": mon_log,
                 "window": "%d s (ends by itself)" % MONITOR_WINDOW_S})
    return jobs


# ---------------------------------------------------------------------------
# Eboot monitor (internal subcommand; bounded, read-only, never kills)
# ---------------------------------------------------------------------------

def run_eboot_monitor(console_ip, out_dir, poll_s, window_s, log_path=None):
    if not log_path:
        log_path = os.path.join(out_dir, "eboot-monitor-%s.log" % ts_slug())
    watched = (GAME_PROC_NAME, PLOADER_PROC_NAME)
    with open(log_path, "a", encoding="utf-8") as log:
        def w(line):
            log.write("%s %s\n" % (now_utc(), line))
            log.flush()

        w("# eboot-monitor start poll=%ss window=%ss console=%s "
          "watched=%s (read-only; never kills; ends by itself)"
          % (poll_s, window_s, console_ip, ",".join(watched)))
        prev = {}
        deadline = time.time() + window_s
        errors = 0
        while time.time() < deadline:
            r = run_cmd(http_get_cmd(console_ip, PROCESSES_PATH,
                                     "-"), 30)
            procs = None
            if r["rc"] == 0:
                try:
                    procs = json.loads(r["out"]).get("processes")
                except ValueError:
                    procs = None
            if procs is None:
                errors += 1
                w("ERROR processes_list fetch failed (rc=%s err=%s) "
                  "error #%d" % (r["rc"], r["err"][:80], errors))
            else:
                state = {n: sorted(p.get("pid") for p in procs_named(procs, n))
                         for n in watched}
                for base in REQUIRED_DAEMONS:
                    if daemon_pid(procs, base) is None:
                        state["daemon-missing:" + base] = True
                if state != prev:
                    if prev:
                        for k in watched:
                            if state.get(k) != prev.get(k):
                                w("TRANSITION %s %s -> %s"
                                  % (k, prev.get(k), state.get(k)))
                        for k, v in state.items():
                            if k.startswith("daemon-missing:") and \
                                    k not in prev:
                                w("TRANSITION daemon missing: %s" % k[15:])
                    else:
                        w("state baseline: %s"
                          % json.dumps(state, sort_keys=True))
                    prev = state
            time.sleep(poll_s)
        w("# eboot-monitor end reason=window-elapsed errors=%d" % errors)
    return 0


# ---------------------------------------------------------------------------
# Dry-run: print every command + every check with expected values
# ---------------------------------------------------------------------------

def dry_run(args, ledger):
    out_dir = args.out_dir or default_out_dir(args.variant)
    ip = args.console_ip
    prx_size = os.path.getsize(args.prx_path) \
        if os.path.isfile(args.prx_path) else None
    prx_sha = sha256_of(args.prx_path)
    prx_magic = magic_of(args.prx_path)
    print("=== DRY-RUN (no network, no console writes, no background "
          "processes, no file writes; exit 0) ===")
    print("variant=%s console=%s ftp=ftp://%s:%d out-dir=%s"
          % (args.variant, ip, ip, FTP_PORT, out_dir))
    print()
    print("STEP 1 -- preconditions (all remote; printed, not executed)")
    print("  cmd: %s"
          % fmt_cmd(http_get_cmd(ip, PROCESSES_PATH,
                                 out_dir + "/processes-precond.json")))
    print("  check: daemons present: %s (names observed as <base>.elf)"
          % ", ".join(REQUIRED_DAEMONS))
    print("  check: no process named '%s' (game must NOT be running)"
          % GAME_PROC_NAME)
    print("  cmd: %s"
          % fmt_cmd(ftp_get_cmd(ip, PLOADER_LOCK_REMOTE,
                                out_dir + "/ploader-lock.check", "10")))
    print("  check: curl rc 78 (RETR 550) = no resident ploader;")
    print("         rc 0 + LIVE pid => ABORT: wait <=%d s for the "
          "INJECTOR_TIMEOUT_S exit, then re-run (Z2 law); dead pid = stale"
          % PLOADER_TIMEOUT_S)
    print("         lock, auto-unlinked at next arm (Z2 L704) -> proceed")
    ledger.add_would("1", "processes_list fetch + daemon/eboot/lock checks",
                     "4 daemons up; no eboot.bin; lock RETR 78/550")
    print()
    print("STEP 2 -- PRX safety gate (LOCAL; executed read-only now)")
    exists = os.path.isfile(args.prx_path)
    print("  file exists: %s -> %s" % (args.prx_path,
                                       "PASS" if exists else "FAIL"))
    signed = prx_magic in (m.hex() for m in MAGIC_SIGNED)
    verdict = ("PASS (signed SELF)" if signed else
               ("FAIL: %s -> STOP: %s" % (prx_magic,
                                          UNSIGNED_ELF_MESSAGE)
                if prx_magic == MAGIC_ELF.hex() else
                "FAIL: unrecognized magic %r" % prx_magic))
    print("  magic: %r (expect %s) -> %s"
          % (prx_magic, " or ".join(m.hex() for m in MAGIC_SIGNED),
             verdict))
    print("  sha256 (recorded for the STEP 3 readback match): %s" % prx_sha)
    ledger.add("2", "PRX file exists (dry-run local check)", "regular file",
               args.prx_path if exists else "MISSING", exists)
    ledger.add("2", "PRX magic (dry-run local check)",
               " or ".join(m.hex() for m in MAGIC_SIGNED), str(prx_magic),
               signed,
               note="dry-run: a live failure here would STOP with: %s"
                    % UNSIGNED_ELF_MESSAGE if prx_magic == MAGIC_ELF.hex()
                    else "")
    print()
    print("STEP 3 -- resident backup + upload + decrypt-OFF readback")
    print("  cmd: %s"
          % fmt_cmd(ftp_get_cmd(ip, PRX_REMOTE,
                                out_dir + "/backup-resident.decrypt-off.prx",
                                CURL_PRX_RB_MAX, self_mode=True,
                                max_filesize=FTP_MAX_FILESIZE)))
    print("  check: current resident reads back SIGNED (else ABORT: never")
    print("         overwrite an unverifiable executable); rc 78/550 = no")
    print("         resident (first install) -> proceed")
    print("  cmd: %s"
          % fmt_cmd(ftp_put_cmd(ip, args.prx_path, PRX_REMOTE,
                                CURL_PRX_MAX)))
    print("  cmd: %s"
          % fmt_cmd(ftp_get_cmd(ip, PRX_REMOTE,
                                out_dir + "/prx-readback.decrypt-off.prx",
                                CURL_PRX_RB_MAX, self_mode=True,
                                max_filesize=FTP_MAX_FILESIZE)))
    print("  check: magic %s AND sha256 == %s (byte-exact)"
          % (" or ".join(m.hex() for m in MAGIC_SIGNED), prx_sha))
    print("         mismatch => one verified restore-from-backup attempt,")
    print("         then ABORT (never leave a broken executable in place)")
    ledger.add_would("3", "backup + upload + decrypt-OFF readback",
                     "SELF magic + sha256 %s byte-exact" % prx_sha)
    print()
    print("STEP 4 -- settings restore (MFA size-exact law)")
    print("  LOCAL check (executed read-only now): %s must be EXACTLY %d "
          "bytes, sha256 %s"
          % (SETTINGS_LOCAL, SETTINGS_SIZE, SETTINGS_SHA256))
    s_ok = (os.path.isfile(SETTINGS_LOCAL)
            and os.path.getsize(SETTINGS_LOCAL) == SETTINGS_SIZE
            and sha256_of(SETTINGS_LOCAL) == SETTINGS_SHA256)
    print("  local verify -> %s (%d bytes, sha %s)"
          % ("PASS" if s_ok else "FAIL/WOULD-ABORT",
             os.path.getsize(SETTINGS_LOCAL)
             if os.path.isfile(SETTINGS_LOCAL) else -1,
             sha256_of(SETTINGS_LOCAL)))
    ledger.add("4", "settings local backup (dry-run local check)",
               "%d bytes, sha256 %s" % (SETTINGS_SIZE, SETTINGS_SHA256),
               "%s bytes, sha256 %s"
               % (os.path.getsize(SETTINGS_LOCAL)
                  if os.path.isfile(SETTINGS_LOCAL) else -1,
                  sha256_of(SETTINGS_LOCAL)), s_ok)
    print("  cmd: %s"
          % fmt_cmd(ftp_put_cmd(ip, SETTINGS_LOCAL, SETTINGS_REMOTE,
                                CURL_INI_MAX)))
    print("  cmd: %s"
          % fmt_cmd(ftp_get_cmd(ip, SETTINGS_REMOTE,
                                out_dir + "/UE4SS-settings.ini.readback",
                                CURL_INI_MAX)))
    print("  check: readback == local byte-exact (cmp) AND size still %d"
          % SETTINGS_SIZE)
    ledger.add_would("4", "settings upload + readback cmp",
                     "byte-exact, %d bytes (1949-byte v1 file was MFA "
                     "code=-8 rejected)" % SETTINGS_SIZE)
    print()
    print("STEP 5 -- ploader.ini ENABLED state restore")
    print("  LOCAL check (executed read-only now): %s sha256 %s"
          % (PLOADER_INI_LOCAL, PLOADER_INI_SHA256))
    p_ok = (os.path.isfile(PLOADER_INI_LOCAL)
            and sha256_of(PLOADER_INI_LOCAL) == PLOADER_INI_SHA256)
    row = PLOADER_INI_INJECT_ROW in open(
        PLOADER_INI_LOCAL, "r", encoding="utf-8",
        errors="replace").read() if os.path.isfile(PLOADER_INI_LOCAL) \
        else False
    print("  local verify -> %s (%d bytes, sha %s, inject row '%s' %s)"
          % ("PASS" if p_ok else "FAIL/WOULD-ABORT",
             os.path.getsize(PLOADER_INI_LOCAL)
             if os.path.isfile(PLOADER_INI_LOCAL) else -1,
             sha256_of(PLOADER_INI_LOCAL),
             os.path.basename(PLOADER_INI_INJECT_ROW),
             "present" if row else "MISSING"))
    ledger.add("5", "ploader.ini local backup (dry-run local check)",
               "sha256 %s + inject row" % PLOADER_INI_SHA256[:16] + "...",
               "sha256 %s, row %s" % (sha256_of(PLOADER_INI_LOCAL),
                                      "present" if row else "MISSING"),
               p_ok and row)
    print("  cmd: %s"
          % fmt_cmd(ftp_put_cmd(ip, PLOADER_INI_LOCAL, PLOADER_INI_REMOTE,
                                CURL_INI_MAX)))
    print("  cmd: %s"
          % fmt_cmd(ftp_get_cmd(ip, PLOADER_INI_REMOTE,
                                out_dir + "/ploader.ini.readback",
                                CURL_INI_MAX)))
    print("  check: readback == local byte-exact (cmp)")
    ledger.add_would("5", "ploader.ini upload + readback cmp", "byte-exact")
    print()
    print("STEP 5.5 -- loadlist row deploy gate (Claude round-8 item 6; the")
    print("         2026-10-11 -6 incident: a declared-but-unuploaded row")
    print("         made MFA refuse the WHOLE module at boot)")
    print("  cmd: %s"
          % fmt_cmd(["python3", ROWCHECK_SCRIPT, "--console-ip", ip,
                     "--out-dir", out_dir + "/rowcheck", "--json"]))
    print("  check: fetch %s; parse EVERY 'file<TAB>role<TAB>path<TAB>"
          "size'" % ROWCHECK_LOADLIST_REMOTE)
    print("         row; fetch each file; verify it EXISTS at its EXACT")
    print("         declared size (game closed, per STEP 1). Any MISSING /")
    print("         SIZE-MISMATCH / malformed row => ABORT with the exact")
    print("         row -- FIX: upload the missing rows or fix the")
    print("         loadlist. MANDATORY step; no skip flag.")
    ledger.add_would("5.5", "loadlist row gate (every file row present at "
                     "its exact size)", "all rows PASS (rc 0)")
    print()
    print("STEP 6 -- arm ploader (ini-edit law: REQUIRED after any ini edit)")
    print("  cmd: %s"
          % fmt_cmd(http_get_cmd(ip, ARM_URL_PATH, out_dir + "/arm.response")))
    print("  check: rc 0 (body 'OK'); then poll processes_list up to %d s"
          % ARM_POLL_BUDGET_S)
    print("         (every %d s) for a FRESH '%s' pid absent from the "
          "STEP 1 baseline;" % (ARM_POLL_INTERVAL_S, PLOADER_PROC_NAME))
    print("         none => ABORT (never duplicate-send the dispatch)")
    ledger.add_would("6", "single loadpayload dispatch + fresh pid",
                     "new payload.elf pid <=%d s" % ARM_POLL_BUDGET_S)
    print()
    print("STEP 7 -- background captures (started only after all gates)")
    if args.skip_klog:
        print("  klog tap: SKIPPED (--skip-klog) -- NO klog evidence will "
              "exist; recorded as a gap")
    else:
        print("  cmd: python3 %s --out-dir %s --window-seconds %d"
              % (KLOG_CAPTURE, out_dir, KLOG_WINDOW_S))
        print("         (detached; stderr -> %s/klog-process-<ts>.log; "
              "ready when the" % out_dir)
        print("          '[klog-capture] ... file=' line names a NEW "
              "non-empty capture file)")
    print("  cmd: python3 %s --run-eboot-monitor --out-dir %s "
          "--poll-seconds %d --window-seconds %d --console-ip %s"
          % (os.path.abspath(__file__), out_dir, MONITOR_POLL_S,
             MONITOR_WINDOW_S, ip))
    print("         (detached; logs '%s'/'%s' pid transitions + daemon"
          % (GAME_PROC_NAME, PLOADER_PROC_NAME))
    print("          loss every %d s for %d s, then exits by itself)"
          % (MONITOR_POLL_S, MONITOR_WINDOW_S))
    ledger.add_would("7", "klog tap + eboot monitor started + confirmed",
                     "capture file created; monitor alive")
    print()
    print("STEP 8 -- report")
    print("  would write: %s"
          % os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         "LAUNCH-READY-%s-<timestamp>.md" % args.variant))
    print()
    print("--- LAUNCH-READY-%s skeleton (would be written on the live run) "
          "---" % args.variant)
    print()
    print(generate_ready_md(args.variant, True, ip, args.prx_path, prx_size,
                            prx_sha, prx_magic, out_dir, ledger,
                            pids={"precondition daemons": "(live values)",
                                  "pre-arm payload.elf pids": "(live values)",
                                  "armed ploader pid": "(live value)"},
                            jobs=[{"job": "(live values)"}]))
    return 0


# ---------------------------------------------------------------------------
# Argparse + main
# ---------------------------------------------------------------------------

def parse_args(argv):
    p = argparse.ArgumentParser(
        prog="prep_modded_launch.py",
        description="One-command launch preparation for the PS5 UE4SS l25 "
                    "variant A/B tests. Verifies EVERY step and aborts on "
                    "any mismatch. Never touches autoload.txt, never kills "
                    "processes, never bypasses a failed verification.")
    p.add_argument("--variant", required=False, choices=("A", "B"),
                   help="which twin to prepare (launch order v3: B FIRST "
                        "-- registration SKIPPED, diagnostic-only; A second, "
                        "to confirm the mechanism. Either way B is never "
                        "left resident)")
    p.add_argument("--prx-path", required=False,
                   help="local path to the fake-signed variant PRX (must "
                        "carry SELF magic 4f153d1d/5414f5ee)")
    p.add_argument("--skip-klog", action="store_true",
                   help="do not start the klog tap (the launch then has NO "
                        "klog evidence; recorded as a gap)")
    p.add_argument("--console-ip", default=DEFAULT_CONSOLE_IP,
                   help="console host (default %(default)s)")
    p.add_argument("--dry-run", action="store_true",
                   help="print all commands + all checks with expected "
                        "values; nothing executes (local read-only checks "
                        "do run); exit 0")
    # internal subcommand (spawned by step 7)
    p.add_argument("--run-eboot-monitor", action="store_true",
                   help=argparse.SUPPRESS)
    p.add_argument("--out-dir", default=None,
                   help="session out-dir for ALL run artifacts (klog taps, "
                        "eboot-monitor logs, readbacks, receipts). Default "
                        "since 2026-10-10 (evidence-retention law): the "
                        "durable tree outputs/2026-10-08-l25-prep/evidence/"
                        "runtime/l25-variant-<X>/ -- never /tmp.")
    p.add_argument("--poll-seconds", type=float, default=MONITOR_POLL_S,
                   help=argparse.SUPPRESS)
    p.add_argument("--window-seconds", type=float,
                   default=MONITOR_WINDOW_S, help=argparse.SUPPRESS)
    p.add_argument("--log-path", default=None, help=argparse.SUPPRESS)
    return p.parse_args(argv)


def main(argv):
    args = parse_args(argv)

    if args.run_eboot_monitor:  # internal bounded monitor loop
        if not args.out_dir:
            print("--run-eboot-monitor requires --out-dir", file=sys.stderr)
            return 2
        return run_eboot_monitor(args.console_ip, args.out_dir,
                                 args.poll_seconds, args.window_seconds,
                                 args.log_path)

    if not args.variant or not args.prx_path:
        print("prep_modded_launch.py: error: the following arguments are "
              "required: --variant, --prx-path (not needed only for the "
              "internal --run-eboot-monitor mode)", file=sys.stderr)
        return 2

    if args.dry_run:
        return dry_run(args, Ledger())

    # ---- live ----
    ledger = Ledger()
    out_dir = args.out_dir or default_out_dir(args.variant)
    os.makedirs(out_dir, exist_ok=True)
    prx_size = prx_sha = prx_magic = None
    pids, jobs = {}, []
    abort_reason = None
    try:
        # STEP 1
        procs = step1_preconditions(args.console_ip, out_dir, ledger)
        pids["precondition daemons (kstuff/pldmgr/ftpsrv/klogsrv)"] = \
            ", ".join("%s=%s" % (b, daemon_pid(procs, b))
                      for b in REQUIRED_DAEMONS)
        pids["pre-arm '%s' pids" % PLOADER_PROC_NAME] = \
            sorted(p.get("pid") for p in procs_named(procs,
                                                     PLOADER_PROC_NAME))
        # STEP 2
        prx_size, prx_sha, prx_magic = step2_prx_gate(args.prx_path, ledger)
        # STEP 3
        backup_info, rb_info = step3_upload_prx(
            args.console_ip, out_dir, args.prx_path, prx_sha, prx_size,
            ledger)
        # STEP 4
        verify_local_ini(SETTINGS_LOCAL, SETTINGS_SIZE, SETTINGS_SHA256,
                         "UE4SS-settings (1947 B original)", ledger, "4")
        upload_ini_and_verify(args.console_ip, out_dir, SETTINGS_LOCAL,
                              SETTINGS_REMOTE, "UE4SS-settings.ini", "4",
                              ledger)
        # STEP 5
        verify_local_ini(PLOADER_INI_LOCAL, PLOADER_INI_SIZE,
                         PLOADER_INI_SHA256, "ploader.ini (enabled state)",
                         ledger, "5")
        upload_ini_and_verify(args.console_ip, out_dir, PLOADER_INI_LOCAL,
                              PLOADER_INI_REMOTE, "ploader.ini", "5",
                              ledger)
        # STEP 5.5 -- loadlist row deploy gate (after uploads, BEFORE the
        # one-shot arm dispatch and the LAUNCH-READY handoff)
        step5_5_rowcheck(args.console_ip, out_dir, ledger)
        # STEP 5.6 -- settled readback (wait, then the exact sha again)
        step5_6_settle_readback(args.console_ip, out_dir, prx_sha, prx_size, ledger)
        # STEP 7 (moved before the arm, 2026-10-13): klog tap + eboot monitor first, so a console drop at the
        # arm is recorded
        jobs = step7_background(args.console_ip, out_dir, args.variant,
                                ledger, args.skip_klog)
        # STEP 6
        fresh_pid, baseline, _ = step6_arm_ploader(
            args.console_ip, out_dir, procs, ledger)
        pids["armed ploader pid (FRESH, vs baseline %s)"] = \
            "%s (baseline %s)" % (fresh_pid, sorted(baseline))
    except Abort as e:
        abort_reason = str(e)
        print("[prep] ABORT: %s" % abort_reason, file=sys.stderr)
    except Exception as e:  # unexpected local failure: same abort contract
        abort_reason = "UNEXPECTED ERROR (%s: %s) -- treated as a failed " \
                       "verification; nothing bypassed" % (
                           type(e).__name__, e)
        print("[prep] ABORT: %s" % abort_reason, file=sys.stderr)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    name = ("LAUNCH-PREP-FAILED-%s-%s.md" % (args.variant, ts_slug())
            if abort_reason else "LAUNCH-READY-%s-%s.md"
            % (args.variant, ts_slug()))
    report = generate_ready_md(args.variant, False, args.console_ip,
                               args.prx_path, prx_size, prx_sha, prx_magic,
                               out_dir, ledger, pids, jobs, abort_reason)
    path = os.path.join(script_dir, name)
    with open(path, "w", encoding="utf-8") as f:
        f.write(report + "\n")
    print("[prep] report: %s" % path)
    if abort_reason:
        print("[prep] LAUNCH IS NOT AUTHORIZED BY THIS PREP -- fix the "
              "gate above and re-run from STEP 1.", file=sys.stderr)
        return 1
    print("[prep] all gates passed; klog tap + eboot monitor running; "
          "hand the user instruction sequence to the player NOW.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
