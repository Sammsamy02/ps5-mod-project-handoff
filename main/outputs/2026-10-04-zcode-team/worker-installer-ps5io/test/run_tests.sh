#!/bin/sh
# Host build + tests for worker-installer-ps5io (POSIX-mappable surface).
# Console object builds live in build_console.sh (receipts under receipts/).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
OUT=$ROOT/build
mkdir -p "$OUT"

CC=${CC:-cc}

echo "== compile host ps5_io + bridge (PS5IO_HOST) =="
$CC -std=c99 -O2 -Wall -Wextra -DPS5IO_HOST=1 -I"$ROOT/src" -I"$ROOT/vendored" \
    -o "$OUT/test_ps5io_host" "$ROOT/test/test_ps5io_host.c" "$ROOT/src/ps5_io.c"
$CC -std=c99 -O2 -Wall -Wextra -DPS5IO_HOST=1 -I"$ROOT/src" -I"$ROOT/vendored" \
    -o "$OUT/test_prepare_bridge" "$ROOT/test/test_prepare_bridge.c" \
    "$ROOT/src/ps5_io.c" "$ROOT/src/ps5_prepare_bridge.c"

echo "== sanitizer build (address+undefined) =="
$CC -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DPS5IO_HOST=1 -I"$ROOT/src" -I"$ROOT/vendored" \
    -o "$OUT/test_ps5io_host_asan" "$ROOT/test/test_ps5io_host.c" "$ROOT/src/ps5_io.c"
$CC -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DPS5IO_HOST=1 -I"$ROOT/src" -I"$ROOT/vendored" \
    -o "$OUT/test_prepare_bridge_asan" "$ROOT/test/test_prepare_bridge.c" \
    "$ROOT/src/ps5_io.c" "$ROOT/src/ps5_prepare_bridge.c"

echo "== run =="
"$OUT/test_ps5io_host"
"$OUT/test_prepare_bridge"
echo "== run under sanitizers =="
export ASAN_OPTIONS=abort_on_error=0
export UBSAN_OPTIONS=halt_on_error=0
"$OUT/test_ps5io_host_asan"
"$OUT/test_prepare_bridge_asan"
echo "ALL HOST TESTS DONE"
