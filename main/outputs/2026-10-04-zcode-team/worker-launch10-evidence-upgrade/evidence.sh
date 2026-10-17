#!/bin/sh
# evidence.sh — launch-10 (shortest-config) post-launch evidence collector.
# worker-launch10-evidence-upgrade · 2026-10-04 · host-side; NO console contact
# except when the CONTROLLER runs live mode after the user confirms "in game".
#
# Upgraded from worker-launch9-evidence/evidence.sh (kept unmodified as
# labeled history). Launch-9 gaps fixed here:
#   G1 stale embedded baselines (2892 B was a 411 B error page; the entry
#      2210306 B was mid-constructor)  -> baselines are CLI flags DEFAULTING to
#      the fresh controller-captured launch10-prep files, plus --baseline-dir
#      reference files with a size+sha256+head-window drift guard.
#   G2 attribution ambiguity (two plugins shared the aligned-entry mirror)
#      -> launch-10 runs the SHORTEST config (only supplier_composition +
#      path_test enabled; ini sha 1216aab6...), so every modbridge failure
#      line now belongs to supplier_composition; growth of DISABLED channels
#      (aligned-entry log, xwp jsonl, DIAG-line plugin prints) is a loud
#      CONFIG-DRIFT warning, not silently classified evidence.
#   G3 no timing extraction -> per-writer timing sequences with explicit
#      "not comparable across writers" guards (r4 [t=<ms>ms pid=<pid>]
#      prefixes, PATH_TEST '... at <ns> monotonic-ns' stamps, supplier-diag
#      jsonl t_ms events, file-level sequence order).
# New: r13/r14 marker->stage classification (STAGE TABLE), launch-10 staging
# runbook via --plan.
#
# Rev 2026-10-04 (worker-evidence-gap-fix), per verdict-rehearsal RUN-RECEIPT:
#   GAP-1 lmf_extent_base_low/high + lmf_extent_end_low/high + lmf_anchor_count
#      + lmf_intervals_found now stage S6_CONVERSION in BOTH classifiers (the
#      success chains include conversion, not just the D-style refusal fan-out)
#      and anchor_count/intervals_found are carried in SDIAGSUM next to the
#      lease spans (controller predicted-pair check: anchor_count=7,
#      intervals_found=1 next to lease_spans=6).
#   GAP-2 the BEST-decisive [TownfallItemSpawner] lines are printed verbatim
#      (MODLOADLINE, capped 6) in a dedicated mod-load prints section, so
#      G2 (load prints) vs G3 (mod-error lines, also BEST) is decidable from
#      the one-pager without opening the raw log.
#   GAP-3 first-seen chains may read S7 before S4/S5 because bridge_exepath_source
#      is emitted mid-walk by the bridge; annotated as a display quirk, order
#      itself is NOT reordered (first-seen order remains the honest record).
#
# Rev 2026-10-05 (worker-evidence-gap-fix-2), per the FIRST REAL launch-10
# classification (worker-verdict-first-real LAUNCH10-RESULTS.md §5, gaps G1-G9;
# the three real runs are the regression corpus — rerun after this patch):
#   G1 verdict-name fidelity: the two refusal classes the real runs exposed are
#      now named with the oracle's own vocabulary — FAILED_E2_START (oracle E2:
#      runtime_start_failed code=3, EXPECTED-REFUSAL, verdict >= MINIMAL-ACCEPT)
#      and FAILED_B7_MFA (oracle B7: mfa_activate_failed, EXPECTED-REFUSAL, FAIL
#      with named cause, nothing downstream ran). When a code=3 refusal coexists
#      with the current session's sdiag conversion quartet (all 4 lmf_extent
#      events + anchor_count>0 + intervals_found>0 + cfg events), no D6 line, no
#      crash tail and a fresh failed status whose detail matches, the verdict is
#      MINIMAL_ACCEPT_E2_REFUSAL (E4 'activated' is not literally on file — the
#      terminal-write O_TRUNC caveat is printed with the verdict). ORACLE-CLASS
#      lines name the row on the one-pager and in the JSON tail. Bare
#      FAILED_code<N> remains the fallback for unpinned codes.
#   G2 sdiag per-session split: the jsonl is append-only across a whole console
#      boot, so events are grouped PER PID (session). SDIAGSUM is now the
#      file-total line (events/sessions/pid_first/pid_last; value tags are the
#      LAST session's) and each session gets a PIDSUM line (per-session values:
#      start_ok/start_failed, mfa/runtime_start_failed codes, extent_events,
#      the predicted pair anchor_count/intervals_found next to lease_spans/reg,
#      vmentry/walk_stop, cfg, spanpub, stat_ownmod, module_start_return) plus a
#      PIDCHAIN first-seen chain and a PIDJSON record for the JSON tail. STIME
#      deltas are computed ONLY within one pid — never across sessions. Optional
#      --baseline-sdiag-bytes / --baseline-dir modbridge-supplier-diag-baseline.
#      txt cut (launch-10 pinned NO sdiag baseline: cut defaults OFF; the
#      per-pid split scopes cumulative files regardless).
#   G3 the JSON tail carries the sdiag-derived fields (sdiag object with the
#      current session's quartet/predicted pair + per-session records), so
#      JSON-only consumers see the evidence that achieved MINIMAL-ACCEPT.
#   G4 note: S11_STOP after a failure line is module_start_return=0 ("entry
#      returned 0, game continues UNMODDED"), annotated — not re-staged.
#   G5 --sdiag FILE alias for fixtures whose diag file is not named
#      modbridge-supplier-diag.jsonl (documented-placement operator trap).
#   G6 failure lines are grouped by their r4 pid and the cumulative-window
#      caveat is printed (PATH_TEST stamps carry no pid; verdict = LAST
#      failure line, append-only ordering discipline).
#   G7 note only: current-session vmentry > pinned kBridgeVmmapEntries=16384
#      with walk_stop=0 is reported as a diag-note (oracle C1/S3 reconciliation
#      question); no verdict impact. JSON: vm_capacity object.
#   G8 note only: exepath_source=0 WITH a successful r10 fork (stat_ownmod=1)
#      and conversion proceeding is annotated — the fork refuses only when ALL
#      THREE stats fail; oracle C7 wording is looser than live behavior.
#   G9 note only: spanpub env pair printed as the 64-bit low/high split +
#      spanpub_cap=1 saturation note (both real boots saturated at 16).
#
# Evidence sources (console):
#   ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log                (append-only mirror; PRIMARY)
#   ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/runtime-status.txt (session/pid-stamped, last-writer-wins; PRIMARY)
#   ftp://ps5-host-not-configured.invalid:2121/data/townfall_runtime_aligned_entry.log (HISTORY: aligned_entry disabled this config)
#   ftp://ps5-host-not-configured.invalid:2121/data/modbridge-exec-write-probe.jsonl  (xwp; DISABLED this config — drift guard)
#   ftp://ps5-host-not-configured.invalid:2121/data/modbridge-supplier-diag.jsonl     (OPTIONAL: diagnostic-sink; absence is normal)
#
# Launch-9 history note (labeled, never used as launch-10 baselines):
#   UE4SS.log 2892 B "baseline" was INVALID — the fetch returned a 411 B error
#   page. aligned-entry 2210306 B was the clean attempt boundary (worker-
#   launch9-diagnostics-mine). Fresh launch-10 baselines (launch10-prep):
#   ue4ss-log-baseline.txt 7591 B edf404a5..., entry-log-baseline.txt 2343756 B
#   4b33b985..., runtime-status-baseline.txt 169 B 57c40d4c...,
#   xwp-jsonl-baseline.txt 4362 B 20cff601....
#
# Known launch-9 terminal (stage table anchor): code=1 'no observed mapping
# interval intersects the module image extent with valid geometry' at
# derive_readable_ranges (lmf_to_native_loader_facts.cpp:228-231). The PLH
# lease throw 'Owned read range includes unregistered bytes' is DORMANT in
# r13 (launch-8's r10 threw it, pid 234); r14 replaces it with the loud
# pre-lease coverage gate ('lease coverage gap: requested [0x... +0x...)
# outside registered readable spans', MRS_ERR_CONVERSION by design).
#
# Exit codes: 0 classified, 2 usage error, 4 FETCH_FAILED (both primary
# channels unreachable — retry guidance).
#
# Modes: live FTP (controller only) | --from-dir DIR (host fixtures:
# UE4SS.log, runtime-status.txt, townfall_runtime_aligned_entry.log,
# modbridge-exec-write-probe.jsonl, modbridge-supplier-diag.jsonl optional) |
# --dry-run (print curl commands) | --plan (launch-10 staging runbook) |
# --json-tail | --out FILE. POSIX sh + awk; shasum or sha256sum for the
# drift guard; no other dependencies.

set -u

HOST=ps5-host-not-configured.invalid
PORT=2121
TIMEOUT=60
MAX_BYTES=$((32 * 1024 * 1024))
# Launch-10 pinned baselines (controller-captured 2026-10-05, launch10-prep).
BASE_UE4SS=7591        # ue4ss-log-baseline.txt   edf404a5...
BASE_ENTRY=2343756     # entry-log-baseline.txt   4b33b985...
BASE_STATUS=169        # runtime-status-baseline.txt 57c40d4c... (session 273)
BASE_XWP=4362          # xwp-jsonl-baseline.txt   20cff601...
PIN_UE4SS_SHA=edf404a56d0a9ed85e15d2b20a00bdd07e93b189d4e70a9db988d46acd22a0ff
PIN_ENTRY_SHA=4b33b9852ef53d714b32e839db2c927b5c79814821d76ac383295e487941129f
PIN_STATUS_SHA=57c40d4cf7e91c8ac73fd726b3a268e7b613ee3b202459d8d181db7b79f51c4b
PIN_XWP_SHA=20cff60141e356c2a30822c8e37d5225430c8cb487b9b3311e1115499339c2fe
# The runtime-status baseline session (launch-9 terminal, pid 273). A fetched
# status holding a DIFFERENT session is a fresh post-launch window.
BASE_SESS_PID=273
BASE_SESS_MS=44372083
FROM_DIR=""
BASELINE_DIR=""
SDIAG_ARG=""
BASE_SDIAG=0
DRY_RUN=0
PLAN=0
OUT_FILE=""

usage() {
  cat <<'USAGE'
usage: evidence.sh [--host H] [--port P] [--timeout S] [--max-bytes N]
                   [--baseline-ue4ss-bytes N] [--baseline-entry-bytes N]
                   [--baseline-status-bytes N] [--baseline-xwp-bytes N]
                   [--baseline-session-pid N] [--baseline-session-ms N]
                   [--baseline-sdiag-bytes N]
                   [--baseline-dir DIR]
                   [--from-dir DIR] [--sdiag FILE]
                   [--dry-run] [--plan] [--out FILE] [--json-tail]
  --baseline-*-bytes N      pinned pre-launch baseline sizes (launch10-prep
                            defaults: ue4ss 7591, entry 2343756, status 169,
                            xwp 4362). --baseline-aligned-bytes is kept as a
                            deprecated alias of --baseline-entry-bytes.
  --baseline-session-*      runtime-status baseline session (273/44372083);
                            a different session = fresh post-launch window.
  --baseline-dir DIR        local launch10-prep-layout reference files
                            (ue4ss-log-baseline.txt, entry-log-baseline.txt,
                            runtime-status-baseline.txt, xwp-jsonl-baseline.txt).
                            Enables the drift guard: pinned size/sha check of
                            the baselines themselves + head-window byte compare
                            of each fetched file against its baseline.
  --baseline-sdiag-bytes N  optional append-only baseline cut for the supplier-
                            diag jsonl. Launch-10 pinned NO sdiag baseline (sink
                            absence is normal), so this defaults to 0 (OFF);
                            per-pid session split scopes cumulative files
                            regardless. A modbridge-supplier-diag-baseline.txt
                            in --baseline-dir also enables the cut.
  --from-dir DIR            read fixtures from a local directory (NO FTP)
  --sdiag FILE              explicit path to the supplier-diag jsonl (from-dir
                            mode) when the fixture is not named
                            modbridge-supplier-diag.jsonl (G5 alias)
  --dry-run                 print the exact curl commands and exit
  --plan                    print the launch-10 staging runbook and exit
  --out FILE                also write the one-page summary to FILE
  --json-tail               append a machine-readable JSON object after the summary
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --max-bytes) MAX_BYTES="$2"; shift 2 ;;
    --baseline-ue4ss-bytes) BASE_UE4SS="$2"; shift 2 ;;
    --baseline-entry-bytes) BASE_ENTRY="$2"; shift 2 ;;
    --baseline-aligned-bytes) BASE_ENTRY="$2"; shift 2 ;;  # deprecated alias
    --baseline-status-bytes) BASE_STATUS="$2"; shift 2 ;;
    --baseline-xwp-bytes) BASE_XWP="$2"; shift 2 ;;
    --baseline-session-pid) BASE_SESS_PID="$2"; shift 2 ;;
    --baseline-session-ms) BASE_SESS_MS="$2"; shift 2 ;;
    --baseline-sdiag-bytes) BASE_SDIAG="$2"; shift 2 ;;
    --baseline-dir) BASELINE_DIR="$2"; shift 2 ;;
    --from-dir) FROM_DIR="$2"; shift 2 ;;
    --sdiag) SDIAG_ARG="$2"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --plan) PLAN=1; shift ;;
    --out) OUT_FILE="$2"; shift 2 ;;
    --json-tail) JSON_TAIL=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) echo "evidence.sh: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

