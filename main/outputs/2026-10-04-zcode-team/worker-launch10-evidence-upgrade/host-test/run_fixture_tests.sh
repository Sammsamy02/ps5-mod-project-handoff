#!/bin/sh
# run_fixture_tests.sh — host fixture tests for evidence.sh (launch-10
# shortest-config classifier, worker-launch10-evidence-upgrade).
#
# NO console contact: evidence.sh is invoked only in --from-dir mode.
# Fixture bodies are built from the REAL controller-captured launch10-prep
# baseline files (read-only use; never modified) + synthetic appended new
# lines, so the pinned size/sha drift guard and the head-window byte compare
# are exercised against the exact bytes they will meet on launch day.
# Asserts: baseline rebase, verdict precedence (launch-10 set), r13/r14
# stage classification (launch-9 terminal chain + hypothetical launch-10
# success chain), per-writer timing extraction (2 writers, different
# epochs/units), and the baseline-drift guards.
# Writes a receipt to receipts/fixture-test-pass.txt. 0 failures required.

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
EV="$HERE/../evidence.sh"
FIXROOT="$HERE/../fixtures"
RECEIPT="$HERE/../receipts/fixture-test-pass.txt"
# Real launch10-prep baselines (controller-captured; read-only).
PREP="/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-prep"

chmod +x "$EV" 2>/dev/null || true
mkdir -p "$FIXROOT" "$HERE/../receipts"

pass=0; fail=0

check() { # $1=case  $2=desc  $3=pattern (grep -q against last run's output)
  if echo "$OUT" | grep -q "$3"; then pass=$((pass+1)); echo "PASS [$1] $2"
  else fail=$((fail+1)); echo "FAIL [$1] $2 (wanted /$3/)"; fi
}
check_absent() { # $1=case $2=desc $3=pattern that must NOT appear
  if echo "$OUT" | grep -q "$3"; then fail=$((fail+1)); echo "FAIL [$1] $2 (must be absent: /$3/)"
  else pass=$((pass+1)); echo "PASS [$1] $2 (absent: /$3/)"; fi
}
check_rc() { # $1=case $2=desc $3=expected rc
  if [ "$RC" = "$3" ]; then pass=$((pass+1)); echo "PASS [$1] $2 (rc=$RC)"
  else fail=$((fail+1)); echo "FAIL [$1] $2 (wanted rc=$3 got $RC)"; fi
}
run_case() { # $1=case name  $2=optional extra args
  OUT=$(sh "$EV" --from-dir "$FIXROOT/$1/from-dir" ${2:-} 2>&1); RC=$?
  echo "=== case $1 (rc=$RC) ==="
  echo "$OUT"
  echo
}

mkcase() { mkdir -p "$FIXROOT/$1/from-dir"; }
# Standard 4-channel fixture skeleton from the REAL launch10-prep baselines.
std_channels() { # $1=case dir
  cp "$PREP/ue4ss-log-baseline.txt"      "$1/UE4SS.log"
  cp "$PREP/entry-log-baseline.txt"      "$1/townfall_runtime_aligned_entry.log"
  cp "$PREP/runtime-status-baseline.txt" "$1/runtime-status.txt"
  cp "$PREP/xwp-jsonl-baseline.txt"      "$1/modbridge-exec-write-probe.jsonl"
}
fresh_status() { # $1=case dir  $2=pid  $3=ms  $4..=state lines (already tab-formatted)
  d="$1"; pid="$2"; ms="$3"; shift 3
  printf 'modforge-runtime-status\t1\nsession\t%s\t%s\n' "$pid" "$ms" > "$d/runtime-status.txt"
  seqn=1
  for st in "$@"; do
    printf 'state\t%s\t%s\n' "$seqn" "$st" >> "$d/runtime-status.txt"
    seqn=$((seqn + 1))
  done
}

