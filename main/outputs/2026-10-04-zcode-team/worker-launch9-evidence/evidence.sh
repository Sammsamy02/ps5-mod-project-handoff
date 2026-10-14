#!/bin/sh
# evidence.sh — composition-6 (launch 9) post-launch evidence collector.
# worker-launch9-evidence · 2026-10-04 · host-side; NO console contact except
# when the CONTROLLER runs live mode after the user confirms "in game".
#
# Pulls the three evidence files over anonymous FTP (ONE fresh curl connection
# per fetch, bounded timeouts), extracts the new lines of the append-only
# UE4SS.log and townfall_runtime_aligned_entry.log against the
# controller-captured pre-launch baseline sizes (UE4SS.log 2892 B,
# aligned-entry log 2210306 B), classifies them against the launch-9
# signatures table, parses runtime-status.txt, and prints ONE one-page
# summary ending in VERDICT + NEXT.
#
# Evidence sources (console):
#   ftp://ps5-host-not-configured.invalid:2121/data/plugins/UE4SS.log
#   ftp://ps5-host-not-configured.invalid:2121/data/modbridge/roots/PPSA33286/runtime-status.txt
#   ftp://ps5-host-not-configured.invalid:2121/data/townfall_runtime_aligned_entry.log
#
# Signatures table (transcribed from worker-launch9-prediction
# LAUNCH9-PREDICTION.md + worker-evidence-tools-final; provenance in
# CLAIM.json signatures_table_provenance):
#   (a) PASS markers — conversion/roots/lease/D2 diagnostics:
#         bridge_vmentry_entries, anchor_* (anchor_verify, anchor_ladder_verify,
#         anchor_site_verify, anchor_windows, anchor_mm, anchor_match,
#         anchor_cov, anchor_c_either, anchor_mappings), lmf_*
#         (lmf_to_native_loader_facts, lmf_vmentry_cap, lmf_provider_code,
#         lmf_vm_scan_cap, lmf_linkmap_cap, lmf_identity_source,
#         lmf_scan_image_bases), plus bridge_cfg_gamedir=/bridge_cfg_workdir=
#         and bridge_exepath_source= (LAUNCH9-PREDICTION §2 E2–E4).
#   (b) SHAPE-A TERMINAL (EXPECTED for this launch — NOT a new failure):
#         "[Seeds] Required-slot seed table incomplete: present
#         [g_uobject_array fname_ctor_wchar static_construct_object_internal]
#         missing [gmalloc]" (LAUNCH9-PREDICTION §4 #2; verbatim fixture in
#         worker-evidence-tools-final/fixtures/seeds_refusal_launch9).
#   (c) [Seeds] other paths: "[Seeds] REFUTED: ..." variants (terminal per
#         launch, §2 I3/§4 #5); ANY OTHER "[Seeds]" line is reported verbatim
#         as seeds-other (armed-path wording not verbatim-pinned — honesty).
#   (d) engine-ready / mods-started progress markers (§2 I13 ack chain) and
#         "[TownfallItemSpawner]" Lua prints (rung-4 evidence).
#   (e) runtime-status.txt grammar: "modforge-runtime-status\t1" header,
#         "session\t<pid>\t<monotonic-ms>",
#         "state\t<seq>\t<state>\t<code>\t<detail>"; ladder activated ->
#         engine-ready -> mods-started; terminal failed/stopped outside it.
#   (f) entry r4 freshness prefix "[t=<ms>ms pid=<pid>]"
#         (runtime_module_entry_r4.c :190-202) — only prefixed aligned-log
#         lines are entry-r4-era; prefixless lines predate r4.
#   (g) bridge r13 markers: failure grammar
#         "modbridge module_start failed: <stage> code=<n> detail=<...>"
#         (entry :166-192); stage codes 1-4 CONVERSION/CONTEXT/START/INTERNAL
#         (bridge.h:36-41) + MRS_ERR_* tokens (MRS_ERR_STOP_EXN renumbered
#         2->5, worker-bridge-r13-polish P3-4); diag_channel_open_failed
#         code=-22 is SILENT in the delivered entry (channel-postmortem -22);
#         observer event grammar
#         "[PPSA33286] modbridge-runtime-aligned-entry/v1 build=<hex> <tag>
#         value=0x<hex>" (tags runtime_start_ok / runtime_start_failed /
#         module_start_return / runtime_stop_result / ...).
#   Extra evidence markers always reported: IGW exec-write probe
#         {W_WORKS, W_FAILS_R_WORKS, BOTH_FAIL} (worker-exec-writability §3);
#         crash-tail vicinity "LoadMap"/"Finding GameEngine" (igf6 class,
#         §2 I6/§4 #3); "GMalloc is uninitialized" (PostInitialize gate,
#         §4 #1).
#
# Exit codes: 0 classified (INCLUDING the expected SHAPE_A_SEEDS_GATE stop),
# 2 usage error, 4 FETCH_FAILED (no evidence reachable — retry guidance).
#
# Modes: live FTP (controller only) | --from-dir DIR (host fixtures:
# UE4SS.log, runtime-status.txt, townfall_runtime_aligned_entry.log) |
# --dry-run (print curl commands). POSIX sh + awk; no other dependencies.

