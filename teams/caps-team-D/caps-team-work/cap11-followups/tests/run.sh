#!/bin/zsh
# cap11 follow-ups: host suites (Mac-only; no PRX is built, nothing touches the console or the live tree).
# Usage: tests/run.sh live|patched
#   live    = Team D's cap09 + cap11 host suites compiled against the LIVE typescan-r4 headers / .inc files
#             (FOLLOWUPS item 4: the suites used to compile Team D's own, stale copies)
#   patched = the same suites + test_followups.cpp with PATCH/post/include FIRST, then the live headers
# Every suite must exit 0 AND print a non-zero count (checks= / items_checked=); a missing / crashed / zero-count
# suite, or fewer suites than expected, fails the run. At most 2 compiles in parallel; stops (exit 3) if less
# than 4 GB of memory is available (the integrator's build has priority).
set -u
MODE=${1:-}
[[ $MODE == live || $MODE == patched ]] || { print "usage: run.sh live|patched"; exit 2 }
HERE=${0:A:h}
FU=${HERE:h}
TEAM=/WORKSPACE/.codex/worktrees/translation-core/caps-team-D/caps-team-work/cap09-cap11
C9=$TEAM/cap09
C11=$TEAM/cap11
ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
LIVE=$ROOT/outputs/2026-10-08-l26-runtime/typescan-r4
IM=$ROOT/.superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui
LOADER_SRC=$ROOT/.superpowers/native-dependency-spike/native-coherent-cpp-loader-1/build-1/source
PORT=$ROOT/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port
PORT_INC=$PORT/ue4ss/include
PORT_KEYDEF=$PORT/first/include/Input/KeyDef.hpp
EBOOT=$ROOT/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin
INV=$C9/tests/output/reverify/ACTUAL-SURFACE.rerun.json
OUT=$HERE/output/$MODE
BUILD=${TMPDIR:-/tmp}/cap11-fu-$MODE-$$
mkdir -p $OUT $BUILD
: > $OUT/SUMMARY.txt
note() { print -r -- "$*" | tee -a $OUT/SUMMARY.txt }

avail_gb() { vm_stat | awk '/page size of/ {ps=$8} /Pages free/ {f=$3} /Pages inactive/ {i=$3} /Pages speculative/ {s=$3} /Pages purgeable/ {p=$3} END {printf "%d", (f+i+s+p)*ps/1073741824}' }
# pause (poll every 15 s, up to 10 min) while less than 4 GB is available or an integrator compile runs; then give up (exit 3)
mem_gate() {
  local g tries=0
  while true; do
    g=$(avail_gb)
    if (( g >= 4 )) && ! ps -axo command | grep -v grep | grep -q "prospero-clang"; then return 0; fi
    (( tries == 0 )) && note "pausing: ${g} GB available or an integrator compile is running"
    tries=$((tries+1))
    if (( tries > 40 )); then note "PAUSED: only ${g} GB available (< 4 GB) or integrator compile running; giving up"; rm -rf $BUILD; exit 3; fi
    python3 -I -c "import time; time.sleep(15)"
  done
}
mem_gate