: > "$RECEIPT"
exec > "$RECEIPT" 2>&1
echo "== evidence.sh (launch-10 upgrade) fixture tests — $(date -u +%Y-%m-%dT%H:%M:%SZ) =="
echo "host: $(uname -srm) / shell: $(sh -c 'echo $0' 2>/dev/null || echo sh)"
echo "evidence.sh sha256: $(shasum -a 256 "$EV" 2>/dev/null | awk '{print $1}' || true)"
echo "baseline source: $PREP (controller-captured launch10-prep; read-only use)"
echo "pinned: ue4ss 7591B edf404a5... / entry 2343756B 4b33b985... / status 169B 57c40d4c... (session 273) / xwp 4362B 20cff601..."
echo

for f in ue4ss-log-baseline.txt entry-log-baseline.txt runtime-status-baseline.txt xwp-jsonl-baseline.txt; do
  [ -f "$PREP/$f" ] || { echo "FATAL: baseline source missing: $PREP/$f"; exit 1; }
done

# --- case 1: not_started — files exactly at the launch-10 baselines ----------
mkcase not_started
std_channels "$FIXROOT/not_started/from-dir"
run_case not_started "--baseline-dir $PREP"
check     not_started "verdict NOT_STARTED" "^VERDICT: NOT_STARTED$"
check     not_started "launch-9 session 273 treated as stale baseline session" "stale launch-9 session 273"
check_absent not_started "no drift warnings at exact pinned baselines" "^WARN:"
check     not_started "no timestamped lines note" "no timestamped lines in the new content"
check_rc  not_started "exit 0" 0

# --- case 2: l9_terminal — the known launch-9 terminal reappears from the
#     composition (entry disabled): stage classification + attribution --------
mkcase l9_terminal
std_channels "$FIXROOT/l9_terminal/from-dir"
printf '[t=45000111ms pid=310] modbridge module_start failed: runtime_start_failed code=1 detail=no observed mapping interval intersects the module image extent with valid geometry\n' >> "$FIXROOT/l9_terminal/from-dir/UE4SS.log"
fresh_status "$FIXROOT/l9_terminal/from-dir" 310 45000112 'failed	1	runtime_start_failed no observed mapping interval intersects the module image extent with valid geometry'
run_case l9_terminal "--baseline-dir $PREP"
check     l9_terminal "verdict FAILED_S6_CONVERSION" "^VERDICT: FAILED_S6_CONVERSION$"
check     l9_terminal "stage attributed to derive_readable_ranges" "derive_readable_ranges refusal"
check     l9_terminal "attribution: entry disabled, composition owns the line" "aligned_entry is DISABLED in this config"
check     l9_terminal "stage chain shows S6_CONVERSION" "S6_CONVERSION"
check     l9_terminal "timing writer extracted (r4 prefix)" "writer=failline pid=310 t=45000111ms (first stamp for this pid)"
check     l9_terminal "fresh session detected" "fresh=yes"
check     l9_terminal "timing guard present" "NEVER subtract across writers"
check_absent l9_terminal "no drift warnings" "^WARN:"
check_rc  l9_terminal "exit 0" 0

