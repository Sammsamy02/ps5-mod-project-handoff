#!/bin/bash
# fast_cycle.sh -- FAST-CYCLE launcher for the l25/l26 PS5 mod-menu proving
# ladder (agent-EN, 2026-10-09; Claude directives 2/3/4: each launch cycle
# ~20 min -> target ~5 min by collapsing the per-cycle command storm).
#
# ONE command per cycle phase; every phase REUSES the verified tools in this
# directory verbatim (nothing here reimplements upload/verify/analysis):
#   prep_modded_launch.py  -- the gate-verified upload/readback/settings/
#                             ploader-arm/klog-tap flow (agent-Z16, host-
#                             tested, console-proven; eboot-not-running +
#                             ploader-lock residency + decrypt-OFF readback
#                             laws all live INSIDE it)
#   harvest.py             -- evidence pull + marker analysis + verdicts
#                             (agent-Y + agent-AJ/CK/EH; --local-ue4ss is
#                             its analysis-only regression mode)
#   bind_launch.sh         -- source of the build/package command shapes and
#                             the ladder-watch marker regex (agent-EJ)
#
# MODES (one command per cycle):
#   fast_cycle.sh prep --prx <path> [--variant A|B] [--console-ip IP]
#                      [--skip-klog]
#         prep_modded_launch.py's full flow (preconditions -> PRX gate ->
#         upload + decrypt-off readback -> settings -> ploader.ini -> arm
#         ploader LAST -> klog tap + eboot monitor). Prints the big
#         '=== LAUNCH NOW ===' banner ONLY if every gate passed and the
#         LAUNCH-READY receipt exists; on any failure prints the failing
#         gate + a remediation one-liner and exits nonzero.
#   fast_cycle.sh watch [--console-ip IP] [--source FILE] [--interval N]
#         live acceptance-ladder watcher (bind_launch --watch pattern,
#         improved): poll the console UE4SS.log every N s (default 5), print
#         only NEW matches of the marker set, print '*** RUNG HIT ***' the
#         moment 'Menu created' or 'Spawned ' appears, and print
#         'runtime live' when a NEW UE4SS session banner shows up
#         (injection worked). --source FILE polls a local log instead
#         (offline testing / saved-log replay).
#   fast_cycle.sh verdict [--console-ip IP] [--source FILE]
#         directive 3 (verdict first): quick-pull UE4SS.log once, run
#         harvest.py's verified analysis (--local-ue4ss --skip-klog) on it,
#         print ONLY the compact verdict line + the three key fields
#         (db state, menu, spawn) -- then background the FULL
#         harvest.py --label auto-<ts> run (all pulls + klog pickup +
#         HARVEST-auto-<ts>.md report write) in a nohup log.
#   fast_cycle.sh next [--dry-check]
#         directive 4 (start the next build while the run is fresh):
#         check the three CHANGES.json files[] row sets (profile /
#         typescan-r4 / thread) against disk (bind_launch preflight gate-2
#         mirror). Clean -> nohup the archive+build+package chain
#         (build_variants.py --execute + l26_packaging_driver.py --execute,
#         the exact bind_launch steps 1-3 command shapes, PRX magic +
#         not-byte-identical-to-5ea2c845 checks) into a log; stale rows ->
#         print exactly which rows are stale and do NOTHING else.
#         NO console contact in any case. --dry-check runs the row check
#         and prints the chain without launching it.
#
# SAFETY (inherited, never weakened):
#   - ploader is dispatched at most once per prep: prep_modded_launch.py
#     step 1 aborts if the ploader lock holds a LIVE pid (agent-Z2 law) and
#     step 6 sends the loadpayload dispatch exactly once.
#   - no upload while eboot.bin runs: prep_modded_launch.py step 1 aborts.
#   - decrypt-off readback (-Q 'SITE SELF') with full sha256 match on every
#     executable upload; PS5 rules 1/2/4 (AGENTS.md) live inside the reused
#     tool and are never bypassed here.
#   - watch/verdict are read-only FTP pulls (text logs only, no SITE SELF,
#     no uploads); next never contacts the console at all.
#   - next refuses a double kick: one background build chain at a time
#     (pidfile guard; the archive step moves shared dirs).
#
# Exit codes: 0 ok / 2 usage / 1 prep or verdict quick-phase failed /
#             4 next: stale CHANGES rows (nothing launched).
set -o pipefail

