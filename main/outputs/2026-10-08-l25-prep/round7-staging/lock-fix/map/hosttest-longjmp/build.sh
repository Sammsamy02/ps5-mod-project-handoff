#!/bin/sh
# MAP-2 host test build (Mac-only). NOT RUN TO COMPLETION in the MAP-2 session:
# the first attempt compiled the 32 Lua objects but failed to link (LuaLock /
# LuaUnlock / LuaLockInitial / LuaLockFinal undefined: LuaRaw luaconf.h:796-800
# maps lua_lock to them; on PS5 they come from the composed platform_locks.c).
# This version adds that file and a hard memory gate. Sequential compiles only.
#
# usage: sh build.sh <LuaRaw dir> <platform_locks.c>
#   LuaRaw dir     : .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw
#   platform_locks : .superpowers/native-dependency-spike/ue4ss-native-lua-lock-final-1/src/platform_locks.c
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
LUA="$1"
LOCKS="$2"
OUT="$HERE/build"

# Memory gate (task rule: skip if under 2 GB free). Counts "Pages free" only.
free_bytes=$(vm_stat | awk '/page size of/ {ps=$8} /Pages free/ {gsub(/\./,"",$3); f=$3} END {print f*ps}')
min_bytes=$((2 * 1024 * 1024 * 1024))
if [ "$free_bytes" -lt "$min_bytes" ]; then
  echo "gate: vm_stat Pages free = $free_bytes bytes (< 2 GB); skipping build"
  exit 3
fi

mkdir -p "$OUT"
n=0
for f in lapi lauxlib lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc lgc linit liolib llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio; do
  clang -std=c11 -Os -I"$LUA/include" -c "$LUA/src/$f.c" -o "$OUT/$f.o"
  n=$((n+1))
done
clang -std=c11 -Os -I"$LUA/include" -c "$LOCKS" -o "$OUT/platform_locks.o"
n=$((n+1))
echo "C objects compiled: $n (expect 33)"
[ "$n" -gt 0 ] || { echo "gate: zero objects compiled"; exit 1; }
clang++ -std=c++20 -O2 -I"$LUA/include" -c "$HERE/lua_pcall_lock_leak.cpp" -o "$OUT/test.o"
clang++ "$OUT"/*.o -o "$HERE/lua_pcall_lock_leak"
echo "linked: $HERE/lua_pcall_lock_leak"
echo "run:    $HERE/lua_pcall_lock_leak   (exit 0 = leak reproduced and fix shape clean)"
