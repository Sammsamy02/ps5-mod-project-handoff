#!/bin/sh
# Console (PS5 payload) object build + symbol receipts for worker-ps5io-rdwr
# (extended ps5_io: PS5IO_RDWR + flag validation). Compile-only: nothing here
# executes on a target, no network, no console access. Mirrors the upstream
# worker-installer-ps5io/build_console.sh contract against THIS dir's vendored
# + patched sources; the upstream dir is only read (include path).
#
# Pinned toolchain: the SDK's prospero-clang wrapper driven by the pinned
# LLVM 21.1.8 llvm-config (same distribution the archive closure proof binds
# to; see native-probe/build/deps/llvm-selection.json).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT="$HERE"
UP=$(cd "$ROOT/../worker-installer-ps5io" && pwd)
SDK=${PS5_PAYLOAD_SDK:-/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk}
LLVM=${PS5_LLVM_CONFIG:-/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config}
export LLVM_CONFIG="$LLVM"

CC="$SDK/bin/prospero-clang"
NM="$SDK/bin/llvm-nm"
OUT="$ROOT/build"
REC="$ROOT/receipts"
mkdir -p "$OUT" "$REC"

echo "== compiler =="
"$CC" --version | head -2

echo "== SDK O-flag citation (the bit value PS5IO_RDWR binds to) =="
grep -n -E "^#define[[:space:]]+O_(RDONLY|WRONLY|RDWR|ACCMODE)[[:space:]]" \
  "$SDK/target/include/sys/fcntl.h" | tee "$REC/sdk-oflags.txt"
grep -q -E "#define[[:space:]]+O_RDWR[[:space:]]+0x0002" "$REC/sdk-oflags.txt" \
  || { echo "FATAL: pinned SDK O_RDWR is not 0x0002 — re-cite before linking"; exit 1; }

echo "== compile ps5_io.c (console TU, extended) =="
"$CC" -std=c99 -O2 -Wall -Wextra -c \
  -I"$ROOT/src" -I"$UP/vendored" \
  -o "$OUT/ps5_io.ps5.o" "$ROOT/src/ps5_io.c"

echo "== compile ps5_prepare_bridge.c (console TU, unchanged consumer) =="
"$CC" -std=c99 -O2 -Wall -Wextra -c \
  -I"$ROOT/src" -I"$UP/vendored" \
  -o "$OUT/ps5_prepare_bridge.ps5.o" "$ROOT/src/ps5_prepare_bridge.c"

echo "== undefined symbols of the console objects =="
for o in "$OUT/ps5_io.ps5.o" "$OUT/ps5_prepare_bridge.ps5.o"; do
  echo "-- $(basename "$o")"
  "$NM" --undefined-only "$o"
done | tee "$REC/console-undef.txt"

echo "== provider check: every undefined symbol must be defined by a pinned target lib (windows-candidate/build.py pattern) =="
for o in "$OUT/ps5_io.ps5.o" "$OUT/ps5_prepare_bridge.ps5.o"; do
  echo "-- $(basename "$o")"
  for name in $("$NM" --undefined-only "$o" | awk '{print $NF}' | sort -u); do
    hits=""
    for lib in "$SDK"/target/lib/*.so; do
      if "$NM" -D --defined-only "$lib" 2>/dev/null | awk -v s="$name" '$NF==s{found=1} END{exit !found}'; then
        hits="$hits $(basename "$lib")"
      fi
    done
    # intra-bundle symbols (ps5io_*) resolve at link time from ps5_io.o
    case "$name" in
      ps5io_*) echo "$name -> resolved by ps5_io.ps5.o (same composition)";;
      *) if [ -n "$hits" ]; then echo "$name ->$hits"; else echo "$name -> MISSING FROM PINNED LIBS"; exit 1; fi;;
    esac
  done
done | tee "$REC/console-providers.txt"
echo "CONSOLE OBJECT BUILD + PROVIDER CHECK OK"