# --- case 3: l10_success — hypothetical full success chain -------------------
mkcase l10_success
std_channels "$FIXROOT/l10_success/from-dir"
{
  printf 'PATH_TEST: /data/plugins/UE4SS.log writable at 44661000000000 monotonic-ns\n'
  printf 'ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: 6d6f64666f7267652d6c6f61646c6973\n'
  printf '[Lua] [TownfallItemSpawner] mod started\n'
} >> "$FIXROOT/l10_success/from-dir/UE4SS.log"
{
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":1,"pid":320,"tag":"module_start_entered","value":0,"value_hex":"0x00000000","t_ms":1000}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":2,"pid":320,"tag":"constructor_count","value":12,"value_hex":"0x0000000c","t_ms":1002}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":3,"pid":320,"tag":"runtime_wiring_begin","value":0,"value_hex":"0x00000000","t_ms":1005}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":4,"pid":320,"tag":"mfa_activated_low","value":4096,"value_hex":"0x00001000","t_ms":1040}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":5,"pid":320,"tag":"bridge_vmentry_entries","value":800,"value_hex":"0x00000320","t_ms":1060}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":6,"pid":320,"tag":"bridge_lease_reg_spans","value":6,"value_hex":"0x00000006","t_ms":1080}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":7,"pid":320,"tag":"bridge_lease_reg","value":6,"value_hex":"0x00000006","t_ms":1081}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":8,"pid":320,"tag":"anchor_c_either","value":1,"value_hex":"0x00000001","t_ms":1200}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":9,"pid":320,"tag":"bridge_cfg_gamedir","value":1,"value_hex":"0x00000001","t_ms":1500}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":10,"pid":320,"tag":"bridge_cfg_workdir","value":1,"value_hex":"0x00000001","t_ms":1501}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":11,"pid":320,"tag":"runtime_start_ok","value":0,"value_hex":"0x00000000","t_ms":1800}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":12,"pid":320,"tag":"module_start_return","value":0,"value_hex":"0x00000000","t_ms":1801}\n'
} > "$FIXROOT/l10_success/from-dir/modbridge-supplier-diag.jsonl"
fresh_status "$FIXROOT/l10_success/from-dir" 320 46000000 'activated	0	' 'engine-ready	0	' 'mods-started	0	'
run_case l10_success "--baseline-dir $PREP"
check     l10_success "verdict MODS_STARTED" "^VERDICT: MODS_STARTED$"
check     l10_success "first-time ladder flagged" "FULL LADDER REACHED for the first time"
check     l10_success "ge.5 gate opens only now" "ge.5 gate now OPENS"
check     l10_success "supplier-diag chain S1..S9" "supplier-diag: S1_ENTRY_CTORS -> S2_ACTIVATION -> S3_VMMAP_WALK -> S4_LEASE_REG -> S5_IDENTITY_ANCHORS -> S7_CFG_ATTACH -> S9_START_ACK -> S11_STOP"
check     l10_success "lease spans 6 == reg 6 (expected on this target)" "lease_spans=6 lease_reg=6"
check     l10_success "per-stage elapsed table present" "writer=supplier-diag-jsonl"
check     l10_success "first t_ms marked" "module_start_entered t=1000 (first t_ms)"
check     l10_success "delta computed within one writer" "runtime_start_ok t=1800 +299ms"
check     l10_success "cross-writer guard" "NEVER subtract across writers"
check     l10_success "status ladder shown" "ladder=\[ activated engine-ready mods-started"
check     l10_success "path_test stamp timed" "writer=pathtest stamp=44661000000000ns (first stamp)"
check_absent l10_success "no drift warnings" "^WARN:"
check_rc  l10_success "exit 0" 0

# --- case 4: timing_two_writers — two writers, different epochs/units --------
mkcase timing_two_writers
std_channels "$FIXROOT/timing_two_writers/from-dir"
{
  printf 'PATH_TEST: /data/plugins/UE4SS.log writable at 1000000000000 monotonic-ns\n'
  printf '[t=5000ms pid=400] modbridge module_start failed: runtime_start_failed code=3 detail=StartActivatedRuntimeWithAck returned null owner\n'
  printf 'PATH_TEST: /data/plugins/UE4SS.log writable at 2000000000000 monotonic-ns\n'
  printf '[t=9000ms pid=401] modbridge module_start failed: runtime_start_failed code=1 detail=no image anchor (status NoImageAnchor); refusing conversion; dlopen(NULL) is never a fallback\n'
} >> "$FIXROOT/timing_two_writers/from-dir/UE4SS.log"
run_case timing_two_writers
check     timing_two_writers "pathtest first stamp" "writer=pathtest stamp=1000000000000ns (first stamp)"
check     timing_two_writers "pathtest same-writer delta (ns domain)" "writer=pathtest stamp=2000000000000ns delta=+1000000000000ns (vs previous pathtest stamp)"
check     timing_two_writers "failline pid=400 own sequence" "writer=failline pid=400 t=5000ms (first stamp for this pid)"
check     timing_two_writers "failline pid=401 own sequence (no cross-pid delta)" "writer=failline pid=401 t=9000ms (first stamp for this pid)"
check     timing_two_writers "per-writer guard text" "NOT comparable across writers"
check_absent timing_two_writers "no ms/ns mixed arithmetic" "delta=+\[0-9\]*ms.*ns"
check_absent timing_two_writers "no cross-pid failline delta" "pid=401.*delta"
check_rc  timing_two_writers "exit 0" 0

