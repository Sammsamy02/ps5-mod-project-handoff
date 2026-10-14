#!/bin/bash
# bind_launch.sh -- post-bind single-run launch pipeline (agent EJ, 2026-10-09).
#
# RE-BASELINED 2026-10-10 (agent FH, Claude Q4d): every identity pin now lives
# in harvest/pipeline-pins.json -- THAT FILE is the single place to re-baseline
# (labels + freeze procedure inside). Tonight's stale expectations fixed there:
#   - EI tostring-bind check now requires the SUPERSEDED markers (agent ER's
#     in-port pool decoder deliberately replaced the L27 bind);
#   - build_variants.py pinned in "recompute-at-preflight" mode (EZ/FG edits
#     in flight): preflight accepts + RECORDS the current sha; steps 2/3 fail
#     on any drift between preflight and execute (incl. the driver receipts);
#   - l26_packaging_driver.py re-baselined to the build-proven ee3aa230...
#
# Purpose: the moment agent EI's ToStringInternal bind report lands
# (outputs/2026-10-08-l25-prep/agent-EI-tostring-bind.md), turn
# build -> package -> upload -> user-sequence + watch into ONE scripted run
# with zero manual steps. Until that report exists the script cleanly reports
# WAITING-FOR-EI and does nothing else.
#
# Modes (THE DECISION is the parent's; this script implements both):
#   --all      carry EVERYTHING staged in the l26 tree: EI's ToString bind +
#              EB's enriched [FFO] marker (both inside candidate/src/
#              UObjectGlobals.cpp) + EE's variant C engine delete-listener
#              registration + Z15 copyin-gate early release (gated bridge
#              94f81a17) + EC's StaticFindObject long-name path fix.
#   --minimal  carry ONLY EI's bind + EB's marker on top of the console-proven
#              5ea2c845 verified-query state (DA). Variant C, Z15 gating and
#              EC's StaticFindObject fix are DEFERRED one round: the tree must
#              be pivoted back to the 5ea2c845 identities by their owners
#              (EI/parent) BEFORE this mode can pass pre-flight. This script
#              never edits the trees; it verifies pins and tells you exactly
#              which file/row still needs the pivot.
#
# Steps (each with an abort gate and a clear message):
#   0. pre-flight pin check (CHANGES files[] rows vs disk, EI-report gate,
#      mode pin-set; tolerates NOTHING once the EI report exists -- row owners
#      must have refreshed their rows).
#   1. archive prior build/package dirs (drivers refuse non-fresh dirs).
#   2. build_variants.py --execute          (twin variants A+B, from what/ cwd)
#   3. l26_packaging_driver.py --execute    (20/20 static, 207 imports, 238/2)
#   4. upload PRX-A by CALLING prep_modded_launch.py --variant A (it does
#      PRX + ORIGINAL cache-off settings + ploader arm + klog tap; Claude
#      2026-10-09 wants the original settings = the prep default -- NO
#      cache-on override afterwards; wakeup_prep.sh is NOT invoked here).
#   5. print the user sequence + the live watch commands (ladder markers).
#
# Extra flags:
#   --preflight-only   run step 0 only (the dry-run)
#   --watch            run the acceptance-ladder live watcher (task 4 helper;
#                      polls the console UE4SS.log over read-only FTP)
#   --console-ip IP    default ps5-host-not-configured.invalid (LAUNCH-PLAN.md header)
#   --skip-upload      stop after packaging (no console contact at all)
#
# Exit codes: 0 ok / 2 usage / 3 WAITING-FOR-EI (clean wait) /
#             4 pre-flight pin mismatch / 5 build failed / 6 packaging failed /
#             7 upload (prep) failed.
#
# Trees are READ-ONLY to this script. It writes only: the archive moves and
# build/package outputs the drivers own, the pipeline logs (DURABLE since
# 2026-10-10: outputs/2026-10-08-l25-prep/evidence/runtime/bind-pipeline/;
# legacy location was /tmp/bind-pipeline), and whatever
# prep_modded_launch.py itself is designed to write (its receipts next to it).
#
# Pinned identities (all cited) -- moved 2026-10-10 (agent FH, Claude Q4d)
# into harvest/pipeline-pins.json, the SINGLE re-baseline point; keys keep
# their labels/citations there:
#   - tested baseline PRX 5ea2c845... (FINAL-RECEIPT-VARIANT-A.json,
#     l26-package-A)                                    -> prx_baseline_5ea2
#   - 5ea2c845 runtime source prefixes (NEXT-BUILD.md "Baseline identity"):
#       UObjectArray.cpp ea7221e8d753...                -> uarray_cpp_5
#       UObjectArray.hpp 69a2391aae35...                -> uarray_hpp_5
#       outer_bridge f510c01484e3... (ungated, pre-Z15) -> bridge_5ea2
#   - EE-integrated state (NEXT-BUILD.md): gated bridge 94f81a17...
#     -> bridge_gated ; EngineListenerOpsProfile.hpp 39cbe0e8...
#     -> listener_hdr ; CTOKEN MODBRIDGE_L25_ENGINE_ADD_LISTENER (token,
#     stays in the preflight code below)
#   - build_variants.py  -> driver_all (recompute-at-preflight while EZ/FG
#     edit; lineage EE 4fa39211... -> EZ f61af2e8... -> FG stamp gen)
#   - l26_packaging_driver.py -> pkgdrv_all (re-baselined 2026-10-10 to the
#     build-proven ee3aa230...; superseded EE's 68146c3f...)
#   - CM-era drivers (the ones that built 5ea2c845) stay receipt-driven:
#     l26-build-variants/{EXECUTION,PACKAGING-EXECUTION}-RECEIPT.json
#     driver_sha256 (build e1d6ca0c..., packaging 9ce15914...).
set -o pipefail