set -u

HOST=ps5-host-not-configured.invalid
PORT=2121
TIMEOUT=60
MAX_BYTES=$((32 * 1024 * 1024))
BASE_UE4SS=2892        # controller-captured pre-launch-9 baseline size (B)
BASE_ALIGNED=2210306   # controller-captured pre-launch-9 baseline size (B)
FROM_DIR=""
DRY_RUN=0
OUT_FILE=""

usage() {
  cat <<'USAGE'
usage: evidence.sh [--host H] [--port P] [--timeout S] [--max-bytes N]
                   [--baseline-ue4ss-bytes N] [--baseline-aligned-bytes N]
                   [--from-dir DIR] [--dry-run] [--out FILE] [--json-tail]
  --from-dir DIR   read UE4SS.log, runtime-status.txt and
                   townfall_runtime_aligned_entry.log from a local directory
                   (host fixture mode; NO FTP)
  --dry-run        print the exact curl commands and exit
  --out FILE       also write the one-page summary to FILE
  --json-tail      append a machine-readable JSON object after the summary
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    --host) HOST="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --max-bytes) MAX_BYTES="$2"; shift 2 ;;
    --baseline-ue4ss-bytes) BASE_UE4SS="$2"; shift 2 ;;
    --baseline-aligned-bytes) BASE_ALIGNED="$2"; shift 2 ;;
    --from-dir) FROM_DIR="$2"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --out) OUT_FILE="$2"; shift 2 ;;
    --json-tail) JSON_TAIL=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) echo "evidence.sh: unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

UE4SS_PATH=/data/plugins/UE4SS.log
STATUS_PATH=/data/modbridge/roots/PPSA33286/runtime-status.txt
ALIGNED_PATH=/data/townfall_runtime_aligned_entry.log