WHAT="/WORKSPACE/.codex/worktrees/translation-core/what"
L25PREP="$WHAT/outputs/2026-10-08-l25-prep"
HARVEST_DIR="$L25PREP/harvest"
PREP="$HARVEST_DIR/prep_modded_launch.py"
HARVEST="$HARVEST_DIR/harvest.py"
L26="$WHAT/outputs/2026-10-08-l26-runtime"
PAIR_ROOT="$WHAT/outputs/2026-10-08-l26-build-variants"
PKG_A="$WHAT/outputs/2026-10-08-l26-package-A"
PKG_B="$WHAT/outputs/2026-10-08-l26-package-B"
PRX_A="$PKG_A/pass-1/modbridge_supplier_composition.prx"
BASELINE_PRX_SHA="5ea2c845ba4a01dde4599c08f1930262b394e76279d1bfbaa235a19499dbfc51"

FCTMP="/tmp/fast-cycle"
mkdir -p "$FCTMP"
# EVIDENCE-RETENTION (agent-HA 2026-10-10; Claude's law: nothing evidential
# lives in /tmp): FCTMP stays TRANSIENT state (pidfiles, runners, watch
# buffers), but every RECEIPT this tool produces (prep/verdict/next logs,
# quick-fetched UE4SS.log) is copied DURABLE here at write time.
EVCF="$WHAT/outputs/2026-10-08-l25-prep/evidence/runtime/fast-cycle"
mkdir -p "$EVCF"

CONSOLE_IP="${CONSOLE_IP:-ps5-host-not-configured.invalid}"   # LAUNCH-PLAN.md header
UE4SS_REMOTE_LOG="/data/plugins/UE4SS.log" # harvest.py UE4SS_REMOTE
DECODE="$HARVEST_DIR/decode_session_log.py"  # agent GZ 2026-10-08: dual-path
                                             # NUL-interleave decoder; watch now
                                             # decodes BEFORE grepping (Claude's
                                             # plain-grep-miss finding).
                                             # RAW=1 fast_cycle.sh watch restores
                                             # the legacy tr -d normalization.

# normalize_log <in> <out>: decode the fetched console log per the writer
# grammar (narrow UTF-8 + wide-doubled byte,NUL runs). A NUL-free file is a
# strict no-op. Falls back to tr only if the decoder is missing.
normalize_log() {
  if [ -z "${RAW:-}" ] && [ -f "$DECODE" ]; then
    python3 "$DECODE" "$1" "$2" --stats 2>/dev/null
  else
    tr -d '\000' < "$1" > "$2"
  fi
}

log()  { printf '[%s] %s\n' "$(date -u +%H:%M:%SZ)" "$*"; }
usage() {
  sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
  echo
  echo "usage: $0 prep --prx <path> [--variant A|B] [--console-ip IP] [--skip-klog]"
  echo "       $0 watch [--console-ip IP] [--source FILE] [--interval 5]"
  echo "       $0 verdict [--console-ip IP] [--source FILE]"
  echo "       $0 next [--dry-check]"
  exit 2
}
need_file() { [ -f "$1" ] || { log "ABORT: required file missing: $1"; exit 2; }; }

