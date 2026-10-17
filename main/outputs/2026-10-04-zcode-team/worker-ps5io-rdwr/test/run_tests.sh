#!/bin/sh
# Host build + tests for worker-ps5io-rdwr (extended ps5_io: RDWR + validation).
# The vendored modforge headers are consumed read-only from the upstream
# worker-installer-ps5io dir (nothing there is modified).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
VENDORED=$(cd "$ROOT/../worker-installer-ps5io/vendored" && pwd)
OUT=$ROOT/build
mkdir -p "$OUT" "$ROOT/receipts"

CC=${CC:-cc}

echo "== compile host ps5_io + bridge (PS5IO_HOST) =="
$CC -std=c99 -O2 -Wall -Wextra -DPS5IO_HOST=1 -I"$ROOT/src" -I"$VENDORED" \
    -o "$OUT/test_ps5io_host" "$ROOT/test/test_ps5io_host.c" "$ROOT/src/ps5_io.c"
$CC -std=c99 -O2 -Wall -Wextra -DPS5IO_HOST=1 -I"$ROOT/src" -I"$VENDORED" \
    -o "$OUT/test_prepare_bridge" "$ROOT/test/test_prepare_bridge.c" \
    "$ROOT/src/ps5_io.c" "$ROOT/src/ps5_prepare_bridge.c"

echo "== sanitizer build (address+undefined) =="
$CC -std=c99 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DPS5IO_HOST=1 -I"$ROOT/src" -I"$VENDORED" \
    -o "$OUT/test_ps5io_host_asan" "$ROOT/test/test_ps5io_host.c" "$ROOT/src/ps5_io.c"

echo "== run =="
"$OUT/test_ps5io_host" 2>&1 | tee "$ROOT/receipts/host-test.log"
echo "== run under sanitizers =="
export ASAN_OPTIONS=abort_on_error=0
export UBSAN_OPTIONS=halt_on_error=0
"$OUT/test_ps5io_host_asan" 2>&1 | tee "$ROOT/receipts/host-test-asan.log"
echo "ALL HOST TESTS DONE"