# ---------------------------------------------------------------------------
# Fetch layer
# ---------------------------------------------------------------------------
fetch_file() { # $1=remote path  $2=local output  -> 0 ok / 1 fail (err in $ERR)
  if command curl >/dev/null 2>&1; then :; else ERRV="curl not found on PATH"; return 1; fi
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

TMPDIR_E=""
cleanup() { [ -n "$TMPDIR_E" ] && rm -rf "$TMPDIR_E"; }
trap cleanup EXIT INT TERM
TMPDIR_E=$(mktemp -d "${TMPDIR:-/tmp}/evidence-launch9.XXXXXX") || exit 4

if [ "$DRY_RUN" = 1 ]; then
  echo "curl -fsS --connect-timeout 10 --max-time $TIMEOUT --max-filesize $MAX_BYTES ftp://$HOST:$PORT$UE4SS_PATH"
  echo "curl -fsS --connect-timeout 10 --max-time $TIMEOUT --max-filesize $MAX_BYTES ftp://$HOST:$PORT$STATUS_PATH"
  echo "curl -fsS --connect-timeout 10 --max-time $TIMEOUT --max-filesize $MAX_BYTES ftp://$HOST:$PORT$ALIGNED_PATH"
  exit 0
fi

UE4SS_F="$TMPDIR_E/UE4SS.log"
STATUS_F="$TMPDIR_E/runtime-status.txt"
ALIGNED_F="$TMPDIR_E/townfall_runtime_aligned_entry.log"

ok_ue4ss=1; ok_status=1; ok_aligned=1
err_ue4ss=""; err_status=""; err_aligned=""
src_ue4ss=ftp; src_status=ftp; src_aligned=ftp

if [ -n "$FROM_DIR" ]; then
  for pair in "UE4SS.log:$UE4SS_F" "runtime-status.txt:$STATUS_F" \
              "townfall_runtime_aligned_entry.log:$ALIGNED_F"; do
    name=${pair%%:*}; dest=${pair#*:}
    if [ -f "$FROM_DIR/$name" ]; then cp "$FROM_DIR/$name" "$dest"; else :; fi
  done
  [ -f "$UE4SS_F" ]   || { ok_ue4ss=0;   err_ue4ss="fixture missing: $FROM_DIR/UE4SS.log"; }
  [ -f "$STATUS_F" ]  || { ok_status=0;  err_status="fixture missing: $FROM_DIR/runtime-status.txt"; }
  [ -f "$ALIGNED_F" ] || { ok_aligned=0; err_aligned="fixture missing: $FROM_DIR/townfall_runtime_aligned_entry.log"; }
  src_ue4ss=dir; src_status=dir; src_aligned=dir
else
  fetch_file "$UE4SS_PATH" "$UE4SS_F"     || { ok_ue4ss=0;   err_ue4ss="$ERRV (curl exit $?)"; }
  fetch_file "$STATUS_PATH" "$STATUS_F"   || { ok_status=0;  err_status="$ERRV (curl exit $?)"; }
  fetch_file "$ALIGNED_PATH" "$ALIGNED_F" || { ok_aligned=0; err_aligned="$ERRV (curl exit $?)"; }
fi

# ---------------------------------------------------------------------------
# Classify the new lines (signatures table (a)-(g) + extra evidence markers)
# ---------------------------------------------------------------------------
# Class codes:
#   PASS_MARKER   (a) conversion/roots/lease/D2 diagnostics
#   SHAPE_A       (b) expected terminal seeds-incomplete line
#   SEEDS_REFUTED (c) [Seeds] REFUTED variants
#   SEEDS_OTHER   (c) any other [Seeds] line (verbatim report)
#   PROGRESS      (d) engine-ready/mods-started/ItemSpawner prints
#   FAIL_LINE     (g) modbridge module_start failed: ...
#   MRS_TOKEN     (g) MRS_ERR_* / MRS_OK tokens
#   DIAG22        (g) diag_channel_open_failed
#   BRIDGE_CFG    (a/g) bridge_cfg_gamedir=/workdir=/bridge_exepath_source=
#   OBSERVER      (g) aligned-entry observer event line
#   ENTRY4        (f) [t=<ms>ms pid=<pid>] prefixed line (freshness oracle)
#   GMALLOC       extra: PostInitialize GMalloc throw
#   CRASH_TAIL    extra: LoadMap / Finding GameEngine vicinity
#   IGW           extra: exec-write probe verdict markers
#   OTHER         anything else new (reported raw, capped)

classify_stream() { # $1=file  $2=stream name (ue4ss|aligned)
  awk -v stream="$2" '
    function bump(c,  n) { cnt[c]++; if (!(c in first)) first[c] = $0; last[c] = $0 }
    function notable(c)  { if (notables < 40) { nc[notables] = c; nl[notables] = $0; notables++ } }
    {
      line = $0
      # ---- (f) entry r4 freshness prefix --------------------------------
      if (line ~ /^\[t=[0-9]+ms pid=[0-9]+\] /) { bump("ENTRY4"); sub(/^\[[^]]*\] /, "", line) }
      # ---- (g) bridge failure grammar (after prefix strip) --------------
      if (line ~ /^modbridge module_start failed: /) {
        bump("FAIL_LINE"); notable("FAIL_LINE")
        if (line ~ /diag_channel_open_failed/) { bump("DIAG22"); notable("DIAG22") }
        print "CLASSLINE\t" stream "\t" line
        next
      }
      # ---- (b)(c) [Seeds] family ----------------------------------------
      if (line ~ /\[Seeds\]/) {
        if (line ~ /Required-slot seed table incomplete/) { bump("SHAPE_A"); notable("SHAPE_A") }
        else if (line ~ /\[Seeds\] REFUTED/)              { bump("SEEDS_REFUTED"); notable("SEEDS_REFUTED") }
        else                                              { bump("SEEDS_OTHER"); notable("SEEDS_OTHER") }
        print "CLASSLINE\t" stream "\t" line
        next
      }
      # ---- extra: GMalloc PostInitialize gate ---------------------------
      if (line ~ /GMalloc is uninitialized/) { bump("GMALLOC"); notable("GMALLOC"); print "CLASSLINE\t" stream "\t" line; next }
      # ---- (a)+(g) pass/diagnostic markers ------------------------------
      if (line ~ /bridge_vmentry_entries/) { bump("PASS_MARKER") }
      if (line ~ /anchor_(verify|ladder_verify|site_verify|windows|mm|match|cov|c_either|mappings)/) { bump("PASS_MARKER") }
      if (line ~ /lmf_(to_native_loader_facts|vmentry_cap|provider_code|vm_scan_cap|linkmap_cap|identity_source|scan_image_bases)/) { bump("PASS_MARKER") }
      if (line ~ /bridge_cfg_gamedir=|bridge_cfg_workdir=|bridge_exepath_source=/) { bump("BRIDGE_CFG"); bump("PASS_MARKER") }
      if (line ~ /MRS_ERR_[A-Z_]+|MRS_OK/) { bump("MRS_TOKEN") }
      # ---- (g) observer events ------------------------------------------
      if (line ~ /\[PPSA33286\] modbridge-runtime-aligned-entry\/v1 build=/) {
        bump("OBSERVER")
        if (line ~ /runtime_start_ok/)         { bump("OBS_START_OK"); notable("OBS_START_OK") }
        if (line ~ /runtime_start_failed/)     { bump("OBS_START_FAILED"); notable("OBS_START_FAILED") }
        if (line ~ /runtime_stop_result/)      { bump("OBS_STOP_RESULT"); notable("OBS_STOP_RESULT") }
        if (line ~ /module_start_return/)      { bump("OBS_MODULE_RETURN") }
        if (line ~ /mfa_activate_failed/)      { bump("OBS_MFA_FAILED"); notable("OBS_MFA_FAILED") }
        print "CLASSLINE\t" stream "\t" line
        next
      }
      # ---- extra: IGW exec-write probe verdict markers -------------------
      if (line ~ /W_WORKS|W_FAILS_R_WORKS|BOTH_FAIL/) { bump("IGW"); notable("IGW"); print "CLASSLINE\t" stream "\t" line; next }
      # ---- (d) progress markers ------------------------------------------
      if (line ~ /engine-ready|mods-started/) { bump("PROGRESS") }
      if (line ~ /TownfallItemSpawner/)       { bump("PROGRESS"); notable("PROGRESS") }
      # ---- extra: crash-tail vicinity (igf6 class) ------------------------
      if (line ~ /[Ll]oadMap|Finding GameEngine/) { bump("CRASH_TAIL"); tail[ntail++ % 5] = line }
      # ---- UE4SS banner / everything else ---------------------------------
      if (line ~ /UE4SS - v|root directory:|mods directories:|Setting up mods|Native console, GUI, crash dumping/) { bump("BANNER"); next }
      if (line ~ /^[[:space:]]*$/) next
      bump("OTHER")
      if (noth < 10) { print "OTHERLINE\t" stream "\t" line; noth++ }
      n_other++
    }
    END {
      for (c in cnt) printf "COUNT\t%s\t%s\t%d\t%s\n", stream, c, cnt[c], first[c]
      printf "TAIL\t%s\t", stream
      for (i in tail) printf " ~ %s", tail[i]
      printf "\n"
    }
  ' "$1"
}

# ---------------------------------------------------------------------------
# Status parse
# ---------------------------------------------------------------------------
status_summary() { # $1=status file -> key=value lines
  awk '
    /^modforge-runtime-status\t1$/ { print "header_ok=1"; next }
    /^session\t/ {
      n = split($0, p, "\t")
      if (n >= 3) { print "session_pid=" p[2]; print "session_ms=" p[3] }
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
    }' "$1"
}

# ---------------------------------------------------------------------------
# Verdict + one-page summary
# ---------------------------------------------------------------------------
sumf="$TMPDIR_E/summary.txt"
: > "$sumf"

emit() { echo "$1" >> "$sumf"; }

total_fetched=$((ok_ue4ss + ok_status + ok_aligned))
if [ "$total_fetched" -eq 0 ]; then
  emit "== launch-9 evidence =="
  emit "fetch UE4SS.log   FAIL ($err_ue4ss)"
  emit "fetch status      FAIL ($err_status)"
  emit "fetch aligned log FAIL ($err_aligned)"
  emit ""
  emit "VERDICT: FETCH_FAILED"
  emit "NEXT: RETRY — records appear on FTP ~1 min after launch; wait ~60 s and re-run. If a second retry still fails, check the console FTP service/network; do NOT touch staging on this signature."
  finish() { cat "$sumf"; [ -n "$OUT_FILE" ] && cp "$sumf" "$OUT_FILE"; exit 4; }
  finish
fi

# baseline size guards
WARNF="$TMPDIR_E/warn.txt"; : > "$WARNF"
sz_ue4ss=0;   [ -f "$UE4SS_F" ]   && sz_ue4ss=$(wc -c < "$UE4SS_F" | tr -d ' ')
sz_aligned=0; [ -f "$ALIGNED_F" ] && sz_aligned=$(wc -c < "$ALIGNED_F" | tr -d ' ')
[ "$ok_ue4ss" = 1 ]   && [ "$sz_ue4ss" -lt "$BASE_UE4SS" ] \
  && echo "UE4SS.log fetched ${sz_ue4ss}B < baseline ${BASE_UE4SS}B (log replaced/truncated or wrong file fetched)" >> "$WARNF"
[ "$ok_aligned" = 1 ] && [ "$sz_aligned" -lt "$BASE_ALIGNED" ] \
  && echo "aligned-entry log fetched ${sz_aligned}B < baseline ${BASE_ALIGNED}B (log replaced/truncated or wrong file fetched)" >> "$WARNF"

# classify
CLS="$TMPDIR_E/classes.txt"
: > "$CLS"
[ "$ok_ue4ss" = 1 ]   && extract_new_lines "$UE4SS_F" "$BASE_UE4SS"   | classify_stream /dev/stdin ue4ss   >> "$CLS"
[ "$ok_aligned" = 1 ] && extract_new_lines "$ALIGNED_F" "$BASE_ALIGNED" | classify_stream /dev/stdin aligned >> "$CLS"

count_class() { awk -F'\t' -v c="$1" '$1=="COUNT" && $3==c {s+=$4} END{print s+0}' "$CLS"; }
first_of()    { awk -F'\t' -v c="$1" '$1=="COUNT" && $3==c {print $5; exit}' "$CLS"; }

n_shapea=$(count_class SHAPE_A)
n_refuted=$(count_class SEEDS_REFUTED)
n_seedsother=$(count_class SEEDS_OTHER)
n_fail=$(count_class FAIL_LINE)
n_diag22=$(count_class DIAG22)
n_gmalloc=$(count_class GMALLOC)
n_pass=$(count_class PASS_MARKER)
n_bridgecfg=$(count_class BRIDGE_CFG)
n_mrs=$(count_class MRS_TOKEN)
n_obs_start_ok=$(count_class OBS_START_OK)
n_obs_start_failed=$(count_class OBS_START_FAILED)
n_obs_stop=$(count_class OBS_STOP_RESULT)
n_obs_mfa_failed=$(count_class OBS_MFA_FAILED)
n_progress=$(count_class PROGRESS)
n_igw=$(count_class IGW)
n_crash_tail=$(count_class CRASH_TAIL)
n_entry4=$(count_class ENTRY4)
n_other_ue4ss=$(awk -F'\t' '$1=="COUNT" && $2=="ue4ss" && $3=="OTHER" {print $4}' "$CLS")
[ -n "$n_other_ue4ss" ] || n_other_ue4ss=0

# status facts
if [ "$ok_status" = 1 ]; then
  ST=$(status_summary "$STATUS_F")
  last_state=$(printf '%s\n' "$ST" | sed -n 's/^last_state=//p')
  last_code=$(printf '%s\n' "$ST" | sed -n 's/^last_code=//p')
  last_detail=$(printf '%s\n' "$ST" | sed -n 's/^last_detail=//p')
  session_pid=$(printf '%s\n' "$ST" | sed -n 's/^session_pid=//p')
  session_ms=$(printf '%s\n' "$ST" | sed -n 's/^session_ms=//p')
  ladder=$(printf '%s\n' "$ST" | sed -n 's/^ladder=//p')
  n_states=$(printf '%s\n' "$ST" | sed -n 's/^nstates=//p')
else
  last_state=""; last_code=""; last_detail=""; session_pid=""; session_ms=""
  ladder=""; n_states=0
fi
# freshness: session monotonic-ms after the path-test probe (playbook O2/O3
# oracle, worker-post-launch-tools threshold) — proves post-path-test, NOT
# launch attribution on its own.
FRESH_MS=8866785
fresh=no
case "$session_ms" in ''|*[!0-9]*) fresh="unknown" ;; *) [ "$session_ms" -gt "$FRESH_MS" ] && fresh=yes ;; esac