UE4SS_PATH=/data/plugins/UE4SS.log
STATUS_PATH=/data/modbridge/roots/PPSA33286/runtime-status.txt
ENTRY_PATH=/data/townfall_runtime_aligned_entry.log
XWP_PATH=/data/modbridge-exec-write-probe.jsonl
SDIAG_PATH=/data/modbridge-supplier-diag.jsonl

if [ "$PLAN" = 1 ]; then
  cat <<'PLAN'
== launch-10 staging runbook (shortest config; controller only; no console contact from workers) ==

Configuration truth (controller-verified 2026-10-05):
  /data/plugins/ploader.ini = SHORTEST 2-plugin config, sha 1216aab61236b0fc9734b1647c96e03db48beb3d333efe174ca08e97abededa0
    modbridge_supplier_composition.prx = 2 (rebuilt fself: r14 lease fix + armed SeedTable + GMalloc V1/V2 validator)
    modbridge_path_test.prx            = 2 (timing stamps ride along; NO separate diagnostic launch)
    aligned_entry and ALL probes DISABLED (=0), incl. xwp -> the aligned-entry
    log and the xwp jsonl must NOT grow this launch; growth = config drift.
  Primary evidence channels: /data/plugins/UE4SS.log (append-only mirror) and
  /data/modbridge/roots/PPSA33286/runtime-status.txt (session/pid-stamped,
  last-writer-wins; O_TRUNC per session).

0. Preconditions (game CLOSED): stray-root disposition reconciled (package_prx
   root shadow), gate v2 GO, candidate fself sha recorded. ini NOT republished.
1. Single-file upload of the rebuilt supplier_composition PRX (filenames unchanged):
     curl -T <candidate>.prx ftp://ps5-host-not-configured.invalid:2121/data/plugins/modbridge_supplier_composition.prx
   readback: fresh RETR of the staged path; byte-verify vs the candidate
   (content law: all PT_LOADs identical; SHT-strip artifacts only). ONE file
   only — no ini touch, no settings touch (settings v3 d8faa4ec..., loadlist
   4d98ecd7... stay).
2. ini pre-image gate (no write): fetch ploader.ini, expect sha 1216aab6...
   STOP on drift (32d0c0d9... / 8d331b3f... / fd9c7393... are history).
3. Baselines re-fetch BEFORE launch (this replaces launch-9's stale 2892 B
   error-page incident): fetch all four baseline channels into a
   launch10-prep-layout directory:
     UE4SS.log                 -> ue4ss-log-baseline.txt      (expect 7591 B edf404a5...)
     townfall_runtime_aligned_entry.log -> entry-log-baseline.txt (2343756 B 4b33b985...)
     runtime-status.txt        -> runtime-status-baseline.txt (169 B 57c40d4c... session 273)
     modbridge-exec-write-probe.jsonl   -> xwp-jsonl-baseline.txt (4362 B 20cff601...)
   Then classify pre-launch once: evidence.sh --from-dir <dir> --baseline-dir <dir>
   must say NOT_STARTED with ZERO warnings (that is the drift-free proof).
4. Launch. Timing rides along: PATH_TEST monotonic-ns stamps (path_test
   plugin) + r4 [t=<ms>ms pid=<pid>] failure prefixes + supplier-diag jsonl
   t_ms events (if the diagnostic-sink landed in the rebuild). No separate
   diagnostic launch.
5. Post-launch fetch order (one fresh curl connection each):
     a. /data/plugins/UE4SS.log
     b. /data/modbridge/roots/PPSA33286/runtime-status.txt
     c. /data/modbridge-supplier-diag.jsonl   (only if the diagnostic-sink
        landed; absence is a normal outcome, not a failure)
     d. /data/modbridge-exec-write-probe.jsonl (drift guard only)
     e. /data/townfall_runtime_aligned_entry.log (history guard only)
6. Classify: evidence.sh --from-dir <fetched-dir> --baseline-dir <baseline-dir>
   Read VERDICT + stage chain + timing table. Any CONFIG-DRIFT warning
   (entry/xwp growth, DIAG lines) -> stop and reconcile before interpreting
   the composition evidence.
PLAN
  exit 0
fi

# ---------------------------------------------------------------------------
# Fetch layer
# ---------------------------------------------------------------------------
fetch_file() { # $1=remote path  $2=local output  -> 0 ok / nonzero fail (err in $ERRV)
  if command curl >/dev/null 2>&1; then :; else ERRV="curl not found on PATH"; return 127; fi
  ERRV=$(curl -fsS --connect-timeout 10 --max-time "$TIMEOUT" \
        --max-filesize "$MAX_BYTES" "ftp://$HOST:$PORT$1" -o "$2" 2>&1)
  return $?
}

# New-line extraction: a line is NEW iff its start offset >= baseline bytes.
# Assumes LF newlines (CR stripped later); conservative round-up at the
# boundary so no baseline content leaks into the new-line set.
extract_new_lines() { # $1=file  $2=baseline bytes  -> new lines on stdout
  awk -v cut="$2" 'BEGIN { pos = 0 }
    { if (pos >= cut) print $0; pos += length($0) + 1 }' "$1" | tr -d '\r'
}

sha256_of() { # $1=file -> hex sha256 (shasum or sha256sum)
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | awk '{print $1}'
  elif command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'
  else echo "unavailable"; fi
}
sha256_stdin() { # stdin -> hex sha256 (command -v: never touch stdin while probing)
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 | awk '{print $1}'
  elif command -v sha256sum >/dev/null 2>&1; then sha256sum | awk '{print $1}'
  else echo "unavailable"; fi
}

TMPDIR_E=""
cleanup() { [ -n "$TMPDIR_E" ] && rm -rf "$TMPDIR_E"; }
trap cleanup EXIT INT TERM
TMPDIR_E=$(mktemp -d "${TMPDIR:-/tmp}/evidence-launch10.XXXXXX") || exit 4

if [ "$DRY_RUN" = 1 ]; then
  for p in "$UE4SS_PATH" "$STATUS_PATH" "$ENTRY_PATH" "$XWP_PATH" "$SDIAG_PATH"; do
    echo "curl -fsS --connect-timeout 10 --max-time $TIMEOUT --max-filesize $MAX_BYTES ftp://$HOST:$PORT$p"
  done
  exit 0
fi

UE4SS_F="$TMPDIR_E/UE4SS.log"
STATUS_F="$TMPDIR_E/runtime-status.txt"
ENTRY_F="$TMPDIR_E/townfall_runtime_aligned_entry.log"
XWP_F="$TMPDIR_E/modbridge-exec-write-probe.jsonl"
SDIAG_F="$TMPDIR_E/modbridge-supplier-diag.jsonl"

ok_ue4ss=0; ok_status=0; ok_entry=0; ok_xwp=0; ok_sdiag=0
err_ue4ss=""; err_status=""; err_entry=""; err_xwp=""; err_sdiag=""
src_all=ftp

if [ -n "$FROM_DIR" ]; then
  src_all=dir
  [ -f "$FROM_DIR/UE4SS.log" ]                          && cp "$FROM_DIR/UE4SS.log" "$UE4SS_F"   && ok_ue4ss=1
  [ -f "$FROM_DIR/runtime-status.txt" ]                 && cp "$FROM_DIR/runtime-status.txt" "$STATUS_F" && ok_status=1
  [ -f "$FROM_DIR/townfall_runtime_aligned_entry.log" ] && cp "$FROM_DIR/townfall_runtime_aligned_entry.log" "$ENTRY_F" && ok_entry=1
  [ -f "$FROM_DIR/modbridge-exec-write-probe.jsonl" ]   && cp "$FROM_DIR/modbridge-exec-write-probe.jsonl" "$XWP_F" && ok_xwp=1
  # G5: --sdiag FILE wins when given; otherwise the documented fixture name.
  if [ -n "$SDIAG_ARG" ]; then
    [ -f "$SDIAG_ARG" ] && cp "$SDIAG_ARG" "$SDIAG_F" && ok_sdiag=1
  elif [ -f "$FROM_DIR/modbridge-supplier-diag.jsonl" ]; then
    cp "$FROM_DIR/modbridge-supplier-diag.jsonl" "$SDIAG_F" && ok_sdiag=1
  fi
  [ "$ok_ue4ss" = 1 ]   || err_ue4ss="fixture missing: $FROM_DIR/UE4SS.log"
  [ "$ok_status" = 1 ]  || err_status="fixture missing: $FROM_DIR/runtime-status.txt"
  [ "$ok_entry" = 1 ]   || err_entry="fixture missing: $FROM_DIR/townfall_runtime_aligned_entry.log"
  [ "$ok_xwp" = 1 ]     || err_xwp="fixture missing: $FROM_DIR/modbridge-exec-write-probe.jsonl"
  [ "$ok_sdiag" = 1 ]   || err_sdiag="absent (normal unless the diagnostic-sink landed)"
else
  fetch_file "$UE4SS_PATH" "$UE4SS_F"   || { err_ue4ss="$ERRV (curl exit $?)"; }
  [ -s "$UE4SS_F" ] && ok_ue4ss=1
  fetch_file "$STATUS_PATH" "$STATUS_F" || { err_status="$ERRV (curl exit $?)"; }
  [ -s "$STATUS_F" ] && ok_status=1
  fetch_file "$ENTRY_PATH" "$ENTRY_F"   || { err_entry="$ERRV (curl exit $?)"; }
  [ -s "$ENTRY_F" ] && ok_entry=1
  fetch_file "$XWP_PATH" "$XWP_F"       || { err_xwp="$ERRV (curl exit $?)"; }
  [ -s "$XWP_F" ] && ok_xwp=1
  # supplier diag jsonl: absence is EXPECTED when the sink did not land.
  fetch_file "$SDIAG_PATH" "$SDIAG_F"   || { err_sdiag="$ERRV (curl exit $?)"; }
  [ -s "$SDIAG_F" ] && ok_sdiag=1
  [ "$ok_sdiag" = 0 ] && err_sdiag="absent on the console (normal unless the diagnostic-sink landed)"
fi

# Both PRIMARY channels unreachable -> FETCH_FAILED.
if [ "$ok_ue4ss" = 0 ] && [ "$ok_status" = 0 ]; then
  {
    echo "== launch-10 evidence =="
    echo "fetch UE4SS.log       FAIL ($err_ue4ss)"
    echo "fetch runtime-status  FAIL ($err_status)"
    echo ""
    echo "VERDICT: FETCH_FAILED"
    echo "NEXT: RETRY — records appear on FTP ~1 min after launch; wait ~60 s and re-run. If a second retry still fails, check the console FTP service/network; do NOT touch staging on this signature."
  } | tee "${OUT_FILE:-/dev/null}"
  exit 4
fi

