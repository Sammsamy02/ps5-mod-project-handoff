#!/usr/bin/env zsh
# run_host_tests.sh — fixture host-tests for rung_check.py (stdlib-only tool).
# Builds fixtures under host-test/fixtures/<case>/ and asserts exit codes and
# verdict lines. No console contact; everything is local.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
TOOL="$HERE/../rung_check.py"
FX="$HERE/fixtures"
PASS=0; FAIL=0

check() { # name expected_exit expected_pattern actual_exit actual_output
  local name="$1" want_code="$2" want_pat="$3" got_code="$4" got_out="$5"
  if [[ "$got_code" == "$want_code" ]] && printf '%s' "$got_out" | grep -q -- "$want_pat"; then
    printf 'PASS %-42s exit=%s match=%s\n' "$name" "$got_code" "$want_pat"; PASS=$((PASS+1))
  else
    printf 'FAIL %-42s exit=%s (want %s) pattern=%s\n' "$name" "$got_code" "$want_code" "$want_pat"
    printf '%s\n' "$got_out" | sed -n '1,12p'
    FAIL=$((FAIL+1))
  fi
}

KEY="0x0000000000000f3c"
QH5="{\"record\":\"native_query_harness\",\"magic\":\"NQH1\",\"status\":\"pending\",\"engine\":\"5.6\",\"axes\":{\"non_shipping\":1,\"stats\":0},\"storage\":\"unknown\",\"count_source\":\"engine_uobject_array\",\"game_thread_id_key\":\"$KEY\",\"counts\":{\"first\":0,\"second\":0,\"recorded\":0},\"delta\":0,\"monotonic\":\"unknown\",\"identity_checks\":1,\"threshold\":10000,\"ticks\":{\"dispatched\":9,\"accepted\":5,\"refusals_between\":0},\"gate_source\":\"PublishStagedNativeLayoutMaps\"}"
QH9="{\"record\":\"native_query_harness\",\"magic\":\"NQH1\",\"status\":\"completed\",\"engine\":\"5.6\",\"axes\":{\"non_shipping\":1,\"stats\":0},\"storage\":\"chunked_array\",\"count_source\":\"engine_uobject_array\",\"game_thread_id_key\":\"$KEY\",\"counts\":{\"first\":48152,\"second\":48152,\"recorded\":2},\"delta\":0,\"monotonic\":\"equal\",\"identity_checks\":2,\"threshold\":10000,\"ticks\":{\"dispatched\":12,\"accepted\":9,\"refusals_between\":0},\"gate_source\":\"PublishStagedNativeLayoutMaps\"}"
IG="{\"marker\":\"modbridge.ingamefacts\",\"schema\":1,\"build_id\":\"igf6-run1\",\"seq\":3,\"exp\":\"objarray_count\",\"rc\":0,\"errno\":0,\"fields\":{\"count\":48152}}"
REFusal="{\"refused_wrong_thread\":2,\"refused_identity_unknown\":1}"

mk_status() { # dir last-ladder-lines...
  local d="$1"; shift
  # tools-final audit fix: "$(( $+1 ))" was an invalid arithmetic token — under
  # bash/sh it is a syntax error (state rows silently LOST -> rung1-pass and
  # placeholders cases failed); under zsh it silently evaluated to 0 (every seq
  # 0). Emit the real incrementing seq per the mfa status grammar.
  local seq=1
  { printf 'modforge-runtime-status\t1\n'; printf 'session\t298\t8866900000000\n'; for s in "$@"; do printf 'state\t%d\t%s\t0\t\n' "$seq" "$s"; seq=$((seq + 1)); done; } > "$d/runtime-status.txt"
}

mk_case() {
  local d="$FX/$1"; mkdir -p "$d"; echo "$d"
}