WHAT="/WORKSPACE/.codex/worktrees/translation-core/what"
L26="$WHAT/outputs/2026-10-08-l26-runtime"
L25PREP="$WHAT/outputs/2026-10-08-l25-prep"
EI_REPORT="$L25PREP/agent-EI-tostring-bind.md"
PREP="$L25PREP/harvest/prep_modded_launch.py"
HARVEST="$L25PREP/harvest/harvest.py"
PINS_FILE="$L25PREP/harvest/pipeline-pins.json"
PINS_LAST="$L25PREP/harvest/pipeline-pins.last.json"
PAIR_ROOT="$WHAT/outputs/2026-10-08-l26-build-variants"
PKG_A="$WHAT/outputs/2026-10-08-l26-package-A"
PKG_B="$WHAT/outputs/2026-10-08-l26-package-B"
PRX_A="$PKG_A/pass-1/modbridge_supplier_composition.prx"
# EVIDENCE-RETENTION (agent-HA 2026-10-10): pipeline receipts/logs moved off
# /tmp (Claude's law) into the durable tree. Legacy runs remain preserved at
# evidence/tmp-preserved-20261010/tmp/bind-pipeline/. Override: LOGROOT=... env.
LOGROOT="${LOGROOT:-$L25PREP/evidence/runtime/bind-pipeline}"
CONSOLE_IP="${CONSOLE_IP:-ps5-host-not-configured.invalid}"
DECODE="$L25PREP/harvest/decode_session_log.py"  # agent GZ 2026-10-08:
                                                  # dual-path NUL-interleave
                                                  # decoder (Claude's plain-grep
                                                  # miss finding); watch_ladder
                                                  # decodes BEFORE grepping.
                                                  # RAW=1 keeps legacy raw grep.
TS="$(date -u +%Y%m%dT%H%M%SZ)"
MODE=""
PREFLIGHT_ONLY=0
SKIP_UPLOAD=0

# ---- pinned hashes (all in harvest/pipeline-pins.json) --------------------
# Every identity pin (label + value) lives in $PINS_FILE -- the single place
# to re-baseline (agent FH, Claude Q4d, 2026-10-10). driver_all carries the
# special value "recompute-at-preflight" while agents edit build_variants.py:
# preflight accepts + records the current sha into $PINS_LAST (and
# /tmp/bind-pipeline/<ts>/pins.log); steps 2/3 refuse drift. See that file's
# freeze_procedure for the one-line freeze once FG lands. Mode flags:
# --minimal / --all.

usage() {
  sed -n '2,38p' "$0" | sed 's/^# \{0,1\}//'
  echo "usage: $0 --minimal|--all [--preflight-only] [--skip-upload] [--console-ip IP]"
  echo "       $0 --watch [--console-ip IP]"
  exit 2
}

log()  { printf '[%s] %s\n' "$(date -u +%H:%M:%SZ)" "$*"; }
fail() { log "ABORT: $*"; exit "$1"; }