# ---------------------------------------------------------------------------
# Baseline drift guard (launch-9's stale-baseline incident guard)
# ---------------------------------------------------------------------------
WARNF="$TMPDIR_E/warn.txt"; : > "$WARNF"

check_baseline_ref() { # $1=ref file  $2=pinned size  $3=pinned sha  $4=label
  if [ -n "$BASELINE_DIR" ] && [ -f "$BASELINE_DIR/$1" ]; then
    rsz=$(wc -c < "$BASELINE_DIR/$1" | tr -d ' ')
    [ "$rsz" = "$2" ] || echo "baseline drift: $BASELINE_DIR/$1 is ${rsz}B, pinned launch10-prep size is $2 ($4) — re-fetch baselines before classifying" >> "$WARNF"
    rsha=$(sha256_of "$BASELINE_DIR/$1")
    [ "$rsha" = "$3" ] || echo "baseline drift: $BASELINE_DIR/$1 sha ${rsha%${rsha#????????}}... != pinned $3 ($4) — baselines are NOT the pinned launch10-prep capture" >> "$WARNF"
  fi
}
check_baseline_ref ue4ss-log-baseline.txt      "$BASE_UE4SS"   "$PIN_UE4SS_SHA"   ue4ss
check_baseline_ref entry-log-baseline.txt      "$BASE_ENTRY"   "$PIN_ENTRY_SHA"   entry
check_baseline_ref runtime-status-baseline.txt "$BASE_STATUS"  "$PIN_STATUS_SHA"  status
check_baseline_ref xwp-jsonl-baseline.txt      "$BASE_XWP"     "$PIN_XWP_SHA"     xwp

# G2: optional sdiag baseline cut. Launch-10 pinned NO sdiag baseline (sink
# absence is normal), so the cut defaults OFF; enable via --baseline-sdiag-bytes
# or by dropping modbridge-supplier-diag-baseline.txt into --baseline-dir.
# (Size-only reference: there is no pinned sdiag sha to drift-guard.)
if [ "$BASE_SDIAG" -eq 0 ] && [ -n "$BASELINE_DIR" ] && [ -s "$BASELINE_DIR/modbridge-supplier-diag-baseline.txt" ]; then
  BASE_SDIAG=$(wc -c < "$BASELINE_DIR/modbridge-supplier-diag-baseline.txt" | tr -d ' ')
fi

head_window_drift() { # $1=fetched file  $2=baseline bytes  $3=ref file  $4=label
  [ -n "$BASELINE_DIR" ] && [ -f "$BASELINE_DIR/$3" ] && [ -f "$1" ] || return 0
  fbytes=$(wc -c < "$1" | tr -d ' ')
  n="$2"
  [ "$fbytes" -lt "$n" ] && n="$fbytes"
  [ "$n" -le 0 ] && return 0
  a=$(head -c "$n" "$1" | sha256_stdin)
  b=$(head -c "$n" "$BASELINE_DIR/$3" | sha256_stdin)
  [ "$a" != "$b" ] && echo "baseline drift: first ${n}B of $4 differ from the baseline reference head (append-only contract violated, or the wrong file was fetched)" >> "$WARNF"
  return 0
}
head_window_drift "$UE4SS_F" "$BASE_UE4SS" ue4ss-log-baseline.txt "UE4SS.log"
head_window_drift "$ENTRY_F" "$BASE_ENTRY" entry-log-baseline.txt "aligned-entry log"
head_window_drift "$XWP_F"   "$BASE_XWP"   xwp-jsonl-baseline.txt "xwp jsonl"

sz_ue4ss=0; [ -f "$UE4SS_F" ] && sz_ue4ss=$(wc -c < "$UE4SS_F" | tr -d ' ')
sz_entry=0; [ -f "$ENTRY_F" ] && sz_entry=$(wc -c < "$ENTRY_F" | tr -d ' ')
sz_xwp=0;   [ -f "$XWP_F" ]   && sz_xwp=$(wc -c < "$XWP_F" | tr -d ' ')
sz_sdiag=0; [ -f "$SDIAG_F" ] && sz_sdiag=$(wc -c < "$SDIAG_F" | tr -d ' ')

[ "$ok_ue4ss" = 1 ] && [ "$sz_ue4ss" -lt "$BASE_UE4SS" ] \
  && echo "UE4SS.log fetched ${sz_ue4ss}B < baseline ${BASE_UE4SS}B (log replaced/truncated or wrong file fetched)" >> "$WARNF"
[ "$ok_entry" = 1 ] && [ "$sz_entry" -lt "$BASE_ENTRY" ] \
  && echo "aligned-entry log fetched ${sz_entry}B < baseline ${BASE_ENTRY}B (log replaced/truncated or wrong file fetched)" >> "$WARNF"
[ "$ok_entry" = 1 ] && [ "$sz_entry" -gt "$BASE_ENTRY" ] \
  && echo "CONFIG-DRIFT: aligned-entry log GREW (+$((sz_entry - BASE_ENTRY))B) but aligned_entry is DISABLED in the SHORTEST 2-plugin config — some other writer used the shared channel; verify ini sha 1216aab6... before attributing anything" >> "$WARNF"
[ "$ok_xwp" = 1 ] && [ "$sz_xwp" -gt "$BASE_XWP" ] \
  && echo "CONFIG-DRIFT: xwp jsonl GREW (+$((sz_xwp - BASE_XWP))B) but xwp is DISABLED in the SHORTEST config — verify ini sha 1216aab6..." >> "$WARNF"

