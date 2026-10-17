#!/bin/zsh
# HOST-TEST BUILD — worker-r3-ladder (launch-11 RUNBOOK 5.1 item 1, the
# WithDelay-family remainder). Builds two harness binaries (control / fixed)
# against the REAL Lua 5.4 (LuaRaw) compiled as C++ — the same language route
# as the shipped module. HOST harness route (macOS clang++), distinct from the
# PROSPERO compile gate (receipts/gate-*, frozen argv work/argv-*.json).

set -euo pipefail

WS=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM=$WS/outputs/2026-10-04-zcode-team
HT=$TEAM/worker-r3-ladder/host-test
SPIKE=$WS/.superpowers/native-dependency-spike
LRAW=$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw
LMS=$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaMadeSimple
FMT_INC=$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/fmt/include
PORT_UE4SS=$SPIKE/native-coherent-runtime-1/source/port/ue4ss/include
BUILD=$HT/build

rm -rf "$BUILD"
mkdir -p "$BUILD/lua" "$BUILD/lms"

CXX=clang++
CXXFLAGS="-std=c++23 -O1 -g -fno-omit-frame-pointer -DFMT_HEADER_ONLY -DRC_LUA_MADE_SIMPLE_BUILD_STATIC -Dprintf_s=printf -DLUA_COMPILED_AS_CPP -D__PROSPERO__ -Wno-deprecated-declarations -Wno-unused-parameter"

echo "== toolchain =="
clang++ --version | head -1

echo "== 1. Lua 5.4 (LuaRaw) compiled as C++ =="
for src in "$LRAW"/src/*.c; do
  base=$(basename "${src%.c}")
  [ "$base" = "luauser" ] && continue
  obj="$BUILD/lua/$base.o"
  $CXX -c ${=CXXFLAGS} -x c++ -I"$LRAW/include" "$src" -o "$obj"
done
$CXX -c ${=CXXFLAGS} -I"$LRAW/include" "$HT/shims/luauser_host_shim.c" -o "$BUILD/lua/luauser_host_shim.o"
echo "lua objects: $(ls "$BUILD/lua" | wc -l | tr -d ' ')"

echo "== 2. LuaMadeSimple (real wrapper) =="
$CXX -c ${=CXXFLAGS} -I"$LMS/include" -I"$LRAW/include" -I"$HT/shims" -I"$FMT_INC" "$LMS/src/LuaMadeSimple.cpp" -o "$BUILD/lms/luamadesimple.o"
$CXX -c ${=CXXFLAGS} -I"$LMS/include" -I"$LRAW/include" -I"$HT/shims" -I"$FMT_INC" "$LMS/src/LuaObject.cpp" -o "$BUILD/lms/luaobject.o"

echo "== 3. harness binaries (control / fixed) =="
LUA_OBJS=$(ls "$BUILD"/lua/*.o | grep -v -E "/lua\.o$|/luac\.o$")
LMS_OBJS="$BUILD/lms/luamadesimple.o $BUILD/lms/luaobject.o"

$CXX ${=CXXFLAGS} \
  -I"$HT/shims" -I"$FMT_INC" -I"$PORT_UE4SS" -I"$LMS/include" -I"$LRAW/include" \
  -DL11RL_WORLD_CONTROL -I"$HT/regions/control" \
  "$HT/host_test.cpp" ${=LMS_OBJS} ${=LUA_OBJS} \
  -o "$BUILD/host_test_control"

$CXX ${=CXXFLAGS} \
  -I"$HT/shims" -I"$FMT_INC" -I"$PORT_UE4SS" -I"$LMS/include" -I"$LRAW/include" \
  -DL11RL_WORLD_FIXED -I"$HT/regions/fixed" \
  "$HT/host_test.cpp" ${=LMS_OBJS} ${=LUA_OBJS} \
  -o "$BUILD/host_test_fixed"

ls -la "$BUILD"/host_test_control "$BUILD"/host_test_fixed
echo "== build done =="
