#!/bin/sh
# worker-io-controls host test — extraction step.
# Extracts the HOST-TEST-EXTRACT region VERBATIM from the staged bridge so
# the host test compiles the exact shipped bytes (single source of truth —
# never a hand-mirrored copy).
set -e
cd "$(dirname "$0")"
BRIDGE="../staging/runtime_start_bridge_r14_diag_spanpub.cpp"
OUT="io_controls_extract.inc"
awk '/HOST-TEST-EXTRACT-BEGIN/{f=1} f{print} f&&/HOST-TEST-EXTRACT-END/{exit}' "$BRIDGE" > "$OUT"
if ! grep -q "HOST-TEST-EXTRACT-BEGIN" "$OUT" || ! grep -q "HOST-TEST-EXTRACT-END" "$OUT"; then
    echo "FAIL: extraction markers not found in $BRIDGE" >&2
    rm -f "$OUT"
    exit 1
fi
echo "extracted $(wc -l < "$OUT" | tr -d ' ') lines -> $OUT"
