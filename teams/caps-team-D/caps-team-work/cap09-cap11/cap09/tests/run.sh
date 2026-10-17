#!/bin/zsh
# cap09 host tests (Mac-only). At most 2 compiles in parallel; no PRX is built.
# Usage: tests/run.sh   (writes tests/output/*.log and tests/output/SUMMARY.txt)
# Every suite must exit 0 AND print a non-zero count (checks= / items_checked=);
# a suite that is missing, crashes, prints no count or counts 0 fails the run,
# and so does a run with fewer suites than expected.
set -u
HERE=${0:A:h}
CAP=${HERE:h}
ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
LOADER_SRC=$ROOT/.superpowers/native-dependency-spike/native-coherent-cpp-loader-1/build-1/source
PORT_INC=$ROOT/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include
PORT_KEYDEF=$ROOT/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Input/KeyDef.hpp
OUT=$HERE/output
BUILD=${TMPDIR:-/tmp}/cap09-build-$$
EXPECTED_SUITES=8
mkdir -p $OUT $BUILD
CXX=/usr/bin/clang++
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"
STD="-std=c++20 -O1 -g -Wall -Wextra -Wno-unused-parameter"
SDK=$CAP/PATCH/sdk
fail=0
suites=0
failed_suites=()
: > $OUT/SUMMARY.txt

note() { print -r -- "$*" | tee -a $OUT/SUMMARY.txt }

# run_suite NAME LOG CMD...: runs CMD, requires exit 0 and a non-zero count.
run_suite() {
  local name=$1 log=$2; shift 2
  "$@" > $log 2>&1
  local rc=$?
  local n=$(grep -Eo '(checks|items_checked)=[0-9]+' $log | tail -1 | cut -d= -f2)
  suites=$((suites+1))
  if [[ $rc -ne 0 || -z "$n" || "$n" -eq 0 ]]; then fail=1; failed_suites+=($name); fi
  note "$name: exit=$rc count=${n:-MISSING} ($(tail -1 $log))"
}
build_fail() { note "BUILD FAILED: $1"; tail -40 $2; fail=1; failed_suites+=("build:$1") }

# 1. pinned-source checks + lints (python, no compile)
run_suite check_sources $OUT/check_sources.log python3 -I $HERE/check_sources.py $CAP $LOADER_SRC $PORT_KEYDEF

# 2. anchored hunks: exact-once anchors on the LIVE candidate tree (read-only),
#    staged UE4SSProgram.cpp / LuaMod.cpp == regenerated
run_suite apply_anchored $OUT/apply_anchored.log python3 -I $CAP/PATCH/apply_anchored.py --check

# 3. API-usage check of the engine binding against pinned port + candidate trees
run_suite api_usage_check $OUT/api_usage_check.log python3 -I $HERE/api_usage_check.py $CAP/PATCH/candidate/src/Mod/NativeModEngineServices.inc \
  $ROOT/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port \
  $ROOT/outputs/2026-10-08-l26-runtime/typescan-r4/candidate

# 4. host core unit tests (ASan+UBSan)
if $CXX ${=STD} ${=SAN} -I$CAP/PATCH/new/include $HERE/test_host_core.cpp -o $BUILD/t_core > $OUT/build_core.log 2>&1; then
  run_suite test_host_core $OUT/test_host_core.log $BUILD/t_core
else build_fail t_core $OUT/build_core.log; fi