# --- case 5: baseline_drift — on-disk baseline differs from pinned constants -
mkcase baseline_drift
std_channels "$FIXROOT/baseline_drift/from-dir"
DRIFTDIR="$FIXROOT/baseline_drift/baselines"
mkdir -p "$DRIFTDIR"
cp "$PREP/entry-log-baseline.txt" "$DRIFTDIR/entry-log-baseline.txt"
cp "$PREP/runtime-status-baseline.txt" "$DRIFTDIR/runtime-status-baseline.txt"
cp "$PREP/xwp-jsonl-baseline.txt" "$DRIFTDIR/xwp-jsonl-baseline.txt"
cp "$PREP/ue4ss-log-baseline.txt" "$DRIFTDIR/ue4ss-log-baseline.txt"
printf 'DRIFT-LINE appended to the on-disk baseline reference\n' >> "$DRIFTDIR/ue4ss-log-baseline.txt"
run_case baseline_drift "--baseline-dir $DRIFTDIR"
check     baseline_drift "size drift detected" "pinned launch10-prep size is 7591"
check     baseline_drift "sha drift detected (pinned hash named)" "!= pinned edf404a5"
check     baseline_drift "sha drift named pinned hash" "edf404a5"
check     baseline_drift "re-fetch guidance" "re-fetch baselines before classifying"
check     baseline_drift "warnings surfaced as NOTE too" "NOTE: baseline drift"
check_rc  baseline_drift "exit 0" 0

# --- case 6: content_drift — fetched file head differs from baseline head -----
# Scenario: the wrong file was fetched (same size, different bytes) — appending
# new lines could never trigger this guard, so the fixture REPLACES the
# baseline window with foreign same-size content.
mkcase content_drift
std_channels "$FIXROOT/content_drift/from-dir"
awk 'BEGIN { for (i = 0; i < 7590; i++) printf "X"; printf "\n" }' > "$FIXROOT/content_drift/from-dir/UE4SS.log"
run_case content_drift "--baseline-dir $PREP"
check     content_drift "head-window mismatch detected" "first .*B of UE4SS.log differ from the baseline reference head"
check     content_drift "names the append-only contract" "append-only contract violated, or the wrong file was fetched"
check_rc  content_drift "exit 0" 0

# --- case 7: shape_a_regress — armed table did NOT ship ----------------------
mkcase shape_a_regress
std_channels "$FIXROOT/shape_a_regress/from-dir"
printf '[Seeds] Required-slot seed table incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal] missing [gmalloc]. Arming requires all four REQUIRED slots.\n' >> "$FIXROOT/shape_a_regress/from-dir/UE4SS.log"
fresh_status "$FIXROOT/shape_a_regress/from-dir" 330 47000000 'failed	1	[Seeds] Required-slot seed table incomplete'
run_case shape_a_regress "--baseline-dir $PREP"
check     shape_a_regress "verdict SHAPE_A_SEEDS_GATE" "^VERDICT: SHAPE_A_SEEDS_GATE$"
check     shape_a_regress "re-labeled: no longer the expected terminal" "did NOT ship in this fself"
check     shape_a_regress "no bridge-triage guidance" "never run bridge stage-1 CONVERSION vm-scan triage"
check     shape_a_regress "initializer stage attributed" "S8B_INITIALIZER"
check_rc  shape_a_regress "exit 0" 0

# --- case 8: lease_gate_r14 — r14 loud pre-lease coverage gate ---------------
mkcase lease_gate_r14
std_channels "$FIXROOT/lease_gate_r14/from-dir"
printf '[t=48000001ms pid=340] modbridge module_start failed: runtime_start_failed code=1 detail=lease coverage gap: requested [0x787c000 +0x4000) outside registered readable spans (nearest registered span ends at 0x7878000)\n' >> "$FIXROOT/lease_gate_r14/from-dir/UE4SS.log"
fresh_status "$FIXROOT/lease_gate_r14/from-dir" 340 48000002 'failed	1	runtime_start_failed lease coverage gap'
run_case lease_gate_r14 "--baseline-dir $PREP"
check     lease_gate_r14 "verdict FAILED_S6_LEASEGATE" "^VERDICT: FAILED_S6_LEASEGATE$"
check     lease_gate_r14 "names the r14 gate" "r14 pre-lease coverage gate"
check     lease_gate_r14 "MRS_ERR_CONVERSION-by-design note" "MRS_ERR_CONVERSION by design"
check     lease_gate_r14 "loud-replacement framing" "LOUD replacement of launch-8"
check_rc  lease_gate_r14 "exit 0" 0

