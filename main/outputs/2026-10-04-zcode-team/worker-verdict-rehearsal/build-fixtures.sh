#!/bin/sh
# build-fixtures.sh — worker-verdict-rehearsal · 2026-10-04 · local only
# Rebuilds the five synthetic launch-10 evidence sets (A BEST, B GOOD,
# C MINIMAL-ACCEPT, D FAIL, E S-30S timing) from the PINNED launch10-prep
# baselines + oracle-verbatim strings only. Deterministic; re-run any time.
set -eu
ROOT="$(cd "$(dirname "$0")" && pwd)"
FX="$ROOT/fixtures"
BASE="$FX/_baselines"

UE4SS_BASE="$BASE/ue4ss-log-baseline.txt"   # 7591 B edf404a5... (recovered from launch10-evidence-upgrade fixtures, sha-verified)
ENTRY_BASE="$BASE/entry-log-baseline.txt"   # 2343756 B 4b33b985...
STATUS_BASE="$BASE/runtime-status-baseline.txt" # 169 B 57c40d4c... (session 273)
XWP_BASE="$BASE/xwp-jsonl-baseline.txt"     # 4362 B 20cff601...

mk() { mkdir -p "$FX/$1/from-dir"; }

# shared immutable channels: entry log + xwp must NOT grow in the 2-plugin config
for s in A B C D E; do
  mk "$s"
  cp "$ENTRY_BASE" "$FX/$s/from-dir/townfall_runtime_aligned_entry.log"
  cp "$XWP_BASE"   "$FX/$s/from-dir/modbridge-exec-write-probe.jsonl"
done

write_ue4ss() { # $1=set  $2=path_test-ns-stamp  (new lines on stdin)
  { cat "$UE4SS_BASE"; cat; } > "$FX/$1/from-dir/UE4SS.log"
}

# ---------------------------------------------------------------- prologue --
prologue() { # $1 = ns stamp
  cat <<EOF
Native console, GUI, crash dumping and filesystem watching are unavailable
UE4SS - v3.0.1
Timezone: UTC
UE4SS Build Configuration: Shipping
root directory: /data/plugins
working directory: /data/plugins
mods directories: /data/plugins/Mods
Setting up mods...
PATH_TEST: /data/plugins/UE4SS.log writable at $1 monotonic-ns
ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: 6d6f64666f7267652d6c6f61646c6973
EOF
}

