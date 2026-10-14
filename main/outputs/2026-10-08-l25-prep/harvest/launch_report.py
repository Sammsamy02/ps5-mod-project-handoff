#!/usr/bin/env python3
"""launch_report.py -- post-launch reviewer REPORT DRAFT (CLAUDE.md section 7).

Written 2026-10-12 (round6-prep, harvest-report track). MAC-ONLY,
STDLIB-ONLY, READ-ONLY on every input. It never contacts the console: it
only reads local files that harvest.py and prep_modded_launch.py already
wrote, and it writes nothing except the --out file (stdout when --out is
not given). Bytecode caching is switched off, so importing the sibling
helpers never drops .pyc files into harvest/__pycache__.

WHAT IT READS
  A harvested session = the four files harvest.py pulls after a launch
  (harvest.py:519 build_pull_plan; harvest.py writes them next to itself,
  harvest.py:1784):
      harvest-<label>-ue4ss.log            console UE4SS log (harvest.py:122)
      harvest-<label>-runtime-status.txt   runtime status rows (harvest.py:129)
      harvest-<label>-supplier-diag.jsonl  supplier diag JSONL (harvest.py:140)
      harvest-<label>-aligned-entry.log    aligned-entry diag log (harvest.py:153)
  --klog FILE       klog capture (klog-*.log written by the local TCP tap).
                    If --receipt names a 'capture file:' that exists on the
                    Mac it is used, unless --no-klog.
  --receipt FILE    LAUNCH-READY-<variant>-<ts>.md (prep_modded_launch.py):
                    PRX sha256 + readback row, variant, the rowcheck receipt
                    it cites (-> loadlist rows), the klog capture path.
  --loadlist FILE   loadlist.txt ('file<TAB>role<TAB>path<TAB>size' rows,
                    same shape as prelaunch_rowcheck.py:203 parse_loadlist)
                    or a prelaunch_rowcheck receipt JSON (its 'rows'). This
                    is the ONLY source of mod names: no mod name is written
                    in this script. Default: the rowcheck receipt that
                    --receipt cites, when it exists on the Mac.
  --ledger FILE     build ledger(s) (default ../BUILD-LEDGER-*.md) and
  --frozen-root DIR frozen build dirs (default outputs/2026-10-08-l26-frozen)
                    give sha -> snapshot; the log's [BUILD] line
                    (candidate/src/BuildStamp.cpp:64) is checked against them.

DECODE FIRST: every input goes through decode_session_log.py (strip_boms
:208 + decode_bytes :89 + line_stats :151) before any matching; nothing is
plain-grepped. Physical lines are kept (the decoder preserves every
newline) and each physical line is split into messages at every UE4SS
timestamp prefix '[YYYY-MM-DD HH:MM:SS] ' (harvest.py:210), because a
message printed without a trailing newline shares its physical line with
the next message (e.g. a mod's print followed by '[l22] Lua mod started').

WHAT IT WRITES: one markdown draft in the CLAUDE.md section 7 shape: build
sha + snapshot, a 'what changed' placeholder, per-mod verdicts with the
exact (decoded) log lines, the self-test table (PASS/FAIL/SKIP/SLOW with
timings, SUMMARY and GAPS lines), the B2-B4 diagnostic lines, Lua errors
with the mod they came from, crash/panic/abort/longjmp markers, an
open-issues list and a next-step placeholder. Every 192.168.x.x address is
replaced by <console-ip> in everything printed (report, stdout, stderr).

LABELS: everything here is Mac-only parsing of local files. Every section
carries a 'Label:' line. Log lines are labelled console-proven ONLY when
both freshness checks pass: (a) every harvested file's Mac time is after
the receipt time, and (b) the log CONTENT is from this launch: the analysed
session's own console-clock timestamps start after the receipt and end
before the UE4SS file's Mac time (console clock read as the Mac's local
time, CONSOLE_CLOCK_NOTE; --console-utc-offset overrides), and the log's
[BUILD] id matches the ledger build id of the receipt sha when both exist.
Anything unknown is labelled NOT console-proven (fail closed).
--fixture-test stamps 'fixture test, not a verdict' on the title, on every
section heading and on every per-mod verdict, and never labels anything
console-proven: use it for any earlier session (CLAUDE.md section 5).

EXIT: 0 report produced; 1 zero files or zero lines parsed, the UE4SS log
missing/empty (the report is still produced, marked INCOMPLETE), or a
loadlist with zero mod rows (zero items checked); 2 usage/input error; 3
internal redaction self-check failed (a full address or a '192.168.<digit>'
fragment left). --self-test runs built-in synthetic checks in memory (no
file writes): exit 0 when all pass, 1 on any failure or zero checks ran.

USAGE
  python3 -B launch_report.py --label l25-a-7 --receipt LAUNCH-READY-A-<ts>.md \\
          --out ../evidence/<dir>/LAUNCH-REPORT-l25-a-7.md
  python3 -B launch_report.py --dir SESSION_DIR [--label LABEL] ...
  python3 -B launch_report.py --ue4ss F [--runtime-status F] [--diag F] [--aligned F] ...
  python3 -B launch_report.py --self-test
"""

import argparse
import collections
import datetime
import glob
import json
import os
import re
import sys
import time

sys.dont_write_bytecode = True  # never write .pyc into harvest/__pycache__

HERE = os.path.dirname(os.path.abspath(__file__))
PREP_ROOT = os.path.dirname(HERE)          # outputs/2026-10-08-l25-prep
OUTPUTS_ROOT = os.path.dirname(PREP_ROOT)  # outputs/
if HERE not in sys.path:
    sys.path.insert(0, HERE)

try:
    import decode_session_log as DSL  # decode-first is mandatory
    _DSL_ERROR = None
except Exception as _e:  # pragma: no cover - missing decoder = hard stop
    DSL = None
    _DSL_ERROR = "%s: %s" % (type(_e).__name__, _e)

try:  # second source for the B2-B4 marker texts
    import add_build_markers as _ABM
    ABM_SETS = getattr(_ABM, "SETS", None)
except Exception:
    ABM_SETS = None

try:  # same row parser as the pre-launch row gate
    import prelaunch_rowcheck as _ROWCHECK
    _ROWCHECK_PARSE = getattr(_ROWCHECK, "parse_loadlist", None)
except Exception:
    _ROWCHECK_PARSE = None

VERSION = "launch_report.py 1.1 (2026-10-12, round6-prep harvest-report; review fixes)"
FIXTURE_TAG = "fixture test, not a verdict"
IP_RE = re.compile(r"192\.168\.\d{1,3}\.\d{1,3}")
IP_MASK = "<console-ip>"
IP_FRAGMENT_RE = re.compile(r"192\.168\.\d")  # final self-check: no address piece may survive
REPORT_LIMIT_BYTES = 29 * 1024 * 1024  # CLAUDE.md section 7: under 30 MB

# (key, harvest.py local-name suffix, plain-name globs, description)
HARVEST_KINDS = (
    ("ue4ss", "ue4ss.log", ("UE4SS.log", "*ue4ss*.log"),
     "UE4SS log (console /data/plugins/UE4SS.log; harvest.py:122)"),
    ("runtime-status", "runtime-status.txt",
     ("runtime-status.txt", "*runtime-status*.txt"),
     "runtime status rows (harvest.py:129)"),
    ("diag", "supplier-diag.jsonl", ("*supplier-diag*.jsonl",),
     "supplier diag JSONL (harvest.py:140)"),
    ("aligned", "aligned-entry.log", ("*aligned*entry*.log",),
     "aligned-entry diag log (harvest.py:153)"),
)
KIND_ORDER = ("ue4ss", "runtime-status", "diag", "aligned", "klog")

# --- UE4SS log structure ---------------------------------------------------
TS_RE = re.compile(r"\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\] ")  # harvest.py:210 (unanchored)
BANNER_RE = re.compile(r"UE4SS\s*-?\s*v\d")                       # harvest.py:213
APPENDED_RE = re.compile(r"\[t=\d+ms pid=\d+\]")                   # harvest.py:214
BUILD_RE = re.compile(r"\[BUILD\] id=(\S+) changes=(\S+) snapshot=(\S+)")  # BuildStamp.cpp:64
LUA_PRINT_PREFIX = "[Lua] "  # candidate/src/LuaLibrary.cpp:40 global_print
RUNTIME_LUA_MSGS = (         # runtime's own '[Lua] ' lines, not mod prints
    re.compile(r"^\[Lua\] Exposing size of '"),          # candidate/src/LuaMod.cpp:1182
    re.compile(r"^\[Lua\] Failed to expose size of '"),  # candidate/src/LuaMod.cpp:1191
)

# Mod lifecycle lines (kind, regex on one message, source).
LIFECYCLE = (
    ("announce", re.compile(r"^Starting (?:Lua|C\+\+) mod '(.+?)'\s*$"),
     "candidate/src/UE4SSProgram.cpp:2284"),
    ("announce", re.compile(r"^Mod '(.+?)' has enabled\.txt, starting mod\.\s*$"),
     "candidate/src/UE4SSProgram.cpp:2343"),
    ("announce", re.compile(r"^Starting mod: (.+?)\s*$"),
     "candidate/src/UE4SSProgram.cpp:2787"),
    ("started", re.compile(r"^\[l22\] Lua mod started: (.+?) \(main script returned\)"),
     "candidate/src/LuaMod.cpp:8327"),
    ("started-hot", re.compile(r"^Mod '(.+?)' started\s*$"),
     "candidate/src/UE4SSProgram.cpp:2797"),
    ("disabled", re.compile(r"^Mod '(.+?)' disabled in mods\.txt\."),
     "candidate/src/UE4SSProgram.cpp:2289"),
    ("already-running", re.compile(r"^Mod '(.+?)' is already running"),
     "candidate/src/UE4SSProgram.cpp:2765"),
    ("cannot-start", re.compile(r"^Cannot start mod '(.+?)': main\.lua not found"),
     "candidate/src/UE4SSProgram.cpp:2783"),
    ("stopping", re.compile(r"^Stopping mod '?(.+?)'? for uninstall"),
     "candidate/src/LuaMod.cpp:8414"),
    ("shim-armed", re.compile(r"^\[SHIM\] write-area (?!dir )(\S+) -> "),
     "candidate/src/LuaPathShim.cpp:1699"),
)
BOUNDARY_KINDS = ("announce", "started", "cannot-start")

MOD_PATH_RE = re.compile(r"(?:^|[^A-Za-z0-9_])Mods/([^/\s'\"\]\)\[:]+)/")
TRUNC_CHUNK_RE = re.compile(r"\.\.\.([^/\s'\"]*)/Scripts/")  # Lua shortens long chunk names to '...<tail>'

# Lua error shapes (kind, regex, source). First match wins.
LUA_ERRORS = (
    ("lua-panic", re.compile(r"PANIC: unprotected error in call to Lua API"),
     "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:100"),
    ("pcall-error", re.compile(r"\[Lua::call_function\] lua_pcall returned (\S+) => "),
     "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:1465"),
    ("execute-file-error",
     re.compile(r"\[Lua::execute_file\] (?:luaL_loadfile|lua_pcall) returned "),
     "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:1065,1075"),
    ("execute-string-error",
     re.compile(r"\[Lua::execute_string\] (?:luaL_loadstring|lua_pcall) returned "),
     "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:1101,1110"),
    ("error-loading-script", re.compile(r"Error loading script: "),
     "candidate/src/LuaMod.cpp:8270"),
    ("main-script-failed", re.compile(r"Failed to execute main script: "),
     "candidate/src/LuaMod.cpp:8322"),
    ("main-lua-missing",
     re.compile(r"Main script 'main\.lua' not found in scripts directory: |"
                r"Cannot start mod '.+?': main\.lua not found"),
     "candidate/src/LuaMod.cpp:8338; candidate/src/UE4SSProgram.cpp:2783"),
    ("mod-exception",
     re.compile(r"Exception during mod startup: |Exception in load_and_execute_script: |"
                r"Exception during mod preparation: |Unknown exception during mod preparation"),
     "candidate/src/LuaMod.cpp:8344,8292,8178,8183"),
    ("execute-script-error", re.compile(r"Error executing script: "),
     "candidate/src/LuaMod.cpp:8281"),
    ("script-hook-exception",
     re.compile(r"\[script_hook\] A callback threw (?:an exception: |a non-standard exception)"),
     "candidate/src/LuaMod.cpp:8746,8751"),
    ("runtime-exception",
     re.compile(r"\[L25\] (?:delete-listener registration|carrier arming) exception: |"
                r"Exception in (?:setup_lua_require_paths|iterate_directory|IterateGameDirectories|"
                r"CreateLogicModsDirectory): |\[ST\] lua-seam probe (?:chunk failed|exception)"),
     "candidate/src/LuaMod.cpp:8781,8785,8806,8810,1640,3724,3734,3807,237,241"),
    ("runtime-error-line",
     re.compile(r"Error (?:processing|iterating) (?:directory|file)|Error iterating files|"
                r"CreateLogicModsDirectory: (?:Failed|Error)|IterateGameDirectories: Could not grow|"
                r"Mod path doesn't exist |Mod is absent from native Lua source: |"
                r"package(?:\.searchers)? table not found"),
     "candidate/src/LuaMod.cpp:3519,3530,3663,3686,3693,3698,3776,3787,3446,1159,1135,1361,1369"),
    ("eigt-probe-enqueue-failed", re.compile(r"\[l23\] game-thread self-test enqueue failed: "),
     "candidate/src/LuaMod.cpp:275"),
    ("lua-status", re.compile(r"\bLUA_ERR(?:RUN|SYNTAX|MEM|ERR|FILE)\b"),
     "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:935 status_to_string"),
    ("lua-chunk-error", re.compile(r"(?:[^\s:]+\.lua|\[string \"[^\"]*\"\]):\d+: "),
     "Lua '<chunk>:<line>: <message>' error text"),
    ("lua-warning", re.compile(r"Lua warning: "),
     "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:144"),
)
# Catch-all (nothing dropped without a trace): a runtime (non-'[Lua] ' print)
# message with an error-like word that no shape above matched. Listed in
# section 7 as 'unclassified', never counted as a classified Lua error.
CATCHALL_RE = re.compile(r"(?i)(?<![a-z])(error|exception|threw|uncaught|traceback)(?![a-z])")
MOD_PROBLEM_RE = re.compile(
    r"(?i)\b(error|errors|failed|failure|exception|not found|invalid|nil value|cannot|can't)\b")

# Crash / panic / abort / longjmp markers (id, regex, file kinds or None=all, source).
# Each line counts once, under the first (most specific) marker that matches.
CRASH_MARKERS = (
    ("lua-panic", re.compile(r"PANIC: unprotected error in call to Lua API"), None,
     "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:100 (stderr; Lua aborts after it)"),
    # regex rebuilt from --selftest-tag at run time (markers_for_tag)
    ("selftest-crash-resumed", re.compile(r"\[ST2\] \d+/\d+ \S+ CRASH \(resumed\)"), None,
     "self-test Scripts/main.lua:96 (an earlier run died at this entry)"),
    ("kernel-nonsleeping", re.compile(r"nonsleeping"), None,
     "harvest.py:218 CRASH_PATTERNS ('copyin: has nonsleeping lock')"),
    ("klog-copyin", re.compile(r"copyin"), ("klog",),
     "harvest.py:218 CRASH_PATTERNS (klog only, as harvest.py verdict())"),
    ("module-start-failed", re.compile(r"module_start failed"), None,
     "harvest/markers.json 'module_start failed'"),
    ("mfa-failure", re.compile(r"\bmfa_\w+"), ("ue4ss", "klog"),
     "harvest.py:215 MFA_RE (admission failure family)"),
    ("fatal", re.compile(r"(?i)(?<![a-z])fatal"), None,
     "candidate/src/UnrealInitializer.cpp:3056 'Fatal error!'; LuaLibrary.cpp:135 '[Fatal]'"),
    ("signal", re.compile(r"\bSIG(?:SEGV|ABRT|BUS|ILL|FPE|TRAP)\b|exited on signal|"
                          r"(?i:segmentation fault)"), None, "generic signal text"),
    ("terminate", re.compile(r"terminate called|std::terminate|(?i:uncaught exception)"), None,
     "generic C++ runtime text"),
    ("ps-error-code", re.compile(r"\bCE-\d{6}-\d\b"), None,
     "PS5 error-code shape (LAUNCH-READY receipts cite CE-108255-1)"),
    # word markers: 'not preceded by a letter' (not \b) so snake_case diag tags
    # such as 'x_abort' still match; over-reporting is preferred to a miss
    ("panic", re.compile(r"(?i)(?<![a-z])panic"), None, "word match"),
    ("abort", re.compile(r"(?i)(?<![a-z])abort"), None, "word match"),
    ("longjmp", re.compile(r"(?i)longjmp"), None, "word match"),
    ("crash", re.compile(r"(?i)crash"), None, "word match"),
    ("core-dump", re.compile(r"(?i)core ?dump"), None, "word match"),
)
BENIGN = (  # still listed and counted, only annotated
    (re.compile(r"Native console, GUI, crash dumping and filesystem watching are unavailable"),
     "start-up notice (candidate/src/UE4SSProgram.cpp:350), not a crash"),
    (re.compile(r"\[ST2\] SKIP "), "self-test skip-list text (self-test Scripts/part12.lua:5), not a crash"),
)


def markers_for_tag(st_tag):
    """CRASH_MARKERS / BENIGN with the self-test entries rebuilt for the
    active --selftest-tag (default '[ST2] ')."""
    t = re.escape(st_tag)
    cm = []
    for cid, rx, kinds, src in CRASH_MARKERS:
        if cid == "selftest-crash-resumed":
            rx = re.compile(t + r"\d+/\d+ \S+ CRASH \(resumed\)")
        cm.append((cid, rx, kinds, src))
    bn = []
    for rx, note in BENIGN:
        if note.startswith("self-test skip-list"):
            rx = re.compile(t + r"SKIP ")
        bn.append((rx, note))
    return cm, bn