# ===========================================================================
# MODE: prep  (reuse prep_modded_launch.py verbatim; banner only on success)
# ===========================================================================
mode_prep() {
  local prx="" variant="A" skip_klog=""
  while [ $# -gt 0 ]; do
    case "$1" in
      --prx) shift; [ $# -gt 0 ] || usage; prx="$1" ;;
      --variant) shift; [ $# -gt 0 ] || usage; case "$1" in A|B) variant="$1" ;; *) usage ;; esac ;;
      --console-ip) shift; [ $# -gt 0 ] || usage; CONSOLE_IP="$1" ;;
      --skip-klog) skip_klog="--skip-klog" ;;
      *) usage ;;
    esac
    shift
  done
  [ -n "$prx" ] || { log "prep requires --prx <path>"; usage; }
  need_file "$PREP"
  [ -f "$prx" ] || { log "PREP GATE FAILED: --prx file does not exist: $prx"; log "  remediation: build/package first (fast_cycle.sh next), or point --prx at a packaged PRX"; exit 1; }

  local ts logf rc
  ts="$(date -u +%Y%m%dT%H%M%SZ)"
  logf="$FCTMP/prep-$ts.log"
  log "prep: variant=$variant prx=$prx console=$CONSOLE_IP (log: $logf)"
  log "  safety inherited from prep_modded_launch.py: eboot-must-not-run,"
  log "  ploader-lock residency (never dispatch twice), decrypt-off readback."

  python3 "$PREP" --variant "$variant" --prx-path "$prx" \
      --console-ip "$CONSOLE_IP" $skip_klog 2>&1 | tee "$logf"
  rc=${PIPESTATUS[0]}
  cp "$logf" "$EVCF/" 2>/dev/null || true   # HA 2026-10-10: durable receipt

  if [ "$rc" -ne 0 ]; then
    echo
    log "PREP FAILED (exit $rc) -- DO NOT LAUNCH."
    local gate receipt
    gate="$(grep -m1 '\[prep\] ABORT:' "$logf" | sed 's/^\[prep\] ABORT: //')"
    receipt="$(grep -m1 '\[prep\] report: ' "$logf" | sed 's/^\[prep\] report: //')"
    [ -n "$gate" ] && echo "FAILING GATE: $gate"
    [ -n "$receipt" ] && echo "failure receipt: $receipt"
    case "$gate" in
      "STEP 1"*)
        echo "remediation: console precondition (daemons/eboot/ploader lock) -- wait for a clean game exit (or reboot), wait <=120 s for a ploader INJECTOR_TIMEOUT exit, then re-run prep." ;;
      "STEP 2"*)
        echo "remediation: PRX is not a fake-signed SELF -- run make_fself.py first (PS5 rule 3; uploading unsigned bricks the game)." ;;
      "STEP 3"*)
        echo "remediation: upload/readback mismatch -- prep already attempted its one verified restore; reconcile console state before ANY retry, never blind-replay." ;;
      "STEP 4"*)
        echo "remediation: settings backup drift on this Mac -- restore outputs/2026-10-08-l25-prep/evidence/l24-settings-backup.ini (exactly 1947 bytes, sha256 b980cea7...; prep resolves durable-first since 2026-10-10; legacy /tmp copy is fallback) and re-run." ;;
      "STEP 5"*)
        echo "remediation: ploader.ini backup drift -- restore outputs/2026-10-08-l25-prep/evidence/ploader-ini-backup-20261008.ini (sha256 3eab9510..., =2 inject row; durable-first since 2026-10-10; legacy /tmp copy is fallback) and re-run." ;;
      "STEP 6"*)
        echo "remediation: ploader arm failed -- the dispatch is one-shot and was already sent; NEVER re-send blindly; reconcile pldmgr / wait for the process to exit, re-run prep (its lock check protects a double dispatch)." ;;
      "STEP 7"*)
        echo "remediation: klog tap / eboot monitor failed to start -- restore the pinned klog_capture.py, or pass --skip-klog as an explicit operator decision." ;;
      *)
        echo "remediation: read the receipt above; never bypass a failed verification." ;;
    esac
    exit 1
  fi

  # rc==0 is not enough: the receipt must be a LAUNCH-READY one (bind_launch
  # step-4 rule).
  local ready
  ready="$(grep -m1 '\[prep\] report: ' "$logf" | sed 's/^\[prep\] report: //')"
  case "${ready:-}" in
    ""|*[!A-Za-z0-9._/-]*)
      log "PREP FAILED: no usable prep receipt path in log (got: ${ready:-none}) -- reconcile before launching; full log: $logf"
      exit 1 ;;
  esac
  if [ ! -f "$ready" ]; then
    log "PREP FAILED: prep receipt not found on disk: $ready -- full log: $logf"
    exit 1
  fi
  case "$(basename "$ready")" in
    LAUNCH-READY-*) : ;;  # the only success shape
    *)
      log "PREP FAILED: prep exit 0 but receipt is not LAUNCH-READY: $ready -- reconcile before launching; full log: $logf"
      exit 1 ;;
  esac

  echo
  echo "  ###########################################################################"
  echo "  ###                                                                   ###"
  echo "  ###                      ===  L A U N C H  N O W ===                  ###"
  echo "  ###                                                                   ###"
  echo "  ###########################################################################"
  echo
  log "all gates passed; receipt: $ready"
  echo "USER SEQUENCE (Townfall, PPSA33286):"
  echo "  1. Launch the game -- ONE launch."
  echo "  2. Main menu: WAIT 90 s (covers the PS5Pad >=10 s uptime)."
  echo "  3. Load the SAME save used in the l24 crash run."
  echo "  4. WAIT 30 s past save-load."
  echo "  5. Press L3+R3 ONCE (OPTIONS stays the game's pause)."
  echo "  6. CLEAN EXIT via in-game quit (never a power event). On crash:"
  echo "     note code + WHEN, harvest immediately, REBOOT before any relaunch."
  echo
  echo "in another terminal NOW:  $0 watch --console-ip $CONSOLE_IP"
  echo "after the run (before any relaunch):  $0 verdict --console-ip $CONSOLE_IP"
  exit 0
}

