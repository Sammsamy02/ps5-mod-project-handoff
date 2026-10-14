#!/bin/zsh
# cap11 host tests (Mac-only). At most 2 compiles in parallel; no PRX is built.
# Usage: tests/run.sh   (writes tests/output/*.log, COVERAGE-HOST.json, SUMMARY.txt)
# Every suite must exit 0 AND print a non-zero count (checks= / items_checked=);
# a missing / crashed / zero-count suite, or fewer suites than expected, fails the run.
set -u
HERE=${0:A:h}
CAP=${HERE:h}
TEAM=${CAP:h}
C9=$TEAM/cap09
ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
IM=$ROOT/.superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui
LOADER_SRC=$ROOT/.superpowers/native-dependency-spike/native-coherent-cpp-loader-1/build-1/source
INV=$C9/tests/output/reverify/ACTUAL-SURFACE.rerun.json
R11_IMPORTS=$HERE/r11_import_names.txt
OUT=$HERE/output
BUILD=${TMPDIR:-/tmp}/cap11-build-$$
EXPECTED_SUITES=11
mkdir -p $OUT $BUILD
CXX=/usr/bin/clang++
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)
STD=(-std=c++20 -O1 -g)
CFG=(-DIMGUI_USER_CONFIG=\"Platform/mbm_imconfig.h\" -I$CAP/PATCH/new/src -I$CAP/PATCH/new/include -I$C9/PATCH/new/include -I$IM)
fail=0; suites=0; failed_suites=()
: > $OUT/SUMMARY.txt
note() { print -r -- "$*" | tee -a $OUT/SUMMARY.txt }
run_suite() { # NAME LOG CMD...: requires exit 0 and a non-zero count
  local name=$1 log=$2; shift 2
  "$@" > $log 2>&1
  local rc=$?
  local n=$(grep -Eo '(checks|items_checked)=[0-9]+' $log | tail -1 | cut -d= -f2)
  suites=$((suites+1))
  if [[ $rc -ne 0 || -z "$n" || "$n" -eq 0 ]]; then fail=1; failed_suites+=($name); fi
  note "$name: exit=$rc count=${n:-MISSING} ($(tail -1 $log))"
}
build_fail() { note "BUILD FAILED: $1"; tail -40 $2; fail=1; failed_suites+=("build:$1") }

[[ -f $INV ]] || { note "missing $INV (run cap09/tests/run.sh and the reverify step first)"; exit 2 }

# pinned Dear ImGui sources
python3 -I - $IM > $OUT/imgui_pins.log <<'EOF'
import hashlib,sys
from pathlib import Path
d=Path(sys.argv[1]); n=0
for f in ["imgui.h","imgui_internal.h","imgui.cpp","imgui_draw.cpp","imgui_widgets.cpp","imgui_tables.cpp","imstb_rectpack.h","imstb_textedit.h","imstb_truetype.h","imconfig.h"]:
    print(hashlib.sha256((d/f).read_bytes()).hexdigest(), f); n+=1
print(f"imgui_pins items_checked={n}")
EOF

build_imgui() { # tag extra-flags...  (ONE unity TU, exactly as the runtime unit and the SDK compile it)
  local tag=$1; shift
  $CXX $STD $SAN $CFG -I$CAP/PATCH/new/src "$@" -c $CAP/PATCH/sdk/src/mbm_sdk_imgui.cpp -o $BUILD/${tag}_imgui_unity.o >> $OUT/build.log 2>&1
}
: > $OUT/build.log
imgui_ok=1
build_imgui host & p1=$!
build_imgui mod -fPIC -fvisibility=hidden & p2=$!
wait $p1 || imgui_ok=0; wait $p2 || imgui_ok=0
build_imgui modbad -fPIC -fvisibility=hidden '-DImDrawIdx=unsigned int' || imgui_ok=0
# [F7] a mod built with ANOTHER ImGui config (no IMGUI_DISABLE_DEBUG_TOOLS): same six sizes, different ImGuiContext
CFG_BADCTX=(-DIMGUI_USER_CONFIG=\"badctx_imconfig.h\" -I$HERE/mods/badctx -I$CAP/PATCH/new/src -I$CAP/PATCH/new/include -I$C9/PATCH/new/include -I$IM)
$CXX $STD $SAN $CFG_BADCTX -fPIC -fvisibility=hidden -c $CAP/PATCH/sdk/src/mbm_sdk_imgui.cpp -o $BUILD/badctx_imgui_unity.o >> $OUT/build.log 2>&1 || imgui_ok=0
# import-delta object (Mac estimate): optimised, ELF-like math errno semantics, no sanitizers
$CXX -std=c++20 -O2 -fmath-errno $CFG -I$CAP/PATCH/new/src -c $CAP/PATCH/sdk/src/mbm_sdk_imgui.cpp -o $BUILD/delta_imgui_unity.o >> $OUT/build.log 2>&1 || imgui_ok=0
[[ $imgui_ok -eq 1 ]] || build_fail imgui $OUT/build.log

# math hooks accuracy
if $CXX -std=c++20 -O1 -I$CAP/PATCH/new/include $HERE/test_math.cpp -o $BUILD/t_math >> $OUT/build.log 2>&1; then
  run_suite test_math $OUT/test_math.log $BUILD/t_math
else build_fail t_math $OUT/build.log; fi

# 0. static: lint, API usage of the GUI binding, engine names in the game's own executable
run_suite lint_patch $OUT/lint_patch.log python3 -I $HERE/lint_patch.py $CAP
run_suite api_usage_gui $OUT/api_usage_gui.log python3 -I $HERE/api_usage_gui.py $CAP/PATCH/candidate/src/Mod/NativeGuiEngineBackend.inc \
  $ROOT/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port $ROOT/outputs/2026-10-08-l26-runtime/typescan-r4/candidate
run_suite engine_names_check $OUT/engine_names_check.log python3 -I $HERE/engine_names_check.py $CAP/PATCH/GUI-ENGINE-NAMES.json \
  $ROOT/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin

# 1. GUI host + batcher + atlas + PNG (in-process), incl. F1 (first-open input block) and F2 (map-change rebuild)
if [[ $imgui_ok -eq 1 ]] && $CXX $STD $SAN $CFG -Wall -Wextra -Wno-unused-parameter $HERE/test_gui_host.cpp $BUILD/host_*.o -o $BUILD/t_gui >> $OUT/build.log 2>&1; then
  run_gui() { (cd $BUILD && ./t_gui) }
  run_suite test_gui_host $OUT/test_gui_host.log run_gui
  run_suite check_png $OUT/check_png.log python3 -I $HERE/check_png.py $BUILD/atlas.png $BUILD/atlas.alpha
else build_fail t_gui $OUT/build.log; fi

# 2. SDK mods with their own ImGui copy (dylibs)
SDK=$C9/PATCH/sdk
MOD=($STD $SAN -fPIC -shared -fdeclspec -fvisibility=hidden -I$SDK/include $CFG)
xok=1
$CXX $MOD -DMBM_SDK_WITH_IMGUI $CAP/PATCH/sdk/examples/NativeGuiProbe/main.cpp $SDK/src/mbm_sdk_entry.cpp $BUILD/mod_*.o -o $BUILD/NativeGuiProbe.dylib >> $OUT/build.log 2>&1 & p1=$!
$CXX $MOD -DMBM_SDK_WITH_IMGUI '-DImDrawIdx=unsigned int' $CAP/PATCH/sdk/examples/NativeGuiProbe/main.cpp $SDK/src/mbm_sdk_entry.cpp $BUILD/modbad_*.o -o $BUILD/NativeGuiProbe_badlayout.dylib >> $OUT/build.log 2>&1 & p2=$!
wait $p1 || xok=0; wait $p2 || xok=0
$CXX $STD $SAN -fPIC -shared -fdeclspec -fvisibility=hidden -I$SDK/include $CFG_BADCTX -DMBM_SDK_WITH_IMGUI $CAP/PATCH/sdk/examples/NativeGuiProbe/main.cpp $SDK/src/mbm_sdk_entry.cpp $BUILD/badctx_imgui_unity.o -o $BUILD/NativeGuiProbe_badctx.dylib >> $OUT/build.log 2>&1 || xok=0
$CXX $STD $SAN -fPIC -shared -fvisibility=hidden -I$SDK/include $HERE/mods/noimgui/main.cpp $SDK/src/mbm_sdk_entry.cpp -o $BUILD/NoImGui.dylib >> $OUT/build.log 2>&1 & p1=$!
$CXX $STD $SAN $CFG -I$LOADER_SRC -Wall -Wno-unused-parameter $HERE/test_gui_sdk_mods.cpp $LOADER_SRC/CppModModule.cpp $BUILD/host_*.o -o $BUILD/t_gui_sdk >> $OUT/build.log 2>&1 & p2=$!
wait $p1 || xok=0; wait $p2 || xok=0
if [[ $xok -eq 1 && -x $BUILD/t_gui_sdk && -f $BUILD/NativeGuiProbe.dylib ]]; then
  run_suite test_gui_sdk_mods $OUT/test_gui_sdk_mods.log $BUILD/t_gui_sdk $BUILD
else build_fail cross-module $OUT/build.log; fi

# 3. coverage gate v2 (host level) and the Mac import estimate of the ImGui block
run_suite coverage_gate_v2 $OUT/coverage_gate_v2.log python3 -I $HERE/coverage_gate_v2.py $INV $CAP/PATCH/sdk/examples/NativeGuiProbe/main.cpp $OUT/test_gui_sdk_mods.log $OUT/COVERAGE-HOST.json
run_suite import_delta $OUT/import_delta.log python3 -I $HERE/import_delta.py $R11_IMPORTS $BUILD/delta_imgui_unity.o

# 4. [B2/blocker 1] TARGET compile of the whole #11 composition (cap09 + cap11 cppmod, program, luamod)
#    with R11's own commands, and the real import delta vs R11's runtime-l26-A.o.
#    The ImGui .cpp files are copied (sha-checked) into a folder WITHOUT imgui.h,
#    exactly like the integrator's copies under typescan-r4/include/.
mkdir -p $BUILD/imcopy
python3 -I - $CAP/PATCH/CHANGES-ROWS.json $BUILD/imcopy > $OUT/imcopy.log 2>&1 <<'EOF'
import hashlib, json, shutil, sys
from pathlib import Path
rows = json.loads(Path(sys.argv[1]).read_text()); dst = Path(sys.argv[2]); n = bad = 0
for r in rows["added_files"]:
    src = Path(r["from"])
    if not src.is_absolute():
        continue
    n += 1
    ok = hashlib.sha256(src.read_bytes()).hexdigest() == r["sha256"]
    bad += not ok
    shutil.copyfile(src, dst / src.name)
    print(("pass " if ok else "FAIL ") + src.name)
print(f"imcopy items_checked={n} failed={bad}")
sys.exit(2 if n == 0 else (1 if bad else 0))
EOF
[[ $? -ne 0 ]] && build_fail imcopy $OUT/imcopy.log
python3 -I $C9/PATCH/apply_anchored.py --stage --base-dir $C9/PATCH/bases --out-dir $BUILD/r11var > $OUT/r11var.log 2>&1 || build_fail r11var $OUT/r11var.log
GUI_INC=(--include $CAP/PATCH/candidate/src/Mod --include $C9/PATCH/candidate/src/Mod --include $C9/PATCH/candidate/include --include $CAP/PATCH/new/include --include $C9/PATCH/new/include --include $BUILD/imcopy)
# strict vs R11: hunks on the R11-era bases
run_suite target_compile_r11 $OUT/target_compile_check.log python3 -I $C9/tests/target_compile_check.py \
  --unit cppmod=$CAP/PATCH/candidate/src/Mod/CppMod.cpp --unit program=$BUILD/r11var/UE4SSProgram.cpp \
  --unit luamod=$BUILD/r11var/LuaMod.cpp $GUI_INC --out $BUILD/target
# live candidate tree: must compile; only what the staged hunks add counts
LIVE=$ROOT/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src
run_suite target_compile_live $OUT/target_compile_live.log python3 -I $C9/tests/target_compile_check.py \
  --baseline-unit program=$LIVE/UE4SSProgram.cpp --baseline-unit luamod=$LIVE/LuaMod.cpp \
  --unit cppmod=$CAP/PATCH/candidate/src/Mod/CppMod.cpp --unit program=$C9/PATCH/candidate/src/UE4SSProgram.cpp \
  --unit luamod=$C9/PATCH/candidate/src/LuaMod.cpp $GUI_INC --out $BUILD/target-live

total=$(grep -h "checks=" $OUT/test_gui_host.log $OUT/test_gui_sdk_mods.log 2>/dev/null | sed 's/.*checks=\([0-9]*\).*/\1/' | paste -sd+ - | bc)
items=$(grep -h "items_checked=" $OUT/check_png.log $OUT/coverage_gate_v2.log $OUT/imgui_pins.log $OUT/import_delta.log $OUT/test_math.log $OUT/api_usage_gui.log $OUT/engine_names_check.log $OUT/lint_patch.log $OUT/target_compile_check.log $OUT/target_compile_live.log 2>/dev/null | sed 's/.*items_checked=\([0-9]*\).*/\1/' | paste -sd+ - | bc)
[[ $suites -ne $EXPECTED_SUITES ]] && { fail=1; failed_suites+=("suite-count:$suites/$EXPECTED_SUITES") }
[[ -z "$total" || "$total" -eq 0 || -z "$items" || "$items" -eq 0 ]] && fail=1
note "TOTAL suites=$suites/$EXPECTED_SUITES compiled_checks=${total:-0} script_items=${items:-0} result=$([[ $fail -eq 0 ]] && echo PASS || echo "FAIL (${failed_suites[*]})")"
rm -rf $BUILD
exit $fail
