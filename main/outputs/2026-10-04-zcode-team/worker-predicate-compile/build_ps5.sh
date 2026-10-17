#!/usr/bin/env bash
# build_ps5.sh — worker-predicate-compile PS5 cross-compile of the vendored
# batch1 GUObjectArray predicate + C wrapper.
#
# Toolchain and flags are pinned to the frozen v2/v3 in-game probe lineage
# (worker-ingame-probe-v3/build_ingame.py):
#   - prospero-clang++ from ps5-payload-sdk (SDK v0.43), LLVM_CONFIG pinned to
#     the same LLVM 21.1.8 the lineage receipts used;
#   - CPPFLAGS = CFLAGS + ["-fno-exceptions","-fno-rtti","-fno-threadsafe-statics"]
#     with CFLAGS = -Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector
#                   -fvisibility=hidden -ffreestanding -fno-builtin
#                   -fno-asynchronous-unwind-tables
#   - the wrapper predicate_call.c is C++-compiled (like the predicate itself);
#     -x c++ is required because the source keeps its .c extension.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$(cd "$HERE/../../.." && pwd)"
[ "$(basename "$WHAT")" = "what" ] || { echo "bad WHAT: $WHAT" >&2; exit 1; }

SDK_BIN="$WHAT/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin"
LLVM="$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
CLANGXX="$SDK_BIN/prospero-clang++"
NM="$LLVM/llvm-nm"
OBJDUMP="$LLVM/llvm-objdump"
[ -x "$CLANGXX" ] || { echo "missing $CLANGXX" >&2; exit 1; }

CPPFLAGS=(-Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector
          -fvisibility=hidden -ffreestanding -fno-builtin
          -fno-asynchronous-unwind-tables
          -fno-exceptions -fno-rtti -fno-threadsafe-statics)

OUT="$HERE/ps5"
mkdir -p "$OUT"

receipt="$OUT/build-receipt.json"
: > "$receipt"
emit() { printf '%s\n' "$1" >> "$receipt"; }
emit "["

first=1
run() {
  [ $first -eq 0 ] && emit ","
  first=0
  argv_json=$(python3 -c 'import json,sys; print(json.dumps(sys.argv[1:]))' "$@")
  emit "{\"argv\": $argv_json,"
  set +e
  out=$("$@" 2>&1)
  rc=$?
  set -e
  emit "\"rc\": $rc,"
  emit "\"stdout_stderr\": $(python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))' <<<"$out")}"
  [ $rc -eq 0 ] || { echo "FAILED ($rc): $*" >&2; echo "$out" >&2; exit $rc; }
}

export LLVM_CONFIG="$LLVM/llvm-config"

# 1. Predicate (vendored verbatim) and wrapper, same flags as v3 CPP_SOURCES.
run "$CLANGXX" "${CPPFLAGS[@]}" -I"$HERE/src" -x c++ \
    -c "$HERE/src/townfall_layout_probes.cpp" -o "$OUT/townfall_layout_probes.o"
run "$CLANGXX" "${CPPFLAGS[@]}" -I"$HERE/src" -x c++ \
    -c "$HERE/src/predicate_call.c" -o "$OUT/predicate_call.o"

# 2. Verify object identity (AMD64 x86-64 relocatable) and exported symbols.
run "$OBJDUMP" -f "$OUT/townfall_layout_probes.o"
run "$OBJDUMP" -f "$OUT/predicate_call.o"
run "$NM" --defined-only "$OUT/townfall_layout_probes.o"
run "$NM" --defined-only "$OUT/predicate_call.o"
run "$NM" -u "$OUT/townfall_layout_probes.o"
run "$NM" -u "$OUT/predicate_call.o"

emit "]"
echo "PS5 objects built:"; ls -l "$OUT"/*.o