# ---------------------------------------------------------------------------
# One-page summary
# ---------------------------------------------------------------------------
emit "== launch-9 (composition-6) post-launch evidence =="
if [ "$ok_ue4ss" = 1 ]; then emit "fetch ue4ss.log   OK ${sz_ue4ss}B (${src_ue4ss})"; else emit "fetch ue4ss.log   FAIL (${err_ue4ss})"; fi
if [ "$ok_status" = 1 ]; then emit "fetch status      OK (${src_status})"; else emit "fetch status      FAIL (${err_status})"; fi
if [ "$ok_aligned" = 1 ]; then emit "fetch aligned log OK ${sz_aligned}B (${src_aligned})"; else emit "fetch aligned log FAIL (${err_aligned})"; fi
emit "baseline: UE4SS.log >= ${BASE_UE4SS}B, aligned >= ${BASE_ALIGNED}B (controller-captured pre-launch)"
[ -s "$WARNF" ] && while IFS= read -r w; do emit "WARN: $w"; done < "$WARNF"
emit ""
emit "signatures (new lines vs baseline):"
emit "  (a) pass markers conv/roots/lease/D2 : $n_pass (bridge_cfg/exepath diag lines: $n_bridgecfg)"
emit "  (b) Shape-A seeds-incomplete line    : $n_shapea  [EXPECTED for this launch]"
emit "  (c) [Seeds] REFUTED / other          : $n_refuted / $n_seedsother"
emit "  (d) progress markers                 : $n_progress"
emit "  (g) failure lines                    : $n_fail (diag_channel_open_failed/-22: $n_diag22)"
emit "  (g) MRS_* tokens / observer events   : $n_mrs / start_ok=$n_obs_start_ok start_failed=$n_obs_start_failed stop_result=$n_obs_stop mfa_failed=$n_obs_mfa_failed"
emit "  (f) entry-r4 [t= prefixed lines      : $n_entry4 (freshness oracle)"
emit "  IGW exec-write probe markers         : $n_igw; GMalloc throw lines: $n_gmalloc; LoadMap-tail lines: $n_crash_tail"
if [ "$ok_status" = 1 ]; then
  emit ""
  emit "status: pid=$session_pid ms=$session_ms fresh=$fresh states=$n_states ladder=[$ladder ] last=$last_state code=$last_code"
  [ -n "$last_detail" ] && emit "status detail: ${last_detail}"