# ---------------------------------------------------------------------------
# Stage table (r13/r14 marker -> stage; see STAGE-TABLE.md for provenance)
# ---------------------------------------------------------------------------
classify_stream() { # $1=file  $2=stream name (ue4ss|entry)
  awk -v stream="$2" '
    function bump(c,  n) { cnt[c]++; if (!(c in first)) first[c] = $0; last[c] = $0 }
    function notable(c)  { if (notables < 40) { nc[notables] = c; nl[notables] = $0; notables++ } }
    function note_stage(s) {
      if (s == "") return
      if (!(s in sseen)) { sorder[++nsorder] = s; sseen[s] = 1 }
      slast = s
    }
    function stage_of_tag(t,  s) {
      s = ""
      if (t == "diag_channel_open_failed") s = "S0_DIAGCHANNEL"
      else if (t == "module_start_entered" || t == "entry_incoming_sp_mod16" || t == "entry_call_sp_mod16" || t == "constructor_count" || t == "constructor_begin" || t == "constructor_return" || t == "probe_lifecycle_result" || t == "runtime_start_skipped") s = "S1_ENTRY_CTORS"
      else if (t == "runtime_wiring_begin" || t == "mfa_activate_failed" || t == "mfa_activated_low" || t == "mfa_activated_high" || t == "mfa_activate_os_error") s = "S2_ACTIVATION"
      else if (t == "bridge_vmentry_entries" || t == "bridge_vm_walk_stop" || t == "bridge_kve_distinct0" || t == "bridge_kve_distinct1" || t == "bridge_kve_distinct2" || t == "bridge_kve_distinct3" || t == "bridge_kve_eboot" || t == "bridge_exepath_valid" || t == "bridge_exepath_sig") s = "S3_VMMAP_WALK"
      else if (t == "bridge_lease_reg_spans" || t == "bridge_lease_reg" || t == "bridge_lease_cap" || t == "bridge_lease_gap" || t == "bridge_lease_gap_req" || t == "bridge_lease_ref_lo" || t == "bridge_lease_ref_hi" || t == "bridge_lease_ref_len" || t == "bridge_lease_near_lo" || t == "bridge_lease_near_hi" || t == "bridge_lease_near_len") s = "S4_LEASE_REG"
      else if (t == "anchor_cov" || t == "anchor_match" || t == "anchor_mm" || t == "anchor_c_either" || t == "anchor_owned_ref" || t == "anchor_refuse") s = "S5_IDENTITY_ANCHORS"
      else if (t == "lmf_vmentry_cap" || t == "lmf_vm_scan_cap" || t == "lmf_linkmap_cap" || t == "lmf_provider_code" || t == "lmf_identity_source" || t == "lmf_extent_base_low" || t == "lmf_extent_base_high" || t == "lmf_extent_end_low" || t == "lmf_extent_end_high" || t == "lmf_anchor_count" || t == "lmf_intervals_found") s = "S6_CONVERSION"
      else if (t == "bridge_cfg_gamedir" || t == "bridge_cfg_workdir" || t == "bridge_exepath_source") s = "S7_CFG_ATTACH"
      else if (t == "runtime_start_ok") s = "S9_START_ACK"
      else if (t == "module_start_return" || t == "runtime_stop_result") s = "S11_STOP"
      return s
    }
    function map_detail(line) {
      if (line ~ /no observed mapping interval intersects the module image extent with valid geometry/) { tstage = "S6_CONVERSION"; treason = "derive_readable_ranges refusal (lmf_to_native_loader_facts.cpp:228-231); the launch-9 aligned-entry terminal string — in THIS config the aligned entry is disabled, so a live line belongs to supplier_composition" }
      else if (line ~ /lease coverage gap: requested \[/) { tstage = "S6_CONVERSION"; treason = "r14 pre-lease coverage gate (MRS_ERR_CONVERSION by design): lease would walk bytes outside the frozen-snapshot registry; gap + nearest span in bridge_lease_* tags" }
      else if (line ~ /Owned read range includes unregistered bytes/) { tstage = "S8_CONTEXT"; treason = "deep PLH lease throw (OwnedMemory.cpp) — DORMANT in r13 (launch-8 r10 threw it, pid 234); r14 replaces it with the pre-lease gate. Seeing it means the rebuild did NOT carry the r14 gate" }
      else if (line ~ /no image anchor/) { tstage = "S6_CONVERSION"; treason = "supplier status NoImageAnchor — identity gate never anchored" }
      else if (line ~ /vm-scan image span is empty or unrepresentable/) { tstage = "S6_CONVERSION"; treason = "vm-scan extent invalid" }
      else if (line ~ /observed executable path is empty/) { tstage = "S6_CONVERSION"; treason = "Resolution-B opt-in missing (allow_configured_roots_for_missing_executable_path)" }
      else if (line ~ /supplier has not captured facts/) { tstage = "S6_CONVERSION"; treason = "supplier capture status not Ok" }
      else if (line ~ /Native module and executable paths must name existing regular files/) { tstage = "S8_CONTEXT"; treason = "NativeProgramContext path validation (both paths must exist while /data is reachable)" }
      else if (line ~ /GMalloc is uninitialized/) { tstage = "S8B_INITIALIZER"; treason = "PostInitialize GMalloc gate — seeds armed past Shape-A but slot 3 unseeded" }
      else if (line ~ /Required-slot seed table incomplete/) { tstage = "S8B_INITIALIZER"; treason = "Shape-A gate — the armed SeedTable did NOT ship in this fself" }
      else if (line ~ /\[Seeds\] REFUTED/) { tstage = "S8B_INITIALIZER"; treason = "armed seed constant REFUTED in-game (terminal per launch)" }
      else if (line ~ /callback registration failed/) { tstage = "S8B_INITIALIZER"; treason = "tick/SCO detour registration failed (I7 family)" }
      # G1: the two refusal classes the REAL launch-10 runs exposed, named per
      # the oracle rows (B7 Phase-B MFA activation, E2 START-stage code=3).
      else if (line ~ /mfa_activate_failed/) { tstage = "S2_ACTIVATION"; treason = "B7 EXPECTED-REFUSAL — MFA activation refused (Phase B; loadlist/roots drifted; the named offending file is in the detail); NOTHING downstream ran" }
      else if (line ~ /runtime_start_failed code=3/) { tstage = "S9_START_REFUSED"; treason = "E2 EXPECTED-REFUSAL — START-stage refusal (post-conversion: prepare/start threw; pad profile / mod source / sink open); verdict FAILED_E2_START, >= MINIMAL-ACCEPT where the session conversion quartet holds" }
    }
    {
      line = $0
      # ---- r4 freshness prefix [t=<ms>ms pid=<pid>] (composition/entry lineages)
      if (line ~ /^\[t=[0-9]+ms pid=[0-9]+\] /) {
        bump("ENTRY4")
        ts = line; sub(/^\[t=/, "", ts); sub(/ms pid=.*/, "", ts)
        tp = line; sub(/^.*ms pid=/, "", tp); sub(/\].*/, "", tp)
        print "TIMING\tfailline\tpid=" tp "\t" ts
        if (stream == "ue4ss") print "TIMINGGUARD\tfailline\tmonotonic-ms writer (composition entry lineage)"
        sub(/^\[[^]]*\] /, "", line)
      }
      # ---- path_test plugin stamps (monotonic-ns writer)
      if (line ~ /^PATH_TEST: .* writable at [0-9]+ monotonic-ns/) {
        bump("PATHT")
        ns = line; sub(/^.* writable at /, "", ns); sub(/ monotonic-ns.*/, "", ns)
        print "TIMING\tpathtest\tstamp\t" ns
        if (stream == "ue4ss") print "TIMINGGUARD\tpathtest\tmonotonic-ns writer (path_test plugin)"
      }
      if (line ~ /^PATH_TEST: .* OPEN_FAILED errno=[0-9]+ at [0-9]+ monotonic-ns/) {
        bump("PATHT_FAIL"); notable("PATHT_FAIL")
        ns = line; sub(/^.* at /, "", ns); sub(/ monotonic-ns.*/, "", ns)
        print "TIMING\tpathtest\topen_failed\t" ns
      }
      if (line ~ /^ACTIVATION_TEST:/) { bump("ACTIVT") }
      # ---- startup-checkpoint DIAG plugin line (DISABLED in shortest config)
      if (line ~ /^DIAG: module_start reached at /) {
        bump("DIAGL"); notable("DIAGL")
        print "CONFIGDRIFT\tDIAG-line in UE4SS.log: startup-checkpoint diag plugin output but only supplier_composition+path_test are enabled (ini 1216aab6...) — config drift or stale plugin"
      }
      # ---- xwp mirror records (xwp DISABLED in shortest config)
      if (line ~ /\{"marker":"modbridge\.execwriteprobe"/) { bump("XWP_MIRROR") }
      # ---- bridge failure grammar (after prefix strip)
      if (line ~ /^modbridge module_start failed: /) {
        bump("FAIL_LINE"); notable("FAIL_LINE")
        if (line ~ /diag_channel_open_failed/) { bump("DIAG22"); notable("DIAG22"); note_stage("S0_DIAGCHANNEL") }
        tstage = ""; treason = ""
        map_detail(line)
        if (tstage != "") { note_stage(tstage); print "STAGEHIT\t" tstage "\t" treason "\t" line }
        else { note_stage("S9_START_ACK"); print "STAGEHIT\tS9_START_ACK\tfailure line without a pinned detail string (raw line is the evidence)\t" line }
        print "CLASSLINE\t" stream "\t" line
        next
      }
      # ---- [Seeds] family (initializer stage)
      if (line ~ /\[Seeds\]/) {
        if (line ~ /Required-slot seed table incomplete/) { bump("SHAPE_A"); notable("SHAPE_A"); note_stage("S8B_INITIALIZER") }
        else if (line ~ /\[Seeds\] REFUTED/)              { bump("SEEDS_REFUTED"); notable("SEEDS_REFUTED"); note_stage("S8B_INITIALIZER") }
        else                                              { bump("SEEDS_OTHER"); notable("SEEDS_OTHER"); note_stage("S8B_INITIALIZER") }
        print "CLASSLINE\t" stream "\t" line
        next
      }
      if (line ~ /GMalloc is uninitialized/) { bump("GMALLOC"); notable("GMALLOC"); note_stage("S8B_INITIALIZER"); print "CLASSLINE\t" stream "\t" line; next }
      # ---- entry-grammar diagnostic tags ([PPSA33286] ... build=<hex> <tag> value=0x<hex>)
      if (line ~ /\] modbridge-runtime-aligned-entry\/v1 build=/) {
        bump("OBSERVER")
        tag = line; sub(/^.*v1 build=[0-9a-f]+ /, "", tag); sub(/ value=.*/, "", tag)
        s = stage_of_tag(tag)
        if (s != "") note_stage(s)
        if (tag == "runtime_start_ok")     { bump("OBS_START_OK"); notable("OBS_START_OK") }
        if (tag == "runtime_start_failed") { bump("OBS_START_FAILED"); notable("OBS_START_FAILED") }
        if (tag == "runtime_stop_result")  { bump("OBS_STOP_RESULT"); notable("OBS_STOP_RESULT") }
        if (tag == "module_start_return")  { bump("OBS_MODULE_RETURN") }
        if (tag == "mfa_activate_failed")  { bump("OBS_MFA_FAILED"); notable("OBS_MFA_FAILED"); note_stage("S2_ACTIVATION") }
        if (tag == "lua_state_begin")      { bump("OBS_LUA_HARNESS"); notable("OBS_LUA_HARNESS") }
        print "CLASSLINE\t" stream "\t" line
        next
      }
      # ---- r13/r14 bare tags mirrored into a text channel (jsonl-grammar-free)
      if (line ~ /bridge_lease_reg_spans|bridge_lease_reg$|bridge_lease_reg |bridge_lease_gap|bridge_vmentry_entries|bridge_vm_walk_stop|bridge_kve_|bridge_exepath_|bridge_lease_cap/) {
        bump("BRIDGE_TAG")
        tag = line; sub(/[^a-z_0-9].*$/, "", tag)
        s = stage_of_tag(tag); if (s != "") note_stage(s)
      }
      if (line ~ /anchor_(cov|match|mm|c_either|owned_ref|refuse)/) { bump("BRIDGE_TAG"); note_stage("S5_IDENTITY_ANCHORS") }
      if (line ~ /lmf_(vmentry_cap|vm_scan_cap|linkmap_cap|provider_code|identity_source|extent_base_low|extent_base_high|extent_end_low|extent_end_high|anchor_count|intervals_found)/) { bump("BRIDGE_TAG"); note_stage("S6_CONVERSION") }
      if (line ~ /bridge_cfg_gamedir=|bridge_cfg_workdir=|bridge_exepath_source=/) { bump("BRIDGE_CFG"); note_stage("S7_CFG_ATTACH") }
      if (line ~ /MRS_ERR_[A-Z_]+|MRS_OK/) { bump("MRS_TOKEN") }
      # ---- IGW exec-write probe verdict markers
      if (line ~ /W_WORKS|W_FAILS_R_WORKS|BOTH_FAIL/) { bump("IGW"); notable("IGW"); print "CLASSLINE\t" stream "\t" line; next }
      # ---- progress markers
      if (line ~ /engine-ready|mods-started/) { bump("PROGRESS"); note_stage("S10_LADDER") }
      if (line ~ /TownfallItemSpawner/)       { bump("PROGRESS"); notable("PROGRESS"); note_stage("S10_LADDER"); bump("MODLOAD"); print "MODLOADLINE\t" stream "\t" line }
      # ---- crash-tail vicinity (igf6 class)
      if (line ~ /[Ll]oadMap|Finding GameEngine/) { bump("CRASH_TAIL"); tail[ntail++ % 5] = line }
      # ---- UE4SS banner / everything else
      if (line ~ /UE4SS - v|root directory:|mods directories:|Setting up mods|Native console, GUI, crash dumping/) { bump("BANNER"); next }
      if (line ~ /^[[:space:]]*$/) next
      bump("OTHER")
      if (noth < 10) { print "OTHERLINE\t" stream "\t" line; noth++ }
      n_other++
    }
    END {
      for (c in cnt) printf "COUNT\t%s\t%s\t%d\t%s\n", stream, c, cnt[c], first[c]
      printf "STAGECHAIN\t%s\t", stream
      for (i = 1; i <= nsorder; i++) printf "%s%s", (i > 1 ? " -> " : ""), sorder[i]
      printf "\n"
      printf "TAIL\t%s\t", stream
      for (i in tail) printf " ~ %s", tail[i]
      printf "\n"
    }
  ' "$1"
}