# ===========================================================================
# STEP 0 -- pre-flight (mode-aware pin verification; python does the JSON)
# ===========================================================================
preflight() {
python3 - "$MODE" "$WHAT" "$EI_REPORT" "$LOGDIR" "$PINS_FILE" "$PINS_LAST" <<'PYEOF'
import hashlib, json, os, sys, glob, time
mode, WHAT, EI_REPORT = sys.argv[1], sys.argv[2], sys.argv[3]
LOGDIR = sys.argv[4] if len(sys.argv) > 4 else ""
PINS_FILE = sys.argv[5] if len(sys.argv) > 5 else ""
PINS_LAST = sys.argv[6] if len(sys.argv) > 6 else ""
L26 = os.path.join(WHAT, "outputs/2026-10-08-l26-runtime")
PAIR = os.path.join(WHAT, "outputs/2026-10-08-l26-build-variants")

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()

# Identity pins are data-driven (agent FH, Claude Q4d, 2026-10-10):
# harvest/pipeline-pins.json is the SINGLE re-baseline point. Entries carry a
# label plus sha256 (or sha256_prefix). Special value RECOMPUTE means: accept
# the sha found at preflight, RECORD it into the pins receipt, and require
# steps 2/3 to see exactly that sha (drift check + driver-receipt match) --
# used while agents actively edit build_variants.py (EZ --incremental landed;
# FG stamp generation in flight). Freeze procedure: see the JSON header.
RECOMPUTE = "recompute-at-preflight"
try:
    _pdata = json.load(open(PINS_FILE))["pins"]
except Exception as e:
    print("PREFLIGHT: FAILED for --%s (pipeline-pins.json unreadable: %s)" % (mode, e))
    sys.exit(4)
PINS = {k: v.get("sha256") for k, v in _pdata.items()}
PINS_PREFIX = {k: v["sha256_prefix"] for k, v in _pdata.items() if "sha256_prefix" in v}
PIN_LABEL = {k: v.get("label", k) for k, v in _pdata.items()}
accepted = {}   # role (file basename) -> sha accepted at THIS preflight; steps 2/3 drift-check against it
UG = os.path.join(L26, "typescan-r4/candidate/src/UObjectGlobals.cpp")
UA_CPP = os.path.join(L26, "typescan-r4/candidate/src/UObjectArray.cpp")
UA_HPP = os.path.join(L26, "typescan-r4/candidate/include/Unreal/UObjectArray.hpp")
UI_CPP = os.path.join(L26, "typescan-r4/candidate/src/UnrealInitializer.cpp")
HDR = os.path.join(L26, "typescan-r4/include/EngineListenerOpsProfile.hpp")
BRIDGE = os.path.join(L26, "copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp")
DRIVER = os.path.join(L26, "build_variants.py")
PKGDRV = os.path.join(L26, "l26_packaging_driver.py")
CTOK, BTOK = "MODBRIDGE_L25_ENGINE_ADD_LISTENER", "MODBRIDGE_L25_SKIP_DELETE_LISTENER"

def report_ok(msg):   print("  [OK]   " + msg)
def report_info(msg): print("  [info] " + msg)
def report_warn(msg): print("  [WARN] " + msg)

# --- gate 1: the EI report is the launch trigger ---------------------------
if not os.path.isfile(EI_REPORT):
    print("STATUS: WAITING-FOR-EI")
    print("  EI report not present yet: " + EI_REPORT)
    print("  Nothing else runs. Re-run this script (same mode flag) once it lands.")
    if os.path.isfile(UG):
        s = sha(UG); m = time.strftime("%H:%M:%S", time.localtime(os.path.getmtime(UG)))
        try: txt = open(UG, "r", errors="replace").read()
        except OSError: txt = ""
        print("  live tree context (UObjectGlobals.cpp):")
        print("    disk sha256 : %s  (mtime %s -- in-flight if very recent)" % (s[:16] + "...", m))
        print("    DA verified-query block present : %s" % ("yes" if "[FFO] query-fallback" in txt else "NO"))
        print("    EB enriched marker present      : %s" % ("yes" if "num:{}" in txt and "ts:'" in txt else "NO"))
        print("    EI tostring-bind marker present : %s" % ("yes" if "[FN] tostring-bind" in txt else "NO (bind not landed yet)"))
    try:
        d = json.load(open(os.path.join(L26, "typescan-r4/CHANGES.json")))
        for r in d["files"]:
            if r["candidate"].endswith("UObjectGlobals.cpp"):
                act = sha(r["candidate"]) if os.path.exists(r["candidate"]) else "MISSING"
                state = "matches disk" if act == r["candidate_sha256"] else "STALE vs disk"
                print("    files[] row declares %s... ; %s (row owner refreshes it when EI lands)" % (r["candidate_sha256"][:12], state))
    except Exception as e:
        print("    (could not read CHANGES rows: %s)" % e)
    sys.exit(3)

print("  EI report present: " + os.path.basename(EI_REPORT))

errors, warnings = [], []
# the pins file itself must carry every key the gates below rely on
for _k in ("driver_all", "pkgdrv_all", "bridge_gated", "bridge_5ea2", "listener_hdr", "prx_baseline_5ea2"):
    if _k not in PINS:
        errors.append("pipeline-pins.json is missing pin '%s' -- restore it (harvest/pipeline-pins.json is the single pin source)" % _k)
for _k in ("uarray_cpp_5", "uarray_hpp_5"):
    if _k not in PINS_PREFIX:
        errors.append("pipeline-pins.json is missing sha256_prefix pin '%s'" % _k)

# --- gate 2: G2 mirror -- every files[] row of the three parts vs disk -----
rows_seen = 0
for part in ("profile", "typescan-r4", "thread"):
    p = os.path.join(L26, part, "CHANGES.json")
    d = json.load(open(p))
    for i, row in enumerate(d["files"]):
        rows_seen += 1
        cand, orig = row["candidate"], row["original"]
        for role, path, declared in (("candidate", cand, row["candidate_sha256"]),
                                     ("original", orig, row["original_sha256"])):
            if not os.path.exists(path):
                errors.append("%s files[%d] %s %s MISSING on disk: %s" % (part, i, os.path.basename(path), role, path))
                continue
            if sha(path) != declared:
                who = "the row's owning agent (EI for UObjectGlobals.cpp mid-bind; EE for variant-C rows; EC for the StaticFindObject row)"
                errors.append("%s files[%d] %s %s STALE: declared %s vs disk %s -- %s must refresh the row before --execute (build_variants gate G2 will hard-fail)"
                              % (part, i, os.path.basename(path), role, declared[:12], sha(path)[:12], who))
# targeted diagnostic for the row EE flagged (NEXT-BUILD.md 'Proofs already run'):
d4 = json.load(open(os.path.join(L26, "typescan-r4/CHANGES.json")))
for i, row in enumerate(d4["files"]):
    if row["candidate"].endswith("UObjectGlobals.cpp"):
        act = sha(row["candidate"])
        if act != row["candidate_sha256"]:
            print("  [note] the stale UObjectGlobals.cpp CHANGES row EE flagged is still stale:")
            print("         files[%d] declares %s... vs disk %s..." % (i, row["candidate_sha256"][:12], act[:12]))
            print("         DA's old row value was 4abf743b... (the 5ea2c845 verified-query state);")
            print("         the row owner must refresh BOTH files[] and changes[] to the post-EI hash.")
if errors == []:
    report_ok("all %d files[] rows (profile/typescan-r4/thread) match disk -- G2 mirror clean" % rows_seen)

# --- gate 3: the bind content actually landed in UObjectGlobals.cpp -------
txt = open(UG, "r", errors="replace").read()
if "[FFO] query-fallback" not in txt:
    errors.append("UObjectGlobals.cpp lost DA's '[FFO] query-fallback' block (verified-query fix missing)")
else:
    report_ok("DA verified-query block present ([FFO] query-fallback)")
if "num:{}" in txt and "ts:'" in txt:
    report_ok("EB enriched marker present ([FFO] ... num:{} ts:'{}')")
else:
    errors.append("EB's enriched [FFO] marker (num:{} + ts:'{}') NOT in UObjectGlobals.cpp -- apply fname-rootfix/fname-rootfix.diff or its landed equivalent before launching")
nt_txt = open(os.path.join(L26, "typescan-r4/candidate/src/NameTypes.cpp"), "r", errors="replace").read()
ui_txt = open(os.path.join(L26, "typescan-r4/candidate/src/UnrealInitializer.cpp"), "r", errors="replace").read()
# RE-BASELINED 2026-10-10 (agent FH, Claude Q4d): EI's L27 engine bind was
# deliberately SUPERSEDED by agent ER's in-port pool decoder (console-verified;
# see PREFLIGHT-WAIVER-20261010T0253Z.md error 1). The check now requires the
# superseded markers; a tree carrying ONLY the legacy bind family is still
# accepted (warned); a tree with NEITHER family is an error.
SUP_RUNTIME = "[FN] tostring: in-port pool decoder (L27-bind superseded)"
SUP_SEEDS   = "[Seeds][L27], agent-EI): SUPERSEDED"
LEGACY = ("[FN] tostring-bind", "[Seeds][L27] ToString bind")
bind_txt = nt_txt + "\n" + ui_txt
sup_hits = [m for m in (SUP_RUNTIME, SUP_SEEDS) if m in bind_txt]
legacy_hits = [m for m in LEGACY if m in bind_txt]
if sup_hits:
    report_ok("EI L27 bind state = SUPERSEDED by ER's in-port pool decoder (markers: %s)" % " | ".join(sup_hits))
elif legacy_hits:
    report_warn("only EI's legacy bind markers present (%s) -- re-baseline expects the SUPERSEDED markers; accepted, owners should reconcile the tree" % " | ".join(legacy_hits))
else:
    errors.append("NEITHER bind family found in NameTypes.cpp/UnrealInitializer.cpp: no legacy '%s'/'%s' and no superseded '%s'/'%s' -- the tree lost the L27 tostring mechanism entirely"
                  % (LEGACY[0], LEGACY[1], SUP_RUNTIME, SUP_SEEDS))
# [BUILD] stamp generation (FG, driver-side) and the stamp-consumer TU (FF)
# may land in either order -- presence is informational here, never a failure.
bs_hits = []
for _root, _dirs, _files in os.walk(L26):
    _dirs[:] = [x for x in _dirs if x not in ("tests", "original", "results")]
    for _fn in _files:
        if _fn == "BuildStamp.cpp": bs_hits.append(os.path.join(_root, _fn))
if bs_hits:
    report_ok("[BUILD] stamp consumer TU BuildStamp.cpp present (%s)" % bs_hits[0])
else:
    report_info("[BUILD] stamp consumer TU BuildStamp.cpp not landed yet (FG/FF may land in either order) -- informational only, not a failure")
ei_txt = open(EI_REPORT, "r", errors="replace").read()
ug_sha = sha(UG)
if ug_sha in ei_txt:
    report_ok("EI report pins the current on-disk UObjectGlobals.cpp (%s...)" % ug_sha[:12])
else:
    warnings.append("EI report does not mention the current UObjectGlobals.cpp disk hash %s... -- read the report and confirm it describes this exact tree state" % ug_sha[:12])
if BTOK not in open(UI_CPP, "r", errors="replace").read():
    errors.append("variant-B skip token %s not found in UnrealInitializer.cpp -- driver gate G4-token-present would refuse" % BTOK)
else:
    report_ok("variant-B skip token still present (driver G4 requirement)")

# --- gate 4: mode-specific pin set -----------------------------------------
if mode == "all":
    _drv_sha = sha(DRIVER)
    if PINS["driver_all"] == RECOMPUTE:
        report_ok("build_variants.py accepted at CURRENT sha %s... (pin mode '%s': EZ/FG edits in flight; recorded for step-2/3 drift check)" % (_drv_sha[:12], RECOMPUTE))
        accepted["build_variants.py"] = _drv_sha
    elif _drv_sha == PINS["driver_all"]:
        report_ok("build_variants.py = pinned %s... (%s)" % (PINS["driver_all"][:12], PIN_LABEL.get("driver_all", "")))
        accepted["build_variants.py"] = _drv_sha
    else:
        errors.append("build_variants.py is %s..., pipeline-pins.json pins %s... -- re-baseline in harvest/pipeline-pins.json (the single pin source)" % (_drv_sha[:12], str(PINS["driver_all"])[:12]))
    _pkg_sha = sha(PKGDRV)
    if _pkg_sha == PINS["pkgdrv_all"]:
        report_ok("l26_packaging_driver.py = re-baselined pin %s... (build-proven: etfix/seamfix/lfb packages)" % PINS["pkgdrv_all"][:12])
        accepted["l26_packaging_driver.py"] = _pkg_sha
    else:
        errors.append("l26_packaging_driver.py is %s..., pipeline-pins.json pins %s... -- re-baseline in harvest/pipeline-pins.json" % (_pkg_sha[:12], PINS["pkgdrv_all"][:12]))
    if CTOK in open(UA_CPP, "r", errors="replace").read():
        report_ok("variant C switch (%s) present in UObjectArray.cpp" % CTOK)
    else:
        errors.append("CTOKEN %s absent from UObjectArray.cpp -- variant C not landed; driver gate G4-ctoken would refuse" % CTOK)
    if os.path.isfile(HDR):
        if sha(HDR) == PINS["listener_hdr"]:
            report_ok("EngineListenerOpsProfile.hpp present and pinned (%s...)" % PINS["listener_hdr"][:12])
            accepted["EngineListenerOpsProfile.hpp"] = sha(HDR)
        else:
            errors.append("EngineListenerOpsProfile.hpp is %s..., expected %s..." % (sha(HDR)[:12], PINS["listener_hdr"][:12]))
    else:
        errors.append("EngineListenerOpsProfile.hpp missing -- variant C admission cannot compile")
    man = json.load(open(os.path.join(L26, "copyin-probe/MANIFEST.json")))
    qb = man["base_files"]["runtime_start_bridge"]
    if qb["candidate_sha256"] == PINS["bridge_gated"] and os.path.exists(BRIDGE) and sha(BRIDGE) == PINS["bridge_gated"]:
        report_ok("outer_bridge = Z15-gated candidate 94f81a17... (early gate release rides this launch)")
        accepted["runtime_start_bridge.cpp"] = PINS["bridge_gated"]
    else:
        errors.append("outer_bridge pin is %s / disk %s -- expected gated 94f81a17... (gating-proposal/CHANGES.json)"
                      % (qb["candidate_sha256"][:12], sha(BRIDGE)[:12] if os.path.exists(BRIDGE) else "MISSING"))
    report_info("mode ALL carries: EI bind + EB marker + EC StaticFindObject + variant C + Z15 gating")
    report_info("variant C failure mode on-console is LOUD seed_refused (terminal, nothing binds) -- see NEXT-BUILD.md step 4")
else:  # minimal
    want = json.load(open(os.path.join(PAIR, "EXECUTION-RECEIPT.json")))["driver_sha256"]
    got = sha(DRIVER)
    if got == want:
        report_ok("build_variants.py = the CM-era driver that built 5ea2c845 (%s...)" % want[:12])
        accepted["build_variants.py"] = got
    else:
        copies = []
        for p in glob.glob(os.path.join(WHAT, "outputs/*/build_variants.py")):
            if sha(p) == want: copies.append(p)
        errors.append("build_variants.py is %s... but --minimal needs the CM-era driver %s... (recorded in l26-build-variants/EXECUTION-RECEIPT.json). "
                      "Archived copy search: %s. The parent must restore/reconstruct it (EE's driver hard-requires the variant-C token via gate G4-ctoken)."
                      % (got[:12], want[:12], copies if copies else "NONE FOUND on disk"))
    gotp = sha(PKGDRV)
    wantp = json.load(open(os.path.join(PAIR, "PACKAGING-EXECUTION-RECEIPT.json")))["driver_sha256"]
    if gotp == wantp:
        report_ok("l26_packaging_driver.py = the CM-era packaging driver (%s...)" % wantp[:12])
        accepted["l26_packaging_driver.py"] = gotp
    elif gotp == PINS["pkgdrv_all"]:
        warnings.append("packaging driver is the re-baselined %s... -- functionally fine for minimal (it follows the copyin-probe MANIFEST bridge pin), only the receipt note text mentions variant C; preferred is CM-era %s..." % (gotp[:12], wantp[:12]))
        accepted["l26_packaging_driver.py"] = gotp
    else:
        errors.append("l26_packaging_driver.py is %s..., expected CM-era %s... or the re-baselined %s..." % (gotp[:12], wantp[:12], PINS["pkgdrv_all"][:12]))
    # variant C must be absent from the tree
    tok_hits = []
    for root, dirs, files in os.walk(L26):
        dirs[:] = [x for x in dirs if x not in ("tests", "original", "results")]
        for fn in files:
            if os.path.splitext(fn)[1] in (".cpp", ".cc", ".cxx", ".c", ".hpp", ".h", ".inl", ".inc"):
                p = os.path.join(root, fn)
                try: t = open(p, "r", errors="replace").read()
                except OSError: continue
                if CTOK in t: tok_hits.append(p)
    if tok_hits:
        errors.append("variant C still in the tree (%d files, e.g. %s) -- --minimal requires the variant-C files reverted to their 5ea2c845 identities by EE/parent"
                      % (len(tok_hits), tok_hits[0]))
    else:
        report_ok("variant C absent from tree code files (registration path stays the tested hand-rolled one)")
    if os.path.exists(HDR):
        errors.append("EngineListenerOpsProfile.hpp still present -- variant C header must be withdrawn for --minimal")
    # three variant-C rows back at the 5ea2c845 identities. UObjectArray.cpp/.hpp
    # are pure variant-C territory -> exact identity. UnrealInitializer.cpp may
    # ALSO carry EI's bind edits -> require variant-C CONTENT absent, not a hash.
    for path, name, pref in ((UA_CPP, "UObjectArray.cpp", PINS_PREFIX["uarray_cpp_5"]),
                             (UA_HPP, "UObjectArray.hpp", PINS_PREFIX["uarray_hpp_5"])):
        if not os.path.exists(path):
            errors.append("%s MISSING" % name); continue
        if not sha(path).startswith(pref):
            errors.append("%s is %s..., must be reverted to the 5ea2c845 identity %s... (NEXT-BUILD.md 'Baseline identity')" % (name, sha(path)[:12], pref))
        else:
            report_ok("%s at the tested 5ea2c845 identity (%s...)" % (name, pref))
    ui_txt = open(UI_CPP, "r", errors="replace").read() if os.path.exists(UI_CPP) else ""
    vc_in_ui = [m for m in (CTOK, "SetupEngineDeleteListenerOps", "EngineListenerOpsProfile") if m in ui_txt]
    if vc_in_ui:
        errors.append("UnrealInitializer.cpp still carries variant-C content %s -- revert EE's Phase-6 (EI's own bind edits on top of cac95e48... ARE wanted here)" % vc_in_ui)
    else:
        report_ok("UnrealInitializer.cpp free of variant-C content (cac95e48... + EI bind edits)")
    if "SplitLongNameIntoPathParts" in txt:
        errors.append("EC's StaticFindObject fix still in UObjectGlobals.cpp (SplitLongNameIntoPathParts present) -- --minimal defers it one round; EC/parent must revert file+row")
    else:
        report_ok("EC StaticFindObject fix deferred (not in UObjectGlobals.cpp)")
    man = json.load(open(os.path.join(L26, "copyin-probe/MANIFEST.json")))
    qb = man["base_files"]["runtime_start_bridge"]
    if qb["candidate_sha256"] == PINS["bridge_5ea2"] and os.path.exists(BRIDGE) and sha(BRIDGE) == PINS["bridge_5ea2"]:
        report_ok("outer_bridge = ungated f510c014... (Z15 gate early-release deferred)")
        accepted["runtime_start_bridge.cpp"] = PINS["bridge_5ea2"]
    else:
        errors.append("outer_bridge pin is %s / disk %s -- --minimal needs the ungated 5ea2c845 bridge f510c014... (revert copyin-probe/MANIFEST.json pin + file)"
                      % (qb["candidate_sha256"][:12], sha(BRIDGE)[:12] if os.path.exists(BRIDGE) else "MISSING"))
    report_info("mode MINIMAL carries: DA verified-query (already in 5ea2c845) + EI bind + EB marker only")

for w in warnings: report_warn(w)
# pins receipt (agent FH): the stable file that steps 2/3 drift-check against.
# Written even on FAILED preflights (forensics); 'accepted' is authoritative
# only when preflight == READY -- the drift check refuses anything else.
try:
    _rec = {"recorded_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "mode": mode, "preflight": ("READY" if not errors else "FAILED"),
            "accepted": accepted,
            "recompute_mode_pins": [k for k, v in _pdata.items() if v.get("sha256") == RECOMPUTE]}
    open(PINS_LAST, "w").write(json.dumps(_rec, indent=2) + "\n")
    if LOGDIR:
        _lines = ["%s pins receipt mode=%s preflight=%s recorded_utc=%s"
                  % (time.strftime("%H:%M:%SZ", time.gmtime()), mode, _rec["preflight"], _rec["recorded_utc"])]
        _lines += ["accepted %-28s %s" % (r, s) for r, s in sorted(accepted.items())]
        _lines += ["recompute-mode pins: %s" % (", ".join(_rec["recompute_mode_pins"]) or "none")]
        open(os.path.join(LOGDIR, "pins.log"), "w").write("\n".join(_lines) + "\n")
    report_info("pins receipt recorded: %s%s" % (PINS_LAST, (" (+ %s)" % os.path.join(LOGDIR, "pins.log")) if LOGDIR else ""))
except Exception as e:
    print("  [WARN] could not write the pins receipt (%s) -- the execute-time drift check will fail closed" % e)
    errors.append("pins receipt %s unwritable -- steps 2/3 drift check cannot run" % PINS_LAST)
if errors:
    print("PREFLIGHT: FAILED for --%s (%d error(s))" % (mode, len(errors)))
    for e in errors: print("  [FAIL]  " + e)
    print("  The script never edits the trees -- the named owners pivot them; then re-run.")
    sys.exit(4)
print("PREFLIGHT: READY for --%s (EI report present, %d rows clean, mode pins verified)" % (mode, rows_seen))
sys.exit(0)
PYEOF
}