fi

# notable verbatim lines (from CLASSLINE records, capped)
if grep -q '^CLASSLINE' "$CLS"; then
  emit ""
  emit "notable lines (verbatim, capped):"
  awk -F'\t' '$1=="CLASSLINE" {printf "  [%s] %s\n", $2, $3}' "$CLS" | head -12 >> "$sumf"
fi
# other new lines (raw, capped) — the UNKNOWN/NOT_STARTED guidance points here
if grep -q '^OTHERLINE' "$CLS"; then
  emit ""
  emit "other new lines (raw, capped):"
  awk -F'\t' '$1=="OTHERLINE" {printf "  [%s] %s\n", $2, $3}' "$CLS" >> "$sumf"
fi

# ---------------------------------------------------------------------------
# Verdict precedence (launch-9 specific)
# ---------------------------------------------------------------------------
if [ "$n_shapea" -gt 0 ]; then
  VERDICT="SHAPE_A_SEEDS_GATE"
  NEXT="EXPECTED-PREDICTED-STOP (LAUNCH9-PREDICTION §4 #2): the all-4 generator rule refused the 3-slot Shape-A table; nothing bound, game continues UNMODDED, this is NOT a launch failure and NOT a bridge/conversion problem — never run bridge stage-1 CONVERSION vm-scan triage on it. Products of this launch: pass-marker counts above (first console evidence for r11+D2+configured roots), IGW markers, observer/fc1/qdrv1 records. Next work: slot-3 (GMalloc) seed derivation for launch 10 (worker-seed-table-derivation / LAUNCH10 matrix); a NEW launch is a NEW evidence window."
