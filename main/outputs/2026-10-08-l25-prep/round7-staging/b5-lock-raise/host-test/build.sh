#!/bin/sh
# Builds and runs the B5 host test. One compile at a time; skips if < 2 GB free.
# Mac-only. No console contact, no downloads.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
R=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-lua-vm-experiment-1/upstream/re-ue4ss/deps/first/LuaRaw
free_pages=$(vm_stat | awk '/Pages free/ {gsub("\\.","",$3); print $3}')
free_mb=$((free_pages * 16384 / 1048576))
echo "free memory: ${free_mb} MB"
if [ "$free_mb" -lt 2048 ]; then echo "SKIP: under 2 GB free"; exit 3; fi
python3 -I "$HERE/extract_real.py"
mkdir -p "$HERE/build"
n=0
for f in "$R"/src/*.c; do
  b=$(basename "$f" .c)
  case "$b" in lua|luac|luauser) continue;; esac
  clang -x c -std=gnu99 -O1 -DLUA_USE_POSIX -I"$R/include" -c "$f" -o "$HERE/build/$b.o"
  n=$((n+1))
done
clang -x c -std=gnu99 -O1 -I"$R/include" -c "$HERE/luauser_host_stub.c" -o "$HERE/build/luauser_host_stub.o"
echo "compiled $n Lua C files (as C) + stub"
clang++ -std=c++20 -O1 -pthread -I"$R/include" -I"$HERE" "$HERE/lock_raise_host_test.cpp" "$HERE"/build/*.o -o "$HERE/build/lock_raise_host_test"
"$HERE/build/lock_raise_host_test"
