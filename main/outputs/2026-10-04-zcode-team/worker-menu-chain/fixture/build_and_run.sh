#!/bin/zsh
# BUILD + RUN — worker-menu-chain host fixture (the first F7 press as ONE chain).
#
# 1. Extract byte-exact regions (extract_regions.py cross-checks hashes against
#    the accepted worker-r3-ladder extraction and pins all three sources).
# 2. Build the REAL input stack (Handler.cpp / QueueInputSource.cpp / KeyDef.cpp
#    from the port tree), REAL Lua 5.4.7 (LuaRaw compiled as C++), REAL
#    LuaMadeSimple, REAL port TRY — then the fixture TU that includes the
#    extracted LuaMod regions.
# 3. Run the fixture once (S1, S1b, S2 negative control, S3).
#
# HOST harness route (macOS clang++), distinct from any PROSPERO compile gate.
# LOCAL ONLY: no console contact, no game claims.

set -euo pipefail

WS=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM=$WS/outputs/2026-10-04-zcode-team
MC=$TEAM/worker-menu-chain
FX=$MC/fixture
R3HT=$TEAM/worker-r3-ladder/host-test
SPIKE=$WS/.superpowers/native-dependency-spike
LRAW=$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw
LMS=$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaMadeSimple
STR_INC=$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include
FMT_INC=$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/fmt/include
PORT_UE4SS=$SPIKE/native-coherent-runtime-1/source/port/ue4ss/include
PORT_INPUT=$SPIKE/ue4ss-unreal-native-build-1/port/first/include
PORT_INPUT_SRC=$SPIKE/ue4ss-unreal-native-build-1/port/first/src
BUILD=$FX/build

STAGED_LUAMOD=$TEAM/worker-r3-ladder/staging/luamod-l11-r3-ladder/LuaMod.cpp
CONTROL_LUAMOD=$TEAM/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp
MAIN_LUA=$FX/main.lua

rm -rf "$BUILD"
mkdir -p "$BUILD/lua" "$BUILD/lms" "$BUILD/input"

CXX=clang++
CXXFLAGS="-std=c++23 -O1 -g -fno-omit-frame-pointer -DFMT_HEADER_ONLY -DRC_LUA_MADE_SIMPLE_BUILD_STATIC -Dprintf_s=printf -DLUA_COMPILED_AS_CPP -D__PROSPERO__ -Wno-deprecated-declarations -Wno-unused-parameter"
INPUT_FLAGS="-DHAS_INPUT -DRC_INPUT_BUILD_STATIC"

echo "== toolchain =="
clang++ --version | head -1

echo "== 0. byte-exact region provenance =="
# DEFAULT: verify the on-disk regions against the recorded manifest (the
# manifest pins the source sha256s asserted at extraction time). The shared
# control lineage file drifted mid-session AFTER this worker's pinned
# extraction (observed c92c92ff... -> facf993a...; not owned, not touched by
# this worker), so rebuilds pin to the RECORDED extraction instead of re-reading
# mutable shared lineage files. Full re-extraction: EXTRACT=1 (re-asserts all
# three pinned sources; fails loudly on drift).
if [ "${EXTRACT:-0}" = "1" ] || [ ! -f "$FX/regions/regions-manifest.json" ]; then
  python3 "$MC/extract_regions.py" "$STAGED_LUAMOD" "$CONTROL_LUAMOD" "$MAIN_LUA" "$FX/regions"
else
  python3 "$MC/extract_regions.py" --verify "$FX/regions"
fi

echo "== 1. Lua 5.4.7 (LuaRaw) compiled as C++ =="
for src in "$LRAW"/src/*.c; do
  base=$(basename "${src%.c}")
  [ "$base" = "luauser" ] && continue
  obj="$BUILD/lua/$base.o"
  $CXX -c ${=CXXFLAGS} -x c++ -I"$LRAW/include" "$src" -o "$obj"
done
$CXX -c ${=CXXFLAGS} -I"$LRAW/include" "$FX/shims/luauser_host_shim.c" -o "$BUILD/lua/luauser_host_shim.o"
echo "lua objects: $(ls "$BUILD/lua" | wc -l | tr -d ' ')"

echo "== 2. LuaMadeSimple (real wrapper) =="
$CXX -c ${=CXXFLAGS} -I"$LMS/include" -I"$LRAW/include" -I"$FX/shims" -I"$FMT_INC" "$LMS/src/LuaMadeSimple.cpp" -o "$BUILD/lms/luamadesimple.o"
$CXX -c ${=CXXFLAGS} -I"$LMS/include" -I"$LRAW/include" -I"$FX/shims" -I"$FMT_INC" "$LMS/src/LuaObject.cpp" -o "$BUILD/lms/luaobject.o"

echo "== 3. REAL input stack (port tree, compiled whole) =="
$CXX -c ${=CXXFLAGS} ${=INPUT_FLAGS} -I"$PORT_INPUT" -I"$STR_INC" -I"$FX/shims" -I"$FMT_INC" "$PORT_INPUT_SRC/Input/Handler.cpp" -o "$BUILD/input/handler.o"
$CXX -c ${=CXXFLAGS} ${=INPUT_FLAGS} -I"$PORT_INPUT" -I"$STR_INC" -I"$FX/shims" -I"$FMT_INC" "$PORT_INPUT_SRC/Input/KeyDef.cpp" -o "$BUILD/input/keydef.o"
$CXX -c ${=CXXFLAGS} ${=INPUT_FLAGS} -I"$PORT_INPUT" -I"$STR_INC" -I"$FX/shims" -I"$FMT_INC" "$PORT_INPUT_SRC/Input/Platform/QueueInputSource.cpp" -o "$BUILD/input/queueinputsource.o"

echo "== 4. fixture binary =="
LUA_OBJS=$(ls "$BUILD"/lua/*.o | grep -v -E "/lua\.o$|/luac\.o$")
LMS_OBJS="$BUILD/lms/luamadesimple.o $BUILD/lms/luaobject.o"
INPUT_OBJS="$BUILD/input/handler.o $BUILD/input/keydef.o $BUILD/input/queueinputsource.o"

$CXX ${=CXXFLAGS} ${=INPUT_FLAGS} \
  -I"$FX/shims" -I"$FX/regions" -I"$FMT_INC" -I"$PORT_INPUT" -I"$PORT_UE4SS" \
  -I"$STR_INC" -I"$LMS/include" -I"$LRAW/include" \
  "$FX/menu_chain_fixture.cpp" ${=LMS_OBJS} ${=LUA_OBJS} ${=INPUT_OBJS} \
  -o "$BUILD/menu_chain_fixture"

ls -la "$BUILD/menu_chain_fixture"

echo "== 5. run =="
mkdir -p "$MC/results"
set +e
"$BUILD/menu_chain_fixture" "$MAIN_LUA" >"$MC/results/fixture-run.stdout" 2>"$MC/results/fixture-run.stderr"
STATUS=$?
set -e
echo "fixture exit status: $STATUS"
exit $STATUS