elif [ "$n_refuted" -gt 0 ]; then
  VERDICT="SEEDS_REFUTED"
  NEXT="Seed constants REFUTED in-game (LAUNCH9-PREDICTION §4 #5, terminal per launch, never replayed). Capture which variant: pool-never-initialized / probe-id divergence / base moved / GUA probe. Route: seed-table re-derivation; no bridge triage."
elif [ "$n_gmalloc" -gt 0 ]; then
  VERDICT="GMALLOC_GATE"
  NEXT="PostInitialize GMalloc throw (LAUNCH9-PREDICTION §4 #1): seeds armed (past Shape A) but REQUIRED slot 3 unseeded; loud failed(1,<msg>), terminal even with exec pages fully R/W. Route: slot-3 derivation. NOT a conversion refusal."
elif [ "$n_fail" -gt 0 ]; then
  LASTF=$(awk -F'\t' '$1=="CLASSLINE" && $3 ~ /^modbridge module_start failed: / {l=$3} END{print l}' "$CLS")
  STAGE=$(printf '%s' "$LASTF" | sed 's/^modbridge module_start failed: //; s/ code=.*//')
  VERDICT="FAILED_${STAGE}"
  if [ "$n_diag22" -gt 0 ]; then
    NEXT="Diagnostic channel open failed (code -22): game continues UNMODDED. NOTE: this line only appears with the channel-postmortem 3-line patch resident — the delivered entry is SILENT on -22; absence of this line never excludes -22. Check /data/townfall_runtime_aligned_entry.log accessibility."
  else
    NEXT="Bridge/entry failure line present. Stage codes 1-4 = CONVERSION/CONTEXT/START/INTERNAL (bridge.h:36-41); MRS_ERR_STOP_EXN is 5 post-r13. Report the raw line verbatim to the controller; do not re-stage on this signature alone."
  fi