# ===========================================================================
# MODE: watch  (ladder watcher: new matches only, rung hit, runtime live)
# ===========================================================================
mode_watch() {
  local source="" interval=5 ip="$CONSOLE_IP"
  while [ $# -gt 0 ]; do
    case "$1" in
      --console-ip) shift; [ $# -gt 0 ] || usage; ip="$1" ;;
      --source) shift; [ $# -gt 0 ] || usage; source="$1" ;;
      --interval) shift; [ $# -gt 0 ] || usage; interval="$1" ;;
      *) usage ;;
    esac
    shift
  done
  case "$interval" in (*[!0-9]*|'') usage ;; esac
  [ "$interval" -ge 1 ] || usage

  local marker_re rung_re banner_re tmp norm statekey
  # marker set: task directive 2026-10-09 ([FN] tostring-bind / [FFO] /
  # db=v|n|i(e) / Menu created / Spawned / seed_refused / Scan failed);
  # rung set: harvest.py MOD_MENU_CREATED / MOD_SPAWNED shapes.
  marker_re='(\[FN\] tostring-bind|\[FFO\]|db=[vnie]|Menu created|Spawned |seed_refused|Scan failed)'
  rung_re='(Menu created|Spawned )'
  banner_re='UE4SS[[:space:]]*-?[[:space:]]*v[0-9]'   # harvest.py UE4SS_BANNER_RE

  local tmp="$FCTMP/watch-fetch.log" norm="$FCTMP/watch-normalized.log"
  if [ -n "$source" ]; then
    [ -f "$source" ] || { log "ABORT: --source file not found: $source"; exit 2; }
    log "watching local source (offline/test mode): $source (poll ${interval}s) -- Ctrl-C to stop"
  else
    log "watching ftp://$ip:2121$UE4SS_REMOTE_LOG every ${interval}s -- Ctrl-C to stop"
  fi
  log "legend: [FN] tostring-bind (EI bind state; 'disabled'=fell back) | [FFO] (DA/EB fix markers) | db=v/n/i/e (zzdiag v8 probe; v=database LIVE) | 'Menu created' (rung 2) | 'Spawned ' (rung 3, mission) | seed_refused = LOUD fail-closed, do NOT retry | Scan failed (scan-chain failure)"

  local offset=0 banners=0 size=0 blips=0

  fetch_snapshot() {
    if [ -n "$source" ]; then
      cp "$source" "$tmp" 2>/dev/null || return 1
    else
      curl -s --max-time 10 "ftp://$ip:2121$UE4SS_REMOTE_LOG" -o "$tmp" 2>/dev/null || return 1
    fi
    [ -s "$tmp" ] || return 1
    # wide-char writer lines carry NUL bytes (harvest.py zz rule): DECODE
    # per the dual-path writer grammar before any grep (agent GZ 2026-10-08;
    # the old tr -d '\000' hack is kept only as the RAW=1 / decoder-missing
    # fallback). Decoding into a second file keeps the raw bytes untouched.
    normalize_log "$tmp" "$norm"
  }

  # initial baseline: existing content is NOT replayed; only new lines print.
  if fetch_snapshot; then
    size=$(wc -c < "$norm" | tr -d ' ')
    offset=$size
    banners=$(grep -acE "$banner_re" "$norm" || true)
    log "baseline: ${size} bytes, ${banners} UE4SS session banner(s) already in the log (not replayed)"
  else
    log "baseline: source not fetchable yet; will keep trying"
    blips=1
  fi

  trap 'echo; log "watch stopped (offset $offset, banners $banners)"; exit 0' INT TERM
  while :; do
    sleep "$interval"
    if ! fetch_snapshot; then
      blips=$((blips + 1))
      [ $((blips % 6)) -eq 1 ] && log "(fetch failed x$blips -- source unreachable/empty; still watching)"
      continue
    fi
    blips=0
    size=$(wc -c < "$norm" | tr -d ' ')
    if [ "$size" -lt "$offset" ]; then
      log "(log shrank: $offset -> $size bytes; rebaselined, existing content not replayed)"
      offset=$size
    elif [ "$size" -gt "$offset" ]; then
      local new="$FCTMP/watch-newlines"
      tail -c +$((offset + 1)) "$norm" > "$new"
      offset=$size
      # NEW marker matches only (each byte range printed exactly once)
      grep -aE "$marker_re" "$new" | while IFS= read -r ln; do
        printf '[%s] %s\n' "$(date -u +%H:%M:%SZ)" "$ln"
        if printf '%s' "$ln" | grep -aqE "$rung_re"; then
          echo "  *** RUNG HIT *** (Menu created / Spawned -- ladder rung reached; keep the session, harvest before exit)"
        fi
      done
    fi
    local nb
    nb=$(grep -acE "$banner_re" "$norm" || true)
    if [ "$nb" -gt "$banners" ]; then
      local i
      for ((i = banners + 1; i <= nb; i++)); do
        echo "*** runtime live *** (new UE4SS session banner #$i -- injection worked; watch for markers)"
      done
      banners=$nb
    elif [ "$nb" -lt "$banners" ]; then
      banners=$nb
    fi
  done
}

