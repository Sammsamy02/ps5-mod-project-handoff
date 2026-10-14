#!/bin/zsh
# run_tests.sh — worker-lease-coverage-fix fixture runner.
# Compiles the SAME test TU against the PRODUCTION r13 (OLD) and r14 (NEW)
# bridge sources plus the REAL PLH OwnedMemory service, then runs every
# scenario in its own process (the owned-memory service registry is
# process-lifetime). Exit 0 only when every scenario passes in both builds,
# with the launch-8 throw reproduced in OLD and defused in NEW.
set -u
ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
MYDIR="$ROOT/outputs/2026-10-04-zcode-team/worker-lease-coverage-fix/fixtures"
OLD_TU="$ROOT/outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r13.cpp"
NEW_TU="$MYDIR/../runtime_start_bridge_r14.cpp"
OM_CPP="$ROOT/.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/src/OwnedMemory.cpp"
OVERLAY="$ROOT/outputs/2026-10-04-zcode-team/worker-bridge-r13-polish/staging/lineage-overlay-2-r13"
POLYHOOK_VANILLA="$ROOT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/polyhook"
POLYHOOK_PATCHED="$ROOT/.superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/patched/include"
READLEASE_INC="$ROOT/.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/include"
PORT="$ROOT/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port"
UE4SS_INC="$ROOT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include"

CXX=clang++
CXXFLAGS="-std=c++20 -Wall -Wextra -fdeclspec -g -O0"
INC="-I$MYDIR -I$OVERLAY -I$READLEASE_INC -I$POLYHOOK_PATCHED -I$POLYHOOK_VANILLA -I$PORT/ue4ss/include -I$UE4SS_INC -I$PORT/first/include -I$PORT/unreal/include -I$PORT/unreal/include/Unreal -I$PORT/unreal/generated_include -I$PORT/fmt/include"

cd "$MYDIR" || exit 1
FAILED=0

for BUILD in old new; do
  if [ "$BUILD" = old ]; then
    DEF="-DLEASE_TEST_OLD -DLEASE_TEST_BRIDGE_TU_OLD=\"\\\"$OLD_TU\\\"\" -DLEASE_TEST_BRIDGE_TU_NEW=\"\\\"$NEW_TU\\\"\""
  else
    DEF="-DLEASE_TEST_BRIDGE_TU_OLD=\"\\\"$OLD_TU\\\"\" -DLEASE_TEST_BRIDGE_TU_NEW=\"\\\"$NEW_TU\\\"\""
  fi
  echo "=== building $BUILD (production TU: $(basename ${OLD_TU:-x}):$(basename $NEW_TU)) ==="
  # shellcheck disable=SC2086
  eval "$CXX $CXXFLAGS $INC $DEF -c test_lease_coverage.cpp -o test_$BUILD.o" || { echo "BUILD FAIL $BUILD"; exit 1; }
  # shellcheck disable=SC2086
  eval "$CXX $CXXFLAGS $INC $DEF -DPLH_OWNED_MEMORY_HOST_TEST -c '$OM_CPP' -o OwnedMemory_$BUILD.o" || { echo "BUILD FAIL svc $BUILD"; exit 1; }
  eval "$CXX test_$BUILD.o OwnedMemory_$BUILD.o -o test_$BUILD -undefined dynamic_lookup" || { echo "LINK FAIL $BUILD"; exit 1; }
done

SCENARIOS_OLD="happy drift exectext cap capgate gap"
SCENARIOS_NEW="happy drift exectext cap capgate gap seedgate idempotent"

run_one() {  # build scenario
  local b="$1" s="$2"
  echo "--- run [$b] $s"
  "./test_$b" "$s" || { echo "SCENARIO FAIL [$b] $s"; FAILED=1; }
}

for s in ${=SCENARIOS_OLD}; do run_one old "$s"; done
for s in ${=SCENARIOS_NEW}; do run_one new "$s"; done

if [ "$FAILED" = 0 ]; then
  echo "ALL SCENARIOS PASS (old failure reproduced, new coverage proven)"
fi
exit $FAILED