# supplier-diag jsonl classifier: {"marker":"modbridge.supplier-diag/v1","schema":1,"seq":N,"pid":P,"tag":"...","value":D,"value_hex":"0xH","t_ms":MS}
# G2: the jsonl is append-only across a whole console boot, so everything is
# grouped PER PID (session). Value tags are last-write-wins WITHIN a pid only;
# STIME deltas are computed only within one pid (never across sessions);
# pid_first/pid_last label the file's first/last SESSION explicitly instead of
# the stale "first pid seen" reading.
classify_sdiag() { # $1=file
  awk -v cut="${BASE_SDIAG:-0}" '
    function jnum(line, key,  k, v, c, res) {
      k = index(line, "\"" key "\":")
      if (k == 0) return ""
      v = substr(line, k + length(key) + 3)
      res = ""; c = 1
      while (c <= length(v)) {
        ch = substr(v, c, 1)
        if (ch == "," || ch == "}") break
        if (ch != " ") res = res ch
        c++
      }
      return res
    }
    function jstr(line, key,  k, v, res, c, ch) {
      k = index(line, "\"" key "\":\"")
      if (k == 0) return ""
      v = substr(line, k + length(key) + 4)
      res = ""; c = 1
      while (c <= length(v)) {
        ch = substr(v, c, 1)
        if (ch == "\\") { c += 2; continue }
        if (ch == "\"") break
        res = res ch; c++
      }
      return res
    }
    function dv(v) { return (v == "" ? "-" : v) }
    function stage_of_tag(t,  s) {
      s = ""
      if (t == "diag_channel_open_failed") s = "S0_DIAGCHANNEL"
      else if (t == "module_start_entered" || t == "entry_incoming_sp_mod16" || t == "entry_call_sp_mod16" || t == "constructor_count" || t == "constructor_begin" || t == "constructor_return" || t == "probe_lifecycle_result" || t == "runtime_start_skipped") s = "S1_ENTRY_CTORS"
      else if (t == "runtime_wiring_begin" || t == "mfa_activate_failed" || t == "mfa_activated_low" || t == "mfa_activated_high" || t == "mfa_activate_os_error") s = "S2_ACTIVATION"
      else if (t == "bridge_vmentry_entries" || t == "bridge_vm_walk_stop" || t == "bridge_kve_distinct0" || t == "bridge_kve_distinct1" || t == "bridge_kve_distinct2" || t == "bridge_kve_distinct3" || t == "bridge_kve_eboot" || t == "bridge_exepath_valid" || t == "bridge_exepath_sig") s = "S3_VMMAP_WALK"
      else if (t == "bridge_lease_reg_spans" || t == "bridge_lease_reg" || t == "bridge_lease_cap" || t == "bridge_lease_gap" || t == "bridge_lease_gap_req" || t == "bridge_lease_ref_lo" || t == "bridge_lease_ref_hi" || t == "bridge_lease_ref_len" || t == "bridge_lease_near_lo" || t == "bridge_lease_near_hi" || t == "bridge_lease_near_len") s = "S4_LEASE_REG"
      else if (t == "anchor_cov" || t == "anchor_match" || t == "anchor_mm" || t == "anchor_c_either" || t == "anchor_owned_ref" || t == "anchor_refuse") s = "S5_IDENTITY_ANCHORS"
      else if (t == "lmf_vmentry_cap" || t == "lmf_vm_scan_cap" || t == "lmf_linkmap_cap" || t == "lmf_provider_code" || t == "lmf_identity_source" || t == "lmf_extent_base_low" || t == "lmf_extent_base_high" || t == "lmf_extent_end_low" || t == "lmf_extent_end_high" || t == "lmf_anchor_count" || t == "lmf_intervals_found") s = "S6_CONVERSION"
      else if (t == "bridge_cfg_gamedir" || t == "bridge_cfg_workdir" || t == "bridge_exepath_source") s = "S7_CFG_ATTACH"
      else if (t == "runtime_start_ok") s = "S9_START_ACK"
      else if (t == "module_start_return" || t == "runtime_stop_result") s = "S11_STOP"
      return s
    }
    {
      if ($0 !~ /"marker":"modbridge\.supplier-diag\/v1"/) next
      n++
      tag = jstr($0, "tag"); val = jnum($0, "value"); tms = jnum($0, "t_ms")
      pid = jnum($0, "pid"); seq = jnum($0, "seq")
      if (pid == "") pid = "?"
      if (!(pid in psess_ix)) { psess_ix[pid] = nsess + 1; nsess++; psess[nsess] = pid }
      pev[pid]++
      s = stage_of_tag(tag)
      if (s != "") {
        if (!(s in sseen)) { sorder[++nsorder] = s; sseen[s] = 1 }
        slast = s
        if (!(pid in pinit)) { pinit[pid] = 1; pcount[pid] = 0 }
        if (!((pid, s) in pseen)) { pseen[pid, s] = 1; pcount[pid]++; pstage[pid, pcount[pid]] = s }
      }
      if (tms != "" && tms ~ /^[0-9]+$/) {
        if (pfirst[pid] == "") pfirst[pid] = tms
        plast[pid] = tms
      }
      if (tag == "runtime_start_ok")     { sok++; psok[pid]++ }
      if (tag == "runtime_start_failed") { sfail++; psfail[pid]++; prscode[pid] = val }
      if (tag == "mfa_activate_failed")   pmfa[pid] = val
      if (tag == "bridge_lease_reg_spans") pspans[pid] = val
      if (tag == "bridge_lease_reg")       preg[pid] = val
      if (tag == "lmf_anchor_count")       panchor[pid] = val
      if (tag == "lmf_intervals_found")    pintervals[pid] = val
      if (tag == "bridge_vmentry_entries") pvme[pid] = val
      if (tag == "bridge_vm_walk_stop")    pstop[pid] = val
      if (tag == "bridge_exepath_source")  pesrc[pid] = val
      if (tag == "bridge_cfg_gamedir")     { pcfgv[pid] = val; pcfg[pid]++ }
      if (tag == "bridge_cfg_workdir")     pcfg[pid]++
      if (tag ~ /^lmf_extent_(base_low|base_high|end_low|end_high)$/) pextb[pid, tag] = 1
      if (tag == "bridge_spanpub_count") pspubc[pid] = val
      if (tag == "bridge_spanpub_total") pspubt[pid] = val
      if (tag == "bridge_spanpub_cap")   pspubcap[pid] = val
      if (tag == "bridge_spanpub_env_low")  penvl[pid] = jstr($0, "value_hex")
      if (tag == "bridge_spanpub_env_high") penvh[pid] = jstr($0, "value_hex")
      if (tag == "stat_ownmod")    pstato[pid] = val
      if (tag == "stat_ownmod_ec") pstatoec[pid] = val
      if (tag == "module_start_return") pmodret[pid] = val
      if (tms != "") {
        if (ptms != "" && ptms_pid == pid && ptms ~ /^[0-9]+$/ && tms ~ /^[0-9]+$/) {
          d = tms - ptms
          if (nshow < 24) { printf "STIME\t%s\t%s\t+%dms\n", tag, tms, d; nshow++ }
        } else {
          if (nshow < 24) {
            if (ptms != "" && ptms_pid != pid) printf "SDIAGSESS\tpid=%s\n", pid
            printf "STIME\t%s\t%s\t(first t_ms)\n", tag, tms; nshow++
          }
        }
        ptms = tms; ptms_pid = pid
      }
      printf "SDIAG\t%s\t%s\n", tag, val
    }
    END {
      lp = (nsess > 0) ? psess[nsess] : ""
      printf "SDIAGSUM\tevents=%d sessions=%d pid_first=%s pid_last=%s baseline_cut=%s start_ok=%d start_failed=%d lease_spans=%s lease_reg=%s anchor_count=%s intervals_found=%s vmentry=%s cfg_gamedir=%s (file totals; value tags = last session pid=%s; per-session lines follow)\n", n + 0, nsess + 0, dv(psess[1]), dv(lp), (cut + 0 > 0 ? "yes" : "no"), sok + 0, sfail + 0, dv(pspans[lp]), dv(preg[lp]), dv(panchor[lp]), dv(pintervals[lp]), dv(pvme[lp]), dv(pcfgv[lp]), dv(lp)
      for (pi = 1; pi <= nsess; pi++) {
        p = psess[pi]
        extn = ((pextb[p, "lmf_extent_base_low"] ? 1 : 0) + (pextb[p, "lmf_extent_base_high"] ? 1 : 0) + (pextb[p, "lmf_extent_end_low"] ? 1 : 0) + (pextb[p, "lmf_extent_end_high"] ? 1 : 0))
        mfav = dv(pmfa[p])
        if (mfav != "-" && mfav + 0 > 2147483647) mfav = mfav "(int32 " (mfav - 4294967296) ")"
        chain = ""
        for (j = 1; j <= pcount[p]; j++) chain = chain ((j > 1) ? " -> " : "") pstage[p, j]
        printf "PIDSUM\tpid=%s\tevents=%d\tt_first=%s\tt_last=%s\tstart_ok=%d\tstart_failed=%d\tmfa_activate_failed=%s\truntime_start_failed=%s\textent_events=%d\tcfg_events=%d\tlease_spans=%s\tlease_reg=%s\tanchor_count=%s\tintervals_found=%s\tvmentry=%s\tvm_walk_stop=%s\texepath_source=%s\tcfg_gamedir=%s\tspanpub_count=%s\tspanpub_total=%s\tspanpub_cap=%s\tenv_low=%s\tenv_high=%s\tstat_ownmod=%s\tstat_ownmod_ec=%s\tmodule_start_return=%s\n", p, pev[p] + 0, dv(pfirst[p]), dv(plast[p]), psok[p] + 0, psfail[p] + 0, mfav, dv(prscode[p]), extn, pcfg[p] + 0, dv(pspans[p]), dv(preg[p]), dv(panchor[p]), dv(pintervals[p]), dv(pvme[p]), dv(pstop[p]), dv(pesrc[p]), dv(pcfgv[p]), dv(pspubc[p]), dv(pspubt[p]), dv(pspubcap[p]), dv(penvl[p]), dv(penvh[p]), dv(pstato[p]), dv(pstatoec[p]), dv(pmodret[p])
        printf "PIDCHAIN\tpid=%s\t%s\n", p, chain
        printf "PIDJSON\t{\"pid\":\"%s\",\"events\":%d,\"t_first\":\"%s\",\"t_last\":\"%s\",\"start_ok\":%d,\"start_failed\":%d,\"mfa_activate_failed\":\"%s\",\"runtime_start_failed\":\"%s\",\"extent_events\":%d,\"cfg_events\":%d,\"lease_spans\":\"%s\",\"lease_reg\":\"%s\",\"anchor_count\":\"%s\",\"intervals_found\":\"%s\",\"vmentry\":\"%s\",\"vm_walk_stop\":\"%s\",\"exepath_source\":\"%s\",\"cfg_gamedir\":\"%s\",\"module_start_return\":\"%s\"}\n", p, pev[p] + 0, dv(pfirst[p]), dv(plast[p]), psok[p] + 0, psfail[p] + 0, mfav, dv(prscode[p]), extn, pcfg[p] + 0, dv(pspans[p]), dv(preg[p]), dv(panchor[p]), dv(pintervals[p]), dv(pvme[p]), dv(pstop[p]), dv(pesrc[p]), dv(pcfgv[p]), dv(pmodret[p])
      }
      printf "SDIAGCHAIN\tsdiag\t"
      for (i = 1; i <= nsorder; i++) printf "%s%s", (i > 1 ? " -> " : ""), sorder[i]
      printf "\n"
    }
  ' "$1"
}

# ---------------------------------------------------------------------------
# Status parse (session/pid-stamped, last-writer-wins)
# ---------------------------------------------------------------------------
status_summary() { # stdin: status file; $1=baseline session pid  $2=baseline session ms
  awk -v base_pid="$1" -v base_ms="$2" '
    /^modforge-runtime-status\t1$/ { print "header_ok=1"; next }
    /^session\t/ {
      n = split($0, p, "\t")
      if (n >= 3) {
        print "session_pid=" p[2]; print "session_ms=" p[3]
        if (p[2] == base_pid && p[3] == base_ms) print "fresh=no"
        else { print "fresh=yes"; print "freshwhy=session differs from the pinned baseline session " base_pid "/" base_ms }
      }
      next
    }
    /^state\t/ {
      n = split($0, p, "\t")
      if (n >= 4) {
        nstates++
        last_state = p[3]; last_code = p[4]; last_detail = (n >= 5) ? p[5] : ""
        if (p[3] == "activated" || p[3] == "engine-ready" || p[3] == "mods-started") ladder = ladder " " p[3]
      }
      next
    }
    END {
      print "nstates=" nstates+0
      print "last_state=" last_state
      print "last_code=" last_code
      print "last_detail=" last_detail
      print "ladder=" ladder
    }'
}

# ---------------------------------------------------------------------------
# One-page summary
# ---------------------------------------------------------------------------
sumf="$TMPDIR_E/summary.txt"
: > "$sumf"
emit() { echo "$1" >> "$sumf"; }

# classify everything
CLS="$TMPDIR_E/classes.txt"
: > "$CLS"
[ "$ok_ue4ss" = 1 ] && extract_new_lines "$UE4SS_F" "$BASE_UE4SS" | classify_stream /dev/stdin ue4ss >> "$CLS"
[ "$ok_entry" = 1 ] && extract_new_lines "$ENTRY_F" "$BASE_ENTRY" | classify_stream /dev/stdin entry >> "$CLS"
# G2: optional append-only cut for the sdiag jsonl (baseline bytes), then the
# per-session (per-pid) classifier — never cross-session aggregation.
SDIAG_CLASS_F="$SDIAG_F"
SDIAG_CUT=0
if [ "$ok_sdiag" = 1 ] && [ "$BASE_SDIAG" -gt 0 ] && [ "$sz_sdiag" -gt "$BASE_SDIAG" ]; then
  extract_new_lines "$SDIAG_F" "$BASE_SDIAG" > "$TMPDIR_E/sdiag-new.jsonl"
  SDIAG_CLASS_F="$TMPDIR_E/sdiag-new.jsonl"
  SDIAG_CUT=1
fi
[ "$ok_sdiag" = 1 ] && classify_sdiag "$SDIAG_CLASS_F" >> "$CLS"

count_class() { awk -F'\t' -v c="$1" '$1=="COUNT" && $3==c {s+=$4} END{print s+0}' "$CLS"; }

n_shapea=$(count_class SHAPE_A)
n_refuted=$(count_class SEEDS_REFUTED)
n_seedsother=$(count_class SEEDS_OTHER)
n_fail=$(count_class FAIL_LINE)
n_diag22=$(count_class DIAG22)
n_gmalloc=$(count_class GMALLOC)
n_bridgecfg=$(count_class BRIDGE_CFG)
n_bridgetag=$(count_class BRIDGE_TAG)
n_mrs=$(count_class MRS_TOKEN)
n_obs_start_ok=$(count_class OBS_START_OK)
n_obs_start_failed=$(count_class OBS_START_FAILED)
n_obs_stop=$(count_class OBS_STOP_RESULT)
n_obs_mfa_failed=$(count_class OBS_MFA_FAILED)
n_lua=$(count_class OBS_LUA_HARNESS)
n_progress=$(count_class PROGRESS)
n_modload=$(count_class MODLOAD)
n_igw=$(count_class IGW)
n_crash_tail=$(count_class CRASH_TAIL)
n_entry4=$(count_class ENTRY4)
n_patht=$(count_class PATHT)
n_patht_fail=$(count_class PATHT_FAIL)
n_activt=$(count_class ACTIVT)
n_diagl=$(count_class DIAGL)
n_xwp_mirror=$(count_class XWP_MIRROR)
n_other_ue4ss=$(awk -F'\t' '$1=="COUNT" && $2=="ue4ss" && $3=="OTHER" {print $4}' "$CLS")
[ -n "$n_other_ue4ss" ] || n_other_ue4ss=0
n_other_entry=$(awk -F'\t' '$1=="COUNT" && $2=="entry" && $3=="OTHER" {print $4}' "$CLS")
[ -n "$n_other_entry" ] || n_other_entry=0