# ===========================================================================
# pins drift check (agent FH, Claude Q4d) -- steps 2/3 re-verify, at
# invocation time, that every identity preflight accepted is STILL that
# identity (recompute-at-preflight pins + literal pins alike). Optional:
# $2 receipt json + $3 role -> the driver receipt's own driver_sha256 must
# equal what preflight saw (catches drift even if a file changed and changed
# back, and pins the executed build to the vetted driver bytes).
# ===========================================================================
pin_drift_check() {
  local stage="$1" receipt="${2:-}" role="${3:-}"
  python3 - "$PINS_LAST" "$WHAT" "$stage" "$receipt" "$role" <<'PYDRIFT'
import hashlib, json, os, sys
PINS_LAST, WHAT, stage, receipt, role = sys.argv[1:6]
L26 = os.path.join(WHAT, "outputs/2026-10-08-l26-runtime")
ROLE_PATH = {
    "build_variants.py":            os.path.join(L26, "build_variants.py"),
    "l26_packaging_driver.py":      os.path.join(L26, "l26_packaging_driver.py"),
    "EngineListenerOpsProfile.hpp": os.path.join(L26, "typescan-r4/include/EngineListenerOpsProfile.hpp"),
    "runtime_start_bridge.cpp":     os.path.join(L26, "copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp"),
}
def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""): h.update(c)
    return h.hexdigest()