# ===========================================================================
# MODE: verdict  (directive 3: verdict line + 3 key fields FIRST, full
#                 harvest report write backgrounded)
# ===========================================================================
mode_verdict() {
  local source="" ip="$CONSOLE_IP"
  while [ $# -gt 0 ]; do
    case "$1" in
      --console-ip) shift; [ $# -gt 0 ] || usage; ip="$1" ;;
      --source) shift; [ $# -gt 0 ] || usage; source="$1" ;;
      *) usage ;;
    esac
    shift
  done
  need_file "$HARVEST"

  local ts label ue4ss quick quicklog
  ts="$(date -u +%Y%m%dT%H%M%SZ)"
  label="auto-$ts"
  quick="$FCTMP/verdict-$ts"
  quicklog="$quick.quick.log"

  if [ -n "$source" ]; then
    [ -f "$source" ] || { log "ABORT: --source file not found: $source"; exit 2; }
    ue4ss="$source"
    log "verdict (offline/test mode): analyzing $source"
  else
    ue4ss="$quick.UE4SS.log"
    log "verdict: quick-pulling $UE4SS_REMOTE_LOG (single read-only FTP fetch)"
    if ! curl -s --show-error --fail --connect-timeout 5 --max-time 20 \
         -o "$ue4ss" "ftp://$ip:2121$UE4SS_REMOTE_LOG"; then
      log "VERDICT QUICK-PHASE FAILED: cannot fetch ftp://$ip:2121$UE4SS_REMOTE_LOG (console unreachable?). No full harvest launched."
      log "  remediation: check the console/ftpsrv, or re-run with --source <saved-log> for offline analysis."
      exit 1
    fi
    [ -s "$ue4ss" ] || { log "VERDICT QUICK-PHASE FAILED: fetched UE4SS.log is empty"; exit 1; }
  fi

  # quick verdict: harvest.py's own verified analysis, analysis-only mode
  if ! python3 "$HARVEST" --label "${label}-quick" --local-ue4ss "$ue4ss" \
         --skip-klog > "$quicklog" 2>&1; then
    log "VERDICT QUICK-PHASE FAILED: harvest.py --local-ue4ss errored -- see $quicklog"
    cp "$quicklog" "$EVCF/" 2>/dev/null || true   # HA 2026-10-10: durable
    exit 1
  fi
  cp "$quicklog" "$ue4ss" "$EVCF/" 2>/dev/null || true  # HA: durable receipts
  local vline db menu spawn
  # quick.log carries NUL bytes from the raw console log lines -- grep -a
  vline="$(grep -a -m1 '^\[harvest\] verdict:' "$quicklog")"
  # NB: database-live? is nested under the diag-zz block (leading spaces in
  # the report); menu/spawn lines are at column 0.
  db="$(sed -n 's/^[[:space:]]*- \*\*database-live?\*\* \([A-Za-z]*\).*/\1/p' "$quicklog" | head -1)"
  menu="$(sed -n 's/^[[:space:]]*- \*\*mod menu opened?\*\* \([A-Za-z]*\).*/\1/p' "$quicklog" | head -1)"
  spawn="$(sed -n 's/^[[:space:]]*- \*\*item spawned?\*\* \([A-Za-z]*\).*/\1/p' "$quicklog" | head -1)"
  echo
  echo "=================== VERDICT ($label) ==================="
  echo "$vline"
  echo "key: db=${db:-n/a} menu=${menu:-n/a} spawn=${spawn:-n/a}"
  echo "========================================================"

  # directive 3: the FULL report write runs in the background now
  local fulllog="$quick.full.log" bgpid
  if [ -n "$source" ]; then
    nohup python3 "$HARVEST" --label "$label" --local-ue4ss "$source" \
        --skip-klog > "$fulllog" 2>&1 &
    bgpid=$!
    log "full analysis backgrounded (offline mode; report -> $fulllog): pid $bgpid"
  else
    nohup python3 "$HARVEST" --label "$label" --console-ip "$ip" \
        > "$fulllog" 2>&1 &
    bgpid=$!
    log "full harvest backgrounded: pid $bgpid (log: $fulllog)"
    log "full report will land at: $HARVEST_DIR/HARVEST-$label.md"
  fi
  log "quick analysis detail (if needed): $quicklog"
  disown "$bgpid" 2>/dev/null || true
  exit 0
}

