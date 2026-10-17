#!/bin/sh
# run_fixture_tests.sh — host fixture tests for evidence.sh (launch-9
# composition-6 classifier). NO console contact: evidence.sh is invoked only
# in --from-dir mode. Generates synthetic fixtures of the exact baseline
# sizes (UE4SS.log 2892 B, aligned-entry log 2210306 B) and asserts the
# verdict precedence, signature classes, boundary exactness and truncation
# guard. Writes a receipt to receipts/fixture-test-pass.txt.

set -u
HERE=$(cd "$(dirname "$0")" && pwd)
EV="$HERE/../evidence.sh"
FIXROOT="$HERE/../fixtures"
RECEIPT="$HERE/../receipts/fixture-test-pass.txt"

chmod +x "$EV" 2>/dev/null || true
mkdir -p "$FIXROOT" "$HERE/../receipts"

pass=0; fail=0

gen_baseline_ue4ss() { # exactly 2892 bytes, LF-terminated, synthetic
  awk 'BEGIN {
    printf "FIXTURE-SYNTHETIC BASELINE (not console bytes; sizes only)\n"
    printf "PATH_TEST: /data/plugins/UE4SS.log writable at 8866785633337 monotonic-ns\n"
    printf "DIAG: module_start reached at 1791155755.879 pid=298 build=sdg2-6f404cee6b94 variant=imp\n"
    line = "FILLER0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    pad = 2892 - 66 - 76 - 86
    while (length(line) + 1 < pad) printf "%s\n", line
    printf "%s\n", substr(line, 1, pad - length(line) - 1)
  }' | head -c 2892
}

gen_baseline_aligned() { # whole 124-byte lines, 17826 lines = 2210424 B >= 2210306 B
  awk 'BEGIN {
    for (i = 0; i < 17826; i++)
      printf "[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0 fixturefiller_%06d value=0x00000000\n", i
  }'
}

check() { # $1=case  $2=desc  $3=pattern (grep -q against combined output) $4=rc
  if echo "$OUT" | grep -q "$3"; then pass=$((pass+1)); echo "PASS [$1] $2"
  else fail=$((fail+1)); echo "FAIL [$1] $2 (wanted /$3/)"; fi
}
check_rc() { # $1=case $2=desc $3=expected rc
  if [ "$RC" = "$3" ]; then pass=$((pass+1)); echo "PASS [$1] $2 (rc=$RC)"
  else fail=$((fail+1)); echo "FAIL [$1] $2 (wanted rc=$3 got $RC)"; fi
}

run_case() { # $1=case name
  OUT=$(sh "$EV" --from-dir "$FIXROOT/$1/from-dir" 2>&1); RC=$?
  echo "=== case $1 (rc=$RC) ==="
  echo "$OUT"
  echo
}

mkcase() { mkdir -p "$FIXROOT/$1/from-dir"; }

: > "$RECEIPT"
exec > "$RECEIPT" 2>&1
echo "== evidence.sh fixture tests — $(date -u +%Y-%m-%dT%H:%M:%SZ) =="
echo "host: $(uname -srm) / shell: $(sh -c 'echo $0' 2>/dev/null || echo sh)"
echo "evidence.sh sha256: $(shasum -a 256 "$EV" 2>/dev/null | awk '{print $1}' || true)"
echo

# --- build shared fixture bodies -------------------------------------------
BODY_UE4SS=$(mktemp); BODY_ALIGNED=$(mktemp)
gen_baseline_ue4ss > "$BODY_UE4SS"
gen_baseline_aligned > "$BODY_ALIGNED"
echo "fixture body sizes: ue4ss=$(wc -c < "$BODY_UE4SS")B aligned=$(wc -c < "$BODY_ALIGNED")B (baselines 2892/2210306)"

