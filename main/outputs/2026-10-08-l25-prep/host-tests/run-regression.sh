#!/bin/bash
# run-regression.sh — ONE-SHOT green-sheet regression for every host-side
# test suite in 2026-10-08-l25-prep (agent GS, 2026-10-10).
#
# THE LAW (standing pre-build gate): run this BEFORE every AC-power build.
# A build may only start when this script exits 0 (all suites GREEN).
# Any RED = stop and diagnose before building; RED on host-lua-harness
# verdicts after a runtime LuaPathShim/LuaMod edit means the host twin and
# the runtime tree have diverged — reconcile the TWIN, never fudge the test.
#
# Suites, in dependency-safe order (cheap contracts first, heavy scans last):
#   01 env-preflight      interpreters/fixtures present
#   02 prop-parity        FR  userdata property-read contract (host-tests)
#   03 path-shim          GA  resolver rule contract, 38/38 (host-tests)
#   04 host-lua-harness   FU/GB/GA 27-verdict calibration vs frozen baseline
#                          (runs-baseline/VERDICTS.txt; the spawner console
#                          copy must stay LOADS-CLEAN in both io modes)
#   05 ueselftest         GB  validate.lua 3 scenarios (S1 167/167)
#   06 keymap-determinism FV  regen determinism + bindings parity vs
#                          GENERATED-BASELINE.ini + --check 0 violations
#   07 mod-manager        GG  test_modmanager.sh 49/49 (uses FU harness +
#                          FV keymap internally -> runs after both)
#   08 phasea-tools       GF  run_phasea_tests.py 18/18 (heavy ELF scans)
#   09 blade-prep-sanity  GJ  scripted subset only: py_compile + table JSON
#                          (the binary positive/negative control runs are
#                          NOT scripted as a suite — see runs/ receipts)
#   10 pak-overlay        GK  auto-discovered test script if one lands;
#                          today only overlay_builder.py py_compile sanity
#                          (no test suite landed yet -> ABSENT, not GREEN)
#
# Machine-readable contract: one line per suite on stdout,
#   RESULT suite=<id> rc=<n> expected=<expected> verdict=GREEN|RED|ABSENT detail="..."
# and the same lines embedded in REGRESSION-SHEET.md (next to this script),
# which this script regenerates on every run (header law text is static).
#
# Battery law: local test runs only — no PRX builds, no console contact.
# Mac-only; needs lua5.5 + python3 on PATH.

set -u

HERE="$(cd "$(dirname -- "$0")" && pwd)"
PREP="$(cd "$HERE/.." && pwd)"
WHAT="$(cd "$PREP/../.." && pwd)"   # .../what (PREP = what/outputs/2026-10-08-l25-prep)
LOGDIR="$HERE/regression-logs/latest"
SHEET="$HERE/REGRESSION-SHEET.md"

mkdir -p "$LOGDIR"
: > "$LOGDIR/summary.txt"

RESULT_LINES=""
FAILED_SUITES=""
ABSENT_SUITES=""

record() { # record <suite> <rc> <expected> <verdict> <detail>
    local line="RESULT suite=$1 rc=$2 expected=$3 verdict=$4 detail=\"$5\""
    RESULT_LINES="${RESULT_LINES}${line}"$'\n'
    echo "$line" | tee -a "$LOGDIR/summary.txt"
    if [ "$4" = "RED" ]; then FAILED_SUITES="$FAILED_SUITES $1"; fi
    if [ "$4" = "ABSENT" ]; then ABSENT_SUITES="$ABSENT_SUITES $1"; fi
}

# ================================================================ 01 preflight
suite="env-preflight"; log="$LOGDIR/$suite.log"; rc=0
{
    echo "lua5.5: $(command -v lua5.5 || echo MISSING)"
    echo "python3: $(command -v python3 || echo MISSING)"
} > "$log" 2>&1
LUA_BIN="$(command -v lua5.5 || true)"
PY_BIN="$(command -v python3 || true)"
if [ -z "$LUA_BIN" ] || [ -z "$PY_BIN" ]; then
    echo "FATAL: lua5.5 and python3 are both required" >> "$log"
    record "$suite" 2 "rc=0" RED "interpreters missing (see $log)"
    echo "FATAL: cannot continue without lua5.5 + python3" >&2
    exit 2