rec = json.load(open(PINS_LAST))
if rec.get("preflight") != "READY":
    print("  [FAIL]  pins receipt %s is not READY (%s) -- re-run preflight" % (PINS_LAST, rec.get("preflight"))); sys.exit(4)
bad = []
for r, want in sorted(rec.get("accepted", {}).items()):
    p = ROLE_PATH.get(r)
    if not p or not os.path.exists(p):
        bad.append("%s: pinned file missing (%s)" % (r, p or "no path mapped")); continue
    got = sha(p)
    if got != want:
        bad.append("%s drifted since preflight: saw %s..., now %s..." % (r, want[:12], got[:12]))
if receipt and role:
    want = rec.get("accepted", {}).get(role)
    drv = json.load(open(receipt)).get("driver_sha256")
    if not want:
        bad.append("receipt check: preflight accepted nothing for role %s" % role)
    elif drv != want:
        bad.append("%s receipt driver_sha256 %s... != preflight-accepted %s... -- the build ran a driver preflight never vetted" % (role, (drv or "MISSING")[:12], want[:12]))
if bad:
    for b in bad: print("  [FAIL]  " + b)
    print("  mid-pipeline identity drift at %s -- never execute against a moving driver; re-run preflight" % stage)
    sys.exit(4)
print("  [OK]   pins stable at %s: %s%s" % (stage,
      ", ".join("%s=%s..." % (r, v[:12]) for r, v in sorted(rec.get("accepted", {}).items())),
      (", receipt %s driver_sha256 matches" % role if receipt and role else "")))