# --- case 9: lease_throw_dormant — deep PLH throw reappeared -----------------
mkcase lease_throw_dormant
std_channels "$FIXROOT/lease_throw_dormant/from-dir"
printf '[t=49000001ms pid=350] modbridge module_start failed: runtime_start_failed code=2 detail=Owned read range includes unregistered bytes\n' >> "$FIXROOT/lease_throw_dormant/from-dir/UE4SS.log"
fresh_status "$FIXROOT/lease_throw_dormant/from-dir" 350 49000002 'failed	2	runtime_start_failed Owned read range includes unregistered bytes'
run_case lease_throw_dormant "--baseline-dir $PREP"
check     lease_throw_dormant "verdict FAILED_S8_CONTEXT_LEASE" "^VERDICT: FAILED_S8_CONTEXT_LEASE$"
check     lease_throw_dormant "dormancy stated (r13 dormant, launch-8 r10 threw)" "DORMANT in r13"
check     lease_throw_dormant "r14 gate should have caught it" "did NOT carry the r14 gate"
check     lease_throw_dormant "readback-first guidance" "readback sha"
check_rc  lease_throw_dormant "exit 0" 0

# --- case 10: seeds_refuted ---------------------------------------------------
mkcase seeds_refuted
std_channels "$FIXROOT/seeds_refuted/from-dir"
printf '[Seeds] REFUTED: fname pool never initialized (probe window uncovered)\n' >> "$FIXROOT/seeds_refuted/from-dir/UE4SS.log"
fresh_status "$FIXROOT/seeds_refuted/from-dir" 360 50000000 'failed	1	[Seeds] REFUTED: fname pool never initialized'
run_case seeds_refuted "--baseline-dir $PREP"
check     seeds_refuted "verdict SEEDS_REFUTED" "^VERDICT: SEEDS_REFUTED$"
check     seeds_refuted "terminal-per-launch rule" "terminal per launch, never replayed"
check     seeds_refuted "new-ids guidance" "NEW ids"
check_rc  seeds_refuted "exit 0" 0

# --- case 11: gmalloc_gate ----------------------------------------------------
mkcase gmalloc_gate
std_channels "$FIXROOT/gmalloc_gate/from-dir"
printf 'UnrealInitializer::PostInitialize: GMalloc is uninitialized.\n' >> "$FIXROOT/gmalloc_gate/from-dir/UE4SS.log"
fresh_status "$FIXROOT/gmalloc_gate/from-dir" 370 51000000 'failed	1	UnrealInitializer::PostInitialize: GMalloc is uninitialized.'
run_case gmalloc_gate "--baseline-dir $PREP"
check     gmalloc_gate "verdict GMALLOC_GATE" "^VERDICT: GMALLOC_GATE$"
check     gmalloc_gate "validator-diff shipped question" "GMalloc V1/V2 validator diff actually shipped"
check     gmalloc_gate "not-a-conversion-refusal note" "NOT a conversion refusal"
check_rc  gmalloc_gate "exit 0" 0

# --- case 12: crash_shaped ----------------------------------------------------
mkcase crash_shaped
std_channels "$FIXROOT/crash_shaped/from-dir"
{
  printf 'Finding GameEngine CDO\n'
  printf 'Finding GameEngine::LoadMap\n'
} >> "$FIXROOT/crash_shaped/from-dir/UE4SS.log"
fresh_status "$FIXROOT/crash_shaped/from-dir" 380 52000000 'activated	0	'
run_case crash_shaped "--baseline-dir $PREP"
check     crash_shaped "verdict CRASH_SHAPED_SUSPECTED" "^VERDICT: CRASH_SHAPED_SUSPECTED$"
check     crash_shaped "names FLAG-2 exec-read candidate" "FLAG-2"
check_rc  crash_shaped "exit 0" 0