# B2-B4 diagnostic lines (id, build, exact text, source). add_build_markers.py
# SETS is the second source; differences are printed in the report.
DIAG_MARKERS = (
    ("b2-mem-line", "B2", "[MEM] state=", "candidate/src/LuaMod.cpp:6290"),
    ("b2-lua-mem-note", "B2", "[lua-mem: gc_kb=", "candidate/src/LuaMadeSimple/LuaMadeSimple.cpp:1447"),
    ("b3-foreachuobject", "B3", "[FOU] ForEachUObject: ", "candidate/src/LuaMod.cpp:2942,2961,2989"),
    ("b3-uenum-foreachname", "B3", "[UEN] ForEachName: expected ", "candidate/src/LuaType/LuaUEnum.cpp:140"),
    ("b4-shim-write-area-dir", "B4", "[SHIM] write-area dir ", "candidate/src/LuaPathShim.cpp:644"),
)
SHIM_AREA_TEXT = "[SHIM] write-area "  # candidate/src/LuaPathShim.cpp:1699 (old shim crash point)
KV_RE = re.compile(r"(\w+)=(-?\d+)(?![\w.])")
MEM_STATE_RE = re.compile(r"\[MEM\] state=(\S+)")
UEN_RE = re.compile(r"\[UEN\] ForEachName: expected (-?\d+) \(NumEnums\), iterated (-?\d+) for (.*)$")
SHIM_DIR_RE = re.compile(r"\[SHIM\] write-area dir (.*) created=(\S+) usable=(\S+)\s*$")
FOU_KINDS = (
    ("stack-exhausted", re.compile(r"\[FOU\] ForEachUObject: Lua stack exhausted after (\d+) objects"),
     "candidate/src/LuaMod.cpp:2942"),
    ("construct-failed", re.compile(r"\[FOU\] ForEachUObject: object construction failed "),
     "candidate/src/LuaMod.cpp:2961"),
    ("construct-failures-total",
     re.compile(r"\[FOU\] ForEachUObject: (\d+) object constructions failed \((\d+) visited\)"),
     "candidate/src/LuaMod.cpp:2989"),
)
L23_KINDS = (  # per-mod game-thread probe lines (C++ side)
    ("queued", re.compile(r"\[l23\] game-thread self-test queued"), "candidate/src/LuaMod.cpp:272"),
    ("ran", re.compile(r"\[l23\] game-thread self-test ran thread=\S+ drain=(\d+)ms late=(\S+)"),
     "candidate/src/LuaMod.cpp:246"),
    ("refused", re.compile(r"\[l23\] game-thread self-test callback refused"), "candidate/src/LuaMod.cpp:247"),
    ("enqueue-failed", re.compile(r"\[l23\] game-thread self-test enqueue failed"), "candidate/src/LuaMod.cpp:275"),
    ("cancelled", re.compile(r"\[l23\] game-thread self-test cancelled"), "candidate/src/LuaMod.cpp:281"),
    ("not-drained", re.compile(r"\[l23\] game-thread self-test NOT DRAINED"), "candidate/src/LuaMod.cpp:285"),
)

# --- self-test ([ST2]) line formats: ueselftest Scripts/main.lua ---------------
ST2_TAG_DEFAULT = "[ST2] "  # main.lua:11 P(s) = print("[ST2] " .. s .. "\n")
ST_RESULT_RE = re.compile(r"^(\d+)/(\d+) (\S+) (PASS|FAIL|SKIP)(?: (.*))?$")  # main.lua:40-44
ST_SLOW_RE = re.compile(r"(?:^| )SLOW-ms=(\d+)$")                              # main.lua:39
ST_CRASH_RE = re.compile(r"^(\d+)/(\d+) (\S+) CRASH \(resumed\)$")             # main.lua:96
ST_SUMMARY_RE = re.compile(r"^SUMMARY (\d+) of (\d+) PASS SLOW (\d+)$")        # main.lua:100
ST_GAPS_RE = re.compile(r"^GAPS (\d+) soft-refused registrations counted in PASS "
                        r"\(PC registers them\):\s?(.*)$")                     # main.lua:101
ST_MEM_RE = re.compile(r"^MEM (before|after) (.*)$")                           # main.lua:80,106
ST_RUN_RE = re.compile(r"^RUN start 1\.\.(\d+)$")                              # main.lua:81
# main.lua:119 (v1.1 adds one-run=); older v1 self-tests print 'boot v1 M=168 armed=true pf=false'
# (fixture evidence/acceptance-pass2-20261012.log L887), so the tail fields are optional
ST_BOOT_RE = re.compile(r"^boot v(\S+) M=(\d+)(?: armed=(\S+))?(?: pf=(\S+))?(?: one-run=(\S+))?$")
ST_PART_RE = re.compile(r"^part(\d+) (loaded \d+|DESYNC loaded .*|LOAD-FAIL.*)$")  # main.lua:52,59,62
ST_WARN_RE = re.compile(r"^WARN count (\d+) expected (\d+)$")                  # main.lua:94
ST_SKIPNOTE_RE = re.compile(r"^SKIP (.*)$")                                    # part12.lua:5
ST_SIMPLE = {
    "AUTO start": "main.lua:118 (auto run, +180 s after load)",
    "K1 f12": "main.lua:114", "K2 q": "main.lua:114", "busy": "main.lua:68",
    "already-done-once-this-session": "main.lua:74", "done": "main.lua:107",
    "progress-io unavailable (resume disabled)": "main.lua:112",
    "print-probe": "main.lua:121",
}
ST_SLOW_THRESHOLD_MS = 16  # main.lua:39 (ms > 16 prints SLOW-ms=)

# --- other files -------------------------------------------------------------
FAILISH_TAG_RE = re.compile(
    r"(?i)(fail|error|_err\b|_err_|abort|panic|crash|refus|_exc\b|_exc_|exception|fault|"
    r"timeout|trap|denied|invalid)")
# two row shapes seen in the fixture: '... <tag> value=0x..' and '... <tag> result=0x.. errno=0x..'
ALIGNED_RE = re.compile(r"^\[([^\]]+)\] (\S+) build=(\S+) (\S+) (?:value|result)=(\S+)(?: errno=(\S+))?")
SESSION_START_TAG = "module_start_entered"  # first row of every boot in diag + aligned (fixture rows 1)

# --- receipt / ledger --------------------------------------------------------
SHA_RE = re.compile(r"\b[0-9a-f]{64}\b")


class Usage(Exception):
    """Bad invocation or input (exit 2)."""


# ---------------------------------------------------------------------------
# small helpers
# ---------------------------------------------------------------------------

def redact(s):
    return IP_RE.sub(IP_MASK, s)


def esc_ctrl(s):
    return re.sub(r"[\x00-\x08\x0b-\x1f\x7f]", lambda m: "\\x%02x" % ord(m.group(0)), s)


def clip(s, n):
    s = s.rstrip("\r\n")
    if len(s) > n:
        return s[:n] + " ...[+%d chars]" % (len(s) - n)
    return s


def cell(s, n=160):
    return clip(esc_ctrl(str(s)), n).replace("|", "\\|").replace("\n", " ")


def rel(path):
    if not path:
        return "-"
    if path.startswith("<"):  # in-memory (self-test) input
        return path
    ap = os.path.abspath(path)
    root = OUTPUTS_ROOT + os.sep
    if ap.startswith(root):
        return "outputs/" + ap[len(root):]
    return ap


def fmt_mtime(ts):
    if ts is None:
        return "-"
    return datetime.datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M:%S")


def parse_ts(s):
    try:
        return datetime.datetime.strptime(s, "%Y-%m-%d %H:%M:%S")
    except (TypeError, ValueError):
        return None


CLOCK_TOL_MIN_DEFAULT = 10
# CONSOLE_CLOCK_NOTE: the console's log clock reads as the Mac's local time.
# Mac-only evidence, two independent sources (receipt UTC time and Mac file
# time), four earlier sessions (fixture data, not a verdict):
#  - harvest-l25-B-final-ue4ss.log: first line 2026-10-08 18:36:26, receipt
#    LAUNCH-READY-B-20261008T233430Z (= 18:34:30 CDT); last line 18:40:34,
#    Mac file time 18:41:22 CDT.
#  - evidence/acceptance-pass1-20261012.log: first line 2026-10-11 20:30:36,
#    receipt LAUNCH-READY-A-20261012T012817Z (= 20:28:17 CDT); last line
#    20:32:10, Mac file time 20:32:11 CDT.
#  - evidence/round4-spawn-session-20261011.log last 08:50:27 vs file 08:50:31;
#    evidence/launch2-final-session-20261011.log last 16:32:37 vs file 16:32:53.
# Override with --console-utc-offset when the console clock changes.


def console_epoch(ts, offset_min=None):
    dt = parse_ts(ts)
    if dt is None:
        return None
    if offset_min is None:
        try:
            return time.mktime(dt.timetuple())  # Mac local time (CONSOLE_CLOCK_NOTE)
        except (OverflowError, ValueError):
            return None
    return (dt - datetime.timedelta(minutes=offset_min)).replace(tzinfo=datetime.timezone.utc).timestamp()


def utc_str(epoch):
    return datetime.datetime.utcfromtimestamp(epoch).strftime("%Y-%m-%dT%H:%M:%SZ")


def kv(text):
    return dict((k, int(v)) for k, v in KV_RE.findall(text))


def fence_for(lines):
    longest = 0
    for ln in lines:
        for m in re.finditer(r"`+", ln):
            longest = max(longest, len(m.group(0)))
    return "`" * max(3, longest + 1)


# ---------------------------------------------------------------------------
# decoded documents and messages
# ---------------------------------------------------------------------------

class Doc(object):
    """One decoded input file (decode_session_log.py first, always)."""

    def __init__(self, kind, path, raw, mtime=None):
        self.kind = kind
        self.path = path
        self.mtime = mtime
        self.bytes_in = len(raw)
        data, notes = DSL.strip_boms(raw)
        decoded, stats = DSL.decode_bytes(data)
        stats.update(DSL.line_stats(data, decoded))
        self.stats = stats
        self.notes = notes
        text = decoded.decode("utf-8", errors="replace")
        parts = text.split("\n")  # physical lines only (str.splitlines splits more)
        if parts and parts[-1] == "":
            parts.pop()
        lines = [p[:-1] if p.endswith("\r") else p for p in parts]
        # mask the console IP at LOAD time, before any parsing or clipping, so
        # a clipped quote can never keep a piece of an address
        self.ip_masked = 0
        for i, ln in enumerate(lines):
            if "192.168." in ln:
                new, k = IP_RE.subn(IP_MASK, ln)
                self.ip_masked += k
                lines[i] = new
        self.lines = lines


def load_doc(kind, path):
    try:
        with open(path, "rb") as f:
            raw = f.read()
        mtime = os.path.getmtime(path)
    except OSError as e:
        return None, "unreadable: %s" % e
    return Doc(kind, path, raw, mtime), None


class Msg(object):
    __slots__ = ("idx", "lineno", "ts", "text", "cont")

    def __init__(self, idx, lineno, ts, text, cont):
        self.idx, self.lineno, self.ts, self.text, self.cont = idx, lineno, ts, text, cont


def split_messages(line):
    """Split one decoded physical line at every UE4SS timestamp prefix.
    Text before the first prefix is a continuation/race fragment (ts None)."""
    ms = list(TS_RE.finditer(line))
    if not ms:
        return [(None, line)]
    out = []
    if ms[0].start() > 0:
        out.append((None, line[:ms[0].start()]))
    for i, m in enumerate(ms):
        end = ms[i + 1].start() if i + 1 < len(ms) else len(line)
        out.append((m.group(1), line[m.end():end]))
    return out


def ue4ss_sessions(lines):
    """[(start, end)] 0-based half-open physical-line ranges: one session per
    UE4SS banner (harvest.py:213). Up to 3 lines right before a later banner
    that carry the banner's own timestamp belong to that later session (the
    pre-banner start-up notice, candidate/src/UE4SSProgram.cpp:350)."""
    banners = [i for i, ln in enumerate(lines) if BANNER_RE.search(ln)]
    if not banners:
        return [(0, len(lines))], banners
    starts = []
    for n, b in enumerate(banners):
        if n == 0:
            starts.append(0)
            continue
        floor = banners[n - 1] + 1
        s = b
        m = TS_RE.match(lines[b])
        if m:
            k = b - 1
            while k >= floor and b - k <= 3 and lines[k].startswith("[%s] " % m.group(1)):
                k -= 1
            s = k + 1
        starts.append(s)
    ranges = []
    for n, s in enumerate(starts):
        e = starts[n + 1] if n + 1 < len(starts) else len(lines)
        ranges.append((s, e))
    return ranges, banners


def build_messages(lines, start, end):
    msgs, appended = [], []
    for i in range(start, end):
        ln = lines[i]
        if APPENDED_RE.search(ln):
            appended.append((i + 1, ln))  # later boot's appended lines (harvest.py init_passed_528 doc)
            continue
        for ts, text in split_messages(ln):
            if not text.strip():
                continue
            msgs.append(Msg(len(msgs), i + 1, ts, text.rstrip(), ts is None))
    return msgs, appended


# ---------------------------------------------------------------------------
# loadlist / mods
# ---------------------------------------------------------------------------

def parse_loadlist_text(text):
    """Rows in the prelaunch_rowcheck.py:203 shape. Uses that parser when it
    imports (same gate semantics); otherwise an equivalent local copy."""
    data = text.encode("utf-8")
    if _ROWCHECK_PARSE is not None:
        rows, malformed = _ROWCHECK_PARSE(data)
        return [dict(r, status=None) for r in rows], malformed, "prelaunch_rowcheck.parse_loadlist"
    rows, malformed = [], []
    for lineno, raw in enumerate(text.splitlines(), 1):
        if not raw.strip():
            continue
        fields = raw.split("\t")
        if fields[0] != "file":
            continue
        if len(fields) != 4 or not fields[3].isdigit():
            malformed.append({"lineno": lineno, "raw": raw, "detail": "bad row shape"})
            continue
        rows.append({"lineno": lineno, "raw": raw, "role": fields[1], "path": fields[2],
                     "size": int(fields[3]), "status": None})
    return rows, malformed, "local copy of the rowcheck row shape"


def load_loadlist(path):
    """loadlist.txt or a prelaunch_rowcheck receipt JSON -> dict."""
    with open(path, "rb") as f:
        raw = f.read()
    text = redact(raw.decode("utf-8", errors="replace"))
    info = {"path": path, "format": None, "rows": [], "malformed": [], "headers": [], "parser": None}
    stripped = text.lstrip()
    if stripped.startswith("{"):
        try:
            obj = json.loads(text)
        except ValueError as e:
            raise Usage("--loadlist %s looks like JSON but does not parse: %s" % (rel(path), e))
        rows = obj.get("rows") if isinstance(obj, dict) else None
        if not isinstance(rows, list):
            raise Usage("--loadlist %s: JSON without a 'rows' list (not a rowcheck receipt)" % rel(path))
        info["format"] = "rowcheck receipt JSON (tool: %s)" % obj.get("tool", "?")
        info["parser"] = "rowcheck receipt 'rows'"
        for r in rows:
            if isinstance(r, dict) and r.get("path"):
                info["rows"].append({"lineno": r.get("lineno"), "raw": r.get("raw", ""),
                                     "role": r.get("role"), "path": r.get("path"),
                                     "size": r.get("declared_size"), "status": r.get("status")})
        info["counts"] = obj.get("counts")
        return info
    info["format"] = "loadlist.txt rows"
    rows, malformed, parser = parse_loadlist_text(text)
    info["rows"], info["malformed"], info["parser"] = rows, malformed, parser
    for ln in text.splitlines():
        f = ln.split("\t")
        if f and f[0] and f[0] != "file":
            info["headers"].append(ln)
    return info


MOD_ROW_RE = re.compile(r"(?:^|/)Mods/([^/]+)/(.+)$")


def mods_from_loadlist(info):
    """Mod folders from row paths '.../Mods/<name>/...'. A folder with a
    Scripts/main.lua row is a Lua mod; one with neither main.lua nor
    enabled.txt rows is a library/shared folder (not a mod)."""
    mods = collections.OrderedDict()
    for r in info["rows"]:
        m = MOD_ROW_RE.search(r["path"])
        if not m:
            continue
        name, restp = m.group(1), m.group(2)
        d = mods.setdefault(name, {"rows": [], "main": False, "enabled": False, "bad_rows": []})
        d["rows"].append(r)
        if restp == "Scripts/main.lua":
            d["main"] = True
        if restp == "enabled.txt":
            d["enabled"] = True
        if r.get("status") not in (None, "PASS"):
            d["bad_rows"].append(r)
    for name, d in mods.items():
        if d["main"] and d["enabled"]:
            d["class"] = "lua mod (enabled)"
        elif d["main"]:
            d["class"] = "lua mod (no enabled.txt row)"
        elif d["enabled"]:
            d["class"] = "enabled.txt row but no Scripts/main.lua row"
        else:
            d["class"] = "library/shared folder (no main.lua, no enabled.txt row)"
        d["is_lib"] = not d["main"] and not d["enabled"]
    return mods


# ---------------------------------------------------------------------------
# receipt / ledger / frozen snapshots
# ---------------------------------------------------------------------------

def parse_receipt(text, path):
    text = redact(text)  # console IP masked before anything is parsed or clipped
    r = {"path": path, "rows": [], "status_counts": collections.Counter()}

    def one(rx):
        m = re.search(rx, text, re.M)
        return m.group(1).strip() if m else None

    r["title"] = one(r"^# (LAUNCH-READY-\S+)")
    r["generated"] = one(r"^- generated \(UTC\): (\S+)")
    r["mode"] = one(r"^- mode: (.+)$")
    r["variant"] = one(r"^- variant: (.+)$")
    r["prx_path"] = one(r"^- PRX: (.+)$")
    r["prx_size"] = one(r"^\s+- size: (\d+) bytes")
    r["sha256"] = one(r"^\s+- sha256: ([0-9a-f]{64})\s*$")
    r["magic"] = one(r"^\s+- magic: (\S+)")
    r["klog_capture"] = one(r"^- capture file: (.+)$")
    in_table = False
    for ln in text.splitlines():
        if ln.startswith("## Verification ledger"):
            in_table = True
            continue
        if in_table and ln.startswith("## "):
            break
        if in_table and re.match(r"^\|\s*\d+\s*\|", ln):
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            if len(cells) >= 6:
                row = {"n": cells[0], "step": cells[1], "check": cells[2], "expected": cells[3],
                       "actual": " | ".join(cells[4:-1]), "status": cells[-1]}
                r["rows"].append(row)
                r["status_counts"][row["status"]] += 1
    r["readback_sha256"] = None
    r["rowcheck_receipt"] = None
    for row in r["rows"]:
        if "readback sha256" in row["check"]:
            m = SHA_RE.search(row["actual"])
            r["readback_sha256"] = m.group(0) if m else None
        if "loadlist row deploy gate" in row["check"]:
            m = re.search(r"\[([^\]]+\.json)\]", row["actual"])
            r["rowcheck_receipt"] = m.group(1) if m else None
            r["rowcheck_summary"] = row["actual"].split("[")[0].strip()
            r["rowcheck_status"] = row["status"]
    r["generated_epoch"] = None
    if r["generated"]:
        try:
            dt = datetime.datetime.strptime(r["generated"], "%Y-%m-%dT%H:%M:%SZ")
            r["generated_epoch"] = dt.replace(tzinfo=datetime.timezone.utc).timestamp()
        except ValueError:
            pass
    return r