PYDRIFT
}

# ===========================================================================
# STEP 4 helper -- the acceptance-ladder live watcher (task 4)
# ===========================================================================
watch_ladder() {
  local IP="$CONSOLE_IP"
  local tmp="/tmp/bind-watch-ue4ss.log"
  local dec="/tmp/bind-watch-ue4ss.decoded.log"
  local re='(\[FN\] tostring-bind|\[FFO\] query-fallback|db=v|Menu created|Spawned |\[Seeds\]\[L26\] engine delete-listener|engine-call resolved|copyin_probe_gate_l26_early_release|seed_refused|l25_dl_refused)'
  log "watching ftp://$IP:2121/data/plugins/UE4SS.log every 5s -- Ctrl-C to stop"
  log "legend: [FN] tostring-bind (EI bind state; 'disabled'=fell back) | [FFO] query-fallback (DA fix engaging, expected-healthy) | db=v (zzdiag v8: database lookups WORK) | 'Menu created' (rung 2) | 'Spawned ' (rung 3, mission) | [Seeds][L26]/engine-call/early_release (variant C, --all only) | seed_refused/l25_dl_refused = LOUD fail-closed, capture addresses, do NOT retry"
  # agent GZ 2026-10-08: the console UE4SS.log is written by the dual-path
  # writer (narrow UTF-8 + wide-doubled byte,NUL runs; Claude's finding) --
  # plain grep -aE over the raw bytes misses every marker spanning a wide
  # run. Decode after download (in a second file; the raw fetch stays
  # untouched), grep the DECODED text. RAW=1 restores the legacy raw grep.
  while :; do
    curl -s --max-time 10 "ftp://$IP:2121/data/plugins/UE4SS.log" -o "$tmp" 2>/dev/null
    if [ -s "$tmp" ]; then
      if [ -z "${RAW:-}" ] && [ -f "$DECODE" ]; then
        python3 "$DECODE" "$tmp" "$dec" 2>/dev/null
        grep -aE "$re" "$dec" | awk '!seen[$0]++'
      else
        grep -aE "$re" "$tmp" | awk '!seen[$0]++'
      fi
    fi
    sleep 5
  done
}