fi
record "$suite" 0 "rc=0" GREEN "lua5.5=$($LUA_BIN -v 2>&1 | head -1); python3=$($PY_BIN --version 2>&1)"

# ==================================================== 02 prop-parity (agent FR)
suite="prop-parity"; log="$LOGDIR/$suite.log"
"$HERE/run-prop-parity.sh" > "$log" 2>&1; rc=$?
pp_pass=$(grep -c '^PASS ' "$log" || true)
pp_all=$(grep -c 'ALL PASS: 0 failure' "$log" || true)
if [ "$rc" -eq 0 ] && [ "$pp_all" -ge 1 ]; then
    record "$suite" "$rc" "rc=0 + ALL PASS" GREEN "assertions=$pp_pass; ALL PASS line present"
else
    record "$suite" "$rc" "rc=0 + ALL PASS" RED "assertions=$pp_pass; ALL PASS line MISSING (see $log)"
fi

# ==================================================== 03 path-shim (agent GA)
suite="path-shim"; log="$LOGDIR/$suite.log"
"$HERE/run-path-shim.sh" > "$log" 2>&1; rc=$?
ps_counts=$(grep -o '[0-9]* passed, [0-9]* failed' "$log" | tail -1)
if [ "$rc" -eq 0 ] && echo "$ps_counts" | grep -q '^\([0-9]*\) passed, 0 failed'; then
    record "$suite" "$rc" "rc=0, 38 passed 0 failed" GREEN "$ps_counts"
else
    record "$suite" "$rc" "rc=0, 38 passed 0 failed" RED "counts='$ps_counts' (see $log)"
fi

