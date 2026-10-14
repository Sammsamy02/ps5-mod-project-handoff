#!/bin/sh
# run_host_test.sh — worker-walk-site-transitions, 2026-10-04.
# Host logic test for the PROSPERO walk-read guard: extracts the verbatim core block
# from ASMHelper.guarded.cpp (single source of truth — the tested bytes ARE the
# shipped bytes), compiles it against a software mprotect model, runs the ordering /
# refusal / page-math assertions. Host-only; no cross-compilation, no console.
set -eu
cd "$(dirname "$0")"

GUARDED=../ASMHelper.guarded.cpp
CORE=asm_walk_guard_core.inc

# 1. Extract the marked core block; abort loudly on marker drift.
BEGIN_COUNT=$(grep -c "RC_ASM_WALK_GUARD_CORE BEGIN" "$GUARDED")
END_COUNT=$(grep -c "RC_ASM_WALK_GUARD_CORE END" "$GUARDED")
if [ "$BEGIN_COUNT" -ne 1 ] || [ "$END_COUNT" -ne 1 ]; then
    echo "FAIL: marker drift (BEGIN=$BEGIN_COUNT END=$END_COUNT)" >&2
    exit 2
fi
sed -n '/RC_ASM_WALK_GUARD_CORE BEGIN/,/RC_ASM_WALK_GUARD_CORE END/p' "$GUARDED" > "$CORE"
LINES=$(wc -l < "$CORE")
if [ "$LINES" -lt 40 ]; then
    echo "FAIL: extracted core implausibly small ($LINES lines)" >&2
    exit 2
fi
echo "extracted core: $LINES lines"

# 2. Compile with the SAME build-stamp values the pinned SDK verified.
clang++ -std=c++23 -O0 -Wall -Wextra \
    -DRC_ASM_WALK_PROT_READ=0x1 \
    -DRC_ASM_WALK_PROT_EXEC=0x4 \
    -DRC_ASM_WALK_PAGE_SHIFT=14 \
    -I. \
    -o walk_guard_host_test walk_guard_core_host_test.cpp

# 3. Run.
./walk_guard_host_test