# --- case rung1-pass --------------------------------------------------------
d=$(mk_case rung1-pass)
printf '%s\n%s\n%s\n' "$IG" "$QH5" "$REFusal" > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$QH9" > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated engine-ready mods-started
printf 'Native console, GUI, crash dumping and filesystem watching are unavailable\nUE4SS - v1.12.0 - Git SHA #deadbeef\nUsing engine version: 5.6\nSetting up mods...\n[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.\n' > "$d/UE4SS.log"
printf '[PPSA33286] modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556 module_start_entered value=0x00000000\n' > "$d/townfall_runtime_aligned_entry.log"
printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "rung1-pass" 0 "RUNG1: PASS" "$code" "$out"

# --- case rung1-status-stuck-activated --------------------------------------
d=$(mk_case rung1-status-activated)
printf '%s\n%s\n' "$IG" "$QH5" > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$QH9" > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated
printf '' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "rung1-status-activated FAIL" 1 "status_engine_ready_or_later" "$code" "$out"

# --- case rung1-c2-missing ---------------------------------------------------
d=$(mk_case rung1-c2-missing)
mk_status "$d" activated engine-ready mods-started
printf '' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "rung1-c2-missing FAIL" 1 "c2_present" "$code" "$out"

# --- case rung1-ticks-not-increasing ----------------------------------------
d=$(mk_case rung1-ticks-flat)
printf '%s\n%s\n' "$IG" "$QH5" > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$QH5" > "$d/modbridge-ingame-facts.jsonl.snap2"   # same ticks
mk_status "$d" activated engine-ready mods-started
printf '' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "rung1-ticks-flat FAIL" 1 "ticks_accepted_strictly_increasing" "$code" "$out"

# --- case rung1-key-flip ------------------------------------------------------
KEY2="0x0000000000000abc"
QH9B=$(echo "$QH9" | sed "s/$KEY/$KEY2/")
d=$(mk_case rung1-key-flip)
printf '%s\n%s\n' "$IG" "$QH5" > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$QH9B" > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated engine-ready mods-started
printf '' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "rung1-key-flip FAIL" 1 "game_thread_id_key_stable_nonzero" "$code" "$out"

# --- case rung1-no-guard-refusal ---------------------------------------------
d=$(mk_case rung1-no-refusal)
printf '%s\n%s\n' "$IG" "$QH5" > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$QH9" > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated engine-ready mods-started
printf '' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "rung1-no-refusal FAIL" 1 "at_least_one_counted_guard_refusal" "$code" "$out"

# --- case rung1-unverified-driver (ticks move, no qh lines) ------------------
d=$(mk_case rung1-unverified-driver)
printf '{"marker":"modbridge.ingamefacts","schema":1,"build_id":"x","seq":1,"exp":"e","rc":0,"errno":0,"fields":{"ticks_accepted":5,"refused_wrong_thread":1}}\n' > "$d/modbridge-ingame-facts.jsonl"
printf '{"marker":"modbridge.ingamefacts","schema":1,"build_id":"x","seq":2,"exp":"e","rc":0,"errno":0,"fields":{"ticks_accepted":9}}\n' > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated engine-ready mods-started
printf '' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "rung1-unverified-driver" 1 "RUNG1: UNVERIFIED" "$code" "$out"

# --- case rung2-pass ----------------------------------------------------------
d=$(mk_case rung2-pass)
printf '%s\n%s\n%s\n' "$IG" "$QH5" "$QH9" > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$QH9" > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated engine-ready mods-started
printf 'Using engine version: 5.6\n' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG2 2>&1); code=$?
check "rung2-pass" 0 "RUNG2: PASS" "$code" "$out"

# --- case rung2-inconclusive (honest negative -> FAIL, not re-rated) ---------
INCON="{\"record\":\"native_query_harness\",\"magic\":\"NQH1\",\"status\":\"inconclusive_below_threshold\",\"engine\":\"5.6\",\"axes\":{\"non_shipping\":1,\"stats\":0},\"storage\":\"unknown\",\"count_source\":\"engine_uobject_array\",\"game_thread_id_key\":\"$KEY\",\"counts\":{\"first\":9999,\"second\":10000,\"recorded\":2},\"delta\":1,\"monotonic\":\"non_decreasing\",\"identity_checks\":2,\"threshold\":10000,\"ticks\":{\"dispatched\":12,\"accepted\":9,\"refusals_between\":0},\"gate_source\":\"PublishStagedNativeLayoutMaps\"}"
d=$(mk_case rung2-inconclusive)
printf '%s\n' "$INCON" > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$INCON" > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated engine-ready mods-started
printf '' > "$d/UE4SS.log"; printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" --rung RUNG2 2>&1); code=$?
check "rung2-inconclusive FAIL" 1 "do NOT re-rate" "$code" "$out"