if [[ $MODE == patched ]]; then OVER=(-I$FU/PATCH/post/include); EXPECTED_SUITES=14; else OVER=(); EXPECTED_SUITES=13; fi
# include order: [patch overlay] -> LIVE headers -> LIVE .inc dir (NativeGuiImGuiUnity.inc) -> pinned ImGui
LIVEINC=($OVER -I$LIVE/include)
CXX=/usr/bin/clang++
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)
STD=(-std=c++20 -O1 -g)
CFG=(-DIMGUI_USER_CONFIG=\"Platform/mbm_imconfig.h\" $LIVEINC -I$LIVE/candidate/src/Mod -I$IM)
fail=0; suites=0; failed_suites=()
run_suite() { # NAME LOG CMD...
  local name=$1 log=$2; shift 2
  "$@" > $log 2>&1
  local rc=$?
  local n=$(grep -Eo '(checks|items_checked)=[0-9]+' $log | tail -1 | cut -d= -f2)
  suites=$((suites+1))
  if [[ $rc -ne 0 || -z "$n" || "$n" -eq 0 ]]; then fail=1; failed_suites+=($name); fi
  note "$name: exit=$rc count=${n:-MISSING} ($(tail -1 $log))"
}
build_fail() { note "BUILD FAILED: $1 (see $2)"; tail -30 $2; fail=1; failed_suites+=("build:$1") }

note "mode=$MODE include order: ${LIVEINC[*]} -I$LIVE/candidate/src/Mod -I$IM"
python3 -I - $MODE $FU $LIVE > $OUT/inputs.log <<'EOF'
import hashlib, sys
from pathlib import Path
mode, fu, live = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
n = 0
for rel in ["include/Platform/NativeGuiHost.hpp", "include/Platform/NativeModHostCore.hpp", "include/Platform/ImGuiCanvasBatcher.hpp",
            "include/Platform/ImGuiAtlasRuns.hpp", "include/Platform/mbm_imconfig.h", "include/Mod/NativeModAbi.h",
            "candidate/src/Mod/NativeGuiEngineBackend.inc", "candidate/src/Mod/NativeModEngineServices.inc", "candidate/src/Mod/NativeGuiImGuiUnity.inc"]:
    p = fu / "PATCH/post" / rel
    src = p if mode == "patched" and p.exists() else live / rel
    print(hashlib.sha256(src.read_bytes()).hexdigest(), "patch" if src == p else "LIVE ", rel); n += 1
print(f"inputs items_checked={n}")
EOF
cat $OUT/inputs.log >> $OUT/SUMMARY.txt

# ---------------- ImGui unity objects (host + mod flavours), as the runtime unit / SDK compile them ----------------
: > $OUT/build.log
imgui_ok=1
$CXX $STD $SAN $CFG -c $C11/PATCH/sdk/src/mbm_sdk_imgui.cpp -o $BUILD/host_imgui_unity.o >> $OUT/build.log 2>&1 & p1=$!
$CXX $STD $SAN $CFG -fPIC -fvisibility=hidden -c $C11/PATCH/sdk/src/mbm_sdk_imgui.cpp -o $BUILD/mod_imgui_unity.o >> $OUT/build.log 2>&1 & p2=$!
wait $p1 || imgui_ok=0; wait $p2 || imgui_ok=0
mem_gate
$CXX $STD $SAN $CFG -fPIC -fvisibility=hidden '-DImDrawIdx=unsigned int' -c $C11/PATCH/sdk/src/mbm_sdk_imgui.cpp -o $BUILD/modbad_imgui_unity.o >> $OUT/build.log 2>&1 & p1=$!
CFG_BADCTX=(-DIMGUI_USER_CONFIG=\"badctx_imconfig.h\" -I$C11/tests/mods/badctx $LIVEINC -I$LIVE/candidate/src/Mod -I$IM)
$CXX $STD $SAN $CFG_BADCTX -fPIC -fvisibility=hidden -c $C11/PATCH/sdk/src/mbm_sdk_imgui.cpp -o $BUILD/badctx_imgui_unity.o >> $OUT/build.log 2>&1 & p2=$!
wait $p1 || imgui_ok=0; wait $p2 || imgui_ok=0
[[ $imgui_ok -eq 1 ]] || build_fail imgui $OUT/build.log

# ---------------- cap09 suites ----------------
mem_gate
SDK=$C9/PATCH/sdk
if $CXX $STD $SAN -Wall -Wextra -Wno-unused-parameter $LIVEINC $C9/tests/test_host_core.cpp -o $BUILD/t_core > $OUT/build_core.log 2>&1; then
  run_suite cap09_test_host_core $OUT/test_host_core.log $BUILD/t_core
else build_fail t_core $OUT/build_core.log; fi
MODFLAGS=($STD $SAN -fPIC -shared -fdeclspec -fvisibility=hidden -I$SDK/include)
build_mod() { local name=$1; shift; $CXX $MODFLAGS "$@" $SDK/src/mbm_sdk_entry.cpp -o $BUILD/$name.dylib >> $OUT/build_mods.log 2>&1 || { print "build $name FAILED" >> $OUT/build_mods.log; return 1 } }
: > $OUT/build_mods.log
mods_ok=1
build_mod official_example $C9/tests/mods/official_example/dllmain.cpp & p1=$!
build_mod NativeLifecycleProbe $SDK/examples/NativeLifecycleProbe/main.cpp & p2=$!
wait $p1 || mods_ok=0; wait $p2 || mods_ok=0
build_mod throwing $C9/tests/mods/throwing/main.cpp & p1=$!
$CXX $STD $SAN -fPIC -shared $C9/tests/mods/legacy/main.cpp -o $BUILD/legacy.dylib >> $OUT/build_mods.log 2>&1 & p2=$!
wait $p1 || mods_ok=0; wait $p2 || mods_ok=0
if [[ $mods_ok -eq 1 ]]; then
  mem_gate
  $CXX $STD $SAN $LIVEINC -I$LOADER_SRC $C9/tests/test_sdk_mods.cpp $LOADER_SRC/CppModModule.cpp -o $BUILD/t_sdk > $OUT/build_sdk.log 2>&1 & p1=$!
  # the LIVE NativeCppModPath.hpp (candidate/include) shadows the port copy, as the build's overlay does
  $CXX $STD $SAN -I$LIVE/candidate/include $LIVEINC -I$LOADER_SRC -I$PORT_INC $C9/tests/test_startup_path.cpp $LOADER_SRC/CppModModule.cpp -o $BUILD/t_startup > $OUT/build_startup.log 2>&1 & p2=$!
  wait $p1; r1=$?; wait $p2; r2=$?
  if [[ $r1 -eq 0 ]]; then run_suite cap09_test_sdk_mods $OUT/test_sdk_mods.log $BUILD/t_sdk $BUILD; else build_fail t_sdk $OUT/build_sdk.log; fi
  if [[ $r2 -eq 0 ]]; then run_suite cap09_test_startup_path $OUT/test_startup_path.log $BUILD/t_startup $BUILD; else build_fail t_startup $OUT/build_startup.log; fi
else build_fail cap09-mods $OUT/build_mods.log; fi

# ---------------- cap11 suites ----------------
mem_gate
if $CXX -std=c++20 -O1 $LIVEINC $C11/tests/test_math.cpp -o $BUILD/t_math >> $OUT/build.log 2>&1; then
  run_suite cap11_test_math $OUT/test_math.log $BUILD/t_math
else build_fail t_math $OUT/build.log; fi
if [[ $imgui_ok -eq 1 ]] && $CXX $STD $SAN $CFG -Wall -Wextra -Wno-unused-parameter $C11/tests/test_gui_host.cpp $BUILD/host_imgui_unity.o -o $BUILD/t_gui >> $OUT/build.log 2>&1; then
  run_gui() { (cd $BUILD && ./t_gui) }
  run_suite cap11_test_gui_host $OUT/test_gui_host.log run_gui
  run_suite cap11_check_png $OUT/check_png.log python3 -I $C11/tests/check_png.py $BUILD/atlas.png $BUILD/atlas.alpha
else build_fail t_gui $OUT/build.log; fi
MOD=($STD $SAN -fPIC -shared -fdeclspec -fvisibility=hidden -I$SDK/include $CFG)
xok=1
mem_gate
$CXX $MOD -DMBM_SDK_WITH_IMGUI $C11/PATCH/sdk/examples/NativeGuiProbe/main.cpp $SDK/src/mbm_sdk_entry.cpp $BUILD/mod_imgui_unity.o -o $BUILD/NativeGuiProbe.dylib >> $OUT/build.log 2>&1 & p1=$!
$CXX $MOD -DMBM_SDK_WITH_IMGUI '-DImDrawIdx=unsigned int' $C11/PATCH/sdk/examples/NativeGuiProbe/main.cpp $SDK/src/mbm_sdk_entry.cpp $BUILD/modbad_imgui_unity.o -o $BUILD/NativeGuiProbe_badlayout.dylib >> $OUT/build.log 2>&1 & p2=$!
wait $p1 || xok=0; wait $p2 || xok=0
$CXX $STD $SAN -fPIC -shared -fdeclspec -fvisibility=hidden -I$SDK/include $CFG_BADCTX -DMBM_SDK_WITH_IMGUI $C11/PATCH/sdk/examples/NativeGuiProbe/main.cpp $SDK/src/mbm_sdk_entry.cpp $BUILD/badctx_imgui_unity.o -o $BUILD/NativeGuiProbe_badctx.dylib >> $OUT/build.log 2>&1 & p1=$!
$CXX $STD $SAN -fPIC -shared -fvisibility=hidden -I$SDK/include $C11/tests/mods/noimgui/main.cpp $SDK/src/mbm_sdk_entry.cpp -o $BUILD/NoImGui.dylib >> $OUT/build.log 2>&1 & p2=$!
wait $p1 || xok=0; wait $p2 || xok=0
mem_gate
$CXX $STD $SAN $CFG -I$LOADER_SRC -Wall -Wno-unused-parameter $C11/tests/test_gui_sdk_mods.cpp $LOADER_SRC/CppModModule.cpp $BUILD/host_imgui_unity.o -o $BUILD/t_gui_sdk >> $OUT/build.log 2>&1 || xok=0
if [[ $xok -eq 1 && -x $BUILD/t_gui_sdk && -f $BUILD/NativeGuiProbe.dylib ]]; then
  run_suite cap11_test_gui_sdk_mods $OUT/test_gui_sdk_mods.log $BUILD/t_gui_sdk $BUILD
else build_fail cap11-cross-module $OUT/build.log; fi
if [[ -f $INV ]]; then
  run_suite cap11_coverage_gate_v2 $OUT/coverage_gate_v2.log python3 -I $C11/tests/coverage_gate_v2.py $INV $C11/PATCH/sdk/examples/NativeGuiProbe/main.cpp $OUT/test_gui_sdk_mods.log $OUT/COVERAGE-HOST.json
else note "missing $INV"; fail=1; failed_suites+=(coverage-inventory); fi

# ---------------- follow-up host test (patched only) ----------------
if [[ $MODE == patched ]]; then
  mem_gate
  if $CXX $STD $SAN $CFG -I$C9/tests -I$C11/tests -Wall -Wextra -Wno-unused-parameter $HERE/test_followups.cpp $BUILD/host_imgui_unity.o -o $BUILD/t_fu >> $OUT/build.log 2>&1; then
    run_suite test_followups $OUT/test_followups.log $BUILD/t_fu
  else build_fail t_fu $OUT/build.log; fi
fi

# ---------------- include proof: every project header the suites compile resolves to the LIVE tree (or the patch) ----------------
: > $OUT/include_proof.deps
for src in $C9/tests/test_host_core.cpp $C9/tests/test_sdk_mods.cpp $C11/tests/test_gui_host.cpp $C11/tests/test_gui_sdk_mods.cpp $C11/PATCH/sdk/src/mbm_sdk_imgui.cpp; do
  $CXX $STD $CFG -I$LOADER_SRC -MM $src >> $OUT/include_proof.deps 2>> $OUT/build.log
done
$CXX $STD -I$LIVE/candidate/include $LIVEINC -I$LOADER_SRC -I$PORT_INC -MM $C9/tests/test_startup_path.cpp >> $OUT/include_proof.deps 2>> $OUT/build.log
run_suite include_proof $OUT/include_proof.log python3 -I - $OUT/include_proof.deps $TEAM $LIVE $FU/PATCH/post $MODE <<'PYEOF'
import sys
from pathlib import Path
deps, team, live, post, mode = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5]
toks = {t.strip() for t in Path(deps).read_text().replace("\\\n", " ").split() if t.endswith((".hpp", ".h", ".inc"))}
proj = sorted(t for t in toks if "/Platform/" in t or "/Mod/" in t or t.endswith(".inc"))
stale = [t for t in proj if t.startswith(team) and ("/PATCH/new/" in t or "/PATCH/candidate/" in t)]
live_n = sum(t.startswith(live) for t in proj); post_n = sum(t.startswith(post) for t in proj)
for t in proj: print(("LIVE  " if t.startswith(live) else "PATCH " if t.startswith(post) else "other ") + t)
for t in stale: print("FAIL stale Team D copy:", t)
ok = not stale and live_n > 0 and (mode == "live" and post_n == 0 or mode == "patched" and post_n >= 2)
print(f"include_proof items_checked={len(proj)} live={live_n} patch={post_n} stale={len(stale)}")
sys.exit(0 if ok else 1)
PYEOF

# ---------------- static checks of the engine bindings (the .inc files cannot be host-compiled) ----------------
if [[ $MODE == patched ]]; then BE=$FU/PATCH/post/candidate/src/Mod/NativeGuiEngineBackend.inc; SV=$FU/PATCH/post/candidate/src/Mod/NativeModEngineServices.inc
else BE=$LIVE/candidate/src/Mod/NativeGuiEngineBackend.inc; SV=$LIVE/candidate/src/Mod/NativeModEngineServices.inc; fi
run_suite api_usage_services $OUT/api_usage_services.log python3 -I $C9/tests/api_usage_check.py $SV $PORT $LIVE/candidate
run_suite api_usage_gui $OUT/api_usage_gui.log python3 -I $C11/tests/api_usage_gui.py $BE $PORT $LIVE/candidate
run_suite engine_names_check $OUT/engine_names_check.log python3 -I $C11/tests/engine_names_check.py $HERE/GUI-ENGINE-NAMES.fu.json $EBOOT
run_suite backend_structure $OUT/backend_structure.log python3 -I $HERE/check_backend_structure.py $MODE $BE $SV

total=$(grep -h "checks=" $OUT/*.log 2>/dev/null | grep -v items_checked | sed 's/.*checks=\([0-9]*\).*/\1/' | paste -sd+ - | bc)
[[ $suites -ne $EXPECTED_SUITES ]] && { fail=1; failed_suites+=("suite-count:$suites/$EXPECTED_SUITES") }
[[ -z "$total" || "$total" -eq 0 ]] && fail=1
note "TOTAL mode=$MODE suites=$suites/$EXPECTED_SUITES compiled_checks=${total:-0} result=$([[ $fail -eq 0 ]] && echo PASS || echo "FAIL (${failed_suites[*]})")"
rm -rf $BUILD
exit $fail