# --- case 13: truncation — UE4SS.log shorter than baseline --------------------
mkcase truncation
std_channels "$FIXROOT/truncation/from-dir"
head -c 1024 "$PREP/ue4ss-log-baseline.txt" > "$FIXROOT/truncation/from-dir/UE4SS.log"
run_case truncation
check     truncation "truncation warning present" "log replaced/truncated or wrong file fetched"
check_rc  truncation "exit 0" 0

# --- case 14: boundary — straddle line at the 7591-byte cut ------------------
mkcase boundary
std_channels "$FIXROOT/boundary/from-dir"
# Rebuild the fetched log as: first 7500 bytes of the real baseline (a mid-line
# cut), then a 213-char line starting at offset 7500 (< cut 7591) — it straddles
# the cut but must stay entirely baseline-region — then NEWMARKER at 7714 (>=
# cut): the first line that must appear as new content.
cp "$FIXROOT/boundary/from-dir/UE4SS.log" "$FIXROOT/boundary/base7591.bin"
head -c 7500 "$FIXROOT/boundary/base7591.bin" > "$FIXROOT/boundary/from-dir/UE4SS.log"
awk 'BEGIN { printf "%0200dSTRADDLERTAIL\nNEWMARKER-FIRST-NEW-LINE\n", 7 }' >> "$FIXROOT/boundary/from-dir/UE4SS.log"
run_case boundary
check_absent boundary "straddling baseline-region line not leaked" "STRADDLERTAIL"
check     boundary "first line starting at/after the cut is present" "NEWMARKER-FIRST-NEW-LINE"
check     boundary "straddle surfaced as OTHER new content" "^VERDICT:"
check_rc  boundary "exit 0" 0

# --- case 15: entry_growth — disabled aligned_entry channel grew --------------
mkcase entry_growth
std_channels "$FIXROOT/entry_growth/from-dir"
printf '[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0 module_start_entered value=0x00000000\n' >> "$FIXROOT/entry_growth/from-dir/townfall_runtime_aligned_entry.log"
run_case entry_growth "--baseline-dir $PREP"
check     entry_growth "CONFIG-DRIFT: entry growth detected" "CONFIG-DRIFT: aligned-entry log GREW"
check     entry_growth "names the disabled plugin + ini pin" "verify ini sha 1216aab6"
check     entry_growth "attribution untrusted flag" "attribution untrusted"
check_rc  entry_growth "exit 0" 0

# --- case 16: diag_drift — DIAG plugin line with no fresh session -------------
mkcase diag_drift
std_channels "$FIXROOT/diag_drift/from-dir"
printf 'DIAG: module_start reached at 1791159999.999 pid=301 build=sdg2-6f404cee6b94 variant=imp\n' >> "$FIXROOT/diag_drift/from-dir/UE4SS.log"
run_case diag_drift "--baseline-dir $PREP"
check     diag_drift "verdict CONFIG_DRIFT" "^VERDICT: CONFIG_DRIFT$"
check     diag_drift "DIAG-line count shown" "DIAG-line count 1"
check     diag_drift "config question raised" "may not be what ran"
check_rc  diag_drift "exit 0" 0