# ===========================================================================
# MODE: next  (directive 4: kick the NEXT build in background; row check
#              first; NO console contact)
# ===========================================================================
mode_next() {
  local dry=0
  while [ $# -gt 0 ]; do
    case "$1" in
      --dry-check) dry=1 ;;
      *) usage ;;
    esac
    shift
  done
  need_file "$L26/build_variants.py"
  need_file "$L26/l26_packaging_driver.py"

  log "next: CHANGES files[] row check (bind_launch preflight gate-2 mirror: profile/typescan-r4/thread vs disk)"
  local rowout rc
  rowout="$(python3 - "$L26" <<'PYEOF'
import hashlib, json, os, sys
L26 = sys.argv[1]
def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()
rows_seen, stale = 0, []
for part in ("profile", "typescan-r4", "thread"):
    p = os.path.join(L26, part, "CHANGES.json")
    if not os.path.isfile(p):
        stale.append("%s CHANGES.json MISSING: %s" % (part, p)); continue
    d = json.load(open(p))
    for i, row in enumerate(d["files"]):
        rows_seen += 1
        for role, path, declared in (
                ("candidate", row["candidate"], row["candidate_sha256"]),
                ("original", row["original"], row["original_sha256"])):
            if not os.path.exists(path):
                stale.append("%s files[%d] %s %s MISSING on disk: %s"
                             % (part, i, os.path.basename(path), role, path))
            elif sha(path) != declared:
                stale.append("%s files[%d] %s %s STALE: declared %s vs disk %s"
                             % (part, i, os.path.basename(path), role,
                                declared[:12], sha(path)[:12]))