sc_sdiag=$(awk -F'\t' '$1=="SDIAGCHAIN" && $3 != "" {print $3; exit}' "$CLS")
sc_ue4ss=$(awk -F'\t' '$1=="STAGECHAIN" && $2=="ue4ss" && $3 != "" {print $3; exit}' "$CLS")
sc_entry=$(awk -F'\t' '$1=="STAGECHAIN" && $2=="entry" && $3 != "" {print $3; exit}' "$CLS")
stage_chain="$sc_sdiag"
[ -z "$stage_chain" ] && stage_chain="$sc_ue4ss"
[ -z "$stage_chain" ] && stage_chain="$sc_entry"
[ -z "$stage_chain" ] && stage_chain="(no staged markers observed)"
stage_hits=$(awk -F'\t' '$1=="STAGEHIT" {if (n < 6) { printf "  [%s] %s\n     %s\n", $2, $3, $4; n++ }} END{}' "$CLS")
stage_hits_n=$(awk -F'\t' '$1=="STAGEHIT"' "$CLS" | wc -l | tr -d ' ')

# ---------------------------------------------------------------------------
# G2/G3/G7/G8/G9: per-session (per-pid) sdiag facts for the verdict, the
# one-pager notes and the JSON tail. The "current session" is the session of
# the LAST failure line's r4 pid when the sdiag knows it, else the file's
# last session (pid_last) — never a cross-session aggregate.
# ---------------------------------------------------------------------------
is_num() { case "$1" in ''|'-'|*[!0-9]*) return 1 ;; *) return 0 ;; esac; }
SDIAG_SUM_PAYLOAD=$(awk -F'\t' '$1=="SDIAGSUM" {print $2; exit}' "$CLS")
sd_events=$(printf '%s\n' "$SDIAG_SUM_PAYLOAD" | sed -n 's/^events=\([0-9]*\) .*/\1/p')
sd_sessions=$(printf '%s\n' "$SDIAG_SUM_PAYLOAD" | sed -n 's/.* sessions=\([0-9]*\) .*/\1/p')
sd_startfail_total=$(printf '%s\n' "$SDIAG_SUM_PAYLOAD" | sed -n 's/.* start_failed=\([0-9]*\) .*/\1/p')
sd_pid_first=$(printf '%s\n' "$SDIAG_SUM_PAYLOAD" | sed -n 's/.* pid_first=\([^ ]*\) .*/\1/p')
sd_pid_last=$(printf '%s\n' "$SDIAG_SUM_PAYLOAD" | sed -n 's/.* pid_last=\([^ ]*\) .*/\1/p')
n_sessions=${sd_sessions:-0}
FAIL_PID=$(awk -F'\t' '$1=="TIMING" && $2=="failline" {p=$3} END {sub(/^pid=/, "", p); print p}' "$CLS")
CUR_PID="$FAIL_PID"
[ -n "$CUR_PID" ] || CUR_PID="$sd_pid_last"
PF=$(awk -F'\t' -v want="pid=$CUR_PID" '$1=="PIDSUM" && $2==want { for (i = 3; i <= NF; i++) { eq = index($i, "="); print substr($i, 1, eq - 1) "=" substr($i, eq + 1) } exit }' "$CLS")
s_events=$(printf '%s\n' "$PF" | sed -n 's/^events=//p')
s_startfail=$(printf '%s\n' "$PF" | sed -n 's/^start_failed=//p')
s_mfa=$(printf '%s\n' "$PF" | sed -n 's/^mfa_activate_failed=//p')
s_rscode=$(printf '%s\n' "$PF" | sed -n 's/^runtime_start_failed=//p')
s_extent=$(printf '%s\n' "$PF" | sed -n 's/^extent_events=//p')
s_cfgev=$(printf '%s\n' "$PF" | sed -n 's/^cfg_events=//p')
s_spans=$(printf '%s\n' "$PF" | sed -n 's/^lease_spans=//p')
s_reg=$(printf '%s\n' "$PF" | sed -n 's/^lease_reg=//p')
s_anchor=$(printf '%s\n' "$PF" | sed -n 's/^anchor_count=//p')
s_intervals=$(printf '%s\n' "$PF" | sed -n 's/^intervals_found=//p')
s_vme=$(printf '%s\n' "$PF" | sed -n 's/^vmentry=//p')
s_walkstop=$(printf '%s\n' "$PF" | sed -n 's/^vm_walk_stop=//p')
s_exesrc=$(printf '%s\n' "$PF" | sed -n 's/^exepath_source=//p')
s_cfgdir=$(printf '%s\n' "$PF" | sed -n 's/^cfg_gamedir=//p')
s_spanpub=$(printf '%s\n' "$PF" | sed -n 's/^spanpub_count=//p')
s_spanpub_total=$(printf '%s\n' "$PF" | sed -n 's/^spanpub_total=//p')
s_spanpub_cap=$(printf '%s\n' "$PF" | sed -n 's/^spanpub_cap=//p')
s_envlow=$(printf '%s\n' "$PF" | sed -n 's/^env_low=//p')
s_envhigh=$(printf '%s\n' "$PF" | sed -n 's/^env_high=//p')
s_statownmod=$(printf '%s\n' "$PF" | sed -n 's/^stat_ownmod=//p')
s_statownmod_ec=$(printf '%s\n' "$PF" | sed -n 's/^stat_ownmod_ec=//p')
s_modret=$(printf '%s\n' "$PF" | sed -n 's/^module_start_return=//p')
CURCHAIN=$(awk -F'\t' -v want="pid=$CUR_PID" '$1=="PIDCHAIN" && $2==want {print $3; exit}' "$CLS")
# G1: oracle §2 MINIMAL-ACCEPT conversion quartet, scoped to the current session.
QUARTET=no
if [ "$s_extent" = "4" ] && is_num "$s_anchor" && [ "$s_anchor" -gt 0 ] \
   && is_num "$s_intervals" && [ "$s_intervals" -gt 0 ] \
   && is_num "$s_cfgev" && [ "$s_cfgev" -gt 0 ]; then QUARTET=yes; fi
# G1 clause 2: the D6 geometry refusal did NOT occur anywhere in the window.
n_d6=$(awk -F'\t' '$1=="CLASSLINE" && $3 ~ /no observed mapping interval intersects/ {c++} END {print c + 0}' "$CLS")