elif [ "$fresh" = "yes" ] && [ "$last_state" = "failed" ]; then
  VERDICT="FAILED_STATUS"
  NEXT="Status session failed with NO matching UE4SS.log failure line: status is the session truth (O_TRUNC). Detail above is the reason (init-thread throws reach status as failed(1,<what>) without a bridge line, runtime_owner ack path). Report verbatim; init-thread detail NEVER routes to bridge CONVERSION triage."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "mods-started" ]; then
  VERDICT="MODS_STARTED"
  NEXT="FULL LADDER REACHED — beyond the launch-9 prediction (LAUNCH9-PREDICTION §5 said engine-ready was not the expected outcome; treat with fresh-eyes skepticism and re-run to confirm). Check [TownfallItemSpawner] prints (count above) before any rung claim; then FIRST-RUN-PLAYBOOK §5 rung ladder + the F7 contingency table (§3)."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "engine-ready" ]; then
  VERDICT="ENGINE_READY"
  NEXT="Init thread finished Unreal setup — beyond prediction (§5); mods-started ack follows immediately in-program. Re-run in ~30-60 s; if frozen here, bounded-wait hang check (F8)."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "activated" ] && [ "$n_crash_tail" -gt 0 ] && [ "$n_progress" = 0 ]; then
  VERDICT="CRASH_SHAPED_SUSPECTED"
  NEXT="igf6-class candidate (LAUNCH9-PREDICTION §4 #3): fresh session still activated-only, no failure line, and the UE4SS.log new tail sits in the LoadMap/GameEngine vtable-walk vicinity — module likely died mid-init on the first exec-page READ (FLAG-2), game surviving. Confirm with observer/qdrv1 tails and the UE4SS.log tail above; if confirmed, first-in-launch exec-read evidence."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "activated" ]; then
  VERDICT="ACTIVATED"
  NEXT="Activation consumed; init thread still working (or F7/F8 hang). Re-run in ~60 s; frozen at activated with a static UE4SS.log tail = bounded-wait hang check."