# --- case 1: shape_a — the EXPECTED launch-9 terminal -----------------------
mkcase shape_a
cat "$BODY_UE4SS" > "$FIXROOT/shape_a/from-dir/UE4SS.log"
cat >> "$FIXROOT/shape_a/from-dir/UE4SS.log" <<'EOF'
UE4SS - v3.0.1 - Git SHA #a1b2c3d4e5f60718
root directory: /data/plugins
Setting up mods...
lmf_to_native_loader_facts provider=vmmap identity_source=window
anchor_ladder_verify B=1 D=1 E=1 C=1 C2=0
bridge_vmentry_entries n=16384 walk=r5
bridge_cfg_gamedir=1 bridge_cfg_workdir=1 bridge_exepath_source=0
[Seeds] Required-slot seed table incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal] missing [gmalloc]. Arming requires all four REQUIRED slots (SHAPE A safe default, LAUNCH9-PREDICTION): a partial table would walk into the LoadMap vtable-walk text read (igf6 crash class) before the honest PostInitialize GMalloc throw. Derive the missing seeds against the sha-pinned image (SEED-TABLE-FORMAT.md) and rebuild. Startup blocked; nothing bound, nothing exec-side touched.
EOF
cat "$BODY_ALIGNED" > "$FIXROOT/shape_a/from-dir/townfall_runtime_aligned_entry.log"
cat >> "$FIXROOT/shape_a/from-dir/townfall_runtime_aligned_entry.log" <<'EOF'
[t=412887ms pid=414] modbridge runtime wiring begin
[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0 runtime_start_ok value=0x00000000
[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0 module_start_return value=0x00000000
EOF
printf 'modforge-runtime-status\t1\nsession\t414\t9534000\nstate\t1\tactivated\t0\t\nstate\t2\tfailed\t1\t[Seeds] Required-slot seed table incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal] missing [gmalloc]\n' > "$FIXROOT/shape_a/from-dir/runtime-status.txt"
run_case shape_a
check_rc  shape_a "exit 0 for expected terminal" 0
check     shape_a "verdict is SHAPE_A_SEEDS_GATE" "^VERDICT: SHAPE_A_SEEDS_GATE$"
check     shape_a "labeled EXPECTED" "EXPECTED-PREDICTED-STOP"
check     shape_a "no bridge-triage guidance" "never run bridge stage-1 CONVERSION vm-scan triage"
check     shape_a "pass markers counted" "pass markers conv/roots/lease/D2 : [1-9]"
check     shape_a "shape-a count 1" "Shape-A seeds-incomplete line    : 1"
check     shape_a "observer start_ok seen" "start_ok=1"
check     shape_a "entry-r4 prefix lines counted" "entry-r4 \[t= prefixed lines      : 1"
check     shape_a "status shown" "last=failed"

# --- case 2: mods_started — full ladder (beyond prediction) -----------------
mkcase mods_started
cat "$BODY_UE4SS" > "$FIXROOT/mods_started/from-dir/UE4SS.log"
cat >> "$FIXROOT/mods_started/from-dir/UE4SS.log" <<'EOF'
UE4SS - v3.0.1 - Git SHA #a1b2c3d4e5f60718
root directory: /data/plugins
Setting up mods...
[Lua] [TownfallItemSpawner] mod started
EOF
cp "$BODY_ALIGNED" "$FIXROOT/mods_started/from-dir/townfall_runtime_aligned_entry.log"
printf 'modforge-runtime-status\t1\nsession\t501\t9534000\nstate\t1\tactivated\t0\t\nstate\t2\tengine-ready\t0\t\nstate\t3\tmods-started\t0\t\n' > "$FIXROOT/mods_started/from-dir/runtime-status.txt"
run_case mods_started
check     mods_started "verdict is MODS_STARTED" "^VERDICT: MODS_STARTED$"
check     mods_started "beyond-prediction skepticism present" "beyond the launch-9 prediction"
check     mods_started "itemspawner print counted" "TownfallItemSpawner"
check_rc  mods_started "exit 0" 0

# --- case 3: crash_shaped — igf6-class suspicion -----------------------------
mkcase crash_shaped
cat "$BODY_UE4SS" > "$FIXROOT/crash_shaped/from-dir/UE4SS.log"
cat >> "$FIXROOT/crash_shaped/from-dir/UE4SS.log" <<'EOF'
UE4SS - v3.0.1 - Git SHA #a1b2c3d4e5f60718
root directory: /data/plugins
Setting up mods...
Finding GameEngine CDO
Finding GameEngine::LoadMap
EOF
cp "$BODY_ALIGNED" "$FIXROOT/crash_shaped/from-dir/townfall_runtime_aligned_entry.log"
printf 'modforge-runtime-status\t1\nsession\t502\t9534000\nstate\t1\tactivated\t0\t\n' > "$FIXROOT/crash_shaped/from-dir/runtime-status.txt"
run_case crash_shaped
check     crash_shaped "verdict is CRASH_SHAPED_SUSPECTED" "^VERDICT: CRASH_SHAPED_SUSPECTED$"
check     crash_shaped "names the exec-read candidate" "FLAG-2"
check_rc  crash_shaped "exit 0" 0

# --- case 4: failed_diag — loud diag_channel_open_failed (-22) ---------------
mkcase failed_diag
cat "$BODY_UE4SS" > "$FIXROOT/failed_diag/from-dir/UE4SS.log"
cp "$BODY_ALIGNED" "$FIXROOT/failed_diag/from-dir/townfall_runtime_aligned_entry.log"
printf '[t=998811ms pid=500] modbridge module_start failed: diag_channel_open_failed code=-22 detail=diagnostic channel unavailable before wiring\n' >> "$FIXROOT/failed_diag/from-dir/townfall_runtime_aligned_entry.log"
printf 'modforge-runtime-status\t1\nsession\t410\t100\n' > "$FIXROOT/failed_diag/from-dir/runtime-status.txt"
run_case failed_diag
check     failed_diag "verdict FAILED_diag_channel_open_failed (stage verbatim)" "^VERDICT: FAILED_diag_channel_open_failed$"
check     failed_diag "-22 silence guidance" "delivered entry is SILENT on -22"
check     failed_diag "entry-r4 prefix line counted" "entry-r4 \[t= prefixed lines      : 1"
check_rc  failed_diag "exit 0" 0

# --- case 5: not_started — files exactly at baseline sizes -------------------
mkcase not_started
cp "$BODY_UE4SS"   "$FIXROOT/not_started/from-dir/UE4SS.log"
cp "$BODY_ALIGNED" "$FIXROOT/not_started/from-dir/townfall_runtime_aligned_entry.log"
printf 'modforge-runtime-status\t1\nsession\t411\t100\n' > "$FIXROOT/not_started/from-dir/runtime-status.txt"
run_case not_started
check     not_started "verdict NOT_STARTED" "^VERDICT: NOT_STARTED$"
if echo "$OUT" | grep -q "WARN:"; then
  fail=$((fail+1)); echo "FAIL [not_started] unexpected truncation warning at exact baseline sizes"
else
  pass=$((pass+1)); echo "PASS [not_started] no truncation warning at exact-or-greater sizes"
fi
check_rc  not_started "exit 0" 0

# --- case 6: truncation — UE4SS.log shorter than baseline --------------------
mkcase truncation
head -c 1024 "$BODY_UE4SS" > "$FIXROOT/truncation/from-dir/UE4SS.log"
cp "$BODY_ALIGNED" "$FIXROOT/truncation/from-dir/townfall_runtime_aligned_entry.log"
printf 'modforge-runtime-status\t1\nsession\t412\t100\n' > "$FIXROOT/truncation/from-dir/runtime-status.txt"
run_case truncation
check     truncation "truncation warning present" "log replaced/truncated or wrong file fetched"
check_rc  truncation "exit 0" 0

# --- case 7: boundary exactness — round-up at the baseline byte offset -------
mkcase boundary
# line1 = 2987 digits + "STRADDLERTAIL" (13) + LF = 3001 B; it STARTS at 0
# (< cut 2892) so it is entirely baseline-region even though it STRADDLES the
# offset — its past-cut tail must never appear as new content. line2 starts at
# 3001 (>= cut) and is the first new line.
awk 'BEGIN { printf "%02987dSTRADDLERTAIL\nNEWMARKER-FIRST-NEW-LINE\n", 1 }' \
  > "$FIXROOT/boundary/from-dir/UE4SS.log"
cp "$BODY_ALIGNED" "$FIXROOT/boundary/from-dir/townfall_runtime_aligned_entry.log"
printf 'modforge-runtime-status\t1\nsession\t413\t100\n' > "$FIXROOT/boundary/from-dir/runtime-status.txt"
run_case boundary
if echo "$OUT" | grep -q "STRADDLERTAIL"; then
  fail=$((fail+1)); echo "FAIL [boundary] straddling baseline-region line leaked past the cut"
else
  pass=$((pass+1)); echo "PASS [boundary] straddling baseline-region line not leaked"
fi
check boundary "first line starting at/after the cut is present" "NEWMARKER-FIRST-NEW-LINE"
check_rc boundary "exit 0" 0

# --- summary -----------------------------------------------------------------
echo
echo "== fixture test summary: $pass passed, $fail failed =="
echo "fixture shas:"
shasum -a 256 "$FIXROOT"/*/*/UE4SS.log "$FIXROOT"/*/*/townfall_runtime_aligned_entry.log 2>/dev/null || true

[ "$fail" -eq 0 ] || exit 1
exit 0