# GAP-3 display quirk: bridge_exepath_source stages S7_CFG_ATTACH but the
# bridge emits it mid-walk (before anchors/lease), so first-seen chains can
# read S3 -> S7 -> S4/S5. Annotate it when it happens; the order itself is
# the honest first-seen record and is NOT reordered.
s7_quirk=$(printf '%s\n%s\n' "$sc_sdiag" "$sc_ue4ss" | awk '
  /S7_CFG_ATTACH/ {
    p7 = index($0, "S7_CFG_ATTACH")
    p4 = index($0, "S4_LEASE_REG")
    p5 = index($0, "S5_IDENTITY_ANCHORS")
    if ((p4 > 0 && p7 < p4) || (p5 > 0 && p7 < p5)) { print "yes"; exit }
  }')

# status facts
if [ "$ok_status" = 1 ]; then
  ST=$(status_summary "$BASE_SESS_PID" "$BASE_SESS_MS" < "$STATUS_F")  last_state=$(printf '%s\n' "$ST" | sed -n 's/^last_state=//p')
  last_code=$(printf '%s\n' "$ST" | sed -n 's/^last_code=//p')
  last_detail=$(printf '%s\n' "$ST" | sed -n 's/^last_detail=//p')
  session_pid=$(printf '%s\n' "$ST" | sed -n 's/^session_pid=//p')
  session_ms=$(printf '%s\n' "$ST" | sed -n 's/^session_ms=//p')
  ladder=$(printf '%s\n' "$ST" | sed -n 's/^ladder=//p')
  n_states=$(printf '%s\n' "$ST" | sed -n 's/^nstates=//p')
  fresh=$(printf '%s\n' "$ST" | sed -n 's/^fresh=//p')
else
  last_state=""; last_code=""; last_detail=""; session_pid=""; session_ms=""
  ladder=""; n_states=0; fresh="unknown"
fi
[ -n "$fresh" ] || fresh=unknown

# ---------------------------------------------------------------------------
# Timing table (per-writer sequences; never subtract across writers)
# ---------------------------------------------------------------------------
emit ""
emit "timing (per-writer monotonic sequences; NOT comparable across writers — plugins use different stamp formats/units and the per-writer rule forbids cross-writer subtraction):"
awk -F'\t' '
  $1=="TIMING" {
    key = $2 (($2=="failline") ? "[" $3 "]" : "")
    val = ($2=="failline") ? $4 : $4
    seen[key]++; order[nk] = key; if (!(key in kseen)) { kseen[key] = 1; nk++ }
    if ($2 == "pathtest") {
      if (prev[key] != "" && prev[key] ~ /^[0-9]+$/ && val ~ /^[0-9]+$/) {
        d = val - prev[key]
        if (n < 14) { printf "  writer=pathtest stamp=%sns delta=+%dns (vs previous pathtest stamp)\n", val, d; n++ }
      } else if (n < 14) { printf "  writer=pathtest stamp=%sns (first stamp)\n", val; n++ }
      prev[key] = val
    } else if ($2 == "failline") {
      if (prev[key] != "" && prev[key] ~ /^[0-9]+$/ && val ~ /^[0-9]+$/) {
        d = val - prev[key]
        if (n < 20) { printf "  writer=failline %s t=%sms delta=+%dms (vs previous stamp, same pid)\n", $3, val, d; n++ }
      } else if (n < 20) { printf "  writer=failline %s t=%sms (first stamp for this pid)\n", $3, val; n++ }
      prev[key] = val
    }
  }
  END { if (nk == 0) print "  (no timestamped lines in the new content)" }
' "$CLS" >> "$sumf"
if [ "$ok_sdiag" = 1 ]; then
  emit "  writer=supplier-diag-jsonl (t_ms, monotonic-ms; per-SESSION deltas — deltas NEVER cross sessions/pids; '(first t_ms)' opens each session):"
  awk -F'\t' '$1=="SDIAGSESS" {printf "    -- session %s (delta base resets) --\n", $2} $1=="STIME" {printf "    %s t=%s %s\n", $2, $3, $4}' "$CLS" | head -40 >> "$sumf"
fi
if [ "$ok_status" = 1 ] && [ -n "$session_ms" ]; then
  emit "  writer=runtime-status session stamp: pid=$session_pid t=${session_ms}ms (single point; the state lines carry no per-state timestamps — file sequence order is the only intra-status ordering)"
fi
emit "  GUARD: per-writer sequences only. path_test=monotonic-ns, failure lines=monotonic-ms, supplier-diag=monotonic-ms t_ms, status=monotonic-ms session stamp. NEVER subtract across writers."

# ---------------------------------------------------------------------------
# Signature counts + stage chain
# ---------------------------------------------------------------------------
emit ""
emit "signatures (new lines vs launch-10 baselines):"
emit "  stage-table markers (bridge/lmf/anchor tags): $n_bridgetag (bridge_cfg/exepath diag lines: $n_bridgecfg)"
emit "  failure lines: $n_fail (diag_channel_open_failed/-22: $n_diag22) [r4-prefixed: $n_entry4]"
if [ "$n_fail" -gt 0 ] && [ "$n_entry4" -gt 0 ]; then
  emit "  failure lines by session (r4 pid): $(awk -F'\t' '$1=="TIMING" && $2=="failline" {print $3}' "$CLS" | sort | uniq -c | awk '{printf "%s x%s  ", $2, $1}')— window note (G6): the UE4SS.log window is CUMULATIVE across this boot's launches; failure lines carry r4 pid prefixes (per-session above), PATH_TEST/ACTIVATION_TEST stamps carry none (positional attribution only); verdict precedence = LAST failure line (append-only ordering discipline)"
fi
emit "  initializer: shape_a=$n_shapea refuted=$n_refuted other-seeds=$n_seedsother gmalloc-throw=$n_gmalloc"
emit "  observer events: start_ok=$n_obs_start_ok start_failed=$n_obs_start_failed stop_result=$n_obs_stop mfa_failed=$n_obs_mfa_failed lua_harness=$n_lua"
emit "  progress markers: $n_progress; IGW exec-write markers: $n_igw; LoadMap-tail lines: $n_crash_tail"
emit "  path_test stamps: ok=$n_patht open_failed=$n_patht_fail activation_test=$n_activt; MRS_* tokens: $n_mrs"
[ "$n_diagl" -gt 0 ] &&       emit "  CONFIG-DRIFT: DIAG-line count $n_diagl (diag plugin disabled in this config)"
[ "$n_xwp_mirror" -gt 0 ] &&  emit "  CONFIG-DRIFT: xwp mirror records in UE4SS.log: $n_xwp_mirror (xwp disabled in this config)"
if [ "$ok_sdiag" = 1 ]; then
  awk -F'\t' '$1=="SDIAGSUM" {print "  supplier-diag jsonl: " $2}' "$CLS" >> "$sumf"
  awk -F'\t' '$1=="PIDSUM" { msg = "    session " $2; for (i = 3; i <= NF; i++) msg = msg " " $i; print msg }' "$CLS" >> "$sumf"
  # G7/G8/G9 diag-notes on the CURRENT session — note fields only, no verdict impact.
  if [ -n "$CUR_PID" ] && [ -n "$s_events" ]; then
    if is_num "$s_vme" && [ "$s_vme" -gt 16384 ]; then
      emit "  diag-note (G7, oracle C1/S3): current-session bridge_vmentry_entries=$s_vme > pinned kBridgeVmmapEntries=16384 while bridge_vm_walk_stop=$s_walkstop (S3 predicts stop=2 when vm_total > 16384) — either the shipped capacity is larger than pinned, the tag counts walked records rather than stored entries, or the truncation flag lives outside the diag set. Reconciliation question only; note field, no verdict impact."
    fi
    if [ "$s_exesrc" = "0" ] && [ "$s_statownmod" = "1" ]; then
      emit "  diag-note (G8, oracle C7): bridge_exepath_source=0 WITH the r10 fork resolving successfully (stat_ownmod=1, stat_ownmod_ec=$s_statownmod_ec) and conversion proceeding on this session — the fork refuses only when ALL THREE stats fail; oracle C7's 'source 0 leads to conversion refusal' wording is looser than live behavior (record against the row; wording update at next oracle revision)."
    fi
    if [ "$s_spanpub_cap" = "1" ]; then
      emit "  diag-note (G9, oracle S2.2 item 6): spanpub env pair is the 64-bit low/high split env_low=$s_envlow env_high=$s_envhigh (NOT a lone 32-bit max-record-end); bridge_spanpub_count=$s_spanpub of spanpub_total=$s_spanpub_total with spanpub_cap=1 — saturation at kMaxPublishedSpans=16 (both real 2026-10-05 boots saturated; oracle note pending)."
    fi
  fi
fi
[ "$ok_sdiag" = 0 ] && emit "  supplier-diag jsonl: ABSENT (normal — sink not landed or no events)"

emit ""
emit "stage chain (r13/r14 marker table; file-sequence order):"
n_chains=0
[ -n "$sc_sdiag" ] && { emit "  supplier-diag: $sc_sdiag"; n_chains=1; }
[ -n "$sc_ue4ss" ] && { emit "  ue4ss-text:    $sc_ue4ss"; n_chains=1; }
[ -n "$sc_entry" ] && [ "$ok_entry" = 1 ] && [ "$sz_entry" -gt "$BASE_ENTRY" ] && { emit "  entry-history: $sc_entry (entry log GREW — CONFIG-DRIFT, attribution untrusted)"; n_chains=1; }
[ "$n_chains" = 0 ] && emit "  (no staged markers observed)"
if [ "$n_sessions" -gt 1 ]; then
  # G2: per-session first-seen chains — the file-level chain above mixes sessions.
  awk -F'\t' -v cur="pid=$CUR_PID" '$1=="PIDCHAIN" {printf "  session chain %s:%s %s\n", $2, ($2 == cur ? " (current)" : ""), $3}' "$CLS" >> "$sumf"
fi
[ -n "$s7_quirk" ] && emit "  note: S7_CFG_ATTACH appears before S4/S5 — bridge_exepath_source is emitted mid-walk by the bridge (before anchors/lease); first-seen order is correct, only the stage numbering looks out of order (display quirk, no misattribution)"
if [ "$n_fail" -gt 0 ] && [ "$s_modret" = "0" ] && printf '%s' "$CURCHAIN" | grep -q "S11_STOP"; then
  emit "  note (G4): S11_STOP in the current session's chain is module_start_return=0 AFTER the failure line — the entry returned 0 and the game continues UNMODDED (later launches ride the same boot); it is NOT a stop/unload event (display-granularity quirk, no value misattribution)"
fi
if [ "$stage_hits_n" -gt 0 ]; then
  emit "terminal/attributed stage hits:"
  printf '%s\n' "$stage_hits" >> "$sumf"
fi

if [ "$ok_status" = 1 ]; then
  emit ""
  emit "status: pid=$session_pid ms=$session_ms fresh=$fresh states=$n_states ladder=[$ladder ] last=$last_state code=$last_code"
  [ -n "$last_detail" ] && emit "status detail: ${last_detail}"
fi

# notable verbatim lines
if grep -q '^CLASSLINE' "$CLS"; then
  emit ""
  emit "notable lines (verbatim, capped):"
  awk -F'\t' '$1=="CLASSLINE" {printf "  [%s] %s\n", $2, $3}' "$CLS" | head -12 >> "$sumf"
fi
# mod-load prints (GAP-2): the BEST-decisive [TownfallItemSpawner] lines,
# verbatim and bounded — G2 (load prints present) vs G3 (mod-error lines,
# also BEST) must be decidable without opening the raw log.
if [ "$n_modload" -gt 0 ]; then
  emit ""
  emit "mod-load prints ([TownfallItemSpawner]): $n_modload — BEST-decisive, verbatim (capped 6; absence here + mod-error lines under 'other new lines' = G3):"
  awk -F'\t' '$1=="MODLOADLINE" {printf "  [%s] %s\n", $2, $3}' "$CLS" | head -6 >> "$sumf"
fi
if grep -q '^OTHERLINE' "$CLS"; then
  emit ""
  emit "other new lines (raw, capped):"
  awk -F'\t' '$1=="OTHERLINE" {printf "  [%s] %s\n", $2, $3}' "$CLS" | head -10 >> "$sumf"
fi

# ---------------------------------------------------------------------------
# Verdict precedence (launch-10 specific)
# ---------------------------------------------------------------------------
ORACLE_CLASS=""
REFUSAL_CLASS="-"
ST_MATCH=no
if [ "$n_shapea" -gt 0 ]; then
  VERDICT="SHAPE_A_SEEDS_GATE"
  NEXT="Shape-A refusal re-fired: the ARMED SeedTable did NOT ship in this fself (launch-10 stages the armed table + GMalloc validator; the all-4 rule refused a partial table). This is NOT the expected launch-10 terminal anymore — verify the rebuild/readback (which SeedTable.h went in?) before any in-game interpretation. Nothing bound, game continues UNMODDED; never run bridge stage-1 CONVERSION vm-scan triage on it."
elif [ "$n_refuted" -gt 0 ]; then
  VERDICT="SEEDS_REFUTED"
  NEXT="An armed seed constant is REFUTED in-game (terminal per launch, never replayed). Capture which variant: pool-never-initialized / probe-id divergence / base moved / GUA probe. Route: seed-table re-derivation with NEW ids; no bridge triage."
elif [ "$n_gmalloc" -gt 0 ]; then
  VERDICT="GMALLOC_GATE"
  NEXT="PostInitialize GMalloc throw: seeds armed (past Shape-A) but REQUIRED slot 3 unseeded/unvalidated; loud failed(1,<msg>), terminal even with exec pages fully R/W. Check the GMalloc V1/V2 validator diff actually shipped in the rebuild. NOT a conversion refusal."
elif [ "$n_fail" -gt 0 ]; then
  LASTF=$(awk -F'\t' '$1=="CLASSLINE" && $3 ~ /modbridge module_start failed: / {l=$3} END{print l}' "$CLS")
  DET=$(printf '%s' "$LASTF" | sed 's/^.* detail=//')
  CODE=$(printf '%s' "$LASTF" | sed 's/^.* code=//; s/ detail=.*//')
  LASTF_STAGE=$(printf '%s' "$LASTF" | sed 's/^modbridge module_start failed: //; s/ code=.*//')
  [ "$last_detail" = "$DET" ] && [ -n "$last_detail" ] && ST_MATCH=yes
  case "$DET" in
    *"lease coverage gap"*)                                        VSTAGE="FAILED_S6_LEASEGATE" ;;
    *"no observed mapping interval intersects"*)                   VSTAGE="FAILED_S6_CONVERSION" ;;
    *"Owned read range includes unregistered bytes"*)              VSTAGE="FAILED_S8_CONTEXT_LEASE" ;;
    *"Native module and executable paths must name"*)              VSTAGE="FAILED_S8_CONTEXT" ;;
    *"no image anchor"*|*"executable path is empty"*|*"not captured facts"*|*"vm-scan image span"*) VSTAGE="FAILED_S6_CONVERSION" ;;
    *"GMalloc is uninitialized"*)                                  VSTAGE="GMALLOC_GATE" ;;
    *"seed table incomplete"*)                                     VSTAGE="SHAPE_A_SEEDS_GATE" ;;
    *diag_channel_open_failed*)                                    VSTAGE="FAILED_S0_DIAGCHANNEL" ;;
    *)                                                             VSTAGE="FAILED_code${CODE}" ;;
  esac
  # G1: name the two refusal classes the REAL launch-10 runs exposed with the
  # oracle's own vocabulary (LAUNCH10-RESULTS §5 G1), instead of bare FAILED_code<N>.
  if [ "$VSTAGE" = "FAILED_code${CODE}" ]; then
    if [ "$LASTF_STAGE" = "mfa_activate_failed" ]; then
      VSTAGE="FAILED_B7_MFA"
      REFUSAL_CLASS="B7-EXPECTED-REFUSAL"
      ORACLE_CLASS="B7 EXPECTED-REFUSAL - FAIL with named cause; nothing downstream ran (oracle section 1 row B7)"
    elif [ "$CODE" = "3" ] && [ "$LASTF_STAGE" = "runtime_start_failed" ]; then
      VSTAGE="FAILED_E2_START"
      REFUSAL_CLASS="E2-EXPECTED-REFUSAL"
      ORACLE_CLASS="E2 EXPECTED-REFUSAL - verdict >= MINIMAL-ACCEPT where the session's conversion quartet + NO-CRASH + a fresh failed status hold (oracle section 1 row E2)"
    fi
  fi
  # G1: MINIMAL-ACCEPT state — oracle section 2: everything after conversion may
  # be ANY named failure. Require the CURRENT session's sdiag conversion quartet,
  # no D6 line in the window, no crash tail, and a fresh failed status whose
  # detail matches the refusal. E4 'activated' is not literally on file (the
  # terminal write O_TRUNC-replaces earlier acks of the same launch) — the
  # evidence-equivalence caveat rides with the verdict.
  if [ "$VSTAGE" = "FAILED_E2_START" ] && [ "$QUARTET" = "yes" ] && [ "$n_d6" -eq 0 ] \
     && [ "$n_crash_tail" -eq 0 ] && [ "$fresh" = "yes" ] && [ "$last_state" = "failed" ]; then
    VSTAGE="MINIMAL_ACCEPT_E2_REFUSAL"
    REFUSAL_CLASS="E2-EXPECTED-REFUSAL-MINIMAL-ACCEPT"
    ORACLE_CLASS="E2 EXPECTED-REFUSAL - verdict >= MINIMAL-ACCEPT (oracle section 1 row E2 + section 2 ladder); E4 'activated' not literally on file (terminal-write O_TRUNC caveat) — evidence-equivalence per LAUNCH10-RESULTS section 2"
  fi
  VERDICT="$VSTAGE"
  if [ "$n_diag22" -gt 0 ]; then
    NEXT="Diagnostic channel open failed (code -22): game continues UNMODDED. NOTE: this line only appears with the channel-postmortem 3-line patch resident — absence of this line never excludes -22. Check /data accessibility."
  elif [ "$VSTAGE" = "MINIMAL_ACCEPT_E2_REFUSAL" ]; then
    NEXT="Oracle E2 EXPECTED-REFUSAL (verdict >= MINIMAL-ACCEPT): prepare/start refused AFTER conversion (named new info in the detail). This session's supplier-diag proves the conversion quartet (extent events=$s_extent/4 + anchor_count=$s_anchor + intervals_found=$s_intervals next to lease_spans=$s_spans + cfg_events=$s_cfgev), no D6 recurrence, no crash tail, fresh failed status. CAVEAT: E4 'activated' is NOT literally in the fetched status (terminal write O_TRUNC-replaces earlier acks of the same launch) — recorded by evidence-equivalence; belt-and-braces for the next launch: fetch runtime-status.txt immediately on any failed run (or before the terminal write lands)."
    [ "$ST_MATCH" = "yes" ] && NEXT="$NEXT Status detail matches the refusal line verbatim."
  elif [ "$VSTAGE" = "FAILED_E2_START" ]; then
    NEXT="Oracle E2 EXPECTED-REFUSAL: prepare/start threw after conversion (pad profile / mod source / sink open — named new info). Label kept conservative: the >= MINIMAL-ACCEPT rung needs the current session's conversion quartet + NO-CRASH + a fresh failed status, and this fetch is missing at least one of them (quartet=$QUARTET, fresh=$fresh, last_state=$last_state). The conversion clauses may still be diag-proven for this pid — read the PIDSUM line; rung claim reserved."
  elif [ "$VSTAGE" = "FAILED_B7_MFA" ]; then
    NEXT="Oracle B7 EXPECTED-REFUSAL: MFA activation refused (Phase B) — NOTHING downstream ran (no bridge walk/lease/conversion events exist for this session; its PIDSUM/chain shows the prologue only). The named offending file is in the detail; game continues UNMODDED; loadlist/roots drifted. FAIL-with-named-cause per the oracle; no rung claim."
  elif [ "$VSTAGE" = "FAILED_S6_CONVERSION" ]; then
    NEXT="Conversion refusal at derive_readable_ranges (code=1). ATTRIBUTION: aligned_entry is DISABLED in this config (ini 1216aab6...), so this line belongs to supplier_composition — the launch-9 terminal string reappearing from the composition means the composition's standalone inputs still derive a zero-intersection extent. Report the raw line verbatim; do not replay; reconcile with worker-facts-geometry-fix evidence."
  elif [ "$VSTAGE" = "FAILED_S6_LEASEGATE" ]; then
    NEXT="r14 pre-lease coverage gate fired (MRS_ERR_CONVERSION by design): the frozen-snapshot registry does not cover a range the conversion would lease; gap + nearest span are in the bridge_lease_* tags (supplier-diag jsonl if the sink landed). This is the LOUD replacement of launch-8's deep PLH throw — treat as the r14 hardening working, then reconcile the registry derivation."
  elif [ "$VSTAGE" = "FAILED_S8_CONTEXT_LEASE" ]; then
    NEXT="Deep PLH lease throw re-appeared (code=2). The lease throw is DORMANT in r13 (launch-8's r10 threw it, pid 234) and r14 replaces it with the pre-lease gate — seeing it means the rebuild did NOT carry the r14 gate. Verify which fself actually ran (readback sha), then reconcile."
  else
    NEXT="Bridge failure line present. Stage codes 1-4 = CONVERSION/CONTEXT/START/INTERNAL (bridge.h:36-41); MRS_ERR_STOP_EXN is 5 post-r13. Raw line above is the evidence; report verbatim; do not re-stage on this signature alone."
  fi