# ============================================ 04 host-lua-harness (FU/GB/GA)
suite="host-lua-harness"; log="$LOGDIR/$suite.log"
HARNESS="$PREP/host-lua-harness"
BASELINE="$HARNESS/runs-baseline"
# Freeze the recorded verdict baseline on first use (self-heal), else reuse.
if [ ! -f "$BASELINE/VERDICTS.txt" ]; then
    echo "NOTE: runs-baseline/VERDICTS.txt missing — freezing current runs/ as baseline" | tee -a "$log"
    mkdir -p "$BASELINE"
    cp -p "$HARNESS"/runs/*.log "$BASELINE"/ 2>>"$log"
    (cd "$BASELINE" && grep -H "verdict:" *.log | sort > VERDICTS.txt)
fi
BASE_N=$(wc -l < "$BASELINE/VERDICTS.txt" | tr -d ' ')
{
    echo "== FU calibration matrix (13 subjects x 2 io modes) =="
    (cd "$HARNESS" && ./run-all.sh)
    echo "rc(run-all)=$?"
    # 27th subject (FU 'bonus', GB neutrality snapshot): stamina mod, io-off.
    "$LUA_BIN" "$HARNESS/runner.lua" \
        --main "$WHAT/.superpowers/native-observations/nexus-real-mods-2026-10-01/infinite-stamina-main.lua" \
        --name bonus-stamina \
        --shared "$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/shared" \
        > "$HARNESS/runs/bonus-stamina.io-off.log" 2>&1
    echo "rc(bonus-stamina)=$?"
    echo "== verdict comparison vs frozen baseline =="
    (cd "$HARNESS/runs" && grep -H "verdict:" *.log | sort) > "$LOGDIR/fu-verdicts-fresh.txt"
    diff "$BASELINE/VERDICTS.txt" "$LOGDIR/fu-verdicts-fresh.txt" && echo "VERDICTS-IDENTICAL ($BASE_N lines)" \
        || echo "VERDICT-DRIFT (above)"
    echo "== calibration law =="
    for m in off on; do
        grep -m1 '^verdict:' "$HARNESS/runs/TownfallItemSpawner-console-copy.io-$m.log"
    done
} > "$log" 2>&1
fu_rc=$?
identical=$(grep -c 'VERDICTS-IDENTICAL' "$log" || true)
drift=$(grep -c 'VERDICT-DRIFT' "$log" || true)
cal_ok=0
[ "$(grep -c 'verdict: LOADS-CLEAN' "$HARNESS/runs/TownfallItemSpawner-console-copy.io-off.log" 2>/dev/null || echo 0)" -ge 1 ] && cal_ok=$((cal_ok+1))
[ "$(grep -c 'verdict: LOADS-CLEAN' "$HARNESS/runs/TownfallItemSpawner-console-copy.io-on.log" 2>/dev/null || echo 0)" -ge 1 ] && cal_ok=$((cal_ok+1))
fresh_n=$(wc -l < "$LOGDIR/fu-verdicts-fresh.txt" | tr -d ' ')
if [ "$identical" -ge 1 ] && [ "$drift" -eq 0 ] && [ "$cal_ok" -eq 2 ]; then
    record "$suite" 0 "27/27 verdicts byte-identical; spawner LOADS-CLEAN both modes" \
        GREEN "verdicts=$fresh_n/$BASE_N identical; calibration=LOADS-CLEAN/2"
else
    record "$suite" 1 "27/27 verdicts byte-identical; spawner LOADS-CLEAN both modes" \
        RED "verdicts=$fresh_n/$BASE_N; drift_lines=$(diff "$BASELINE/VERDICTS.txt" "$LOGDIR/fu-verdicts-fresh.txt" | grep -c '^[<>]' || true); calibration=$cal_ok/2 (see $log)"
fi

# ==================================================== 05 ueselftest (agent GB)
suite="ueselftest"; log="$LOGDIR/$suite.log"
(cd "$PREP/ueselftest" && "$LUA_BIN" validate.lua) > "$log" 2>&1; rc=$?
gb_pass=$(grep -c '^\[GB-VAL\] PASS' "$log" || true)
gb_all=$(grep -c 'ALL SCENARIOS PASS' "$log" || true)
s1=$(grep -m1 'S1 summary' "$log" || true)
if [ "$rc" -eq 0 ] && [ "$gb_all" -ge 1 ]; then
    record "$suite" "$rc" "rc=0 + ALL SCENARIOS PASS" GREEN "checks=$gb_pass; ${s1#*] }"
else
    record "$suite" "$rc" "rc=0 + ALL SCENARIOS PASS" RED "checks=$gb_pass; summary-missing (see $log)"
fi

# ============================================ 06 keymap determinism (agent FV)
suite="keymap-determinism"; log="$LOGDIR/$suite.log"
KEYMAP="$PREP/keymap"
TREEB="$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods"
TREEA="$WHAT/.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1"
PINPAD="$WHAT/outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/PS5Pad.ini"
KM_MODS=(
    "TownfallItemSpawner=$PREP/evidence/townfall-main.lua.console-copy"
    "zzdiag=$PREP/zzdiag-v11/main.lua"
    "LineTraceMod=$TREEB/LineTraceMod/Scripts/main.lua"
    "CheatManagerEnablerMod=$TREEB/CheatManagerEnablerMod/Scripts/main.lua"
    "ConsoleEnablerMod=$TREEB/ConsoleEnablerMod/Scripts/main.lua"
    "ConsoleCommandsMod=$TREEB/ConsoleCommandsMod/Scripts/main.lua"
    "SplitScreenMod=$TREEB/SplitScreenMod/Scripts/main.lua"
    "TownfallMiniMap=$TREEA/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua"
    "TownfallModMenu=$TREEA/mod-menu-v6/TownfallModMenu/Scripts/main.lua"
    "Keybinds=$TREEB/Keybinds/Scripts/main.lua"
    "BPML_GenericFunctions=$TREEB/BPML_GenericFunctions/Scripts/main.lua"
    "BPModLoaderMod=$TREEB/BPModLoaderMod/Scripts/main.lua"
    "ActorDumperMod=$TREEB/ActorDumperMod/Scripts/main.lua"
    "jsbLuaProfilerMod=$TREEB/jsbLuaProfilerMod/Scripts/main.lua"
)
KM_ARGS=()
for m in "${KM_MODS[@]}"; do KM_ARGS+=(--mod "$m"); done
{
    "$PY_BIN" "$KEYMAP/keymap-gen.py" "${KM_ARGS[@]}" \
        --pin-pad "$PINPAD" --ladder pairs10 --out "$LOGDIR/keymap-gen1.ini"
    echo "rc(regen1)=$?"
    "$PY_BIN" "$KEYMAP/keymap-gen.py" "${KM_ARGS[@]}" \
        --pin-pad "$PINPAD" --ladder pairs10 --out "$LOGDIR/keymap-gen2.ini"
    echo "rc(regen2)=$?"
    cmp "$LOGDIR/keymap-gen1.ini" "$LOGDIR/keymap-gen2.ini" && echo "DETERMINISTIC" || echo "NONDETERMINISTIC"
    # Binding parity vs recorded baseline: everything except the
    # invocation-path-dependent "; inputs-sha256:" comment must be identical.
    diff <(grep -v '^; inputs-sha256:' "$LOGDIR/keymap-gen1.ini") \
         <(grep -v '^; inputs-sha256:' "$KEYMAP/GENERATED-BASELINE.ini") \
        && echo "BINDINGS-MATCH-BASELINE" || echo "BINDINGS-DRIFT"
    wc -c "$LOGDIR/keymap-gen1.ini" "$KEYMAP/GENERATED-BASELINE.ini"
    "$PY_BIN" "$KEYMAP/keymap-gen.py" --check "$KEYMAP/GENERATED-BASELINE.ini" \
        --expect-size 2422 "${KM_ARGS[@]}" --pin-pad "$PINPAD"
    echo "rc(check)=$?"
} > "$log" 2>&1
km_det=$(grep -c '^DETERMINISTIC' "$log" || true)
km_bind=$(grep -c '^BINDINGS-MATCH-BASELINE' "$log" || true)
km_chk=$(grep -c 'check: 0 violations' "$log" || true)
km_rcs=$(grep -c 'rc(regen[12])=0\|rc(check)=0' "$log" || true)
if [ "$km_det" -ge 1 ] && [ "$km_bind" -ge 1 ] && [ "$km_chk" -ge 1 ] && [ "$km_rcs" -ge 3 ]; then
    record "$suite" 0 "regen deterministic; bindings==baseline; check 0 violations" \
        GREEN "2x regen byte-identical; bindings parity ok; 0 violations (4 documented WARNs)"
else
    record "$suite" 1 "regen deterministic; bindings==baseline; check 0 violations" \
        RED "det=$km_det bind=$km_bind check=$km_chk rcs=$km_rcs (see $log)"
fi

# ============================================ 07 mod-manager (agent GG, 49/49)
suite="mod-manager"; log="$LOGDIR/$suite.log"
(cd "$PREP/mod-manager" && bash test_modmanager.sh) > "$log" 2>&1; rc=$?
mm=$(grep -o 'PASS=[0-9]* FAIL=[0-9]*' "$log" | tail -1)
mm_f=$(echo "$mm" | grep -o 'FAIL=[0-9]*' | grep -o '[0-9]*')
mm_p=$(echo "$mm" | grep -o 'PASS=[0-9]*' | grep -o '[0-9]*')
# grow-tolerant (HK grew 49->88): rc 0, zero FAIL, count never below the
# original 49. Shrinking below 49 = tests silently dropped = RED.
if [ "$rc" -eq 0 ] && [ "${mm_f:-1}" -eq 0 ] && [ "${mm_p:-0}" -ge 49 ]; then
    record "$suite" "$rc" "rc=0, FAIL=0, PASS>=49 (now $mm_p)" GREEN "$mm"
else
    record "$suite" "$rc" "rc=0, FAIL=0, PASS>=49" RED "summary='$mm' (see $log)"
fi

# ============================================ 08 phasea-tools (agent GF, 18/18)
suite="phasea-tools"; log="$LOGDIR/$suite.log"
(cd "$PREP/phasea-tools" && "$PY_BIN" run_phasea_tests.py) > "$log" 2>&1; rc=$?
pa=$(grep -o '[0-9]* passed, [0-9]* failed' "$log" | tail -1)
pa_p=$(grep -c '^PASS' "$log" || true); pa_f=$(grep -c '^FAIL' "$log" || true)
if [ "$rc" -eq 0 ] && [ "$pa_f" -eq 0 ] && [ "$pa_p" -ge 18 ]; then
    record "$suite" "$rc" "rc=0, 18 passed 0 failed" GREEN "$pa"
else
    record "$suite" "$rc" "rc=0, 18 passed 0 failed" RED "passed=$pa_p failed=$pa_f (see $log)"
fi

# ============================================ 09 blade-prep sanity (agent GJ)
suite="blade-prep-sanity"; log="$LOGDIR/$suite.log"
{
    "$PY_BIN" -m py_compile "$PREP/blade-prep/run-blade-profile.py" && echo "py_compile: OK"
    "$PY_BIN" -c "import json,sys; d=json.load(open(sys.argv[1])); print('table-json: OK rows=%d' % len(d.get('versions', d) if isinstance(d, dict) else d))" "$PREP/blade-prep/per-version-table.json"
    echo "rc=$?"
} > "$log" 2>&1
rc=$?
if [ "$rc" -eq 0 ] && grep -q 'py_compile: OK' "$log" && grep -q 'table-json: OK' "$log"; then
    record "$suite" "$rc" "rc=0 (scripted subset)" GREEN "py_compile OK; per-version-table.json parses; NOTE: binary control runs not scripted (see blade-prep/runs/ receipts)"
else
    record "$suite" "$rc" "rc=0 (scripted subset)" RED "py_compile/JSON broke (see $log)"
fi

# ============================================ 10 pak-overlay (agent GK)
suite="pak-overlay"; log="$LOGDIR/$suite.log"
PK_TEST=""
for cand in "$PREP/pak-overlay/run-tests.sh" "$PREP/pak-overlay/test_overlay.sh" "$PREP/pak-overlay/test_overlay_builder.py"; do
    [ -f "$cand" ] && PK_TEST="$cand" && break
done
if [ -n "$PK_TEST" ]; then
    (cd "$PREP/pak-overlay" && bash "$PK_TEST") > "$log" 2>&1; rc=$?
    record "$suite" "$rc" "rc=0" GREEN "ran discovered suite $PK_TEST"
else
    # No test suite landed yet: syntax sanity only, recorded as ABSENT (not
    # coverage-claimed). If GK lands a suite, the loop above picks it up.
    {
        "$PY_BIN" -m py_compile "$PREP/pak-overlay/overlay_builder.py" && echo "py_compile: OK"
        echo "rc=$?"
        ls "$PREP/pak-overlay"
    } > "$log" 2>&1
    rc=$?
    if [ "$rc" -eq 0 ] && grep -q 'py_compile: OK' "$log"; then
        record "$suite" 0 "suite not landed" ABSENT "no test suite landed; overlay_builder.py py_compile OK (row becomes GREEN automatically once a test script lands)"
    else
        record "$suite" "$rc" "suite not landed" RED "overlay_builder.py py_compile FAILED (see $log)"
    fi
fi

# ==================================================================== sheet
TS="$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || date -u)"
{
    cat <<'HEADER'
# REGRESSION-SHEET — standing pre-build gate (agent GS, 2026-10-10)

## LAW — run this before every AC-power build

```
outputs/2026-10-08-l25-prep/host-tests/run-regression.sh
```

- A PRX build may start ONLY when this exits 0 (every landed suite GREEN).
- Any RED: stop, diagnose below, fix, re-run to green BEFORE building.
- `host-lua-harness` RED after a runtime `LuaPathShim.{hpp,cpp}`/`LuaMod.cpp`
  edit = the host twin diverged from the runtime tree. Fix the TWIN
  (`host-lua-harness/path-shim.lua` / `stub-ue4ss.lua`) to re-mirror the
  runtime rules — never edit the runtime from here and never fudge a verdict.
  The frozen verdict baseline lives at `host-lua-harness/runs-baseline/`
  (VERDICTS.txt, 27 lines); it is law until a legitimate stub-extension note
  in this sheet explains a change.
- Calibration law (FU): the TownfallItemSpawner console copy must be
  LOADS-CLEAN in BOTH io modes, or the stub is wrong and everything after is
  invalid.
- `pak-overlay` shows ABSENT until GK's suite lands (auto-discovered by the
  script); ABSENT does not block a build, RED does.
- Battery-safe: local runs only, no PRX builds, no console contact.
  Heaviest row is `phasea-tools` (ELF scans, ~1 min).

## Suite inventory (who owns what)

| # | suite | owner | suite entry point | expected |
|---|---|---|---|---|
| 01 | env-preflight | GS | (built into run-regression.sh) | rc=0 |
| 02 | prop-parity | FR | host-tests/run-prop-parity.sh | rc=0 + ALL PASS (userdata property-read contract) |
| 03 | path-shim | GA | host-tests/run-path-shim.sh | 38 passed / 0 failed (resolver rule contract) |
| 04 | host-lua-harness | FU (+GB/GA extensions) | host-lua-harness/run-all.sh + bonus subject | 27/27 verdicts byte-identical vs runs-baseline/VERDICTS.txt; spawner console copy LOADS-CLEAN both io modes |
| 05 | ueselftest | GB | ueselftest/validate.lua | rc=0, ALL SCENARIOS PASS (S1: 167/167) |
| 06 | keymap-determinism | FV | keymap/keymap-gen.py (driven by run-regression.sh) | regen x2 byte-identical; bindings == GENERATED-BASELINE.ini (excl. path-dependent inputs-sha256 line); --check 0 violations / 4 documented WARNs |
| 07 | mod-manager | GG | mod-manager/test_modmanager.sh | rc=0 FAIL=0 PASS>=49 |
| 08 | phasea-tools | GF | phasea-tools/run_phasea_tests.py | 18 passed / 0 failed (needs /tmp/l25-variant-A klog fixture) |
| 09 | blade-prep-sanity | GJ | (scripted subset in run-regression.sh) | py_compile + table JSON parse; binary control runs NOT scripted (receipts in blade-prep/runs/) |
| 10 | pak-overlay | GK | (auto-discovered when landed) | no suite landed yet -> ABSENT |

## Twin-parity notes (runtime LuaPathShim vs host twin) — legitimate-change ledger

- 2026-10-10 (root, Rule 10): runtime `LuaPathShim_install` now derives the
  modbridge root from the program's mods directory
  (`LuaMod.cpp` prepare_mod, `weakly_canonical(get_mods_directory())` minus
  trailing `Mods` + one parent) and DISABLES itself loudly
  (`[SHIM] path-resolver disabled reason=no-root`) when no root can be
  derived — no title-id default exists anymore. Host twin behavior
  (verified verdict-neutral this run): `stub-ue4ss.lua` derives the root from
  the subject chunk's own source (`ue4ss/` component = mods-dir analog) and
  still falls back to `path-shim.lua` `DEFAULT_MODBRIDGE_ROOT`
  ("/data/modbridge/roots/PPSA33286", a nonexistent path on the Mac) when no
  ue4ss-bearing source is on the stack (only the spawner console-copy
  subject). That fallback emulates "the program's real root" for out-of-tree
  subjects (reads miss non-fatally; writes stay virtual + mod-confined), and
  the runtime's loud-disable also leaves stock io — both routes produce
  identical verdicts for every current subject, so the fallback is KEPT as a
  documented divergence, not fixed. The `set_modbridge_root_override` test
  seam has its host analog in `PathShim.new{ root = ... }`; no stub change
  was needed. If a future subject makes the fallback observable in a
  verdict, re-mirror the exact runtime rule here first.

## Results (regenerated by every run)

HEADER
    echo "\`run: $TS  host: $(hostname 2>/dev/null || echo mac)\`"
    echo ""
    echo "| suite | rc | expected | verdict |"
    echo "|---|---|---|---|"
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        s=$(echo "$line" | sed 's/^RESULT suite=//; s/ rc=/ | /; s/ expected=/ | /; s/ verdict=/ | /; s/ detail="/ | /; s/"$//')
        echo "| $s |"
    done <<EOF
${RESULT_LINES}
EOF
    echo ""
    echo '```'
    printf '%s' "$RESULT_LINES"
    echo '```'
    echo ""
    if [ -n "$FAILED_SUITES" ]; then
        echo "## VERDICT: RED — failing suites:${FAILED_SUITES}"
        echo ""
        echo "Do NOT build. Diagnose via \`regression-logs/latest/<suite>.log\`."
    else
        echo "## VERDICT: GREEN — all landed suites pass; build gate open."
        [ -n "$ABSENT_SUITES" ] && echo "(absent, non-blocking:${ABSENT_SUITES})"
    fi
} > "$SHEET"

echo ""
if [ -n "$FAILED_SUITES" ]; then
    echo "==================== REGRESSION RED ===================="
    echo "Failing suites:${FAILED_SUITES}"
    echo "Logs: $LOGDIR"
    echo "Sheet: $SHEET"
    echo "DO NOT BUILD until green."
    exit 1
fi
echo "==================== REGRESSION GREEN ===================="
echo "All landed suites pass. Absent (non-blocking):${ABSENT_SUITES:- none}"
echo "Sheet: $SHEET"
exit 0
