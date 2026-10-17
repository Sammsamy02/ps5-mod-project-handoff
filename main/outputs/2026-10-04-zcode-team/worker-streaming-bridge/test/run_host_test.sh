#!/bin/sh
# Host build + test for the streaming download-to-preparation bridge.
# worker-streaming-bridge, 2026-10-04. Host-only: no network, no console.
set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
TEAM="$WHAT/outputs/2026-10-04-zcode-team/worker-streaming-bridge"
ARCHIVE_SRC="$WHAT/outputs/ps5-mod-bridge/native-host/archive"          # read-only input
LIBARCHIVE_INC="$WHAT/outputs/ps5-mod-bridge/third_party/libarchive"    # read-only input
SPIKE="$WHAT/.superpowers/native-dependency-spike"
LIBARCHIVE_A="$SPIKE/native-zip-experiment-1/host/libarchive-build/.libs/libarchive.a"
MBED_INC="$SPIKE/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/include"
MBED_LIB="$SPIKE/native-https-host-experiment-2/host-libraries"

BUILD="$TEAM/build"
BIN="$BUILD/bridge_host_test"
FIXTURE="$BUILD/fixture.zip"
META="$BUILD/meta.txt"
WORK="$BUILD/work"

rm -rf "$BUILD"
mkdir -p "$BUILD" "$WORK"

echo "== fixture =="
python3 "$TEAM/test/make_fixture.py" "$FIXTURE" "$META"
ROWS=$(sed -n 's/^rows=//p' "$META")
RBYTES=$(sed -n 's/^regular_bytes=//p' "$META")
echo "fixture: rows=$ROWS regular_bytes=$RBYTES size=$(wc -c < "$FIXTURE" | tr -d ' ')"

echo "== compile =="
cc -std=c11 -O1 -g -Wall -Wextra \
   -c "$TEAM/owned/mb_stream_bridge.c" -o "$BUILD/mb_stream_bridge.o"
cc -std=c11 -O1 -g -Wall \
   -I"$ARCHIVE_SRC" -I"$LIBARCHIVE_INC" -I"$MBED_INC" \
   -c "$ARCHIVE_SRC/intake.c" -o "$BUILD/intake.o"
cc -std=c11 -O1 -g -Wall \
   -I"$ARCHIVE_SRC" \
   -c "$ARCHIVE_SRC/private/posix_compat.c" -o "$BUILD/posix_compat.o"
cc -std=c11 -O1 -g -Wall -Wextra \
   -I"$TEAM/owned" -I"$ARCHIVE_SRC" -I"$LIBARCHIVE_INC" -I"$MBED_INC" \
   -c "$TEAM/test/bridge_host_test.c" -o "$BUILD/bridge_host_test.o"

echo "== link (read-only pinned closures) =="
cc "$BUILD/bridge_host_test.o" "$BUILD/mb_stream_bridge.o" \
   "$BUILD/intake.o" "$BUILD/posix_compat.o" \
   "$LIBARCHIVE_A" "$MBED_LIB/libmbedtls.a" "$MBED_LIB/libmbedx509.a" "$MBED_LIB/libmbedcrypto.a" \
   -lz -o "$BIN"

echo "== run =="
cd "$BUILD"
WORK="$WORK" "$BIN" "$FIXTURE" "$ROWS" "$RBYTES" "$WORK"