# --- case missing-everything (CANNOT_ASSERT) ----------------------------------
d=$(mk_case empty)
out=$("$TOOL" --from-dir "$d" --rung RUNG1 2>&1); code=$?
check "empty-dir CANNOT_ASSERT" 3 "CANNOT_ASSERT" "$code" "$out"

# --- case placeholders (ALL) ---------------------------------------------------
d=$(mk_case placeholders)
{
  printf '%s\n' "$IG"
  printf '%s\n' "$QH5"
  printf '%s\n' "$REFusal"
  printf '%s\n' '{"marker":"modbridge.ingamefacts","schema":1,"build_id":"ig_run_fc1_probe-a","seq":1,"exp":"fc1a","rc":0,"errno":0,"fields":{"match":1}}'
} > "$d/modbridge-ingame-facts.jsonl"
printf '%s\n' "$QH9" > "$d/modbridge-ingame-facts.jsonl.snap2"
mk_status "$d" activated engine-ready mods-started
printf 'Using engine version: 5.6\n[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.\n[Lua] [TownfallItemSpawner] Scan failed: Item database not ready. Load a save first.\n[Lua] [TownfallItemSpawner] Spawned\n' > "$d/UE4SS.log"
printf '' > "$d/townfall_runtime_aligned_entry.log"; printf '' > "$d/townfall_runtime_aligned_observer.log"
out=$("$TOOL" --from-dir "$d" 2>&1); code=$?
check "placeholders ALL exit0" 0 "VERDICT: PASS" "$code" "$out"
if printf '%s' "$out" | grep -q "RUNG3: PLACEHOLDER" && printf '%s' "$out" | grep -q '"load_lines": 1' && printf '%s' "$out" | grep -q '"fc1_records_in_c2": 1'; then
  printf 'PASS %-42s\n' "placeholder counts (fc1/load line)"; PASS=$((PASS+1))
else
  printf 'FAIL %-42s\n' "placeholder counts (fc1/load line)"; printf '%s\n' "$out" | grep 'RUNG[345]'; FAIL=$((FAIL+1))
fi

# --- discovery with fake listing ------------------------------------------------
d=$(mk_case discover)
printf 'drwxr-xr-x 2 root root 0 Jan 1 12:00 modbridge\ndrwxr-xr-x 2 root root 0 Jan 1 12:00 plugins\n-rw-r--r-- 1 root root 512 Jan 1 12:00 modbridge-ingame-facts.jsonl\n-rw-r--r-- 1 root root 100 Jan 1 12:00 townfall_runtime_aligned_entry.log\n-rw-r--r-- 1 root root 40 Jan 1 12:00 unrelated.txt\n' > "$d/dirlist-data.txt"
printf 'drwxr-xr-x 2 root root 0 Jan 1 12:00 ue4ss.log\n' > "$d/dirlist-data-modbridge-roots-PPSA33286.txt"
out=$("$TOOL" --from-dir "$d" --discover 2>&1); code=$?
check "discover-fake-listing" 0 "modbridge-ingame-facts.jsonl" "$code" "$out"

# --- dry-run ---------------------------------------------------------------------
out=$("$TOOL" --dry-run 2>&1); code=$?
check "dry-run curl commands" 0 "ftp://ps5-host-not-configured.invalid:2121/data/modbridge-ingame-facts.jsonl" "$code" "$out"

echo "----"
echo "host-test: $PASS passed, $FAIL failed"
exit $(( FAIL > 0 ? 1 : 0 ))
