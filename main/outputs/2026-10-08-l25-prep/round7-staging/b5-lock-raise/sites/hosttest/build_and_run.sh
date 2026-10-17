#!/bin/sh
# Builds the REAL Lua 5.4.7 C sources as C (clang -x c -std=c11, setjmp/longjmp: the port's configuration)
# into a scratch directory and runs b5_sites_host_test.cpp. One compile at a time; skips under 2 GB free.
# Usage: build_and_run.sh SCRATCH_DIR
set -eu
SCRATCH=${1:?scratch dir}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TARBALL=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-lua-vm-experiment-1/upstream/lua-5.4.7.tar.gz
free_pages=$(vm_stat | awk '/Pages free/ {gsub("\\.","",$3); print $3}')
if [ "$((free_pages * 16384))" -lt 2000000000 ]; then echo "SKIP: under 2 GB free"; exit 2; fi
mkdir -p "$SCRATCH/lua" && cd "$SCRATCH/lua"
[ -d lua-5.4.7 ] || tar xzf "$TARBALL"
cd lua-5.4.7/src && mkdir -p ../obj
for f in lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lauxlib lbaselib lcorolib ldblib liolib lmathlib loadlib loslib lstrlib ltablib lutf8lib linit; do
  [ -f ../obj/$f.o ] || clang -x c -std=c11 -O1 -DLUA_USE_MACOSX -c $f.c -o ../obj/$f.o
done
clang++ -std=c++20 -O1 -I"$SCRATCH/lua/lua-5.4.7/src" "$HERE/b5_sites_host_test.cpp" "$SCRATCH/lua/lua-5.4.7/obj"/*.o -o "$SCRATCH/b5_sites_host_test"
"$SCRATCH/b5_sites_host_test"