if stale:
    print("STALE %d" % len(stale))
    for s in stale:
        print("STALE-ROW " + s)
else:
    print("OK %d" % rows_seen)
PYEOF
)" ; rc=$?
  [ $rc -ne 0 ] && { log "next: row check crashed (python rc $rc)"; exit 4; }

  if head -1 <<<"$rowout" | grep -q '^STALE'; then
    log "next: STALE CHANGES rows -- build NOT launched; the row owners must refresh them (build_variants gate G2 would hard-fail anyway):"
    sed -n 's/^STALE-ROW /  /p' <<<"$rowout"
    log "refresh the rows to match disk, then re-run: $0 next"
    exit 4
  fi
  log "  rows clean: $(head -1 <<<"$rowout" | sed 's/^OK //') files[] rows (candidate+original) match disk"

  local ts arch runner logf pidfile
  ts="$(date -u +%Y%m%dT%H%M%SZ)"
  arch="$PAIR_ROOT/archive-$ts"
  logf="$FCTMP/next-$ts.log"
  pidfile="$FCTMP/next.pid"

  # double-kick guard: never two concurrent build chains (the archive step
  # moves the same dirs; a second chain would corrupt the first)
  if [ -f "$pidfile" ]; then
    local oldpid
    oldpid="$(cat "$pidfile" 2>/dev/null)"
    if [ -n "$oldpid" ] && kill -0 "$oldpid" 2>/dev/null; then
      log "next: a build chain is ALREADY running (pid $oldpid; logs: $FCTMP/next-*.log) -- refusing a double kick"
      log "  tail -f $FCTMP/next-*.log ; when 'NEXT PRX READY' appears: $0 prep --prx $PRX_A"
      exit 4
    fi
  fi

  if [ "$dry" -eq 1 ]; then
    log "next --dry-check: rows clean; WOULD run in background (log $logf):"
    echo "  1. archive: l26-A l26-B staging-copyin-probe package-A package-B -> $arch (drivers refuse non-fresh dirs)"
    echo "  2. (cd $WHAT && python3 $L26/build_variants.py --execute)"
    echo "  3. (cd $WHAT && python3 $L26/l26_packaging_driver.py --execute)"
    echo "  4. checks: twin receipts + FINAL-RECEIPT-VARIANT-A + PRX magic 4f153d1d/5414f5ee + not byte-identical to baseline 5ea2c845"
    echo "  5. final line: NEXT PRX READY: $PRX_A -> then: $0 prep --prx $PRX_A"
    log "no build launched, no console contact (dry check)"
    exit 0
  fi

  runner="$FCTMP/next-$ts.runner.sh"
  cat > "$runner" <<RUNEOF
#!/bin/bash
# fast_cycle.sh next background chain (agent-EN; mirrors bind_launch.sh steps
# 1-3 exactly: archive -> build -> package -> artifact checks). NO console
# contact. Generated $(date -u +%Y-%m-%dT%H:%M:%SZ).
set -o pipefail
WHAT="$WHAT"; L26="$L26"; PAIR_ROOT="$PAIR_ROOT"; PKG_A="$PKG_A"; PKG_B="$PKG_B"; PRX_A="$PRX_A"
ARCH="$arch"
# HA 2026-10-10 evidence law: chain log lands durable on EVERY exit path
LOGF="$logf"; EVCF_OUT="$EVCF"
trap 'cp "$LOGF" "$EVCF_OUT/" 2>/dev/null || true' EXIT
say() { printf '[%s] %s\n' "\$(date -u +%H:%M:%SZ)" "\$*"; }

