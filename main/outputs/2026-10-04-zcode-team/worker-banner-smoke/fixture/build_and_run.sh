#!/bin/sh
# worker-banner-smoke host fixture build + run.
# Compiles: vendored Lua 5.4.7 (UE4SS LuaRaw core), the verbatim port-tree
# NativeModSource.cpp, and the driver with byte-exact extractions of the
# staged LuaMod.cpp loader functions (A/B, reusing worker-require-path's
# verified extractions) and the FIXED to_generic_string from the staged
# Helpers/String.hpp (c76be502). No network, no engine, host only.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../.." && pwd)"      # fixture -> worker -> team -> outputs -> what
TEAM="$WHAT/outputs/2026-10-04-zcode-team"
RAW="$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw"
BUNDLE="$TEAM/worker-itemspawner-bundle/bundle"
RESULTS="$HERE/../results"
BUILD="$HERE/build"
MAIN_LUA="$BUNDLE/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
STAGED_STRING_HPP="$TEAM/worker-iniparser-repro/staged/Helpers/String.hpp"

mkdir -p "$BUILD" "$RESULTS"

echo "== provenance checks =="
shasum -a 256 "$STAGED_STRING_HPP" | awk '{print "staged String.hpp:", $1, "(expect c76be502...)"}'
shasum -a 256 "$MAIN_LUA" | awk '{print "staged main.lua  :", $1}'
MAIN_SHA="$(shasum -a 256 "$MAIN_LUA" | awk '{print $1}')"
case "$MAIN_SHA" in
  f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658)
    echo "main.lua sha256 MATCH f586043a (task-staged bytes)";;
  *) echo "FATAL: main.lua sha mismatch"; exit 1;;
esac
MAIN_SIZE="$(wc -c < "$MAIN_LUA" | tr -d ' ')"
[ "$MAIN_SIZE" = "21415" ] || { echo "FATAL: main.lua size $MAIN_SIZE != 21415"; exit 1; }
echo "main.lua size MATCH 21415"

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

echo "== driver (extraction includes + glue; __PROSPERO__ defined ONLY for the"
echo "    staged to_generic_string __PROSPERO__ branch — the byte-widening fix"
echo "    under test; no other extraction branches on it, verified by grep) =="
clang++ -std=c++20 -O1 -Wall -D__PROSPERO__ \
  -I "$HERE" -I "$HERE/extraction" -I "$RAW/include" \
  -c "$HERE/banner_smoke_fixture.cpp" -o "$BUILD/banner_smoke_fixture.o"

clang++ "$BUILD"/banner_smoke_fixture.o "$BUILD"/NativeModSource.o "$BUILD"/l*.o \
  -o "$BUILD/banner_smoke_fixture"

echo "== run =="
cd "$RESULTS"
"$BUILD/banner_smoke_fixture" "$BUNDLE" "$RESULTS" 2>&1 | tee "$RESULTS/fixture-run.log"
RUN_STATUS="${PIPESTATUS:-0}"

echo "== external evidence on captured banner bytes =="
if [ -f "$RESULTS/banner_capture.bin" ]; then
  echo "--- banner_capture.bin sha256/size ---"
  shasum -a 256 "$RESULTS/banner_capture.bin"
  wc -c < "$RESULTS/banner_capture.bin" | tr -d ' '
  echo "--- banner_capture.bin hex (od) ---"
  od -A d -t x1 "$RESULTS/banner_capture.bin"
  echo "--- NUL byte count (expect 0) ---"
  NULS="$(od -A n -t x1 "$RESULTS/banner_capture.bin" | tr -s ' ' '\n' | grep -c '^00$' || true)"
  echo "NUL bytes: ${NULS:-0}"
else
  echo "banner_capture.bin absent (smoke case did not produce it)"
fi

exit "$RUN_STATUS"