elif [ "$fresh" = "yes" ] && [ "$last_state" = "failed" ]; then
  VERDICT="FAILED_STATUS"
  NEXT="Fresh status session failed with NO matching failure line in the fetched window: status is the session truth (O_TRUNC). Detail above is the reason (init-thread throws reach status as failed(1,<what>) without a bridge line). Report verbatim; init-thread detail NEVER routes to bridge CONVERSION triage."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "mods-started" ]; then
  VERDICT="MODS_STARTED"
  NEXT="FULL LADDER REACHED for the first time (beyond every launch-9/10 prediction — apply fresh-eyes skepticism and re-run to confirm). Check the mod-load prints section ([TownfallItemSpawner] verbatim) before any rung claim; the ge.5 gate now OPENS: only after this evidence may the user test OPTIONS in-game. Then FIRST-RUN-PLAYBOOK §5 rung ladder + F7 contingency table."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "engine-ready" ]; then
  VERDICT="ENGINE_READY"
  NEXT="Init thread finished Unreal setup — beyond prediction; mods-started ack follows immediately in-program. Re-run in ~30-60 s; if frozen here, bounded-wait hang check (F8). The ge.5 gate stays CLOSED until mods-started evidence."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "activated" ] && [ "$n_crash_tail" -gt 0 ] && [ "$n_progress" = 0 ]; then
  VERDICT="CRASH_SHAPED_SUSPECTED"
  NEXT="igf6-class candidate: fresh session still activated-only, no failure line, and the UE4SS.log new tail sits in the LoadMap/GameEngine vtable-walk vicinity — module likely died mid-init on an exec-page READ (FLAG-2), game surviving. Confirm with supplier-diag/observer tails above; if confirmed, first-in-launch exec-read evidence."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "activated" ]; then
  VERDICT="ACTIVATED"
  NEXT="Activation consumed; init thread still working (or F7/F8 hang). Re-run in ~60 s; frozen at activated with a static UE4SS.log tail = bounded-wait hang check."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "stopped" ]; then
  VERDICT="STOPPED"
  NEXT="Runtime stopped (unload/F10). Check observer runtime_stop_result above: nonzero => owner stop reported failure (MRS_ERR_STOP_EXN path)."
elif [ "$n_diagl" -gt 0 ] || [ "$n_xwp_mirror" -gt 0 ]; then
  VERDICT="CONFIG_DRIFT"
  NEXT="Disabled-plugin output in the new content (DIAG line / xwp records) with NO fresh session: the 2-plugin SHORTEST config (ini 1216aab6...) may not be what ran. Verify the ini pre-image and which PRXs executed BEFORE interpreting anything else."
elif [ $((n_shapea + n_refuted + n_seedsother + n_fail + n_gmalloc + n_bridgetag + n_bridgecfg + n_progress + n_entry4 + n_patht + n_patht_fail + n_activt + n_obs_start_ok + n_obs_start_failed + n_other_ue4ss + n_other_entry)) -gt 0 ]; then
  VERDICT="UNKNOWN_NEW_LINES"
  NEXT="New non-baseline lines exist but no staged marker or fresh session gives a verdict. Could be mid-launch: re-run in ~60 s. Compare the raw lines above against the STAGE-TABLE; anything unexpected = report raw to the controller."
else
  VERDICT="NOT_STARTED"
  NEXT="Nothing beyond baseline (or the stale launch-9 session 273 only) and no fresh status session. Records appear ~1 min after launch: wait and re-run. If still empty, treat as F0 (composition never ran): verify staging via FTP readback; the composition's module_start_entered marker (supplier-diag jsonl) is the launch counter."
fi

emit ""
emit "VERDICT: $VERDICT"
[ -n "$ORACLE_CLASS" ] && emit "ORACLE-CLASS: $ORACLE_CLASS"
emit "NEXT: $NEXT"
[ -s "$WARNF" ] && while IFS= read -r w; do emit "WARN: $w"; done < "$WARNF"
[ -s "$WARNF" ] && while IFS= read -r w; do emit "NOTE: $w"; done < "$WARNF"

cat "$sumf"
[ -n "$OUT_FILE" ] && cp "$sumf" "$OUT_FILE"

# optional machine-readable tail
if [ "${JSON_TAIL:-0}" = 1 ]; then
  # G3: per-session records (PIDJSON, emitted by classify_sdiag) + the current
  # session's sdiag facts + the vm-capacity note field (G7) so JSON-only
  # consumers see the evidence that achieved the verdict.
  SESS_JSON=$(awk -F'\t' '$1=="PIDJSON" {printf "%s%s", sep, $2; sep = ","} END {print ""}' "$CLS")
  VM_EXCEEDS=false
  is_num "$s_vme" && [ "$s_vme" -gt 16384 ] && VM_EXCEEDS=true
  printf '{"verdict":"%s","refusal_class":"%s","oracle_class":"%s","stage_chain":"%s","failure_lines":%d,"diag22":%d,"gmalloc":%d,"shape_a":%d,"seeds_refuted":%d,"stage_tags":%d,"bridge_cfg":%d,"observer":{"start_ok":%d,"start_failed":%d,"stop_result":%d},"progress":%d,"mod_load_lines":%d,"igw":%d,"crash_tail":%d,"entry4_lines":%d,"pathtest_stamps":%d,"pathtest_open_failed":%d,"diag_lines":%d,"xwp_mirror":%d,"status":{"present":%s,"session_pid":"%s","session_ms":"%s","fresh":"%s","last_state":"%s","last_code":"%s","ladder":"%s"},"sdiag":{"present":%s,"events":%d,"sessions":%d,"pid_first":"%s","pid_last":"%s","current_pid":"%s","chain":"%s","start_failed_total":%d,"baseline_cut":"%s","quartet":"%s","current":{"events":"%s","start_failed":"%s","mfa_activate_failed":"%s","runtime_start_failed":"%s","extent_events":"%s","cfg_events":"%s","lease_spans":"%s","lease_reg":"%s","anchor_count":"%s","intervals_found":"%s","vmentry":"%s","vm_walk_stop":"%s","exepath_source":"%s","cfg_gamedir":"%s","module_start_return":"%s"}},"sessions":[%s],"vm_capacity":{"pinned_kBridgeVmmapEntries":16384,"current_vmentry":"%s","current_vm_walk_stop":"%s","exceeds_pinned":%s},"sizes":{"ue4ss":%d,"entry":%d,"xwp":%d,"sdiag":%d,"baselines":{"ue4ss":%d,"entry":%d,"status":%d,"xwp":%d,"session_pid":%s,"session_ms":%s}},"config":"shortest-2plugin-1216aab6"}\n' \
    "$VERDICT" "$REFUSAL_CLASS" "$ORACLE_CLASS" "$stage_chain" \
    "$n_fail" "$n_diag22" "$n_gmalloc" "$n_shapea" "$n_refuted" "$n_bridgetag" "$n_bridgecfg" \
    "$n_obs_start_ok" "$n_obs_start_failed" "$n_obs_stop" \
    "$n_progress" "$n_modload" "$n_igw" "$n_crash_tail" "$n_entry4" "$n_patht" "$n_patht_fail" "$n_diagl" "$n_xwp_mirror" \
    "$([ "$ok_status" = 1 ] && echo true || echo false)" "$session_pid" "$session_ms" "$fresh" "$last_state" "$last_code" "$ladder" \
    "$([ "$ok_sdiag" = 1 ] && echo true || echo false)" "${sd_events:-0}" "${n_sessions:-0}" "${sd_pid_first:--}" "${sd_pid_last:--}" "${CUR_PID:--}" "$sc_sdiag" "${sd_startfail_total:-0}" "$([ "$SDIAG_CUT" = 1 ] && echo yes || echo no)" "$QUARTET" \
    "${s_events:--}" "${s_startfail:--}" "${s_mfa:--}" "${s_rscode:--}" "${s_extent:--}" "${s_cfgev:--}" "${s_spans:--}" "${s_reg:--}" "${s_anchor:--}" "${s_intervals:--}" "${s_vme:--}" "${s_walkstop:--}" "${s_exesrc:--}" "${s_cfgdir:--}" "${s_modret:--}" \
    "$SESS_JSON" \
    "${s_vme:--}" "${s_walkstop:--}" "$VM_EXCEEDS" \
    "$sz_ue4ss" "$sz_entry" "$sz_xwp" "$sz_sdiag" \
    "$BASE_UE4SS" "$BASE_ENTRY" "$BASE_STATUS" "$BASE_XWP" "$BASE_SESS_PID" "$BASE_SESS_MS"
fi

exit 0