elif [ "$fresh" = "yes" ] && [ "$last_state" = "stopped" ]; then
  VERDICT="STOPPED"
  NEXT="Runtime stopped (unload/F10). Check observer runtime_stop_result above: nonzero => owner stop reported failure (MRS_ERR_STOP_EXN path)."
elif [ $((n_shapea + n_refuted + n_seedsother + n_fail + n_gmalloc + n_pass + n_progress + n_entry4 + n_other_ue4ss)) -gt 0 ]; then
  VERDICT="UNKNOWN_NEW_LINES"
  NEXT="New non-baseline lines exist but status gives no verdict (stale/absent session). Could be mid-launch: re-run in ~60 s. Compare lines above against LAUNCH9-PREDICTION §2; anything unexpected = report raw to the controller."
else
  VERDICT="NOT_STARTED"
  NEXT="Nothing beyond baseline (or stale probe-era repeats only) and no fresh status session. Records appear ~1 min after launch: wait and re-run. If still empty, treat as F0 (composition never ran): verify staging via FTP readback + ploader logs; observer module_start_entered count in the aligned log is the launch counter."
fi

emit ""
emit "VERDICT: $VERDICT"
emit "NEXT: $NEXT"
[ -s "$WARNF" ] && while IFS= read -r w; do emit "NOTE: $w"; done < "$WARNF"

cat "$sumf"
[ -n "$OUT_FILE" ] && cp "$sumf" "$OUT_FILE"

# optional machine-readable tail
if [ "${JSON_TAIL:-0}" = 1 ]; then
  printf '{"verdict":"%s","shape_a":%d,"seeds_refuted":%d,"seeds_other":%d,"failure_lines":%d,"diag22":%d,"gmalloc":%d,"pass_markers":%d,"bridge_cfg":%d,"mrs_tokens":%d,"observer":{"start_ok":%d,"start_failed":%d,"stop_result":%d,"mfa_failed":%d},"progress":%d,"igw":%d,"crash_tail":%d,"entry4_lines":%d,"status":{"present":%s,"session_pid":"%s","session_ms":"%s","fresh":"%s","last_state":"%s","last_code":"%s","ladder":"%s"},"sizes":{"ue4ss":%d,"aligned":%d,"baseline_ue4ss":%d,"baseline_aligned":%d}}\n' \
    "$VERDICT" "$n_shapea" "$n_refuted" "$n_seedsother" "$n_fail" "$n_diag22" "$n_gmalloc" "$n_pass" "$n_bridgecfg" "$n_mrs" \
    "$n_obs_start_ok" "$n_obs_start_failed" "$n_obs_stop" "$n_obs_mfa_failed" \
    "$n_progress" "$n_igw" "$n_crash_tail" "$n_entry4" \
    "$([ "$ok_status" = 1 ] && echo true || echo false)" "$session_pid" "$session_ms" "$fresh" "$last_state" "$last_code" "$ladder" \
    "$sz_ue4ss" "$sz_aligned" "$BASE_UE4SS" "$BASE_ALIGNED"
fi

exit 0