# ===========================================================================
# arg parsing
# ===========================================================================
[ $# -eq 0 ] && usage
while [ $# -gt 0 ]; do
  case "$1" in
    --minimal) MODE="minimal" ;;
    --all) MODE="all" ;;
    --preflight-only) PREFLIGHT_ONLY=1 ;;
    --skip-upload) SKIP_UPLOAD=1 ;;
    --watch) watch_ladder; exit 0 ;;
    --console-ip) shift; [ $# -gt 0 ] || usage; CONSOLE_IP="$1" ;;
    *) usage ;;
  esac
  shift
done
[ -n "$MODE" ] || usage

LOGDIR="$LOGROOT/run-${MODE}-$TS"
mkdir -p "$LOGDIR"

# ===========================================================================
log "STEP 0: pre-flight (--$MODE)"
preflight | tee "$LOGDIR/preflight.log"
rc=${PIPESTATUS[0]}
[ "$rc" -eq 3 ] && { log "clean wait -- nothing else will run"; exit 3; }
[ "$rc" -ne 0 ] && fail 4 "pre-flight failed (see $LOGDIR/preflight.log)"
[ "$PREFLIGHT_ONLY" -eq 1 ] && { log "preflight-only requested -- stopping here (READY)"; exit 0; }

# ===========================================================================
log "STEP 0b: driver plan runs (no toolchain execution; catches composition/order regressions before archiving anything)"
( cd "$WHAT" && python3 "$L26/build_variants.py" --plan ) > "$LOGDIR/build-plan.log" 2>&1
if [ $? -ne 0 ]; then tail -5 "$LOGDIR/build-plan.log"; fail 4 "build_variants --plan reported failed gates -- resolve before archiving/execute"; fi
( cd "$WHAT" && python3 "$L26/l26_packaging_driver.py" --plan ) > "$LOGDIR/package-plan.log" 2>&1
if [ $? -ne 0 ]; then tail -5 "$LOGDIR/package-plan.log"; fail 4 "packaging --plan reported failed gates -- resolve before archiving/execute"; fi
log "  plan gates clean (build + packaging); note: plan tolerates row drift loudly -- pre-flight above already enforced exact pins"

# ===========================================================================
log "STEP 1: archive prior build/package dirs (drivers refuse non-fresh dirs)"
ARCH="$PAIR_ROOT/archive-$TS"
mkdir -p "$ARCH"
for d in "$PAIR_ROOT/l26-A" "$PAIR_ROOT/l26-B" "$PAIR_ROOT/staging-copyin-probe" "$PKG_A" "$PKG_B"; do
  if [ -e "$d" ]; then
    mv "$d" "$ARCH/$(basename "$d")" && log "  archived $(basename "$d") -> $ARCH/"
  fi
done
[ -e "$PKG_A" ] && fail 4 "could not archive $PKG_A (still present) -- resolve manually, never let the packager overwrite"
log "  archive complete"

# ===========================================================================
log "STEP 2: build_variants.py --execute (twin A+B, cwd=what/ per NEXT-BUILD step 2)"
pin_drift_check "step-2 (build invoke)" || fail 4 "driver identity drifted between preflight and build -- re-run preflight, never execute against a moving driver"
( cd "$WHAT" && python3 "$L26/build_variants.py" --execute ) 2>&1 | tee "$LOGDIR/build.log"
[ ${PIPESTATUS[0]} -eq 0 ] || fail 5 "build failed -- log: $LOGDIR/build.log (gate failures are printed as JSON 'gate' lines)"
for f in "$PAIR_ROOT/l26-A/TWIN-RECEIPT.json" "$PAIR_ROOT/l26-B/TWIN-RECEIPT.json" "$PAIR_ROOT/VARIANT-PAIR-RECEIPT.json"; do
  [ -f "$f" ] || fail 5 "expected build receipt missing: $f"
done
log "  build OK: twin receipts + pair receipt present"
pin_drift_check "step-2 (build receipt)" "$PAIR_ROOT/EXECUTION-RECEIPT.json" build_variants.py || fail 4 "build receipt driver_sha256 does not match what preflight saw"