# 5. SDK mods as dylibs (2 parallel compiles at a time)
MODFLAGS=(${=STD} ${=SAN} -fPIC -shared -fdeclspec -fvisibility=hidden -I$SDK/include)
build_mod() { # name src...
  local name=$1; shift
  $CXX $MODFLAGS "$@" $SDK/src/mbm_sdk_entry.cpp -o $BUILD/$name.dylib >> $OUT/build_mods.log 2>&1 || { print "build $name FAILED" >> $OUT/build_mods.log; return 1 }
}
: > $OUT/build_mods.log
mods_ok=1
build_mod official_example $HERE/mods/official_example/dllmain.cpp & p1=$!
build_mod NativeLifecycleProbe $SDK/examples/NativeLifecycleProbe/main.cpp & p2=$!
wait $p1 || mods_ok=0; wait $p2 || mods_ok=0
build_mod throwing $HERE/mods/throwing/main.cpp & p1=$!
$CXX ${=STD} ${=SAN} -fPIC -shared $HERE/mods/legacy/main.cpp -o $BUILD/legacy.dylib >> $OUT/build_mods.log 2>&1 & p2=$!
wait $p1 || mods_ok=0; wait $p2 || mods_ok=0
if [[ $mods_ok -eq 1 ]]; then
  nm -gU $BUILD/NativeLifecycleProbe.dylib | grep -v " _\?_\?Z\|asan\|ubsan" > $OUT/probe_exports.txt
  note "probe exports: $(tr '\n' ' ' < $OUT/probe_exports.txt)"
  # end-to-end: real CppModModule.cpp (POSIX branch) + policy + host core + SDK mods
  if $CXX ${=STD} ${=SAN} -I$CAP/PATCH/new/include -I$LOADER_SRC $HERE/test_sdk_mods.cpp $LOADER_SRC/CppModModule.cpp -o $BUILD/t_sdk > $OUT/build_sdk.log 2>&1; then
    run_suite test_sdk_mods $OUT/test_sdk_mods.log $BUILD/t_sdk $BUILD
  else build_fail t_sdk $OUT/build_sdk.log; fi
  # 6. [B1] activation-snapshot startup path: inventory -> real loader -> start order
  #    [F1] the STAGED NativeCppModPath.hpp (candidate/include) shadows the port copy, as the build's overlay does
  if $CXX ${=STD} ${=SAN} -I$CAP/PATCH/candidate/include -I$CAP/PATCH/new/include -I$LOADER_SRC -I$PORT_INC $HERE/test_startup_path.cpp $LOADER_SRC/CppModModule.cpp -o $BUILD/t_startup > $OUT/build_startup.log 2>&1; then
    run_suite test_startup_path $OUT/test_startup_path.log $BUILD/t_startup $BUILD
  else build_fail t_startup $OUT/build_startup.log; fi
else build_fail mods $OUT/build_mods.log; fi

# 7. [B1/B2] TARGET compile with R11's own commands + import delta vs R11's runtime object, strict:
#    the hunks applied to the R11-era bases (PATCH/bases, sha 121f2383 / 96e8ff4d).
python3 -I $CAP/PATCH/apply_anchored.py --stage --base-dir $CAP/PATCH/bases --out-dir $BUILD/r11var > $OUT/r11var.log 2>&1 || build_fail r11var $OUT/r11var.log
run_suite target_compile_r11 $OUT/target_compile_check.log python3 -I $HERE/target_compile_check.py \
  --unit cppmod=$CAP/PATCH/candidate/src/Mod/CppMod.cpp --unit program=$BUILD/r11var/UE4SSProgram.cpp \
  --unit luamod=$BUILD/r11var/LuaMod.cpp --include $CAP/PATCH/candidate/include --include $CAP/PATCH/new/include --out $BUILD/target

# 8. the same on the LIVE candidate tree (what the integrator will build): must compile; the delta
#    counts only symbols the staged hunks add (live unpatched units compiled as the baseline).
LIVE=$ROOT/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src
run_suite target_compile_live $OUT/target_compile_live.log python3 -I $HERE/target_compile_check.py \
  --baseline-unit program=$LIVE/UE4SSProgram.cpp --baseline-unit luamod=$LIVE/LuaMod.cpp \
  --unit cppmod=$CAP/PATCH/candidate/src/Mod/CppMod.cpp --unit program=$CAP/PATCH/candidate/src/UE4SSProgram.cpp \
  --unit luamod=$CAP/PATCH/candidate/src/LuaMod.cpp --include $CAP/PATCH/candidate/include --include $CAP/PATCH/new/include --out $BUILD/target-live

total=$(grep -h "checks=" $OUT/test_host_core.log $OUT/test_sdk_mods.log $OUT/test_startup_path.log 2>/dev/null | sed 's/.*checks=\([0-9]*\).*/\1/' | paste -sd+ - | bc)
srcchecks=$(grep -h "items_checked=" $OUT/check_sources.log $OUT/apply_anchored.log $OUT/api_usage_check.log $OUT/target_compile_check.log $OUT/target_compile_live.log 2>/dev/null | sed 's/.*items_checked=\([0-9]*\).*/\1/' | paste -sd+ - | bc)
[[ $suites -ne $EXPECTED_SUITES ]] && { fail=1; failed_suites+=("suite-count:$suites/$EXPECTED_SUITES") }
[[ -z "$total" || "$total" -eq 0 || -z "$srcchecks" || "$srcchecks" -eq 0 ]] && fail=1
note "TOTAL suites=$suites/$EXPECTED_SUITES compiled_checks=${total:-0} source_items=${srcchecks:-0} result=$([[ $fail -eq 0 ]] && echo PASS || echo "FAIL (${failed_suites[*]})")"
rm -rf $BUILD
exit $fail