say "NEXT 1/4: archive prior build/package dirs (drivers refuse non-fresh dirs)"
mkdir -p "\$ARCH"
for d in "\$PAIR_ROOT/l26-A" "\$PAIR_ROOT/l26-B" "\$PAIR_ROOT/staging-copyin-probe" "\$PKG_A" "\$PKG_B"; do
  if [ -e "\$d" ]; then mv "\$d" "\$ARCH/\$(basename "\$d")" && say "  archived \$(basename "\$d")"; fi
done
[ -e "\$PKG_A" ] && { say "ABORT: could not archive \$PKG_A -- resolve manually, never let the packager overwrite"; exit 6; }

say "NEXT 2/4: build_variants.py --execute (twin A+B, cwd=what/)"
( cd "\$WHAT" && python3 "\$L26/build_variants.py" --execute ) || { say "ABORT: build failed -- gate failures are printed as JSON 'gate' lines above"; exit 5; }
for f in "\$PAIR_ROOT/l26-A/TWIN-RECEIPT.json" "\$PAIR_ROOT/l26-B/TWIN-RECEIPT.json" "\$PAIR_ROOT/VARIANT-PAIR-RECEIPT.json"; do
  [ -f "\$f" ] || { say "ABORT: build receipt missing: \$f"; exit 5; }
done
say "  build OK (twin + pair receipts present)"

say "NEXT 3/4: l26_packaging_driver.py --execute"
( cd "\$WHAT" && python3 "\$L26/l26_packaging_driver.py" --execute ) || { say "ABORT: packaging failed"; exit 6; }
[ -f "\$PKG_A/FINAL-RECEIPT-VARIANT-A.json" ] || { say "ABORT: FINAL-RECEIPT-VARIANT-A.json missing"; exit 6; }
[ -f "\$PRX_A" ] || { say "ABORT: packaged PRX missing: \$PRX_A"; exit 6; }

say "NEXT 4/4: artifact checks"
NEWSHA=\$(shasum -a 256 "\$PRX_A" | cut -d' ' -f1)
MAGIC=\$(xxd -l 4 -p "\$PRX_A")
say "  PRX-A sha256=\$NEWSHA magic=\$MAGIC"
case "\$MAGIC" in
  4f153d1d|5414f5ee) say "  PRX magic = signed SELF ok" ;;
  *) say "ABORT: PRX magic \$MAGIC is NOT a signed self -- DO NOT upload (PS5 rule 2); run make_fself path"; exit 6 ;;
esac
if [ "\$NEWSHA" = "$BASELINE_PRX_SHA" ]; then
  say "ABORT: new PRX is byte-identical to the ALREADY-TESTED 5ea2c845 baseline -- staged changes did not reach the artifact; do not re-launch the same build"
  exit 6
fi
say "NEXT PRX READY: \$PRX_A"
say "next step: $0 prep --prx \$PRX_A   (console gates + upload + arm, banner on success)"
say "NEXT DONE (archive: \$ARCH)"
rm -f "$pidfile"
exit 0
RUNEOF
  chmod +x "$runner"
  nohup bash "$runner" > "$logf" 2>&1 &
  local bgpid=$!
  echo "$bgpid" > "$pidfile"
  disown "$bgpid" 2>/dev/null || true
  log "next build+package chain backgrounded: pid $bgpid"
  log "  log: $logf  (tail -f $logf)"
  log "  when the log shows 'NEXT PRX READY': $0 prep --prx $PRX_A"
  log "  no console contact in this chain; prep will re-check console gates."
  exit 0
}

# ===========================================================================
# dispatch
# ===========================================================================
[ $# -eq 0 ] && usage
MODE="$1"; shift
case "$MODE" in
  prep)   mode_prep "$@" ;;
  watch)  mode_watch "$@" ;;
  verdict) mode_verdict "$@" ;;
  next)   mode_next "$@" ;;
  *) usage ;;
esac