# ===========================================================================
log "STEP 3: l26_packaging_driver.py --execute (20/20 static, 207 imports, 238/2 arrays, twins identical)"
pin_drift_check "step-3 (packaging invoke)" || fail 4 "driver identity drifted between preflight and packaging -- re-run preflight"
( cd "$WHAT" && python3 "$L26/l26_packaging_driver.py" --execute ) 2>&1 | tee "$LOGDIR/package.log"
[ ${PIPESTATUS[0]} -eq 0 ] || fail 6 "packaging failed -- log: $LOGDIR/package.log"
[ -f "$PKG_A/FINAL-RECEIPT-VARIANT-A.json" ] || fail 6 "FINAL-RECEIPT-VARIANT-A.json missing"
[ -f "$PRX_A" ] || fail 6 "packaged PRX missing: $PRX_A"
NEWSHA=$(shasum -a 256 "$PRX_A" | cut -d' ' -f1)
MAGIC=$(xxd -l 4 -p "$PRX_A")
log "  package OK: PRX-A sha256=$NEWSHA magic=$MAGIC"
pin_drift_check "step-3 (packaging receipt)" "$PAIR_ROOT/PACKAGING-EXECUTION-RECEIPT.json" l26_packaging_driver.py || fail 6 "packaging receipt driver_sha256 does not match what preflight saw"
case "$MAGIC" in
  4f153d1d|5414f5ee) log "  PRX magic = signed SELF (decrypt-off readback will re-verify on upload)" ;;
  *) fail 6 "PRX magic $MAGIC is NOT a signed self -- DO NOT upload (PS5 rule 2); run make_fself path" ;;
esac
PRX_BASELINE="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["pins"]["prx_baseline_5ea2"]["sha256"])' "$PINS_FILE")"
if [ -z "$PRX_BASELINE" ]; then
  fail 6 "pipeline-pins.json missing prx_baseline_5ea2 -- cannot run the tested-baseline identity check"
fi
if [ "$NEWSHA" = "$PRX_BASELINE" ]; then
  fail 6 "new PRX is byte-identical to the ALREADY-TESTED 5ea2c845 baseline -- the staged changes did not reach the artifact; do not re-launch the same build"
fi

if [ "$SKIP_UPLOAD" -eq 1 ]; then
  log "--skip-upload: stopping before any console contact. PRX ready at: $PRX_A"
  exit 0
fi

# ===========================================================================
log "STEP 4: upload PRX-A via prep_modded_launch.py (PRX + ORIGINAL cache-off settings + ploader + klog tap; NO settings override after)"
python3 "$PREP" --variant A --prx-path "$PRX_A" --console-ip "$CONSOLE_IP" 2>&1 | tee "$LOGDIR/prep.log"
[ ${PIPESTATUS[0]} -eq 0 ] || fail 7 "prep failed -- its LAUNCH-PREP-FAILED-A-*.md receipt is next to prep_modded_launch.py; NEVER blind-replay; reconcile first"
READY=$(ls -t "$L25PREP/harvest/LAUNCH-READY-A-"*.md 2>/dev/null | head -1)
[ -n "$READY" ] || fail 7 "prep exited 0 but no LAUNCH-READY-A receipt found"
log "  upload OK: $READY"
# informational, read-only: zzdiag v8 resident? ('db=v' watch lines depend on it)
V8SZ=$(curl -s --max-time 10 "ftp://$CONSOLE_IP:2121/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua" | wc -c | tr -d ' ')
if [ "$V8SZ" = "1488" ]; then log "  zzdiag v8 resident (main.lua 1488 B) -- 'db=v' auto-probe lines will appear"; else log "  NOTE: zzdiag main.lua is ${V8SZ} B (v8 = 1488) -- 'db=v' lines need v8; wakeup_prep.sh staged it earlier, verify before relying on it"; fi

# ===========================================================================
log "STEP 5: hand the player the sequence (from $READY)"
echo "--------------------- USER SEQUENCE (variant A) ---------------------"
echo "1. Launch the game (Townfall, PPSA33286) -- ONE launch."
echo "2. Main menu: WAIT 90 s (covers the PS5Pad >=10 s uptime needed for the pad borrow)."
echo "3. Load the SAME save used in the l24 crash run."
echo "4. WAIT 30 s past save-load."
echo "5. Press L3+R3 ONCE (OPTIONS stays the game's pause)."
echo "6. CLEAN EXIT via in-game quit to the dashboard (never a power event)."
echo "   If it crashes: note the code + WHEN (menu / save-load / L3+R3); harvest"
echo "   immediately; REBOOT before any next launch (no rapid-fire launches)."
echo "---------------------------------------------------------------------"
echo
echo "LIVE WATCH (acceptance ladder) -- in another terminal:"
echo "  $0 --watch --console-ip $CONSOLE_IP"
echo "or the one-liner:"
echo "  while :; do curl -s --max-time 10 \"ftp://$CONSOLE_IP:2121/data/plugins/UE4SS.log\" -o /tmp/bind-watch-ue4ss.log && grep -aE '(\[FN\] tostring-bind|\[FFO\] query-fallback|db=v|Menu created|Spawned |\[Seeds\]\[L26\]|engine-call resolved|early_release|seed_refused)' /tmp/bind-watch-ue4ss.log | awk '!seen[\$0]++'; sleep 5; done"
echo
echo "POST-RUN harvest (read-only, BEFORE any relaunch):"
echo "  python3 \"$HARVEST\" --label l26-bind-${MODE}-1 --console-ip $CONSOLE_IP"
echo
echo "Settings note: prep_modded_launch restored the ORIGINAL cache-off settings"
echo "(its default; /tmp/l24-settings-backup.ini b980cea7) -- per Claude 2026-10-09"
echo "there is NO cache-on override after this point (wakeup_prep.sh NOT run here)."
log "DONE: pipeline complete for --$MODE (logs: $LOGDIR)"
exit 0
