#!/bin/sh
# worker-require-path host fixture build + run.
# Compiles: vendored Lua 5.4.7 (UE4SS LuaRaw core), the verbatim port-tree
# NativeModSource.cpp, and the driver with byte-exact extractions of the
# staged LuaMod.cpp loader functions. No network, no engine, host only.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../.." && pwd)"      # fixture -> worker -> team -> outputs -> what
TEAM="$WHAT/outputs/2026-10-04-zcode-team"
RAW="$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw"
BUNDLE="$TEAM/worker-itemspawner-bundle/bundle"
RESULTS="$HERE/../results"
BUILD="$HERE/build"

mkdir -p "$BUILD" "$RESULTS"

LUA_CORE="lapi lauxlib lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump \
lfunc lgc linit liolib llex lmathlib lmem loadlib lobject lopcodes lparser \
lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm loslib lzio"

echo "== host compiler =="
clang++ --version | head -1

echo "== Lua 5.4.7 core objects (LuaRaw, excluding lua.c/luac.c/luauser.c) =="
for unit in $LUA_CORE; do
  clang -O1 -I "$RAW/include" -c "$RAW/src/$unit.c" -o "$BUILD/$unit.o"
done

echo "== NativeModSource.cpp (verbatim port tree) =="
clang++ -std=c++20 -O1 -I "$HERE/extraction" \
  -c "$HERE/extraction/NativeModSource.cpp" -o "$BUILD/NativeModSource.o"

echo "== driver (extraction includes + glue) =="
clang++ -std=c++20 -O1 -Wall -I "$HERE" -I "$HERE/extraction" -I "$RAW/include" \
  -c "$HERE/require_path_fixture.cpp" -o "$BUILD/require_path_fixture.o"

clang++ "$BUILD"/require_path_fixture.o "$BUILD"/NativeModSource.o "$BUILD"/l*.o \
  -o "$BUILD/require_path_fixture"

echo "== run =="
cd "$RESULTS"
"$BUILD/require_path_fixture" "$BUNDLE" "$RESULTS" 2>&1 | tee "$RESULTS/fixture-run.log"
exit "${PIPESTATUS:-0}"
