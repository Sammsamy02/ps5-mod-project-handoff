#!/bin/bash
# worker-l11-loadmap-guard r2 host test build+run. LOCAL ONLY.
# Every polyhook-layer unit compiled from the EXACT composed launch-10-walkguard
# receipt source (sha-verified before compile). asmjit/asmtk archives + zydis/
# zycore objects reused from the pinned host-deps builds (source-identical).
set -e
ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM=$ROOT/outputs/2026-10-04-zcode-team
W=$TEAM/worker-l11-loadmap-guard
SP=$ROOT/.superpowers/native-dependency-spike
D=$SP/ue4ss-unreal-native-build-1/source-dependencies
OBJ=$W/host-test/obj-r2; BIN=$W/host-test/test_detour_refusal_r2
mkdir -p "$OBJ"
FLAGS=(clang++ -arch x86_64 -std=c++23 -O0 -g -fPIC -fsanitize=address,undefined
 -fno-omit-frame-pointer -DASMJIT_STATIC -DASMTK_STATIC -DZYDIS_STATIC_BUILD -DZYCORE_STATIC_BUILD
 -DPLH_OWNED_MEMORY_HOST_TEST
 -I $SP/ue4ss-native-read-lease-1/include
 -I $SP/ue4ss-native-instruction-owned-1/include
 -I $SP/ue4ss-native-owned-detour-1/include
 -I $SP/ue4ss-native-bounded-owned-1/include
 -I $SP/ue4ss-polyhook-platform-port-1/patched/include
 -I $D/polyhook
 -I $D/asmjit/src -I $D/asmtk/src -I $D/zydis/include -I $D/zycore/include)
UNITS=(
 "$SP/ue4ss-native-owned-default-detour-1/port/x64Detour.cpp:5bb9cea4ac6a"
 "$SP/ue4ss-native-owned-detour-1/port/sources/ADetour.cpp:920730412286"
 "$SP/ue4ss-native-owned-detour-1/port/sources/ZydisDisassembler.cpp:facfea222dd4"
 "$D/polyhook/sources/RangeAllocator.cpp:c94bc041d663"
 "$D/polyhook/sources/FBAllocator.cpp:61f45fd3bc60"
 "$D/polyhook/sources/ErrorLog.cpp:e92acbbe14d7"
 "$D/polyhook/sources/UID.cpp:fca158c78c46"
 "$SP/ue4ss-native-bounded-alloc-1/port/sources/PolyHookOs.cpp:d4f19e47a568"
 "$SP/ue4ss-native-owned-detour-1/OwnedTrampoline.cpp:99aa5e47455e"
 "$SP/ue4ss-native-read-lease-1/src/OwnedMemory.cpp:d824528d762f"
 "$W/host-test/test_detour_refusal_r2.cpp:TEST"
)
OBJS=()
for u in "${UNITS[@]}"; do
 src=${u%:*}; want=${u#*:}
 sha=$(shasum -a 256 "$src" | cut -c1-12)
 if [ "$want" != "TEST" ] && [ "$sha" != "$want" ]; then echo "SHA MISMATCH $src $sha != $want"; exit 1; fi
 obj=$OBJ/$(basename "$src" .cpp).o
 if [ ! -f "$obj" ]; then "${FLAGS[@]}" -c "$src" -o "$obj" 2> "$obj.stderr" || { cat "$obj.stderr"; exit 1; }; fi
 OBJS+=("$obj")
done
clang++ -arch x86_64 -std=c++23 -fsanitize=address,undefined -fno-omit-frame-pointer -pthread \
  "${OBJS[@]}" \
  $SP/ue4ss-native-bounded-owned-1/build-1/Misc-host-x86.o \
  $SP/ue4ss-native-owned-detour-1/host-2/polyhook-MemProtector.o \
  $SP/ue4ss-native-detour-host-deps-1/build-1/libasmjit-darwin-x86-owned.a \
  $SP/ue4ss-native-detour-host-deps-1/build-1/libasmtk-darwin-x86-owned.a \
  $SP/ue4ss-native-owned-detour-1/host-1/zydis-src-*.o \
  $SP/ue4ss-native-owned-detour-1/host-2/zydis-src-*.o \
  $SP/ue4ss-native-owned-detour-1/host-1/zycore-src-*.o \
  -Wl,-dead_strip -o "$BIN" 2> "$W/host-test/link-r2.stderr" || { cat "$W/host-test/link-r2.stderr"; exit 1; }
"$BIN" 2> "$W/host-test/run-r2.asan.log"; rc=$?
echo "exit=$rc"
tail -5 "$W/host-test/run-r2.asan.log" || true
exit $rc