seeds_full() {
  cat <<'EOF'
[Seeds] Required-slot seed table armed (slots 0/2/3/4); the ps_scan arm and both Lua override scans are skipped
[Seeds] Unbound by design: fname_tostring (Kismet fallback owns it), gnatives, fuobject_hash_tables_get; engine version via EngineVersionOverride settings
[Seeds] slot 0 g_uobject_array bound at 0x97ba110 (data; identity and ABI unverified)
[Seeds] slot 3 gmalloc bound at 0x970add8 -> 0x97089b8 (data; V1 instance + V2 vptr match; identity and ABI unverified)
[Seeds] FName pool validated at 0x9765ec0 (CurrentBlock 37, Blocks[0] 0x80234ae0; 'None' id 0x1a2b, 'ByteProperty' id 0x35c)
[Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified (ByteProperty probe id == pool id 0x35c); detour verification skipped
[Seeds] slot 4 static_construct_object_internal bound at 0x890aa0 (code; identity and ABI unverified)
Using engine version: 5.6
Waiting for object construction...
Still waiting for object construction, 8231 objects constructed so far
Locating KismetStringLibrary...
KismetStringLibrary located: 0x8376c4a0 [n: 0x5c]
Locating KismetStringLibrary:Conv_NameToString...
KismetStringLibrary:Conv_NameToString located: 0x8341f230 [n: 0x41]
Locating KismetStringLibrary CDO...
KismetStringLibrary CDO located: 0x8389e100 [n: 0x3f]
Constructed [1 / 5]: Engine
Constructed [2 / 5]: Default__GameEngine
Constructed [3 / 5]: Default__GameParty
Constructed [4 / 5]: Default__GameSession
Constructed [5 / 5]: Default__GameViewportClient
Constructed 5 of 5 objects
Finding GameEngine::LoadMap
GameEngine::LoadMap address 0x834612a0
Finding GameEngine::Tick
GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x0)
Using vtable address for GameEngine::Tick
Finding GameModeBase::InitGameState
GameModeBase::InitGameState address 0x83ad4740
Finding AActor::BeginPlay/EndPlay/Tick
AActor::BeginPlay address 0x83c2f810
AActor::EndPlay address 0x83c2f9c0
AActor::Tick address 0x83c2fb30
Finding GameViewportClient::Tick
GameViewportClient::Tick address 0x83b90e50
Finding ProcessEvent
ProcessEvent address 0x8335d8c0
Finding ProcessConsoleExec
ProcessConsoleExec address 0x8335da40
Finding UStruct::Link
UStruct::Link address 0x83684a10
Initializing global FName variables
Initializing type system
UnrealConfig.FExecVTableOffsetInLocalPlayer: 0x2a8
Post-initialization: GMalloc: 0x970add8 -> 0x97089b8
Initializing searcher pools
Adding GUObjectArray listeners
Adding shutdown listener
Starting callback garbage collector!
Hooking StaticConstructObject for searcher pools
FAssetData Size: 0x38
FVector Size: 0x18
FQuat Size: 0x18
FFieldVariant Size: 0x10
FText Size: 0x18
FName Size: 0x8
FName Alignment: 0x8
start_mods<LuaMod> TownfallItemSpawner
[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.
[Lua] [TownfallItemSpawner] 42 spawnable pickups found. Pages=3
[Lua] [TownfallItemSpawner] Menu created.
EOF
}

seeds_past_v1v2() {  # B: armed + unbound + slot0 + slot3 (V1/V2 pass); log ends mid-module-lines
  cat <<'EOF'
[Seeds] Required-slot seed table armed (slots 0/2/3/4); the ps_scan arm and both Lua override scans are skipped
[Seeds] Unbound by design: fname_tostring (Kismet fallback owns it), gnatives, fuobject_hash_tables_get; engine version via EngineVersionOverride settings
[Seeds] slot 0 g_uobject_array bound at 0x97ba110 (data; identity and ABI unverified)
[Seeds] slot 3 gmalloc bound at 0x970add8 -> 0x97089b8 (data; V1 instance + V2 vptr match; identity and ABI unverified)
EOF
}

seeds_armed_only() {  # C: armed + unbound; walkguard refuses slot 0
  cat <<'EOF'
[Seeds] Required-slot seed table armed (slots 0/2/3/4); the ps_scan arm and both Lua override scans are skipped
[Seeds] Unbound by design: fname_tostring (Kismet fallback owns it), gnatives, fuobject_hash_tables_get; engine version via EngineVersionOverride settings
EOF
}

# ------------------------------------------------------------------ status --
write_status() { # $1=set  $2=pid  $3=ms  (state lines on stdin)
  { printf 'modforge-runtime-status\t1\n'
    printf 'session\t%s\t%s\n' "$2" "$3"
    cat
  } > "$FX/$1/from-dir/runtime-status.txt"
}
st_activated()    { printf 'state\t1\tactivated\t0\t\n'; }
st_engine_ready() { printf 'state\t2\tengine-ready\t0\t\n'; }
st_mods()         { printf 'state\t3\tmods-started\t0\t\n'; }
st_failed() { # $1=code $2=detail
  printf 'state\t2\tfailed\t%s\t%s\n' "$1" "$2"
}

# ---------------------------------------------------------- supplier diag --
SD_FIELDS='{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":%d,"pid":%s,"tag":"%s","value":%s,"value_hex":"%s","t_ms":%s}'
write_sdiag() { # $1=set  $2=pid  (rows: tag value value_hex t_ms on stdin, tab-separated)
  awk -F'\t' -v fmt="$SD_FIELDS" -v pid="$2" '
    { seq++; printf fmt "\n", seq, pid, $1, $2, $3, $4 }' > "$FX/$1/from-dir/modbridge-supplier-diag.jsonl"
}

# anchor values: BG source encodes value = (w << 16) | flag; order w0..w5 = E,D,B,C,C2,A
anchors_ok() {
  printf 'anchor_cov\t1\t0x00000001\t%s\n'    "$1"
  printf 'anchor_match\t1\t0x00000001\t%s\n'  "$(( $1 + 1 ))"
  printf 'anchor_cov\t65537\t0x00010001\t%s\n'   "$(( $1 + 2 ))"
  printf 'anchor_match\t65537\t0x00010001\t%s\n' "$(( $1 + 3 ))"
  printf 'anchor_cov\t131073\t0x00020001\t%s\n'  "$(( $1 + 4 ))"
  printf 'anchor_match\t131073\t0x00020001\t%s\n' "$(( $1 + 5 ))"
  printf 'anchor_cov\t196609\t0x00030001\t%s\n'  "$(( $1 + 6 ))"
  printf 'anchor_match\t196609\t0x00030001\t%s\n' "$(( $1 + 7 ))"
  printf 'anchor_cov\t262144\t0x00040000\t%s\n'  "$(( $1 + 8 ))"
  printf 'anchor_match\t262144\t0x00040000\t%s\n' "$(( $1 + 9 ))"
  printf 'anchor_cov\t327681\t0x00050001\t%s\n'  "$(( $1 + 10 ))"
  printf 'anchor_match\t327680\t0x00050000\t%s\n' "$(( $1 + 11 ))"
}

entry_ctors() { # $1=t0
  printf 'module_start_entered\t0\t0x00000000\t%s\n' "$1"
  printf 'entry_incoming_sp_mod16\t4108122368\t0xf4e2a000\t%s\n' "$(( $1 + 2 ))"
  printf 'entry_call_sp_mod16\t4108122384\t0xf4e2a010\t%s\n' "$(( $1 + 3 ))"
  printf 'constructor_count\t3\t0x00000003\t%s\n' "$(( $1 + 4 ))"
  printf 'constructor_begin\t0\t0x00000000\t%s\n' "$(( $1 + 5 ))"
  printf 'constructor_return\t0\t0x00000000\t%s\n' "$(( $1 + 17 ))"
  printf 'constructor_begin\t1\t0x00000001\t%s\n' "$(( $1 + 18 ))"
  printf 'constructor_return\t0\t0x00000000\t%s\n' "$(( $1 + 29 ))"
  printf 'constructor_begin\t2\t0x00000002\t%s\n' "$(( $1 + 30 ))"
  printf 'constructor_return\t0\t0x00000000\t%s\n' "$(( $1 + 41 ))"
}

conversion_ok() { # $1=t0  (D1-D5: extent, anchor_count=7, intervals=1, cfg attach)
  printf 'lmf_extent_base_low\t4194304\t0x00400000\t%s\n' "$1"
  printf 'lmf_extent_base_high\t0\t0x00000000\t%s\n' "$(( $1 + 1 ))"
  printf 'lmf_extent_end_low\t201720520\t0x0c0602c8\t%s\n' "$(( $1 + 2 ))"
  printf 'lmf_extent_end_high\t0\t0x00000000\t%s\n' "$(( $1 + 3 ))"
  printf 'lmf_anchor_count\t7\t0x00000007\t%s\n' "$(( $1 + 4 ))"
  printf 'lmf_intervals_found\t1\t0x00000001\t%s\n' "$(( $1 + 5 ))"
  printf 'bridge_cfg_gamedir\t1\t0x00000001\t%s\n' "$(( $1 + 10 ))"
  printf 'bridge_cfg_workdir\t1\t0x00000001\t%s\n' "$(( $1 + 11 ))"
}

walk_ok() { # $1=t0
  printf 'bridge_vmentry_entries\t10019\t0x00002723\t%s\n' "$1"
  printf 'bridge_vm_walk_stop\t0\t0x00000000\t%s\n' "$(( $1 + 140 ))"
  printf 'bridge_kve_distinct0\t3\t0x00000003\t%s\n' "$(( $1 + 141 ))"
  printf 'bridge_kve_distinct1\t2\t0x00000002\t%s\n' "$(( $1 + 142 ))"
  printf 'bridge_kve_distinct2\t1\t0x00000001\t%s\n' "$(( $1 + 143 ))"
  printf 'bridge_kve_distinct3\t1\t0x00000001\t%s\n' "$(( $1 + 144 ))"
  printf 'bridge_kve_eboot\t1\t0x00000001\t%s\n' "$(( $1 + 145 ))"
  printf 'bridge_exepath_source\t2\t0x00000002\t%s\n' "$(( $1 + 160 ))"
}

mfa_pair() { # $1=t0
  printf 'mfa_activated_low\t35890\t0x00008c32\t%s\n' "$1"
  printf 'mfa_activated_high\t0\t0x00000000\t%s\n' "$(( $1 + 9 ))"
}

# ================================ A: BEST ==================================
{
  entry_ctors 27638412
  printf 'runtime_wiring_begin\t0\t0x00000000\t%s\n' 27640155
  mfa_pair 27640201
  walk_ok 27641050
  anchors_ok 27641230
  printf 'bridge_lease_reg_spans\t6\t0x00000006\t%s\n' 27641260
  printf 'bridge_lease_reg\t6\t0x00000006\t%s\n' 27641262
  conversion_ok 27641280
  printf 'runtime_start_ok\t0\t0x00000000\t%s\n' 27644280
  printf 'module_start_return\t0\t0x00000000\t%s\n' 27644302
} | write_sdiag A 417

prologue 27661022133455 > /tmp/vr_A.txt
seeds_full >> /tmp/vr_A.txt
write_ue4ss A < /tmp/vr_A.txt

{ st_activated; st_engine_ready; st_mods; } | write_status A 417 27641000

# ================================ B: GOOD ==================================
# conversion+context pass; V1/V2 pass; pool refusal (F15 value-carrying) in status detail
{
  entry_ctors 27638412
  printf 'runtime_wiring_begin\t0\t0x00000000\t%s\n' 27640155
  mfa_pair 27640201
  walk_ok 27641050
  anchors_ok 27641230
  printf 'bridge_lease_reg_spans\t6\t0x00000006\t%s\n' 27641260
  printf 'bridge_lease_reg\t6\t0x00000006\t%s\n' 27641262
  conversion_ok 27641280
  printf 'runtime_start_ok\t0\t0x00000000\t%s\n' 27644280
  printf 'module_start_return\t0\t0x00000000\t%s\n' 27644302
} | write_sdiag B 417

prologue 27661022133455 > /tmp/vr_B.txt
seeds_past_v1v2 >> /tmp/vr_B.txt
write_ue4ss B < /tmp/vr_B.txt

{ st_activated
  st_failed 1 '[Seeds] REFUTED: FName pool at 0x9765ec0: CurrentBlock < 1 (pool not initialized or wrong pool/base) (observed CurrentBlock 0, Blocks[0] 0x0 vs span [0x8018c000,0x100118000)). Terminal for this launch; derive afresh with new evidence, never replay.'
} | write_status B 417 27641000

# =========================== C: MINIMAL-ACCEPT =============================
# conversion passes (intervals_found=1 + diag events); NEW walkguard refusal at slot 0
{
  entry_ctors 27638412
  printf 'runtime_wiring_begin\t0\t0x00000000\t%s\n' 27640155
  mfa_pair 27640201
  walk_ok 27641050
  anchors_ok 27641230
  printf 'bridge_lease_reg_spans\t6\t0x00000006\t%s\n' 27641260
  printf 'bridge_lease_reg\t6\t0x00000006\t%s\n' 27641262
  conversion_ok 27641280
  printf 'runtime_start_ok\t0\t0x00000000\t%s\n' 27644280
  printf 'module_start_return\t0\t0x00000000\t%s\n' 27644302
} | write_sdiag C 417

prologue 27661022133455 > /tmp/vr_C.txt
seeds_armed_only >> /tmp/vr_C.txt
write_ue4ss C < /tmp/vr_C.txt

{ st_activated
  st_failed 1 '[Seeds] REFUTED: slot 0 g_uobject_array 0x97ba110: seed class exec-only (call-only) (expected data-readable); refusing reads. Terminal for this launch; derive afresh with new evidence, never replay.'
} | write_status C 417 27641000

# ================================ D: FAIL ==================================
# the launch-9 geometry refusal recurs verbatim
{
  entry_ctors 27638412
  printf 'runtime_wiring_begin\t0\t0x00000000\t%s\n' 27640155
  mfa_pair 27640201
  walk_ok 27641050
  anchors_ok 27641230
  printf 'bridge_lease_reg_spans\t6\t0x00000006\t%s\n' 27641260
  printf 'bridge_lease_reg\t6\t0x00000006\t%s\n' 27641262
  printf 'lmf_extent_base_low\t4194304\t0x00400000\t%s\n' 27641280
  printf 'lmf_extent_base_high\t0\t0x00000000\t%s\n' 27641281
  printf 'lmf_extent_end_low\t201720520\t0x0c0602c8\t%s\n' 27641282
  printf 'lmf_extent_end_high\t0\t0x00000000\t%s\n' 27641283
  printf 'lmf_anchor_count\t7\t0x00000007\t%s\n' 27641284
  printf 'lmf_intervals_found\t0\t0x00000000\t%s\n' 27641285
  printf 'lmf_vmentry_cap\t0\t0x00000000\t%s\n' 27641286
  printf 'lmf_vm_scan_cap\t0\t0x00000000\t%s\n' 27641287
  printf 'lmf_linkmap_cap\t0\t0x00000000\t%s\n' 27641288
  printf 'lmf_provider_code\t0\t0x00000000\t%s\n' 27641289
  printf 'lmf_identity_source\t1\t0x00000001\t%s\n' 27641290
  printf 'runtime_start_failed\t1\t0x00000001\t%s\n' 27646987
  printf 'module_start_return\t0\t0x00000000\t%s\n' 27646990
} | write_sdiag D 417

{
  printf '[t=27646987ms pid=417] modbridge module_start failed: runtime_start_failed code=1 detail=no observed mapping interval intersects the module image extent with valid geometry\n'
  printf 'PATH_TEST: /data/plugins/UE4SS.log writable at 27658446233901 monotonic-ns\n'
  printf 'ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: 6d6f64666f7267652d6c6f61646c6973\n'
} | write_ue4ss D

{
  printf 'state\t1\tfailed\t1\truntime_start_failed no observed mapping interval intersects the module image extent with valid geometry\n'
} | write_status D 417 27641100

# ========================= E: S-30S timing row =============================
# sinks show FULL completion while module_start exceeded the 30 s ploader cap
{
  printf 'module_start_entered\t0\t0x00000000\t%s\n' 31200455
  printf 'constructor_count\t3\t0x00000003\t%s\n' 31200456
  printf 'runtime_wiring_begin\t0\t0x00000000\t%s\n' 31201801
  printf 'mfa_activated_low\t35890\t0x00008c32\t%s\n' 31201847
  printf 'mfa_activated_high\t0\t0x00000000\t%s\n' 31201856
  printf 'bridge_vmentry_entries\t10019\t0x00002723\t%s\n' 31202697
  printf 'bridge_vm_walk_stop\t0\t0x00000000\t%s\n' 31202834
  printf 'bridge_lease_reg_spans\t6\t0x00000006\t%s\n' 31202906
  printf 'bridge_lease_reg\t6\t0x00000006\t%s\n' 31202908
  printf 'lmf_extent_base_low\t4194304\t0x00400000\t%s\n' 31202926
  printf 'lmf_extent_base_high\t0\t0x00000000\t%s\n' 31202927
  printf 'lmf_extent_end_low\t201720520\t0x0c0602c8\t%s\n' 31202928
  printf 'lmf_extent_end_high\t0\t0x00000000\t%s\n' 31202929
  printf 'lmf_anchor_count\t7\t0x00000007\t%s\n' 31202930
  printf 'lmf_intervals_found\t1\t0x00000001\t%s\n' 31202931
  printf 'bridge_cfg_gamedir\t1\t0x00000001\t%s\n' 31202936
  printf 'bridge_cfg_workdir\t1\t0x00000001\t%s\n' 31202937
  printf 'runtime_start_ok\t0\t0x00000000\t%s\n' 31237720
  printf 'module_start_return\t0\t0x00000000\t%s\n' 31237761
} | write_sdiag E 419

prologue 31277899001234 > /tmp/vr_E.txt
seeds_full >> /tmp/vr_E.txt
write_ue4ss E < /tmp/vr_E.txt

{ st_activated; st_engine_ready; st_mods; } | write_status E 419 31211900

rm -f /tmp/vr_A.txt /tmp/vr_B.txt /tmp/vr_C.txt /tmp/vr_E.txt
echo "fixtures built under $FX"
for s in A B C D E; do
  printf '%s: ' "$s"
  wc -c "$FX/$s/from-dir/"* | tail -1
done