def parse_ledgers(paths):
    """Build-ledger markdown tables -> entries with a FULL 64-hex sha."""
    entries = []
    for p in paths:
        try:
            with open(p, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        header = None
        for lineno, ln in enumerate(text.splitlines(), 1):
            if not ln.startswith("|"):
                header = None
                continue
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            if header is None:
                low = [c.lower() for c in cells]
                if any("sha256" in c for c in low):
                    header = low
                continue
            if all(set(c) <= set("-: ") for c in cells):
                continue
            row = dict(zip(header, cells))
            sha_cell = next((v for k, v in row.items() if "sha256" in k), "")
            m = SHA_RE.search(sha_cell.replace("`", ""))
            if not m:
                continue
            entries.append({"file": p, "lineno": lineno, "sha": m.group(0),
                            "label": row.get("label", "").replace("*", "").strip(),
                            "snapshot": row.get("snapshot", "").strip("` "),
                            "build_id": row.get("build id", "").strip("` ")})
    return entries


def frozen_snapshots(root):
    """{sha: [(snapshot_id, manifest_path)]} from <root>/*/*.prx.sha256 +
    sibling snapshot-*.manifest.json (tree-snapshot/1 'snapshot_id')."""
    out = collections.defaultdict(list)
    if not root or not os.path.isdir(root):
        return out
    for shafile in sorted(glob.glob(os.path.join(root, "*", "*.prx.sha256"))):
        try:
            with open(shafile, "r", encoding="utf-8", errors="replace") as f:
                m = SHA_RE.search(f.read())
        except OSError:
            continue
        if not m:
            continue
        for man in sorted(glob.glob(os.path.join(os.path.dirname(shafile), "snapshot-*.manifest.json"))):
            try:
                with open(man, "r", encoding="utf-8") as f:
                    sid = json.load(f).get("snapshot_id")
            except (OSError, ValueError, AttributeError):
                continue
            if sid:
                out[m.group(0)].append((str(sid), man))
    return out


# ---------------------------------------------------------------------------
# analysis
# ---------------------------------------------------------------------------

def scan_lifecycle(msgs):
    events = []
    for m in msgs:
        if m.cont:
            continue
        for kind, rx, src in LIFECYCLE:
            mm = rx.search(m.text)
            if mm:
                events.append({"kind": kind, "name": mm.group(1).strip(), "msg": m, "source": src})
                break
    return events


def startup_windows(msgs, events):
    """Message-index windows per mod: (previous start boundary, '[l22] Lua
    mod started: X'] and, for an announced mod with no started line,
    (announce, next boundary). Mods start one after another on one thread
    (candidate/src/UE4SSProgram.cpp:2284/2343 loops), so a '[Lua] ' print
    inside a window PROBABLY belongs to that mod (async prints of earlier
    mods can interleave: 'probable' only)."""
    bidx = sorted(set(e["msg"].idx for e in events if e["kind"] in BOUNDARY_KINDS))

    def prev_b(j):
        p = -1
        for b in bidx:
            if b < j:
                p = b
            else:
                break
        return p

    def next_b(j):
        for b in bidx:
            if b > j:
                return b
        return len(msgs)

    windows = []
    started = [(e["msg"].idx, e["name"]) for e in events if e["kind"] == "started"]
    for j, name in started:
        windows.append((name, prev_b(j) + 1, j))
    for e in events:
        if e["kind"] != "announce":
            continue
        j = e["msg"].idx
        nb = next_b(j)
        if any(sj == nb and sn == e["name"] for sj, sn in started):
            continue
        windows.append((e["name"], j + 1, nb))
    owner = {}
    for name, s, e in windows:
        for k in range(s, e):
            owner.setdefault(k, name)
    return windows, owner


def attribute(blob, mods, window_owner):
    lib_hit = None
    for mm in MOD_PATH_RE.finditer(blob):
        name = mm.group(1)
        if name in mods and not mods[name]["is_lib"]:
            return name, "path 'Mods/%s/'" % name
        if name in mods and mods[name]["is_lib"] and lib_hit is None:
            lib_hit = name
    for mm in MOD_PATH_RE.finditer(blob):
        name = mm.group(1)
        if name not in mods:
            return name, "path 'Mods/%s/' (folder NOT in the loadlist)" % name
    for mm in TRUNC_CHUNK_RE.finditer(blob):
        piece = mm.group(1)
        if piece:
            cands = [n for n in mods if n.endswith(piece) and not mods[n]["is_lib"]]
            if len(cands) == 1:
                return cands[0], "shortened Lua chunk path '...%s/Scripts/' (unique suffix match)" % piece
    for n in sorted(mods, key=len, reverse=True):
        if not mods[n]["is_lib"] and ("[%s]" % n) in blob:
            return n, "tag '[%s]'" % n
    if window_owner:
        return window_owner, "position: inside its start-up window (probable)"
    if lib_hit:
        return lib_hit, "path in library folder '%s' (calling mod unknown)" % lib_hit
    return None, "unknown (no path, tag or start-up window names a mod)"


def scan_lua_errors(msgs, mods, owner, st_tag):
    """-> (classified error lines, unclassified error-like runtime lines)."""
    errs, others = [], []
    i, n = 0, len(msgs)
    while i < n:
        m = msgs[i]
        if st_tag in m.text:  # self-test rows live in the self-test table
            i += 1
            continue
        hit = None
        for kind, rx, src in LUA_ERRORS:
            if rx.search(m.text):
                hit = (kind, src)
                break
        if hit is None:
            if (not m.text.startswith(LUA_PRINT_PREFIX) and CATCHALL_RE.search(m.text)
                    and not m.text.lstrip().startswith("stack traceback")):
                name, how = attribute(m.text, mods, owner.get(m.idx))
                others.append({"kind": "unclassified", "source": "catch-all (CATCHALL_RE)", "msg": m,
                               "ctx": [], "mod": name, "how": how})
            i += 1
            continue
        ctx = []
        j = i + 1
        while j < n and msgs[j].cont and len(ctx) < 12:
            ctx.append(msgs[j])
            j += 1
        blob = m.text + "\n" + "\n".join(c.text for c in ctx)
        name, how = attribute(blob, mods, owner.get(m.idx))
        errs.append({"kind": hit[0], "source": hit[1], "msg": m, "ctx": ctx, "mod": name, "how": how})
        i = j
    return errs, others


def scan_mod_problems(msgs, mods, owner, st_tag, skip_idx):
    out = []
    for m in msgs:
        if m.cont or m.idx in skip_idx or not m.text.startswith(LUA_PRINT_PREFIX) or st_tag in m.text:
            continue
        if any(rx.search(m.text) for rx in RUNTIME_LUA_MSGS):
            continue
        if MOD_PROBLEM_RE.search(m.text[len(LUA_PRINT_PREFIX):]):
            name, how = attribute(m.text, mods, owner.get(m.idx))
            out.append({"msg": m, "mod": name, "how": how})
    return out


def parse_selftest(msgs, tag):
    """A self-test line is the tag at the START of a message, or right after
    the '[Lua] ' print prefix (candidate/src/LuaLibrary.cpp:40). The tag
    anywhere else is an echo of a print (e.g. "[LFB-M] print-widen in='[ST2]
    ...", the first-10 print marker, typescan-r4/include/LuaFNameBridge.hpp:693):
    counted in 'echo', never parsed as a self-test line."""
    st = {"rows": [], "runs": [], "summary": [], "gaps": [], "mem": [], "boot": [], "parts": [],
          "warn": [], "crash": [], "simple": [], "skipnotes": [], "unrecognized": [], "lines": 0,
          "echo": []}
    run = 0
    for m in msgs:
        p = m.text.find(tag)
        if p < 0:
            continue
        if m.text[:p] not in ("", LUA_PRINT_PREFIX):
            st["echo"].append({"msg": m})
            continue
        payload = m.text[p + len(tag):].rstrip()
        st["lines"] += 1
        mm = ST_CRASH_RE.match(payload)
        if mm:
            st["crash"].append({"n": int(mm.group(1)), "M": int(mm.group(2)), "name": mm.group(3), "msg": m})
            continue
        mm = ST_RESULT_RE.match(payload)
        if mm:
            detail = mm.group(5) or ""
            slow = None
            sm = ST_SLOW_RE.search(detail)
            if sm and mm.group(4) != "SKIP":
                slow = int(sm.group(1))
                detail = detail[:sm.start()].rstrip()
            st["rows"].append({"n": int(mm.group(1)), "M": int(mm.group(2)), "name": mm.group(3),
                               "res": mm.group(4), "detail": detail, "slow": slow, "run": run, "msg": m})
            continue
        mm = ST_RUN_RE.match(payload)
        if mm:
            run += 1
            st["runs"].append({"run": run, "M": int(mm.group(1)), "msg": m})
            continue
        mm = ST_SUMMARY_RE.match(payload)
        if mm:
            st["summary"].append({"p": int(mm.group(1)), "M": int(mm.group(2)), "slow": int(mm.group(3)),
                                  "run": run, "msg": m})
            continue
        mm = ST_GAPS_RE.match(payload)
        if mm:
            names = [x for x in mm.group(2).split(",") if x]
            st["gaps"].append({"n": int(mm.group(1)), "names": names, "run": run, "msg": m})
            continue
        mm = ST_MEM_RE.match(payload)
        if mm:
            # 'MEM before' prints right BEFORE 'RUN start' (main.lua:80-81), so it
            # belongs to the run that starts next; 'MEM after' to the current run.
            r_ = run + 1 if mm.group(1) == "before" else run
            st["mem"].append({"when": mm.group(1), "kv": kv(mm.group(2)), "raw": mm.group(2), "run": r_, "msg": m})
            continue
        mm = ST_BOOT_RE.match(payload)
        if mm:
            st["boot"].append({"ver": mm.group(1), "M": int(mm.group(2)), "msg": m})
            continue
        mm = ST_PART_RE.match(payload)
        if mm:
            st["parts"].append({"part": int(mm.group(1)), "state": mm.group(2), "msg": m})
            continue
        mm = ST_WARN_RE.match(payload)
        if mm:
            st["warn"].append({"msg": m})
            continue
        if payload in ST_SIMPLE:
            st["simple"].append({"what": payload, "msg": m})
            continue
        mm = ST_SKIPNOTE_RE.match(payload)
        if mm:
            st["skipnotes"].append({"msg": m})
            continue
        st["unrecognized"].append({"msg": m})
    return st


def selftest_run_view(st):
    """Rows/summary of the LAST run that printed result rows."""
    runs_with_rows = sorted(set(r["run"] for r in st["rows"]))
    if not runs_with_rows:
        return None
    run = runs_with_rows[-1]
    rows = [r for r in st["rows"] if r["run"] == run]
    M = None
    for src in ([x["M"] for x in st["runs"] if x["run"] == run] +
                [x["M"] for x in st["summary"] if x["run"] == run] + [r["M"] for r in rows]):
        M = src
        break
    seen = collections.Counter(r["n"] for r in rows)
    missing = [k for k in range(1, (M or 0) + 1) if k not in seen]
    dups = sorted(k for k, c in seen.items() if c > 1)
    cnt = collections.Counter(r["res"] for r in rows)
    slow = [r for r in rows if r["slow"] is not None]
    start = next((x for x in st["runs"] if x["run"] == run), None)
    summ = next((x for x in st["summary"] if x["run"] == run), None)
    gaps = next((x for x in st["gaps"] if x["run"] == run), None)
    mem = [x for x in st["mem"] if x["run"] == run]
    wall = None
    if start and summ and start["msg"].ts and summ["msg"].ts:
        a, b = parse_ts(start["msg"].ts), parse_ts(summ["msg"].ts)
        if a and b:
            wall = int((b - a).total_seconds())
    return {"run": run, "rows": rows, "M": M, "missing": missing, "dups": dups, "cnt": cnt,
            "slow": slow, "start": start, "summary": summ, "gaps": gaps, "mem": mem, "wall": wall,
            "runs_total": len(st["runs"])}


def scan_diag_markers(msgs):
    res = collections.OrderedDict((d[0], []) for d in DIAG_MARKERS)
    shim_area = []
    for m in msgs:
        for mid, _b, text, _s in DIAG_MARKERS:
            if text in m.text:
                res[mid].append(m)
        if SHIM_AREA_TEXT in m.text and "[SHIM] write-area dir " not in m.text:
            shim_area.append(m)
    return res, shim_area


def summarize_mem(ms):
    per_state = collections.OrderedDict()
    vals = []
    for m in ms:
        sm = MEM_STATE_RE.search(m.text)
        state = sm.group(1) if sm else "?"
        d = kv(m.text[m.text.find("[MEM]"):])
        vals.append((m, state, d))
        ps = per_state.setdefault(state, {"n": 0, "first": None, "last": None})
        ps["n"] += 1
        ps["first"] = ps["first"] or (m, d)
        ps["last"] = (m, d)
    return per_state, vals


def scan_l23(msgs):
    out = collections.OrderedDict((k[0], []) for k in L23_KINDS)
    for m in msgs:
        for kind, rx, _src in L23_KINDS:
            mm = rx.search(m.text)
            if mm:
                out[kind].append((m, mm))
                break
    return out


def scan_crash(kind, numbered, markers, benign):
    hits = collections.OrderedDict((c[0], []) for c in markers)
    for lineno, text in numbered:
        for cid, rx, kinds, _src in markers:
            if kinds and kind not in kinds:
                continue
            if rx.search(text):
                note = next((nt for brx, nt in benign if brx.search(text)), None)
                hits[cid].append((lineno, text, note))
                break
    return hits


def parse_runtime_status(lines):
    rows = [ln.split("\t") for ln in lines if ln.strip()]
    return {"rows": rows, "states": [r for r in rows if r and r[0] == "state"]}


def parse_diag(lines):
    """Rows -> boots. A new boot starts where the pid changes or a
    'module_start_entered' row appears (Mac-only rule: pids are small and
    can repeat across boots, so grouping by pid alone could merge boots)."""
    sessions = []
    cur = None
    prev_pid = object()
    rows = bad = 0
    for i, ln in enumerate(lines, 1):
        if not ln.strip():
            continue
        try:
            d = json.loads(ln)
        except ValueError:
            bad += 1
            continue
        if not isinstance(d, dict):
            bad += 1
            continue
        rows += 1
        pid = d.get("pid")
        tag = str(d.get("tag"))
        if cur is None or pid != prev_pid or tag == SESSION_START_TAG:
            cur = {"pid": pid, "rows": 0, "first_line": i, "last_line": i, "tags": collections.Counter(),
                   "first_val": {}, "t_first": d.get("t_ms"), "t_last": d.get("t_ms")}
            sessions.append(cur)
        prev_pid = pid
        cur["rows"] += 1
        cur["last_line"] = i
        cur["t_last"] = d.get("t_ms", cur["t_last"])
        cur["tags"][tag] += 1
        cur["first_val"].setdefault(tag, (i, d.get("value")))
    return {"rows": rows, "bad": bad, "sessions": sessions,
            "pids": sorted(set(str(s["pid"]) for s in sessions))}


def parse_aligned(lines):
    sessions = []
    cur = None
    other = 0
    for i, ln in enumerate(lines, 1):
        m = ALIGNED_RE.match(ln)
        if not m:
            if ln.strip():
                other += 1
            continue
        tag = m.group(4)
        if cur is None or tag == SESSION_START_TAG:
            cur = {"first_line": i, "last_line": i, "rows": 0, "builds": collections.Counter(),
                   "tags": collections.Counter(), "first_val": {}}
            sessions.append(cur)
        cur["rows"] += 1
        cur["last_line"] = i
        cur["builds"][m.group(3)] += 1
        cur["tags"][tag] += 1
        cur["first_val"].setdefault(tag, (i, m.group(5)))
        if m.group(6) and re.sub(r"[0x]", "", m.group(6)):
            cur.setdefault("errno_nonzero", collections.Counter())[tag] += 1
    return {"sessions": sessions, "other": other}


# ---------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------

class Report(object):
    def __init__(self, fixture):
        self.out = []
        self.fixture = fixture
        self.issues = []

    def a(self, s=""):
        self.out.append(s)

    def heading(self, level, title):
        tag = (" — " + FIXTURE_TAG) if self.fixture else ""
        return "%s %s%s" % ("#" * level, title, tag)

    def h(self, level, title):
        self.a()
        self.a(self.heading(level, title))
        self.a()

    def quote(self, items, max_len=400):
        """items: (lineno, text[, note]). Exact decoded lines, control bytes
        shown as \\xNN, long lines clipped with a '[+N chars]' tail."""
        lines = []
        for it in items:
            lineno, text = it[0], it[1]
            note = it[2] if len(it) > 2 else None
            s = "L%s: %s" % (lineno, clip(esc_ctrl(text), max_len))
            if note:
                s += "    <-- " + note
            lines.append(s)
        if not lines:
            return
        f = fence_for(lines)
        self.a(f + "text")
        for s in lines:
            self.a(s)
        self.a(f)

    def issue(self, s):
        self.issues.append(s)

    def text(self):
        return "\n".join(self.out) + "\n"


def physical(doc, lineno):
    return doc.lines[lineno - 1] if doc and 0 < lineno <= len(doc.lines) else ""


def analyze_and_render(docs, opts, receipt=None, loadlist=None, ledger_entries=None,
                       frozen=None, notes=None):
    """docs: {kind: Doc or None}. Returns (markdown, counts)."""
    rep = Report(opts.get("fixture", False))
    notes = notes or []
    st_tag = opts.get("st_tag", ST2_TAG_DEFAULT)
    maxq = opts.get("max_quotes", 12)
    maxerr = opts.get("max_errors", 40)
    label = opts.get("label") or "unlabelled"
    ledger_entries = ledger_entries or []
    frozen = frozen or {}

    present = [k for k in KIND_ORDER if docs.get(k) is not None]
    files_parsed = sum(1 for k in present if docs[k].lines)
    lines_parsed = sum(len(docs[k].lines) for k in present)
    ue = docs.get("ue4ss")
    ue_ok = ue is not None and len(ue.lines) > 0

    # ---- UE4SS session + messages
    sessions, banners = ue4ss_sessions(ue.lines) if ue_ok else ([], [])
    if ue_ok:
        s0, s1 = (0, len(ue.lines)) if opts.get("all_sessions") else sessions[-1]
        msgs, appended = build_messages(ue.lines, s0, s1)
    else:
        s0 = s1 = 0
        msgs, appended = [], []
    mods = mods_from_loadlist(loadlist) if loadlist else collections.OrderedDict()
    events = scan_lifecycle(msgs)
    windows, owner = startup_windows(msgs, events)
    st = parse_selftest(msgs, st_tag)
    stv = selftest_run_view(st)
    errs, other_errs = scan_lua_errors(msgs, mods, owner, st_tag)
    err_idx = set(e["msg"].idx for e in errs)
    probs = scan_mod_problems(msgs, mods, owner, st_tag, err_idx)
    diag, shim_area = scan_diag_markers(msgs)
    l23 = scan_l23(msgs)
    builds = [(m, BUILD_RE.search(m.text)) for m in msgs if BUILD_RE.search(m.text)]

    selftest_mod = None
    for b in st["boot"]:
        if owner.get(b["msg"].idx):
            selftest_mod = owner[b["msg"].idx]
            break

    log_names = collections.OrderedDict()
    for e in events:
        log_names.setdefault(e["name"], []).append(e)

    # ---- title / header
    t = "# Launch report DRAFT — %s" % label
    if rep.fixture:
        t += " — " + FIXTURE_TAG
    rep.a(t)
    rep.a()
    if rep.fixture:
        rep.a("> **%s.** These files are from an earlier session and are used only to test "
              "this tool. Nothing below is a verdict on any build (CLAUDE.md section 5)." % FIXTURE_TAG.capitalize())
        rep.a()
    rep.a("> DRAFT made by `%s` from local files only (Mac-only parse; no console contact). "
          "A person must check every verdict before sending. Quoted lines are the decoded "
          "log text (decode_session_log.py), console IP masked as `%s`." % (VERSION, IP_MASK))
    if not ue_ok:
        rep.a()
        rep.a("> **INCOMPLETE: the UE4SS log is missing or empty.** Per-mod verdicts and the "
              "self-test table cannot be filled.")
        rep.issue("UE4SS log missing or empty: no per-mod verdict or self-test result possible.")
    short_at = len(rep.out)  # the 'In short' block is inserted here at the end

    # ---- 1. build
    rep.h(2, "1. Build (sha + snapshot)")
    rep.a("- Label: Mac-only (receipt, ledger and frozen manifests are Mac files; the receipt's readback row is the "
          "deploy-time console readback; a `[BUILD]` line is a log line and carries the section 3 label).")
    sha = receipt.get("sha256") if receipt else None
    snap_sources = []
    for e in ledger_entries:
        if sha and e["sha"] == sha and e["snapshot"]:
            snap_sources.append((e["snapshot"], "ledger %s:%d (label '%s', build id %s)" % (
                rel(e["file"]), e["lineno"], e["label"], e["build_id"] or "-")))
    for sid, man in frozen.get(sha, []) if sha else []:
        snap_sources.append((sid, "frozen manifest %s" % rel(man)))
    for m, bm in builds:
        snap_sources.append((bm.group(3), "this log's [BUILD] line L%d (id=%s)" % (m.lineno, bm.group(1))))
    if receipt:
        rep.a("- Receipt: `%s` (%s; generated %s UTC; mode %s)" % (
            rel(receipt["path"]), receipt.get("title") or "?", receipt.get("generated") or "?",
            receipt.get("mode") or "?"))
        rep.a("- Variant: %s" % clip(receipt.get("variant") or "unknown", 300))
        rep.a("- PRX: `%s` (%s bytes, magic %s)" % (receipt.get("prx_path") or "?",
                                                     receipt.get("prx_size") or "?", receipt.get("magic") or "?"))
        rb = receipt.get("readback_sha256")
        if sha and rb and rb == sha:
            rep.a("- **PRX sha256: `%s`** (receipt header; the console readback row reports the same sha)" % sha)
        elif sha and rb:
            rep.a("- **PRX sha256: `%s`** but the readback row says `%s` — MISMATCH" % (sha, rb))
            rep.issue("Receipt sha256 and its readback row disagree (%s vs %s)." % (sha[:12], rb[:12]))
        elif sha:
            rep.a("- **PRX sha256: `%s`** (receipt header only; no readback row found)" % sha)
        else:
            rep.a("- PRX sha256: unknown (no sha256 line in the receipt)")
            rep.issue("Receipt has no PRX sha256 line.")
        sc = receipt.get("status_counts") or {}
        if sc:
            rep.a("- Receipt verification ledger: %d rows (%s)" % (
                sum(sc.values()), ", ".join("%s %d" % (k, v) for k, v in sorted(sc.items()))))
            if any(k != "PASS" for k in sc):
                rep.issue("Launch receipt has non-PASS rows: %s." % dict(sc))
    else:
        rep.a("- PRX sha256: unknown (no --receipt given)")
        rep.issue("No launch receipt given: build sha unknown.")
    vals = sorted(set(v for v, _ in snap_sources))
    snap_short = "snapshot unknown"
    if not snap_sources:
        rep.a("- **Snapshot: unknown** (sha not in any ledger or frozen manifest, and no [BUILD] line in this log)")
        rep.issue("Snapshot unknown (no ledger/frozen/[BUILD] source).")
    else:
        if len(vals) == 1 and len(snap_sources) >= 2:
            verdict = "agree (%d sources; the log's [BUILD] line is the only console-side one)" % len(snap_sources)
        elif len(vals) == 1:
            verdict = "single source only (needs a second source before it ships in a claim)"
            rep.issue("Snapshot %s has only one source." % vals[0])
        else:
            verdict = "DISAGREE"
            rep.issue("Snapshot sources disagree: %s." % ", ".join(vals))
        rep.a("- **Snapshot: %s** — %s" % (" / ".join(vals), verdict))
        for v, src in snap_sources:
            rep.a("  - `%s` from %s" % (v, src))
        snap_short = "snapshot `%s` (%s)" % (" / ".join(vals), verdict.split(" (")[0])
    if builds:
        rep.quote([(m.lineno, physical(ue, m.lineno)) for m, _ in builds[:3]])
        if len(builds) > 1:
            rep.issue("%d [BUILD] lines in one session (expected 1)." % len(builds))
        if any(bm.group(3) == "UNKNOWN" for _, bm in builds):
            rep.issue("[BUILD] line says snapshot=UNKNOWN (generated stamp header missing).")
    elif ue_ok:
        rep.a("- This log has no `[BUILD] id=... snapshot=...` line (candidate/src/BuildStamp.cpp:64); "
              "older builds did not print one.")

    # ---- 2. what changed
    rep.h(2, "2. What changed")
    rep.a("- Label: written by the driver (not parsed from files).")
    hint = ""
    for e in ledger_entries:
        if sha and e["sha"] == sha and e["label"]:
            hint = " (ledger label for this sha: `%s`)" % e["label"]
            break
    rep.a("- _TODO (driver): one plain line on what this build changes since the last launch%s._" % hint)

    # ---- 3. inputs
    rep.h(2, "3. Files parsed")
    rep.a("- Label: Mac-only (file sizes, counts and times measured on the Mac).")
    rep.a("| file | path | bytes | NUL bytes decoded | lines | sessions in file | mtime (Mac) |")
    rep.a("|---|---|---|---|---|---|---|")
    diag_info = parse_diag(docs["diag"].lines) if docs.get("diag") else None
    aligned_info = parse_aligned(docs["aligned"].lines) if docs.get("aligned") else None
    for k in KIND_ORDER:
        d = docs.get(k)
        if d is None:
            rep.a("| %s | %s | - | - | - | - | - |" % (k, cell(opts.get("missing", {}).get(k, "not given"))))
            continue
        if k == "ue4ss":
            sess = "%d banner session(s); analysed lines %d-%d%s" % (
                max(len(banners), 1 if d.lines else 0), s0 + 1, s1,
                " (all sessions)" if opts.get("all_sessions") else " (last session)")
        elif k == "diag" and diag_info:
            sess = "%d boot(s), %d distinct pid(s)" % (len(diag_info["sessions"]), len(diag_info["pids"]))
        elif k == "aligned" and aligned_info:
            sess = "%d boot(s) ('%s' rows)" % (len(aligned_info["sessions"]), SESSION_START_TAG)
        else:
            sess = "-"
        rep.a("| %s | `%s` | %d | %d | %d | %s | %s |" % (
            k, cell(rel(d.path), 200), d.bytes_in, d.stats.get("nul_count", 0), len(d.lines), sess,
            fmt_mtime(d.mtime)))
    rep.a()
    rep.a("- **Parsed: %d file(s), %d line(s).**" % (files_parsed, lines_parsed))
    for n in notes:
        rep.a("- %s" % n)
    if ue_ok and ue.stats.get("partial_final_line"):
        rep.a("- UE4SS log ends without a final newline (partial last line).")
    if ue_ok and len(banners) > 1:
        rep.issue("UE4SS log holds %d sessions; only the last one (lines %d-%d) is analysed." % (
            len(banners), s0 + 1, s1))
    # freshness has TWO independent parts; the per-section label is
    # console-proven only when BOTH pass:
    #   (a) Mac file times: every harvested file written after the receipt;
    #   (b) log CONTENT: the analysed session's own console-clock timestamps
    #       start after the receipt and end before the UE4SS file's Mac time.
    #       (b) catches a harvest that pulled an earlier launch's log with a
    #       fresh Mac file time (e.g. the PRX never loaded, so UE4SS never
    #       rewrote its log). harvest.py has no content check of its own.
    # True = passed; False = failed (stale); None = unknown (fail closed).
    fresh_mtime = None
    fresh_content = None
    tol = 60.0 * opts.get("clock_tol_min", CLOCK_TOL_MIN_DEFAULT)
    if receipt and receipt.get("generated_epoch"):
        dated = [d for d in (docs.get(k) for k in KIND_ORDER) if d is not None and d.mtime is not None]
        older = [d for d in dated if d.mtime < receipt["generated_epoch"]]
        if older:
            fresh_mtime = False
            rep.a("- Freshness (a) Mac file times: **%d file(s) are OLDER than the receipt** (%s UTC): %s. They "
                  "cannot be this launch's evidence." % (len(older), receipt["generated"],
                                                         ", ".join(o.kind for o in older)))
            rep.issue("Harvested files older than the launch receipt (%s): stale evidence." %
                      ", ".join(o.kind for o in older))
        elif dated:
            fresh_mtime = True
            rep.a("- Freshness (a) Mac file times: every file was written on the Mac after the receipt (%s UTC)."
                  % receipt["generated"])
        else:
            rep.a("- Freshness (a) Mac file times: unknown (no file times).")
    else:
        rep.a("- Freshness (a) Mac file times: unknown (no receipt time to compare with).")
    first_ts = next((m.ts for m in msgs if m.ts), None)
    last_ts = next((m.ts for m in reversed(msgs) if m.ts), None)
    off = opts.get("console_utc_offset_min")
    clock_how = ("console clock read as UTC%+d min (--console-utc-offset)" % off) if off is not None else \
        "console clock read as this Mac's local time, which matched the receipt and Mac file times on 4 earlier " \
        "sessions (launch_report.py CONSOLE_CLOCK_NOTE)"
    if first_ts and last_ts:
        rep.a("- Analysed session's console-clock span: %s .. %s (%s)." % (first_ts, last_ts, clock_how))
    if not (first_ts and last_ts):
        rep.a("- Freshness (b) log content: unknown (no timestamped lines in the analysed session).")
    elif not (receipt and receipt.get("generated_epoch")):
        rep.a("- Freshness (b) log content: unknown (no receipt time to compare the log's own timestamps with).")
    else:
        fe, le = console_epoch(first_ts, off), console_epoch(last_ts, off)
        r_ep = receipt["generated_epoch"]
        ue_mt = ue.mtime if ue_ok else None
        if fe is None or le is None:
            rep.a("- Freshness (b) log content: unknown (timestamps do not parse).")
        elif fe < r_ep - tol:
            fresh_content = False
            rep.a("- Freshness (b) log content: **FAILED. The session's first line (%s = %s UTC) is %.1f h BEFORE "
                  "the receipt (%s UTC): this log is from an EARLIER launch**, whatever its Mac file time says." % (
                      first_ts, utc_str(fe), (r_ep - fe) / 3600.0, receipt["generated"]))
            rep.issue("The UE4SS log's own timestamps start before the launch receipt: earlier launch's log "
                      "(stale evidence; NOT console-proven).")
        elif ue_mt is not None and le > ue_mt + tol:
            rep.a("- Freshness (b) log content: unknown. The log's last line (%s = %s UTC) is LATER than the "
                  "file's Mac time (%s), so the console clock reading does not line up (pass "
                  "--console-utc-offset)." % (last_ts, utc_str(le), utc_str(ue_mt)))
            rep.issue("Console clock does not line up with the Mac file time: content freshness unknown.")
        elif fe > r_ep + 12 * 3600:
            rep.a("- Freshness (b) log content: unknown. The session starts %.1f h after the receipt: check that "
                  "this is the right receipt." % ((fe - r_ep) / 3600.0))
            rep.issue("UE4SS session starts more than 12 h after the receipt: receipt pairing unconfirmed.")
        elif ue_mt is None:
            rep.a("- Freshness (b) log content: unknown (no Mac file time for the UE4SS log).")
        else:
            fresh_content = True
            rep.a("- Freshness (b) log content: OK. The session starts %+.0f s after the receipt and its last line "
                  "is %.0f s before the file's Mac time (tolerance %d min)." % (
                      fe - r_ep, ue_mt - le, int(tol // 60)))
    # [BUILD] id written by the PRX vs the ledger's build id for the receipt sha
    led_ids = sorted(set(e["build_id"] for e in ledger_entries
                         if sha and e["sha"] == sha and e["build_id"] and e["build_id"] not in ("-", "—")))
    if builds and led_ids:
        log_ids = sorted(set(bm.group(1) for _, bm in builds))
        if set(log_ids) & set(led_ids):
            rep.a("- Build id check: the log's [BUILD] id %s matches the ledger's build id for the receipt sha." %
                  ", ".join(log_ids))
        else:
            fresh_content = False
            rep.a("- Build id check: **FAILED. The log's [BUILD] id %s is not the ledger's build id %s for the "
                  "receipt sha**: this log was written by a different build." % (
                      ", ".join(log_ids), ", ".join(led_ids)))
            rep.issue("Log [BUILD] id differs from the ledger build id of the receipt sha (different build's log).")
    if last_ts:
        dt = parse_ts(last_ts)
        if dt:
            age = (datetime.datetime.now() - dt).total_seconds() / 86400.0
            if age > 2 and not rep.fixture:
                fresh_content = False
                rep.a("- **WARNING: the log's last line is %.1f days old.** If this is not a fixture "
                      "test, it is an earlier session's evidence and must not be reused." % age)
                rep.issue("Log is %.1f days old (earlier session?)." % age)
    if fresh_mtime is False or fresh_content is False:
        fresh = False
    elif fresh_mtime is True and fresh_content is True:
        fresh = True
    else:
        fresh = None
    if fresh is True:
        lab_real = "console-proven (this launch's log lines; freshness (a) and (b) OK)"
    elif fresh is False:
        lab_real = "NOT console-proven (freshness check failed)"
    else:
        lab_real = "NOT console-proven (freshness unknown)"
    fresh_word = {True: "PASS", False: "FAILED", None: "UNKNOWN"}[fresh]
    if rep.fixture:
        # the freshness logic still runs (so fixture tests exercise it), but
        # nothing from an earlier session is ever labelled console-proven
        lab = FIXTURE_TAG
        rep.a("- Freshness logic result: **%s** (shown only to test the tool; the 2-day age check is skipped in "
              "fixture mode, and fixture mode never labels anything console-proven)." % fresh_word)
    else:
        lab = lab_real
    rep.a("- **Label for the log-based sections below: %s.**" % lab)

    # ---- 4. per-mod verdict
    rep.h(2, "4. Per-mod verdict (draft)")
    rep.a("- Label: %s." % lab)
    loadlist_zero = False
    if not loadlist:
        rep.a("- **Mod list unknown: no loadlist given** (no --loadlist, and no rowcheck receipt cited by "
              "a launch receipt is on this Mac). Mod names come only from loadlist rows; the names below "
              "were seen in the log's mod start lines and are NOT confirmed by a loadlist.")
        rep.issue("No loadlist: mod list not confirmed.")
        modnames = list(log_names.keys())
        for n in modnames:
            mods[n] = {"rows": [], "main": None, "enabled": None, "bad_rows": [],
                       "class": "seen in log only (no loadlist)", "is_lib": False}
    else:
        rep.a("- Mod names from: `%s` (%s; %d file rows; parser: %s)." % (
            rel(loadlist["path"]), loadlist["format"], len(loadlist["rows"]), loadlist["parser"]))
        for hd in loadlist.get("headers", [])[:6]:
            rep.a("  - loadlist header row: `%s`" % cell(hd.replace("\t", " "), 120))
        if loadlist.get("malformed"):
            rep.issue("Loadlist has %d malformed row(s)." % len(loadlist["malformed"]))
        if not loadlist["rows"] or not any(not d_["is_lib"] for d_ in mods.values()):
            rep.a("- **FAIL: the loadlist has %d file row(s) and %d mod folder(s) with a main.lua or enabled.txt "
                  "row: nothing to check the mods against** (a check over zero items is a failed check)." % (
                      len(loadlist["rows"]), sum(1 for d_ in mods.values() if not d_["is_lib"])))
            rep.issue("Loadlist has zero mod rows: per-mod check FAILED (zero items checked).")
            loadlist_zero = True
    if selftest_mod:
        rep.a("- Self-test lines (`%s`) come from mod **%s** (its start-up window holds the `boot` line)." % (
            st_tag.strip(), selftest_mod))
    elif st["lines"]:
        rep.a("- Self-test lines (`%s`) found, but their mod could not be identified (no `boot` line "
              "inside a mod start-up window): unknown." % st_tag.strip())
    rep.a()
    rep.a("| mod | loadlist | start lines | Lua error lines | mod-printed problem lines | draft verdict | label |")
    rep.a("|---|---|---|---|---|---|---|")
    per_mod = collections.OrderedDict()
    for name, d in mods.items():
        ev = log_names.get(name, [])
        kinds = collections.Counter(e["kind"] for e in ev)
        merrs = [e for e in errs if e["mod"] == name]
        mprobs = [p for p in probs if p["mod"] == name]
        fail_err = [e for e in merrs if e["kind"] in ("error-loading-script", "execute-script-error",
                                                      "main-script-failed", "main-lua-missing", "mod-exception")]
        n_prob = sum(1 for e in merrs if "(probable)" in e["how"])
        err_txt = "%d Lua/runtime error line(s) name it" % len(merrs)
        if n_prob:
            err_txt += " (%d of them only probable, by position in its start-up window)" % n_prob
        if d["is_lib"]:
            v = "library folder (not a mod; nothing to start)"
        elif kinds.get("cannot-start") or fail_err:
            v = "FAILED TO START"
        elif kinds.get("started"):
            if merrs:
                v = "STARTED, " + err_txt
            elif mprobs:
                v = "STARTED, no Lua errors; it printed %d problem line(s)" % len(mprobs)
            else:
                v = "STARTED, no error lines name it"
            v += "; working in game: needs the player's check"
        elif kinds.get("announce"):
            v = "ANNOUNCED but no 'Lua mod started' line (start unconfirmed)"
        elif kinds.get("disabled"):
            v = "DISABLED in mods.txt (the log says so): not started"
        elif d.get("main") and not d.get("enabled"):
            v = "not enabled in the loadlist (no enabled.txt row): not expected to start"
        elif d.get("main") is None:
            v = "unknown (no loadlist)"
        else:
            v = "NOT SEEN in this log"
        if name == selftest_mod and stv and stv["summary"]:
            v += "; self-test SUMMARY %d of %d PASS" % (stv["summary"]["p"], stv["summary"]["M"])
        if rep.fixture:
            v = FIXTURE_TAG + ": " + v
        per_mod[name] = {"verdict": v, "ev": ev, "errs": merrs, "probs": mprobs}
        llc = d["class"]
        if d.get("bad_rows"):
            llc += "; %d row(s) not PASS in rowcheck" % len(d["bad_rows"])
            rep.issue("Mod %s: %d loadlist row(s) not PASS in the rowcheck receipt." % (name, len(d["bad_rows"])))
        rep.a("| %s | %s | %s | %d | %d | %s | %s |" % (
            cell(name), cell(llc), cell(", ".join("%s %d" % (k, c) for k, c in kinds.items()) or "none"),
            len(merrs), len(mprobs), cell(v, 220), cell(lab, 220)))
        if not d["is_lib"]:
            if "FAILED" in v:
                rep.issue("Mod %s failed to start." % name)
            elif "NOT SEEN" in v and d.get("enabled"):
                rep.issue("Mod %s is enabled in the loadlist but never appears in the log." % name)
            elif "DISABLED in mods.txt" in v and d.get("enabled"):
                rep.issue("Mod %s has an enabled.txt row in the loadlist, but mods.txt disabled it (log line L%d)." % (
                    name, next(e["msg"].lineno for e in ev if e["kind"] == "disabled")))
            elif "ANNOUNCED" in v:
                rep.issue("Mod %s was announced but its 'Lua mod started' line is missing." % name)
            if merrs:
                rep.issue("Mod %s: %s." % (name, err_txt))
    extra = [n for n in log_names if n not in mods]
    if extra:
        rep.a()
        rep.a("- Mods in the log but NOT in the loadlist: %s" % ", ".join("`%s`" % x for x in extra))
        rep.issue("Mods in the log but not in the loadlist: %s." % ", ".join(extra))
    for name, pm in per_mod.items():
        d = mods[name]
        if d["is_lib"]:
            continue
        rep.h(4, "Mod `%s`" % name)
        rep.a("- %s" % pm["verdict"])
        items = []
        for e in pm["ev"]:
            items.append((e["msg"].lineno, physical(ue, e["msg"].lineno), "%s (%s)" % (e["kind"], e["source"])))
        for e in pm["errs"]:
            items.append((e["msg"].lineno, physical(ue, e["msg"].lineno), "Lua error: %s; %s" % (e["kind"], e["how"])))
        for p in pm["probs"]:
            items.append((p["msg"].lineno, physical(ue, p["msg"].lineno), "mod-printed problem (heuristic); %s" % p["how"]))
        tagged = []
        for m in msgs:
            if m.cont:
                continue
            if ("[%s]" % name) in m.text or ("Mods/%s/" % name) in m.text:
                tagged.append(m)
        seen_lines = set(it[0] for it in items)
        tag_items = [(m.lineno, physical(ue, m.lineno), "names the mod") for m in tagged if m.lineno not in seen_lines]
        win_items = []
        for wname, ws, we in windows:
            if wname != name:
                continue
            for k in range(ws, we):
                m = msgs[k]
                if (m.text.startswith(LUA_PRINT_PREFIX) and not any(rx.search(m.text) for rx in RUNTIME_LUA_MSGS)
                        and m.lineno not in seen_lines):
                    win_items.append((m.lineno, physical(ue, m.lineno), "start-up window print (probable)"))
        all_items = sorted(items, key=lambda x: x[0])
        rest = tag_items + win_items
        dedup = collections.OrderedDict()
        for it in all_items + sorted(rest, key=lambda x: x[0]):
            dedup.setdefault(it[0], it)
        allq = list(dedup.values())
        shown = allq[:maxq]
        if not allq:
            rep.a("- No log line names this mod.")
        else:
            rep.a("- Evidence (%d line(s) name it; %d shown):" % (len(allq), len(shown)))
            rep.quote(sorted(shown, key=lambda x: x[0]))
            if len(allq) > len(shown):
                rep.a("- +%d more line(s): L%s" % (len(allq) - len(shown),
                                                   ", L".join(str(x[0]) for x in allq[len(shown):len(shown) + 40])))

    # ---- 5. self-test
    rep.h(2, "5. Self-test (%s lines; format: the self-test mod's Scripts/main.lua)" % st_tag.strip())
    rep.a("- Label: %s (log lines); counts and checks are Mac-only parsing." % lab)
    if not st["lines"]:
        rep.a("- No `%s` lines in this session: the self-test did not run or did not print." % st_tag.strip())
        if st["echo"]:
            rep.a("- (%d echo line(s) carry the tag inside another message, e.g. `[LFB-M] print-widen in=`; "
                  "they are not self-test lines.)" % len(st["echo"]))
        if ue_ok:
            rep.issue("Self-test did not run in this session (no %s lines)." % st_tag.strip())
    else:
        rep.a("- %s lines: %d (result rows %d, runs started %d, SUMMARY lines %d)." % (
            st_tag.strip(), st["lines"], len(st["rows"]), len(st["runs"]), len(st["summary"])))
        if len(st["runs"]) != 1:
            rep.issue("Self-test RUN start count is %d (expected exactly 1 per launch)." % len(st["runs"]))
        for b in st["boot"]:
            rep.quote([(b["msg"].lineno, physical(ue, b["msg"].lineno))])
        auto = [x for x in st["simple"] if x["what"] == "AUTO start"]
        if st["boot"] and auto and st["boot"][0]["msg"].ts and auto[0]["msg"].ts:
            a, b = parse_ts(st["boot"][0]["msg"].ts), parse_ts(auto[0]["msg"].ts)
            if a and b:
                rep.a("- AUTO start came %d s after the boot line (main.lua:118 arms it at +180 s)." % int((b - a).total_seconds()))
        if stv:
            rep.a("- Run %d of %d (the last run with result rows):" % (stv["run"], stv["runs_total"]))
            if stv["summary"]:
                rep.quote([(stv["summary"]["msg"].lineno, physical(ue, stv["summary"]["msg"].lineno))])
            else:
                rep.a("  - **no SUMMARY line for this run** (run did not finish, or the line was lost)")
                rep.issue("Self-test run %d has no SUMMARY line." % stv["run"])
            if stv["gaps"]:
                rep.quote([(stv["gaps"]["msg"].lineno, physical(ue, stv["gaps"]["msg"].lineno))])
                if stv["gaps"]["n"]:
                    rep.issue("Self-test GAPS %d: refused registrations counted as PASS (%s)." % (
                        stv["gaps"]["n"], ", ".join(stv["gaps"]["names"])[:300]))
            else:
                rep.a("  - no GAPS line for this run")
            c = stv["cnt"]
            rep.a()
            rep.a("| PASS | FAIL | SKIP | SLOW (>%d ms) | rows parsed | expected (M) | missing | duplicates | wall time |" % ST_SLOW_THRESHOLD_MS)
            rep.a("|---|---|---|---|---|---|---|---|---|")
            rep.a("| %d | %d | %d | %d | %d | %s | %s | %s | %s |" % (
                c.get("PASS", 0), c.get("FAIL", 0), c.get("SKIP", 0), len(stv["slow"]), len(stv["rows"]),
                stv["M"] if stv["M"] is not None else "unknown",
                cell(", ".join(map(str, stv["missing"][:30])) + (" ..." if len(stv["missing"]) > 30 else "") or "none"),
                cell(", ".join(map(str, stv["dups"])) or "none"),
                ("%d s (RUN start to SUMMARY, 1 s log resolution)" % stv["wall"]) if stv["wall"] is not None else "unknown"))
            rep.a()
            if stv["summary"] and stv["summary"]["p"] != c.get("PASS", 0):
                rep.a("- Note: SUMMARY says %d PASS but %d PASS rows were parsed (lost or damaged lines?)." % (
                    stv["summary"]["p"], c.get("PASS", 0)))
                rep.issue("Self-test SUMMARY PASS count (%d) differs from parsed PASS rows (%d)." % (
                    stv["summary"]["p"], c.get("PASS", 0)))
            if stv["summary"]:
                cum_slow = sum(1 for r in st["rows"] if r["slow"] is not None and r["run"] <= stv["run"])
                if stv["summary"]["slow"] != len(stv["slow"]):
                    rep.a("- Note: SUMMARY says SLOW %d but this run printed %d SLOW row(s) (%d across runs 1-%d). "
                          "The SLOW counter is never reset between runs (main.lua:30) and also counts SKIP entries, "
                          "which print no time (main.lua:39,42); the table counts this run's printed SLOW rows." % (
                              stv["summary"]["slow"], len(stv["slow"]), cum_slow, stv["run"]))
                    if stv["summary"]["slow"] < len(stv["slow"]):
                        rep.issue("Self-test SUMMARY SLOW %d is LOWER than the %d SLOW rows parsed (lost lines?)." % (
                            stv["summary"]["slow"], len(stv["slow"])))
            if stv["missing"]:
                rep.issue("Self-test rows missing from the log: %d (numbers %s)." % (
                    len(stv["missing"]), ", ".join(map(str, stv["missing"][:20]))))
            if c.get("FAIL"):
                rep.issue("Self-test FAIL rows: %d (%s)." % (c["FAIL"], ", ".join(
                    r["name"] for r in stv["rows"] if r["res"] == "FAIL")[:300]))
            if stv["slow"]:
                rep.issue("Self-test SLOW rows: %d (slowest %s = %d ms)." % (
                    len(stv["slow"]), max(stv["slow"], key=lambda r: r["slow"])["name"],
                    max(r["slow"] for r in stv["slow"])))
            if stv["mem"]:
                rep.a("- Lua memory around the run (GetLuaMemory, candidate/src/LuaMod.cpp:2003):")
                rep.quote([(x["msg"].lineno, physical(ue, x["msg"].lineno)) for x in stv["mem"]])
                bef = next((x for x in stv["mem"] if x["when"] == "before"), None)
                aft = next((x for x in stv["mem"] if x["when"] == "after"), None)
                if bef and aft and bef["kv"] and aft["kv"]:
                    keys = [k for k in bef["kv"] if k in aft["kv"]]
                    rep.a("- after minus before: " + ", ".join("%s %+d" % (k, aft["kv"][k] - bef["kv"][k]) for k in keys))
                    if aft["kv"].get("fails", 0) > bef["kv"].get("fails", 0):
                        rep.issue("Lua allocator failures rose during the self-test (fails %d -> %d)." % (
                            bef["kv"]["fails"], aft["kv"]["fails"]))
            rows_sorted = sorted(stv["rows"], key=lambda r: (r["n"], r["msg"].lineno))
            fails = [r for r in rows_sorted if r["res"] == "FAIL"]
            slows = sorted(stv["slow"], key=lambda r: -r["slow"])
            if fails:
                rep.a("- FAIL rows (exact lines):")
                rep.quote([(r["msg"].lineno, physical(ue, r["msg"].lineno)) for r in fails[:60]])
            if slows:
                rep.a("- SLOW rows, slowest first (exact lines):")
                rep.quote([(r["msg"].lineno, physical(ue, r["msg"].lineno)) for r in slows[:40]])
            rep.a()
            rep.a("Full table (run %d). Entries of %d ms or less print no time (main.lua:39)." % (stv["run"], ST_SLOW_THRESHOLD_MS))
            rep.a()
            rep.a("| # | entry | result | SLOW ms | detail | log line |")
            rep.a("|---|---|---|---|---|---|")
            for r in rows_sorted:
                rep.a("| %d | %s | %s | %s | %s | L%d |" % (
                    r["n"], cell(r["name"], 60), r["res"], r["slow"] if r["slow"] is not None else "-",
                    cell(r["detail"] or "-", 80), r["msg"].lineno))
        for key, what in (("parts", "part loading"), ("warn", "WARN count"), ("crash", "CRASH (resumed)")):
            bad = [x for x in st[key] if key != "parts" or not x["state"].startswith("loaded ")]
            if bad:
                rep.a("- %s lines:" % what)
                rep.quote([(x["msg"].lineno, physical(ue, x["msg"].lineno)) for x in bad[:20]])
                rep.issue("Self-test %s problem lines: %d." % (what, len(bad)))
        again = [x for x in st["simple"] if x["what"] in ("already-done-once-this-session", "busy")]
        if again:
            rep.a("- Re-trigger lines (ignored by design, main.lua:68/74): %d" % len(again))
        if st["unrecognized"]:
            rep.a("- %d `%s` line(s) did not match any known format (damaged by log interleave, or a new format):" % (
                len(st["unrecognized"]), st_tag.strip()))
            rep.quote([(x["msg"].lineno, physical(ue, x["msg"].lineno)) for x in st["unrecognized"][:15]])
            rep.issue("%d unrecognized self-test line(s)." % len(st["unrecognized"]))
        if st["echo"]:
            rep.a("- %d echo line(s) carry the `%s` tag inside another message (e.g. the `[LFB-M] print-widen in=` "
                  "first-10 print marker, typescan-r4/include/LuaFNameBridge.hpp:693); not counted as self-test lines:" % (
                      len(st["echo"]), st_tag.strip()))
            rep.quote([(x["msg"].lineno, physical(ue, x["msg"].lineno)) for x in st["echo"][:5]], max_len=200)
        if st["skipnotes"]:
            rep.a("- Skip-list notes printed by the skip-class-list entry: %d (part12.lua:5)." % len(st["skipnotes"]))

    # ---- 6. B2-B4 diagnostic lines
    rep.h(2, "6. B2-B4 diagnostic lines")
    rep.a("- Label: %s (log lines); trends are Mac-only parsing." % lab)
    if ABM_SETS:
        abm = {}
        for bld, lst in ABM_SETS.items():
            for mid, text, _enc, _src in lst:
                abm[mid] = (bld, text)
        mine = dict((d[0], (d[1], d[2])) for d in DIAG_MARKERS)
        diff = [k for k in set(abm) | set(mine) if abm.get(k) != mine.get(k)]
        if diff:
            rep.a("- WARNING: marker list differs from harvest/add_build_markers.py for: %s" % ", ".join(sorted(diff)))
            rep.issue("B2-B4 marker list differs from add_build_markers.py (%s)." % ", ".join(sorted(diff)))
        else:
            rep.a("- Marker texts match harvest/add_build_markers.py SETS (%d markers, two sources)." % len(mine))
    else:
        rep.a("- harvest/add_build_markers.py not importable: marker texts from the runtime source only.")
    rep.a()
    rep.a("| marker | build | lines | source |")
    rep.a("|---|---|---|---|")
    for mid, bld, text, src in DIAG_MARKERS:
        rep.a("| `%s` | %s | %d | %s |" % (cell(text.strip()), bld, len(diag[mid]), src))
    rep.a("| `%s` (not dir) | B4 context | %d | candidate/src/LuaPathShim.cpp:1699 |" % (SHIM_AREA_TEXT.strip(), len(shim_area)))
    rep.a()
    memms = diag["b2-mem-line"]
    if memms:
        per_state, vals = summarize_mem(memms)
        gc = [d.get("gc_kb") for _, _, d in vals if "gc_kb" in d]
        live = [d.get("live_kb") for _, _, d in vals if "live_kb" in d]
        rep.a("- `[MEM] state=`: %d line(s), %d Lua state(s); gc_kb min %s / max %s; live_kb max %s." % (
            len(memms), len(per_state), min(gc) if gc else "?", max(gc) if gc else "?", max(live) if live else "?"))
        rep.a()
        rep.a("| state | lines | first fails/errs/cxx | last fails/errs/cxx | first gc_kb | last gc_kb |")
        rep.a("|---|---|---|---|---|---|")
        for state, ps in per_state.items():
            f, l = ps["first"][1], ps["last"][1]
            rep.a("| %s | %d | %s/%s/%s | %s/%s/%s | %s | %s |" % (
                cell(state), ps["n"], f.get("fails", "?"), f.get("errs", "?"), f.get("cxx", "?"),
                l.get("fails", "?"), l.get("errs", "?"), l.get("cxx", "?"), f.get("gc_kb", "?"), l.get("gc_kb", "?")))
            if l.get("fails", 0) > f.get("fails", 0):
                rep.issue("[MEM] allocator fails rose on state %s (%s -> %s)." % (state, f.get("fails"), l.get("fails")))
        rep.a()
        sample = memms[:2] + (memms[-2:] if len(memms) > 4 else memms[2:])
        rep.quote([(m.lineno, physical(ue, m.lineno)) for m in sample])
    for mid, what in (("b2-lua-mem-note", "`[lua-mem:` notes (a LUA_ERRMEM 'not enough memory' error)"),
                      ("b3-foreachuobject", "`[FOU] ForEachUObject:` lines"),
                      ("b3-uenum-foreachname", "`[UEN] ForEachName:` lines (first 3 calls only, LuaUEnum.cpp:137)"),
                      ("b4-shim-write-area-dir", "`[SHIM] write-area dir` lines")):
        ms = diag[mid]
        if not ms:
            continue
        rep.a("- %s: %d" % (what, len(ms)))
        rep.quote([(m.lineno, physical(ue, m.lineno)) for m in ms[:15]])
        if mid == "b2-lua-mem-note":
            rep.issue("%d LUA_ERRMEM note(s) ([lua-mem: ...])." % len(ms))
        if mid == "b3-foreachuobject":
            kinds = collections.Counter()
            for m in ms:
                for k, rx, _s in FOU_KINDS:
                    if rx.search(m.text):
                        kinds[k] += 1
            rep.a("  - kinds: %s" % (", ".join("%s %d" % kv_ for kv_ in kinds.items()) or "unparsed"))
            if kinds.get("stack-exhausted"):
                rep.issue("ForEachUObject stopped early (Lua stack exhausted) %d time(s)." % kinds["stack-exhausted"])
        if mid == "b3-uenum-foreachname":
            mism = []
            for m in ms:
                um = UEN_RE.search(m.text)
                if um and um.group(1) != um.group(2):
                    mism.append(m)
            if mism:
                rep.issue("UEnum.ForEachName count mismatch on %d line(s)." % len(mism))
        if mid == "b4-shim-write-area-dir":
            bad = [m for m in ms if SHIM_DIR_RE.search(m.text) and SHIM_DIR_RE.search(m.text).group(3) != "true"]
            if bad:
                rep.issue("%d shim write-area dir(s) not usable (usable=false)." % len(bad))
    if shim_area:
        rep.a("- `[SHIM] write-area <mod> ->` lines: %d" % len(shim_area))
        rep.quote([(m.lineno, physical(ue, m.lineno)) for m in shim_area[:12]])
    if msgs:
        # old shim crash shape: the log ENDS at a '[SHIM] write-area' line
        # (candidate/src/LuaPathShim.cpp:32 comment; MORNING-REPORT-20261012
        # problem 3). Flag only when such a line is the last or second-to-last
        # message and no clean-exit line exists.
        last = msgs[-1]
        clean = any(e["kind"] == "stopping" for e in events)
        near = [m for m in shim_area + diag["b4-shim-write-area-dir"] if m.idx >= last.idx - 1]
        if near and not clean:
            rep.a("- **The session's log ends at a `[SHIM] write-area` line** (the old shim-crash "
                  "shape, MORNING-REPORT-20261012 problem 3).")
            rep.issue("Log ends at a [SHIM] write-area line (shim crash shape).")
    l23_counts = dict((k, len(v)) for k, v in l23.items())
    if any(l23_counts.values()):
        rep.a("- `[l23]` per-mod game-thread probe: %s" % ", ".join("%s %d" % (k, v) for k, v in l23_counts.items() if v))
        drains = [int(mm.group(1)) for _m, mm in l23["ran"]]
        if drains:
            rep.a("  - drain times (ms): %s" % ", ".join(map(str, drains[:20])))
        badl23 = l23["refused"] + l23["enqueue-failed"] + l23["not-drained"]
        if badl23:
            rep.quote([(m.lineno, physical(ue, m.lineno)) for m, _ in badl23[:10]])
            rep.issue("%d [l23] game-thread probe problem line(s) (refused/failed/not drained)." % len(badl23))

    # ---- 7. Lua errors
    rep.h(2, "7. Lua errors (with the mod they came from)")
    rep.a("- Label: %s (log lines); the mod attribution is Mac-only parsing ('probable' = by position only)." % lab)
    by_mod = collections.Counter((e["mod"] or "unknown") for e in errs)
    rep.a("- Lua/runtime error lines: %d (%s). Self-test FAIL rows are in section 5, not here." % (
        len(errs), ", ".join("%s %d" % kv_ for kv_ in by_mod.most_common()) or "none"))
    rep.a("- Shapes checked: %d (%s), plus a catch-all for any other runtime line with an error-like word." % (
        len(LUA_ERRORS), ", ".join(k for k, _r, _s in LUA_ERRORS)))
    if errs:
        rep.a()
        rep.a("| # | mod | how attributed | kind | log line |")
        rep.a("|---|---|---|---|---|")
        for i, e in enumerate(errs[:maxerr], 1):
            rep.a("| %d | %s | %s | %s | L%d |" % (i, cell(e["mod"] or "unknown"), cell(e["how"], 90), e["kind"], e["msg"].lineno))
        rep.a()
        items = []
        for e in errs[:maxerr]:
            items.append((e["msg"].lineno, physical(ue, e["msg"].lineno), e["kind"]))
            for c in e["ctx"][:6]:
                items.append((c.lineno, physical(ue, c.lineno), "context"))
        rep.quote(items)
        if len(errs) > maxerr:
            rep.a("- +%d more Lua error line(s) not shown." % (len(errs) - maxerr))
        unk = by_mod.get("unknown", 0)
        if unk:
            rep.issue("%d Lua error line(s) with no identifiable mod." % unk)
    rep.a("- Unclassified error-like runtime lines (catch-all, no known shape; read each one): %d" % len(other_errs))
    if other_errs:
        rep.quote([(e["msg"].lineno, physical(ue, e["msg"].lineno), "unclassified; mod: %s (%s)" % (
            e["mod"] or "unknown", e["how"])) for e in other_errs[:20]], max_len=300)
        if len(other_errs) > 20:
            rep.a("- +%d more unclassified line(s): L%s" % (len(other_errs) - 20, ", L".join(
                str(e["msg"].lineno) for e in other_errs[20:60])))
    if probs:
        rep.a("- Mod-printed problem lines (heuristic words: error/failed/exception/not found/invalid/nil value/cannot): %d" % len(probs))
        pm = collections.Counter((p["mod"] or "unknown") for p in probs)
        rep.a("  - by mod: %s" % ", ".join("%s %d" % kv_ for kv_ in pm.most_common()))
        rep.quote([(p["msg"].lineno, physical(ue, p["msg"].lineno), p["mod"] or "unknown") for p in probs[:20]])

    # ---- 8. crash markers
    rep.h(2, "8. Crash / panic / abort / longjmp markers")
    rep.a("- Label: UE4SS lines %s; klog/diag/status lines carry the same label only when those files come from "
          "this launch (not checked by content); marker matching is Mac-only." % lab)
    crash_total = crash_benign = 0
    scan_sets = []
    if ue_ok:
        scan_sets.append(("ue4ss", "UE4SS log (analysed session)", ue,
                          [(i + 1, ue.lines[i]) for i in range(s0, s1)]))
    if docs.get("klog"):
        k = docs["klog"]
        scan_sets.append(("klog", "klog capture", k, [(i, ln) for i, ln in enumerate(k.lines, 1)]))
    if docs.get("runtime-status"):
        r_ = docs["runtime-status"]
        scan_sets.append(("runtime-status", "runtime-status", r_, [(i, ln) for i, ln in enumerate(r_.lines, 1)]))
    if docs.get("diag") and diag_info and diag_info["sessions"]:
        lastpid = diag_info["sessions"][-1]
        dd = docs["diag"]
        scan_sets.append(("diag", "supplier-diag (newest boot only)", dd,
                          [(i, dd.lines[i - 1]) for i in range(lastpid["first_line"], lastpid["last_line"] + 1)]))
    if docs.get("aligned") and aligned_info and aligned_info["sessions"]:
        lastb = aligned_info["sessions"][-1]
        ad = docs["aligned"]
        scan_sets.append(("aligned", "aligned-entry (newest boot only)", ad,
                          [(i, ad.lines[i - 1]) for i in range(lastb["first_line"], lastb["last_line"] + 1)]))
    rep.a("| file | lines scanned | marker | hits | known-benign | source |")
    rep.a("|---|---|---|---|---|---|")
    quoted = []
    cmarkers, cbenign = markers_for_tag(st_tag)
    for kind, title, d, numbered in scan_sets:
        hits = scan_crash(kind, numbered, cmarkers, cbenign)
        any_hit = False
        for cid, rx, kinds, src in cmarkers:
            hs = hits[cid]
            if not hs:
                continue
            any_hit = True
            ben = sum(1 for h in hs if h[2])
            crash_total += len(hs) - ben
            crash_benign += ben
            rep.a("| %s | %d | %s | %d | %d | %s |" % (title, len(numbered), cid, len(hs), ben, cell(src, 90)))
            quoted.append((title, cid, hs[:8], len(hs)))
        if not any_hit:
            rep.a("| %s | %d | (none) | 0 | 0 | - |" % (title, len(numbered)))
    rep.a()
    for title, cid, hs, n in quoted:
        rep.a("- %s / `%s` (%d hit(s); first %d):" % (title, cid, n, len(hs)))
        rep.quote([(h[0], h[1], h[2]) for h in hs], max_len=300)
    if appended:
        rep.a("- Lines appended by a LATER boot (modbridge `[t=...ms pid=...]` format, harvest.py:214), not this session:")
        rep.quote(appended[:10])
        rep.issue("%d line(s) appended to the UE4SS log by a later boot." % len(appended))
    if crash_total:
        rep.issue("%d crash/panic/abort/longjmp marker hit(s) (not counting %d known-benign)." % (crash_total, crash_benign))
    if ue_ok:
        stops = [e for e in events if e["kind"] == "stopping"]
        lastm = msgs[-1] if msgs else None
        if stops:
            rep.a("- Clean-exit lines (`Stopping mod ... for uninstall`, LuaMod.cpp:8414): %d." % len(stops))
        else:
            rep.a("- **No clean-exit lines** (`Stopping mod ... for uninstall`): the game may have crashed, been "
                  "killed, or the log was pulled while it still ran (unknown which).")
            rep.issue("No clean-exit lines in the UE4SS log (crash, kill, or pulled while running: unknown).")
        if lastm:
            rep.a("- Last line of the analysed session:")
            rep.quote([(lastm.lineno, physical(ue, lastm.lineno))])

    # ---- 9. other files
    rep.h(2, "9. Other files")
    rep.a("- Label: file lines %s; the boot split and pid comparison are Mac-only inferences." % lab)
    if docs.get("runtime-status"):
        rs = parse_runtime_status(docs["runtime-status"].lines)
        rep.a("- runtime-status rows (tab-separated, raw):")
        rep.quote([(i, ln) for i, ln in enumerate(docs["runtime-status"].lines, 1) if ln.strip()][:20])
        srow = [r for r in rs["rows"] if r and r[0] == "session" and len(r) > 1]
        if srow and diag_info and diag_info["sessions"]:
            newest = str(diag_info["sessions"][-1]["pid"])
            if srow[-1][1] == newest:
                rep.a("  - its `session` number %s equals the newest supplier-diag boot's pid %s (both files look "
                      "like the same boot; Mac-only inference, field meaning not documented here)." % (srow[-1][1], newest))
            else:
                rep.a("  - its `session` number %s differs from the newest supplier-diag boot's pid %s: the two "
                      "files may come from different boots (unknown)." % (srow[-1][1], newest))
                rep.issue("runtime-status session %s differs from the newest supplier-diag pid %s." % (srow[-1][1], newest))
    if diag_info is not None:
        rep.a("- supplier-diag: %d JSON row(s), %d unparsable, %d boot(s) (split where the pid changes or a "
              "`%s` row appears; file order, so the last one is the newest):" % (
                  diag_info["rows"], diag_info["bad"], len(diag_info["sessions"]), SESSION_START_TAG))
        for p in diag_info["sessions"][-4:]:
            rep.a("  - pid %s: %d rows (lines %d-%d), t_ms %s..%s" % (p["pid"], p["rows"], p["first_line"],
                                                                     p["last_line"], p["t_first"], p["t_last"]))
        if diag_info["sessions"]:
            p = diag_info["sessions"][-1]
            fl = [(t_, c) for t_, c in p["tags"].most_common() if FAILISH_TAG_RE.search(t_)]
            if fl:
                rep.a("  - newest boot: tags with failure-like names (meaning not checked here; see the diag tag "
                      "catalog): " + ", ".join("`%s` x%d (first value %s, line %d)" % (
                          t_, c, p["first_val"][t_][1], p["first_val"][t_][0]) for t_, c in fl[:15]))
        if diag_info["bad"]:
            rep.issue("supplier-diag has %d unparsable row(s)." % diag_info["bad"])
    if aligned_info is not None:
        ss = aligned_info["sessions"]
        rep.a("- aligned-entry: %d row(s) in %d boot(s); %d other line(s)." % (
            sum(s["rows"] for s in ss), len(ss), aligned_info["other"]))
        if ss:
            s = ss[-1]
            rep.a("  - newest boot: lines %d-%d, %d rows, build id(s) %s" % (
                s["first_line"], s["last_line"], s["rows"], ", ".join(s["builds"].keys())))
            fl = [(t_, c) for t_, c in s["tags"].most_common() if FAILISH_TAG_RE.search(t_)]
            if fl:
                rep.a("  - tags with failure-like names: " + ", ".join("`%s` x%d (first value %s)" % (
                    t_, c, s["first_val"][t_][1]) for t_, c in fl[:15]))
            if s.get("errno_nonzero"):
                rep.a("  - rows with a non-zero `errno=` (meaning not checked here): " + ", ".join(
                    "`%s` x%d" % kv_ for kv_ in s["errno_nonzero"].most_common(10)))
    if docs.get("klog"):
        k = docs["klog"]
        rep.a("- klog capture: %d line(s). Mod paths named in it: %s" % (len(k.lines), ", ".join(sorted(set(
            mm.group(1) for ln in k.lines for mm in MOD_PATH_RE.finditer(ln)))) or "none"))
    else:
        rep.a("- klog capture: not given (%s)." % opts.get("missing", {}).get("klog", "not given"))

    # ---- 0. in short (computed now, inserted after the header)
    real = [(n, pm) for n, pm in per_mod.items() if not mods[n]["is_lib"]]
    n_started = sum(1 for _, pm in real if "STARTED" in pm["verdict"])
    n_failed = sum(1 for _, pm in real if "FAILED" in pm["verdict"])
    n_unseen = sum(1 for _, pm in real if "NOT SEEN" in pm["verdict"])
    n_expected = sum(1 for n, _ in real if mods[n].get("enabled") is not False and mods[n].get("main") is not False)
    short = ["", rep.heading(2, "0. In short (auto-counted; plain words)"), "",
             "- Label: counts are Mac-only parsing of the lines in sections 4-9; those lines are: %s." % lab]
    short.append("- Build: %s; %s." % (("PRX sha `%s`" % sha[:12]) if sha else "PRX sha unknown", snap_short))
    if loadlist and loadlist_zero:
        short.append("- Mods: FAILED CHECK: the loadlist has no mod rows, so nothing was checked.")
    elif loadlist:
        n_disabled = sum(1 for _, pm in real if "DISABLED in mods.txt" in pm["verdict"])
        short.append("- Mods: %d of %d expected mods started; %d failed to start; %d never showed up in the log%s." % (
            n_started, n_expected, n_failed, n_unseen,
            ("; %d disabled by mods.txt" % n_disabled) if n_disabled else ""))
    else:
        short.append("- Mods: %d started (no loadlist, so the expected list is unknown)." % n_started)
    short.append("- Lua errors: %d line(s)%s; plus %d other error-like line(s) to read by hand (section 7)." % (
        len(errs), (" (" + ", ".join("%s %d" % kv_ for kv_ in by_mod.most_common(4)) + ")") if errs else "",
        len(other_errs)))
    if stv and stv["summary"]:
        short.append("- Self-test: %d of %d passed, %d failed, %d slow (SUMMARY says SLOW %d); %s refused hooks "
                     "counted as passed (GAPS)." % (
                         stv["summary"]["p"], stv["summary"]["M"], stv["cnt"].get("FAIL", 0), len(stv["slow"]),
                         stv["summary"]["slow"], stv["gaps"]["n"] if stv["gaps"] else "unknown"))
    elif st["lines"] and not st["runs"]:
        short.append("- Self-test: loaded (%d boot line(s)) but no run started in this session (AUTO start comes "
                     "+180 s after load, main.lua:118; the game may have closed first)." % len(st["boot"]))
    elif st["lines"]:
        short.append("- Self-test: a run started but no SUMMARY line (did not finish, or the line was lost).")
    else:
        short.append("- Self-test: did not run in this session.")
    short.append("- Crash signs: %d (plus %d harmless start-up/skip-list texts). Clean game exit seen: %s." % (
        crash_total, crash_benign, ("yes" if ue_ok and any(e["kind"] == "stopping" for e in events) else "no")))
    short.append("- Open issues: %d (section 10)." % len(rep.issues))
    rep.out[short_at:short_at] = short

    # ---- 10. open issues
    rep.h(2, "10. Open issues (auto-collected; edit before sending)")
    rep.a("- Label: Mac-only (collected by the parser from the sections above).")
    if rep.issues:
        for s in rep.issues:
            rep.a("- %s" % s)
    else:
        rep.a("- none found by the parser (a person still checks the game itself)")

    # ---- 11. next step
    rep.h(2, "11. Next step")
    rep.a("- Label: written by the driver (not parsed from files).")
    rep.a("- _TODO (driver): next step, in one plain line._")
    rep.a()
    rep.a("---")
    rep.a("Generated %s by %s. Parsed %d file(s), %d line(s)." % (
        datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"), VERSION, files_parsed, lines_parsed))

    counts = {"files": files_parsed, "lines": lines_parsed, "ue4ss_ok": ue_ok,
              "per_file": dict((k, len(docs[k].lines)) for k in present),
              "mods": len(mods), "lua_errors": len(errs), "other_errors": len(other_errs),
              "selftest_rows": len(st["rows"]), "loadlist_zero": loadlist_zero, "label": lab,
              "freshness": fresh_word,
              "ip_masked": sum(getattr(docs[k], "ip_masked", 0) for k in present),
              "crash_hits": crash_total, "issues": len(rep.issues)}
    return rep.text(), counts


# ---------------------------------------------------------------------------
# input resolution
# ---------------------------------------------------------------------------

def resolve_paths(args):
    """-> ({kind: path or None}, {kind: why-missing}, label, notes)."""
    paths = dict((k, None) for k in KIND_ORDER)
    missing = {}
    notes = []
    label = args.label
    explicit = {"ue4ss": args.ue4ss, "runtime-status": args.runtime_status, "diag": args.diag,
                "aligned": args.aligned}
    if args.dir or args.label:
        d = os.path.abspath(args.dir or HERE)
        if not os.path.isdir(d):
            raise Usage("--dir %s is not a directory" % rel(d))
        if not label:
            found = sorted(set(re.sub(r"^harvest-(.+)-ue4ss\.log$", r"\1", os.path.basename(p))
                               for p in glob.glob(os.path.join(d, "harvest-*-ue4ss.log"))))
            if len(found) > 1:
                raise Usage("--dir %s holds %d harvest labels (%s): pass --label" % (
                    rel(d), len(found), ", ".join(found[:12])))
            if len(found) == 1:
                label = found[0]
        for key, suffix, plain, _desc in HARVEST_KINDS:
            if label:
                p = os.path.join(d, "harvest-%s-%s" % (label, suffix))
                if os.path.isfile(p):
                    paths[key] = p
                else:
                    missing[key] = "not found: harvest-%s-%s" % (label, suffix)
            else:
                cands = []
                for g in plain:
                    cands.extend(glob.glob(os.path.join(d, g)))
                cands = sorted(set(cands))
                if len(cands) == 1:
                    paths[key] = cands[0]
                elif len(cands) > 1:
                    raise Usage("--dir %s: %d candidate %s files (%s): pass --%s" % (
                        rel(d), len(cands), key, ", ".join(os.path.basename(c) for c in cands[:6]), key))
                else:
                    missing[key] = "no %s in %s" % ("/".join(plain), rel(d))
        if not label and not args.klog and not args.no_klog:
            kl = sorted(glob.glob(os.path.join(d, "klog-*.log")))
            if len(kl) == 1:
                paths["klog"] = kl[0]
                notes.append("klog taken from the session dir: `%s`" % rel(kl[0]))
            elif len(kl) > 1:
                missing["klog"] = "%d klog-*.log files in the dir; pass --klog" % len(kl)
    for key, p in explicit.items():
        if p:
            if not os.path.isfile(p):
                raise Usage("--%s %s: file not found" % (key, p))
            paths[key] = os.path.abspath(p)
            missing.pop(key, None)
    if args.klog:
        if not os.path.isfile(args.klog):
            raise Usage("--klog %s: file not found" % args.klog)
        paths["klog"] = os.path.abspath(args.klog)
    if not label:
        p = paths.get("ue4ss")
        mm = re.match(r"^harvest-(.+)-ue4ss\.log$", os.path.basename(p)) if p else None
        label = mm.group(1) if mm else None
    return paths, missing, label, notes


def parse_args(argv):
    ap = argparse.ArgumentParser(
        prog="launch_report.py",
        description="Post-launch reviewer report DRAFT (CLAUDE.md section 7) from local harvest "
                    "files. Mac-only, read-only, decode-first; never contacts the console.")
    ap.add_argument("--dir", help="session dir (default: this script's dir, where harvest.py writes)")
    ap.add_argument("--label", help="harvest label: reads <dir>/harvest-<label>-*")
    ap.add_argument("--ue4ss", help="UE4SS log file (overrides --dir/--label)")
    ap.add_argument("--runtime-status", dest="runtime_status", help="runtime-status file")
    ap.add_argument("--diag", help="supplier-diag JSONL file")
    ap.add_argument("--aligned", help="aligned-entry log file")
    ap.add_argument("--klog", help="klog capture file")
    ap.add_argument("--no-klog", dest="no_klog", action="store_true",
                    help="do not read any klog file (also not the one the receipt names)")
    ap.add_argument("--receipt", help="LAUNCH-READY-<variant>-<ts>.md receipt")
    ap.add_argument("--loadlist", help="loadlist.txt or a prelaunch_rowcheck receipt JSON (mod names)")
    ap.add_argument("--ledger", action="append", help="build ledger .md (repeatable; default ../BUILD-LEDGER-*.md)")
    ap.add_argument("--no-ledger", dest="no_ledger", action="store_true", help="skip ledger + frozen lookups")
    ap.add_argument("--frozen-root", dest="frozen_root",
                    default=os.path.join(OUTPUTS_ROOT, "2026-10-08-l26-frozen"),
                    help="dir of frozen builds (<dir>/*/*.prx.sha256 + snapshot-*.manifest.json)")
    ap.add_argument("--all-sessions", dest="all_sessions", action="store_true",
                    help="analyse every UE4SS session in the log (default: the last one)")
    ap.add_argument("--fixture-test", dest="fixture", action="store_true",
                    help="stamp 'fixture test, not a verdict' everywhere (earlier-session files)")
    ap.add_argument("--selftest-tag", dest="st_tag", default=ST2_TAG_DEFAULT,
                    help="self-test line tag (default '[ST2] ', the self-test mod's Scripts/main.lua:11)")
    ap.add_argument("--console-utc-offset", dest="console_utc_offset", type=int, default=None,
                    help="console log clock offset from UTC in minutes (default: read the console clock as "
                         "this Mac's local time; see CONSOLE_CLOCK_NOTE)")
    ap.add_argument("--clock-tolerance-min", dest="clock_tol_min", type=int, default=CLOCK_TOL_MIN_DEFAULT,
                    help="tolerance in minutes for the log-content freshness check (default %d)" % CLOCK_TOL_MIN_DEFAULT)
    ap.add_argument("--max-quotes", dest="max_quotes", type=int, default=12, help="quoted lines per mod")
    ap.add_argument("--max-errors", dest="max_errors", type=int, default=40, help="Lua errors quoted")
    ap.add_argument("--out", help="write the markdown here (refuses to overwrite without --force)")
    ap.add_argument("--force", action="store_true", help="allow --out to overwrite")
    ap.add_argument("--self-test", dest="self_test", action="store_true",
                    help="run the built-in synthetic checks (in memory) and exit")
    return ap.parse_args(argv)


def eprint(s):
    sys.stderr.write(redact(s) + "\n")


def main(argv):
    args = parse_args(argv)
    if args.self_test:
        return run_self_test()
    if DSL is None:
        eprint("ERROR: decode_session_log.py not importable (%s): refusing to parse undecoded logs" % _DSL_ERROR)
        return 2
    if not (args.dir or args.label or args.ue4ss or args.runtime_status or args.diag or args.aligned or args.klog):
        eprint("ERROR: give --label (and/or --dir) or explicit input files; see --help")
        return 2
    try:
        paths, missing, label, notes = resolve_paths(args)
        receipt = None
        if args.receipt:
            if not os.path.isfile(args.receipt):
                raise Usage("--receipt %s: file not found" % args.receipt)
            with open(args.receipt, "r", encoding="utf-8", errors="replace") as f:
                receipt = parse_receipt(f.read(), os.path.abspath(args.receipt))
        if args.no_klog:
            paths["klog"] = None
            missing["klog"] = "--no-klog given"
        elif not paths.get("klog") and receipt and receipt.get("klog_capture"):
            kp = receipt["klog_capture"]
            if os.path.isfile(kp):
                paths["klog"] = kp
                notes.append("klog taken from the receipt's 'capture file:' line")
            else:
                missing["klog"] = "receipt names %s, not on this Mac" % kp
        loadlist = None
        if args.loadlist:
            if not os.path.isfile(args.loadlist):
                raise Usage("--loadlist %s: file not found" % args.loadlist)
            loadlist = load_loadlist(os.path.abspath(args.loadlist))
        elif receipt and receipt.get("rowcheck_receipt"):
            rp = receipt["rowcheck_receipt"]
            if os.path.isfile(rp):
                loadlist = load_loadlist(rp)
                notes.append("loadlist rows taken from the rowcheck receipt the launch receipt cites (%s)" %
                             receipt.get("rowcheck_summary", ""))
            else:
                notes.append("the receipt cites rowcheck receipt %s, which is not on this Mac" % rel(rp))
    except Usage as e:
        eprint("ERROR: %s" % e)
        return 2
    except OSError as e:
        eprint("ERROR: %s" % e)
        return 2

    ledger_entries, frozen = [], {}
    if not args.no_ledger:
        lpaths = args.ledger or sorted(glob.glob(os.path.join(PREP_ROOT, "BUILD-LEDGER-*.md")))
        ledger_entries = parse_ledgers(lpaths)
        frozen = frozen_snapshots(args.frozen_root)
        notes.append("snapshot lookup: %d ledger file(s) with %d full-sha row(s); %d frozen build(s) under `%s`" % (
            len(lpaths), len(ledger_entries), len(frozen), rel(args.frozen_root)))

    docs = {}
    for k in KIND_ORDER:
        p = paths.get(k)
        if not p:
            docs[k] = None
            missing.setdefault(k, "not given")
            continue
        d, err = load_doc(k, p)
        if d is None:
            docs[k] = None
            missing[k] = err
        else:
            docs[k] = d

    opts = {"fixture": args.fixture, "st_tag": args.st_tag, "max_quotes": max(1, args.max_quotes),
            "max_errors": max(1, args.max_errors), "label": label, "all_sessions": args.all_sessions,
            "missing": missing, "console_utc_offset_min": args.console_utc_offset,
            "clock_tol_min": max(0, args.clock_tol_min)}
    report, counts = analyze_and_render(docs, opts, receipt=receipt, loadlist=loadlist,
                                        ledger_entries=ledger_entries, frozen=frozen, notes=notes)
    n_masked = len(IP_RE.findall(report)) + counts.get("ip_masked", 0)
    out = redact(report)
    if IP_RE.search(out) or IP_FRAGMENT_RE.search(out):
        eprint("ERROR: redaction self-check failed")
        return 3
    if len(out.encode("utf-8")) > REPORT_LIMIT_BYTES:
        out = out[:REPORT_LIMIT_BYTES // 2] + "\n\n(REPORT CUT AT ~14 MB: lower --max-quotes)\n"
    if args.out:
        if os.path.exists(args.out) and not args.force:
            eprint("ERROR: --out %s exists (pass --force to overwrite)" % args.out)
            return 2
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(out)
        dest = rel(args.out)
    else:
        sys.stdout.write(out)
        dest = "stdout"
    per = ", ".join("%s=%d" % (k, v) for k, v in counts["per_file"].items()) or "none"
    eprint("[launch_report] parsed %d file(s), %d line(s) (%s); mods %d, Lua errors %d (+%d unclassified), "
           "self-test rows %d, crash hits %d, open issues %d; console-IP masks %d; label: %s; freshness logic: %s; "
           "report -> %s%s" % (
               counts["files"], counts["lines"], per, counts["mods"], counts["lua_errors"], counts["other_errors"],
               counts["selftest_rows"], counts["crash_hits"], counts["issues"], n_masked, counts["label"],
               counts["freshness"], dest,
               (" [" + FIXTURE_TAG + "]") if args.fixture else ""))
    if counts["files"] == 0 or counts["lines"] == 0:
        eprint("FAIL: zero files or zero lines parsed: nothing was checked")
        return 1
    if not counts["ue4ss_ok"]:
        eprint("FAIL: the UE4SS log is missing or empty: report is INCOMPLETE")
        return 1
    if counts["loadlist_zero"]:
        eprint("FAIL: the loadlist has zero mod rows: per-mod check covered 0 items")
        return 1
    return 0


# ---------------------------------------------------------------------------
# built-in synthetic self-test (in memory; no file writes)
# ---------------------------------------------------------------------------

def _wide(s):
    return b"".join(bytes([ord(c), 0]) for c in s)


def run_self_test():
    if DSL is None:
        print("FAIL: decode_session_log.py not importable (%s)" % _DSL_ERROR)
        return 1
    checks = []

    def check(name, cond, detail=""):
        checks.append((name, bool(cond)))
        print("  %s %s%s" % ("PASS" if cond else "FAIL", name, ("  -- " + detail) if (detail and not cond) else ""))

    T = "[2026-01-01 00:00:%02d] "
    sha = "ab" * 32
    L = []

    def ln(sec, text):
        L.append((T % sec).encode() + text + b"\n")

    ln(0, b"SYNTHETIC TEST FIXTURE (launch_report.py --self-test); not a console log")
    ln(0, b"Native console, GUI, crash dumping and filesystem watching are unavailable on this platform")
    ln(0, b"UE4SS - v3.0.1 - Git SHA #0000000")
    ln(0, b"[BUILD] id=l26-synth changes=x snapshot=feedc0de")
    ln(1, b"Mod 'AlphaMod' has enabled.txt, starting mod.")
    L.append((T % 1).encode() + b"[Lua] " + _wide("[AlphaMod] hello from ps5-host-not-configured.invalid") + b" "
             + (T % 1).encode() + b"[l22] Lua mod started: AlphaMod (main script returned)\n")
    ln(2, b"Mod 'BetaMod' has enabled.txt, starting mod.")
    ln(2, b"Error loading script: ue4ss/Mods/BetaMod/Scripts/main.lua:3: unexpected symbol near 'x'")
    ln(3, b"Mod 'stx' has enabled.txt, starting mod.")
    ln(3, b"[LFB-M] print-widen in='[ST2] boot v1.1 M=4 armed=true pf=true one-run=true'/len=55 ok=1")
    ln(3, b"[Lua] [ST2] boot v1.1 M=4 armed=true pf=true one-run=true")
    ln(3, b"[Lua::call_function] lua_pcall returned LUA_ERRRUN => boom")
    ln(3, b"[l22] Lua mod started: stx (main script returned)")
    ln(4, b"[Lua] [ST2] AUTO start")
    ln(4, b"[Lua] [ST2] MEM before gc_kb=100 live_kb=200 blocks=5 allocs=9 fails=0 fail_o=0 fail_n=0 "
          b"arena_cap_kb=0 arena_used_kb=0 arena_ext_kb=0 errs=1 cxx=0")
    ln(4, b"[Lua] [ST2] RUN start 1..4")
    ln(4, b"[Lua] [ST2] part2 loaded 3")
    ln(5, b"[Lua] [ST2] 1/4 print PASS logged")
    ln(5, b"[Lua] [ST2] 2/4 Key.F12 PASS F12=123 SLOW-ms=40")
    L.append((T % 6).encode() + b"[Lua] " + _wide("[ST2] 3/4 Foo.Bar FAIL attempt to index a nil value") + b"\n")
    ln(6, b"[Lua] [ST2] 4/4 Baz SKIP no-instance")
    ln(9, b"[Lua] [ST2] SUMMARY 2 of 4 PASS SLOW 1")
    ln(9, b"[Lua] [ST2] GAPS 1 soft-refused registrations counted in PASS (PC registers them): RegisterBeginPlayPostHook")
    ln(9, b"[Lua] [ST2] MEM after gc_kb=150 live_kb=260 blocks=7 allocs=20 fails=2 fail_o=0 fail_n=0 "
          b"arena_cap_kb=0 arena_used_kb=0 arena_ext_kb=0 errs=3 cxx=0")
    ln(9, b"[Lua] [ST2] done")
    for i, fails in enumerate((0, 1, 3)):
        ln(10 + i, ("[MEM] state=0x1 gc_kb=%d live_kb=300 blocks=1 allocs=2 fails=%d fail_o=0 fail_n=0 "
                    "arena_cap_kb=0 arena_used_kb=0 arena_ext_kb=0 errs=0 cxx=0" % (100 + i, fails)).encode())
    ln(13, b"[Lua::call_function] lua_pcall returned LUA_ERRMEM => not enough memory "
           b"[lua-mem: gc_kb=1 live_kb=2 blocks=3 allocs=4 fails=5 fail_o=0 fail_n=0 errs=1 cxx=0 last_error='x']")
    L.append(b"stack traceback:\n\t...phaMod/Scripts/main.lua:10: in function <...>\n")
    ln(14, b"[FOU] ForEachUObject: Lua stack exhausted after 5 objects; stopping walk")
    ln(14, b"[UEN] ForEachName: expected 5 (NumEnums), iterated 4 for /Script/X.Y")
    ln(15, b"[SHIM] write-area dir /data/w/AlphaMod created=true usable=true")
    ln(15, b"[SHIM] write-area dir /data/w/BetaMod created=false usable=false")
    ln(15, b"[SHIM] write-area AlphaMod -> /data/w/AlphaMod")
    ln(16, b"[Lua] [AlphaMod] Scan failed: something")
    ln(16, b"Error executing script: table: 0x1234")
    ln(16, b"[script_hook] A callback threw a non-standard exception")
    ln(16, b"Something odd happened: error 5 in the widget pump")
    # address placed so the 400-char quote clip cuts through it (review finding 5)
    ln(16, b"[Lua] [AlphaMod] " + b"x" * 352 + b"ps5-host-not-configured.invalid tail")
    ln(16, b"Mod 'EpsMod' disabled in mods.txt.")
    ln(17, b"Stopping mod 'AlphaMod' for uninstall")
    ue_raw = b"".join(L)
    klog_raw = b"[klog-capture] host=ps5-host-not-configured.invalid:3232\nPANIC: unprotected error in call to Lua API (x)\n"
    loadlist_text = "\n".join([
        "modforge-loadlist\t1", "mod\tAnything\t1.0.0",
        "file\tlua_mod\tue4ss/Mods/AlphaMod/Scripts/main.lua\t10",
        "file\tlua_mod\tue4ss/Mods/AlphaMod/enabled.txt\t0",
        "file\tlua_mod\tue4ss/Mods/BetaMod/Scripts/main.lua\t10",
        "file\tlua_mod\tue4ss/Mods/BetaMod/enabled.txt\t0",
        "file\tlua_mod\tue4ss/Mods/stx/Scripts/main.lua\t10",
        "file\tlua_mod\tue4ss/Mods/stx/enabled.txt\t0",
        "file\tlua_mod\tue4ss/Mods/shared/Lib/Lib.lua\t10",
        "file\tlua_mod\tue4ss/Mods/GammaMod/Scripts/main.lua\t10",
        "file\tlua_mod\tue4ss/Mods/GammaMod/enabled.txt\t0",
        "file\tlua_mod\tue4ss/Mods/DeltaMod/Scripts/main.lua\t10",
        "file\tlua_mod\tue4ss/Mods/EpsMod/Scripts/main.lua\t10",
        "file\tlua_mod\tue4ss/Mods/EpsMod/enabled.txt\t0", ""])
    ll = {"path": "<synthetic loadlist>", "format": "loadlist.txt rows", "headers": [], "parser": None}
    ll["rows"], ll["malformed"], ll["parser"] = parse_loadlist_text(loadlist_text)
    receipt_text = "\n".join([
        "# LAUNCH-READY-A-20260101T000000Z", "", "- generated (UTC): 2026-01-01T00:00:00Z", "- mode: live",
        "- console: ps5-host-not-configured.invalid (HTTP 8084)", "- variant: synthetic", "- PRX: x.prx", "  - size: 1 bytes",
        "  - sha256: %s" % sha, "  - magic: 4f153d1d", "", "## Verification ledger", "",
        "| # | step | check | expected | actual | status |", "|---|---|---|---|---|---|",
        "| 14 | 3 | uploaded PRX readback sha256 byte-exact | %s | %s | PASS |" % (sha, sha),
        "| 26 | 5.5 | loadlist row deploy gate (x) | all rows PASS | 5 rows: 5 pass [/nonexistent/rc.json] | PASS |",
        "", "## Pids", ""])
    receipt = parse_receipt(receipt_text, "<synthetic receipt>")
    print("== synthetic self-test (in memory; SYNTHETIC data built from the cited source formats) ==")
    check("receipt-sha", receipt["sha256"] == sha and receipt["readback_sha256"] == sha)
    check("receipt-rowcheck-path", receipt["rowcheck_receipt"] == "/nonexistent/rc.json")
    def drow(pid, tag, seq):
        return json.dumps({"marker": "modbridge.supplier-diag/v1", "schema": 1, "seq": seq, "pid": pid,
                           "tag": tag, "value": seq, "value_hex": "0x0", "t_ms": seq})
    diag_raw = "\n".join([drow(7, SESSION_START_TAG, 1), drow(7, "x_failed", 2), drow(9, SESSION_START_TAG, 3),
                          drow(9, "pad_retry_failed", 4), drow(9, SESSION_START_TAG, 5),
                          drow(9, "probe_abort_seen", 6), "not json"]).encode() + b"\n"
    aligned_raw = (b"[T1] modbridge-runtime-aligned-entry/v1 build=b1 " + SESSION_START_TAG.encode() + b" value=0x0\n"
                   b"[T1] modbridge-runtime-aligned-entry/v1 build=b1 worker_sleep_result result=0xffffffff errno=0x00000004\n")
    status_raw = b"modforge-runtime-status\t1\nsession\t9\t100\nstate\t3\tmods-started\t0\t\n"
    docs = {"ue4ss": Doc("ue4ss", "<synthetic ue4ss>", ue_raw, None),
            "runtime-status": Doc("runtime-status", "<synthetic status>", status_raw, None),
            "diag": Doc("diag", "<synthetic diag>", diag_raw, None),
            "aligned": Doc("aligned", "<synthetic aligned>", aligned_raw, None),
            "klog": Doc("klog", "<synthetic klog>", klog_raw, None)}
    check("decode-wide-run", "[AlphaMod] hello from" in "\n".join(docs["ue4ss"].lines))
    led = [{"file": "<synthetic ledger>", "lineno": 1, "sha": sha, "label": "SYN", "snapshot": "feedc0de",
            "build_id": "l26-synth"}]
    rep, counts = analyze_and_render(docs, {"fixture": True, "label": "synthetic"}, receipt=receipt,
                                     loadlist=ll, ledger_entries=led, frozen={sha: [("feedc0de", "<m>")]})
    out = redact(rep)
    check("ip-masked-in-report", not IP_RE.search(out) and IP_MASK in out)
    check("fixture-tag-title", out.splitlines()[0].endswith(FIXTURE_TAG))
    heads = [x for x in out.splitlines() if x.startswith("## ") or x.startswith("#### ")]
    check("fixture-tag-every-heading", heads and all(x.endswith(FIXTURE_TAG) for x in heads), str(heads[:3]))
    check("snapshot-agree-3-sources", "agree (3 sources;" in out)
    check("in-short-block", "## 0. In short (auto-counted; plain words) — " + FIXTURE_TAG in out
          and "- Mods: 2 of 5 expected mods started; 1 failed to start; 1 never showed up in the log; "
              "1 disabled by mods.txt." in out
          and "- Self-test: 2 of 4 passed, 1 failed, 1 slow (SUMMARY says SLOW 1); 1 refused hooks counted "
              "as passed (GAPS)." in out, "\n".join(x for x in out.splitlines() if x.startswith("- Mods") or x.startswith("- Self-test:")))
    check("alpha-started", re.search(r"\| AlphaMod \|[^\n]*STARTED", out) is not None)
    check("beta-failed", re.search(r"\| BetaMod \|[^\n]*FAILED TO START", out) is not None)
    check("gamma-not-seen", re.search(r"\| GammaMod \|[^\n]*NOT SEEN", out) is not None)
    check("delta-not-enabled", re.search(r"\| DeltaMod \|[^\n]*not enabled", out) is not None)
    check("shared-is-library", re.search(r"\| shared \|[^\n]*library folder", out) is not None)
    check("selftest-mod-identified", "come from mod **stx**" in out)
    check("selftest-summary", "SUMMARY 2 of 4 PASS SLOW 1" in out)
    check("selftest-gaps-line", "GAPS 1 soft-refused" in out)
    check("selftest-counts", "| 2 | 1 | 1 | 1 | 4 | 4 | none | none | 5 s" in out, "count row")
    check("selftest-wide-fail-row", "| 3 | Foo.Bar | FAIL | - | attempt to index a nil value |" in out)
    check("selftest-slow-row", "| 2 | Key.F12 | PASS | 40 | F12=123 |" in out)
    check("mem-delta", "fails +2" in out)
    check("mem-lines-fails-rose", "[MEM] allocator fails rose on state 0x1 (0 -> 3)" in out)
    check("lua-mem-note", "LUA_ERRMEM note(s)" in out)
    check("errmem-attributed-by-truncated-path", "shortened Lua chunk path '...phaMod/Scripts/'" in out)
    amb = attribute("x\n\t...aMod/Scripts/main.lua:1: in f", mods_from_loadlist(ll), None)
    check("ambiguous-suffix-stays-unknown", amb[0] is None, str(amb))
    check("beta-error-by-path", "path 'Mods/BetaMod/'" in out)
    check("fou-stack-exhausted", "stack-exhausted 1" in out)
    check("uen-mismatch", "UEnum.ForEachName count mismatch on 1 line(s)" in out)
    check("shim-unusable", "1 shim write-area dir(s) not usable" in out)
    check("klog-panic", "| klog capture | 2 | lua-panic | 1 | 0 |" in out)
    # the start-up notice IS in the synthetic log: it must be listed, annotated
    # and kept out of the crash count (compare with a run without the line)
    nb = [b for b in L if b"crash dumping and filesystem watching" not in b]
    docs_nb = dict(docs, ue4ss=Doc("ue4ss", "<synthetic ue4ss without notice>", b"".join(nb), None))
    _r_nb, c_nb = analyze_and_render(docs_nb, {"fixture": True, "label": "nb"}, receipt=receipt, loadlist=ll,
                                     ledger_entries=led, frozen={sha: [("feedc0de", "<m>")]})
    check("benign-listed-annotated", "crash dumping" in out and "not a crash" in out)
    check("benign-not-counted", c_nb["crash_hits"] == counts["crash_hits"], "%s vs %s" % (c_nb["crash_hits"], counts["crash_hits"]))
    check("benign-counted-as-harmless", re.search(r"Crash signs: \d+ \(plus [1-9]\d* harmless", out) is not None)
    # review fixes (2026-10-12): error shapes, catch-all, echo lines, IP fragment, disabled, probable
    check("execute-script-error-caught", "| execute-script-error | L" in out)
    check("script-hook-nonstandard-caught", "| script-hook-exception | L" in out)
    check("catchall-lists-unclassified", "Unclassified error-like runtime lines (catch-all, no known shape; read each one): 1"
          in out and "Something odd happened: error 5" in out, str(counts.get("other_errors")))
    st_syn = parse_selftest(build_messages(docs["ue4ss"].lines, 0, len(docs["ue4ss"].lines))[0], ST2_TAG_DEFAULT)
    check("print-widen-echo-not-parsed", "1 echo line(s) carry the `[ST2]` tag" in out
          and len(st_syn["boot"]) == 1 and len(st_syn["echo"]) == 1, "boot %d echo %d" % (
              len(st_syn["boot"]), len(st_syn["echo"])))
    check("ip-fragment-never-survives", not IP_FRAGMENT_RE.search(out) and ("x" * 352 + "<console-") in out
          and ("x" * 352 + "<console-ip> tail") not in out)
    check("disabled-in-mods-txt-verdict", re.search(r"\| EpsMod \|[^\n]*DISABLED in mods\.txt", out) is not None
          and "mods.txt disabled it" in out)
    check("probable-attribution-said", "only probable, by position" in out)
    check("every-section-labelled", all(("- Label:" in blk) for blk in out.split("\n## ")[1:]),
          str([blk.splitlines()[0] for blk in out.split("\n## ")[1:] if "- Label:" not in blk]))
    check("clean-exit-seen", "Clean-exit lines" in out)
    check("mod-printed-problem", "Scan failed: something" in out)
    check("counts", counts["files"] == 5 and counts["lines"] > 40 and counts["ue4ss_ok"], str(counts))
    check("diag-boots-split-on-pid-and-start-tag", "1 unparsable, 3 boot(s)" in out)
    check("status-session-equals-newest-pid", "equals the newest supplier-diag boot's pid 9" in out)
    check("diag-newest-boot-only-scanned", "| supplier-diag (newest boot only) | 2 | abort | 1 |" in out)
    check("aligned-errno-shape", "`worker_sleep_result` x1" in out)
    led2 = [dict(led[0], snapshot="0badf00d")]
    rep2, _ = analyze_and_render(docs, {"fixture": False, "label": "synthetic"}, receipt=receipt,
                                 loadlist=ll, ledger_entries=led2, frozen={})
    check("snapshot-disagree", "DISAGREE" in rep2)
    check("non-fixture-has-no-fixture-tag", FIXTURE_TAG not in rep2)
    # the synthetic log is dated 2026-01-01: outside fixture mode it must be flagged as old
    check("non-fixture-old-log-not-console-proven",
          "NOT console-proven (freshness check failed)" in rep2 and "WARNING: the log's last line is" in rep2)
    rep3, c3 = analyze_and_render(dict((k, None) for k in KIND_ORDER), {"fixture": True}, receipt=None)
    check("zero-inputs-counted-zero", c3["files"] == 0 and c3["lines"] == 0 and "INCOMPLETE" in rep3)
    rep4, _ = analyze_and_render(docs, {"fixture": True, "label": "nolist"}, receipt=receipt, loadlist=None)
    check("no-loadlist-says-unknown", "Mod list unknown: no loadlist given" in rep4)
    check("shim-end-not-flagged-when-clean-exit", "ends at a `[SHIM] write-area` line" not in out)
    cut = next(i for i, b in enumerate(L) if b"[SHIM] write-area AlphaMod" in b)
    docs5 = dict(docs, ue4ss=Doc("ue4ss", "<synthetic ue4ss cut at shim>", b"".join(L[:cut + 1]), None))
    rep5, _ = analyze_and_render(docs5, {"fixture": True, "label": "shimcut"}, receipt=receipt, loadlist=ll)
    check("shim-end-flagged-when-log-stops-there", "ends at a `[SHIM] write-area` line" in rep5
          and "No clean-exit lines" in rep5)
    # SUMMARY SLOW counter vs printed SLOW rows (main.lua:30 never resets it)
    Ls = [b.replace(b"SUMMARY 2 of 4 PASS SLOW 1", b"SUMMARY 2 of 4 PASS SLOW 3") for b in L]
    docs_s = dict(docs, ue4ss=Doc("ue4ss", "<synthetic slow>", b"".join(Ls), None))
    rep_s, _ = analyze_and_render(docs_s, {"fixture": True, "label": "slow"}, receipt=receipt, loadlist=ll)
    check("summary-slow-mismatch-noted", "SUMMARY says SLOW 3 but this run printed 1 SLOW row(s)" in rep_s)
    # zero-row loadlist fails closed
    ll0 = {"path": "<synthetic empty loadlist>", "format": "loadlist.txt rows", "headers": [], "parser": None}
    ll0["rows"], ll0["malformed"], ll0["parser"] = parse_loadlist_text("modforge-loadlist\t1\n")
    rep_z, c_z = analyze_and_render(docs, {"fixture": True, "label": "zero"}, receipt=receipt, loadlist=ll0)
    check("zero-row-loadlist-fails", c_z["loadlist_zero"] and "FAIL: the loadlist has 0 file row(s)" in rep_z
          and "Mods: FAILED CHECK" in rep_z)
    # content freshness (reviewer probe): a fresh Mac file time must NOT make
    # an earlier launch's log console-proven
    now = time.time()

    def fresh_log(start_epoch, build_id="l26-fresh"):
        out_ = []
        for k_, txt in enumerate((b"UE4SS - v3.0.1 - Git SHA #0000000",
                                   ("[BUILD] id=%s changes=x snapshot=feedc0de" % build_id).encode(),
                                   b"Mod 'AlphaMod' has enabled.txt, starting mod.",
                                   b"[l22] Lua mod started: AlphaMod (main script returned)",
                                   b"Stopping mod 'AlphaMod' for uninstall")):
            t_ = datetime.datetime.fromtimestamp(start_epoch + 10 * k_).strftime("%Y-%m-%d %H:%M:%S")
            out_.append(("[%s] " % t_).encode() + txt + b"\n")
        return b"".join(out_)

    def rc_at(epoch):
        r_ = dict(receipt)
        r_["generated"] = utc_str(epoch)
        r_["generated_epoch"] = float(int(epoch))
        return r_

    led_f = [dict(led[0], build_id="l26-fresh")]
    base = dict((k, None) for k in KIND_ORDER)
    d_ok = dict(base, ue4ss=Doc("ue4ss", "<fresh>", fresh_log(now - 3000), now))
    rep_f, c_f = analyze_and_render(d_ok, {"label": "fresh"}, receipt=rc_at(now - 3600), loadlist=ll,
                                    ledger_entries=led_f, frozen={sha: [("feedc0de", "<m>")]})
    check("fresh-log-console-proven", c_f["label"].startswith("console-proven") and "Freshness (b) log content: OK" in rep_f,
          c_f["label"])
    rep_ff, c_ff = analyze_and_render(d_ok, {"label": "fresh-fixture", "fixture": True}, receipt=rc_at(now - 3600),
                                      loadlist=ll, ledger_entries=led_f, frozen={sha: [("feedc0de", "<m>")]})
    check("fixture-mode-never-console-proven", c_ff["label"] == FIXTURE_TAG and "Freshness logic result: **PASS**" in rep_ff
          and "console-proven (this launch" not in rep_ff, c_ff["label"])
    d_old = dict(base, ue4ss=Doc("ue4ss", "<stale, fresh mtime>", fresh_log(now - 8 * 3600 - 300), now))
    rep_o, c_o = analyze_and_render(d_old, {"label": "stale"}, receipt=rc_at(now - 3600), loadlist=ll,
                                    ledger_entries=led_f, frozen={sha: [("feedc0de", "<m>")]})
    check("stale-log-fresh-mtime-not-console-proven", c_o["label"].startswith("NOT console-proven")
          and "this log is from an EARLIER launch" in rep_o and "sections below: console-proven" not in rep_o, c_o["label"])
    d_fut = dict(base, ue4ss=Doc("ue4ss", "<future clock>", fresh_log(now + 7200), now))
    _rep_u, c_u = analyze_and_render(d_fut, {"label": "future"}, receipt=rc_at(now - 3600), loadlist=ll,
                                     ledger_entries=led_f, frozen={sha: [("feedc0de", "<m>")]})
    check("misaligned-clock-is-unknown-not-proven", c_u["label"] == "NOT console-proven (freshness unknown)", c_u["label"])
    d_bid = dict(base, ue4ss=Doc("ue4ss", "<other build>", fresh_log(now - 3000, "l26-other"), now))
    rep_b, c_b = analyze_and_render(d_bid, {"label": "bid"}, receipt=rc_at(now - 3600), loadlist=ll,
                                    ledger_entries=led_f, frozen={sha: [("feedc0de", "<m>")]})
    check("build-id-mismatch-not-proven", c_b["label"].startswith("NOT console-proven")
          and "written by a different build" in rep_b, c_b["label"])
    n = len(checks)
    nf = sum(1 for _, ok in checks if not ok)
    print("== launch_report self-test: %d check(s), %d failure(s): %s ==" % (
        n, nf, "PASS" if (n and not nf) else "FAIL"))
    return 0 if (n and not nf) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