# --- case 17: l10_success_conversion — verdict-rehearsal GAP-1/2/3 regression -
# Conversion success-path tags must stage S6_CONVERSION on the SUCCESS chain
# (not only via the D-style refusal fan-out), anchor_count/intervals_found must
# surface in SDIAGSUM next to the lease spans (controller predicted-pair check:
# anchor_count=7, intervals_found=1, spans=6), the BEST-decisive
# [TownfallItemSpawner] load prints must appear verbatim, and the first-seen
# S7-before-S4/S5 chain order must be annotated as a display quirk.
mkcase l10_success_conversion
std_channels "$FIXROOT/l10_success_conversion/from-dir"
{
  printf 'start_mods<LuaMod> TownfallItemSpawner\n'
  printf '[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.\n'
  printf '[Lua] [TownfallItemSpawner] 42 spawnable pickups found. Pages=3\n'
  printf '[Lua] [TownfallItemSpawner] Menu created.\n'
} >> "$FIXROOT/l10_success_conversion/from-dir/UE4SS.log"
{
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":1,"pid":421,"tag":"module_start_entered","value":0,"value_hex":"0x00000000","t_ms":2000}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":2,"pid":421,"tag":"runtime_wiring_begin","value":0,"value_hex":"0x00000000","t_ms":2005}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":3,"pid":421,"tag":"bridge_vmentry_entries","value":800,"value_hex":"0x00000320","t_ms":2010}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":4,"pid":421,"tag":"bridge_exepath_source","value":2,"value_hex":"0x00000002","t_ms":2020}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":5,"pid":421,"tag":"bridge_lease_reg_spans","value":6,"value_hex":"0x00000006","t_ms":2030}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":6,"pid":421,"tag":"bridge_lease_reg","value":6,"value_hex":"0x00000006","t_ms":2031}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":7,"pid":421,"tag":"lmf_extent_base_low","value":4194304,"value_hex":"0x00400000","t_ms":2040}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":8,"pid":421,"tag":"lmf_extent_base_high","value":0,"value_hex":"0x00000000","t_ms":2041}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":9,"pid":421,"tag":"lmf_extent_end_low","value":201720520,"value_hex":"0x0c0602c8","t_ms":2042}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":10,"pid":421,"tag":"lmf_extent_end_high","value":0,"value_hex":"0x00000000","t_ms":2043}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":11,"pid":421,"tag":"lmf_anchor_count","value":7,"value_hex":"0x00000007","t_ms":2044}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":12,"pid":421,"tag":"lmf_intervals_found","value":1,"value_hex":"0x00000001","t_ms":2045}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":13,"pid":421,"tag":"anchor_c_either","value":1,"value_hex":"0x00000001","t_ms":2050}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":14,"pid":421,"tag":"bridge_cfg_gamedir","value":1,"value_hex":"0x00000001","t_ms":2060}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":15,"pid":421,"tag":"runtime_start_ok","value":0,"value_hex":"0x00000000","t_ms":2100}\n'
  printf '{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":16,"pid":421,"tag":"module_start_return","value":0,"value_hex":"0x00000000","t_ms":2101}\n'
} > "$FIXROOT/l10_success_conversion/from-dir/modbridge-supplier-diag.jsonl"
fresh_status "$FIXROOT/l10_success_conversion/from-dir" 421 61000000 'activated	0	' 'engine-ready	0	' 'mods-started	0	'
run_case l10_success_conversion "--baseline-dir $PREP --json-tail"
check     l10_success_conversion "verdict MODS_STARTED" "^VERDICT: MODS_STARTED$"
check     l10_success_conversion "success chain includes S6_CONVERSION (GAP-1)" "S4_LEASE_REG -> S6_CONVERSION -> S5_IDENTITY_ANCHORS"
check     l10_success_conversion "SDIAGSUM shows anchor_count=7 + intervals_found=1 next to spans=6 (GAP-1)" "lease_spans=6 lease_reg=6 anchor_count=7 intervals_found=1"
check     l10_success_conversion "BEST-decisive load print verbatim on the page (GAP-2)" "\[Lua\] \[TownfallItemSpawner\] Loaded v1.0. Clean pickup-only release. F7 opens the menu."
check     l10_success_conversion "post-OPTIONS drain line verbatim (GAP-2)" "Menu created."
check     l10_success_conversion "mod-load section labeled BEST-decisive (GAP-2)" "mod-load prints (\[TownfallItemSpawner\]): 4"
check     l10_success_conversion "json tail carries mod_load_lines" "\"mod_load_lines\":4"
check     l10_success_conversion "S7-before-S4/S5 quirk annotated, not reordered (GAP-3)" "bridge_exepath_source is emitted mid-walk"
check_absent l10_success_conversion "no drift warnings" "^WARN:"
check_rc  l10_success_conversion "exit 0" 0

# --- summary -----------------------------------------------------------------
echo
echo "== fixture test summary: $pass passed, $fail failed =="
echo "fixture shas (case UE4SS.log files):"
shasum -a 256 "$FIXROOT"/*/from-dir/UE4SS.log 2>/dev/null || true

[ "$fail" -eq 0 ] || exit 1
exit 0
