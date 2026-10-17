#!/bin/bash
# cap03/tests/run_host_test.sh — build + run the Mac host protocol test.
# Mac-only. No console, no kernel contact. 2026-10-13, KD/cap03.
set -e
cd "$(dirname "$0")/.."

SDK_ROOT="$HOME/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap03"
OBJDIR=build/host
mkdir -p "$OBJDIR"

# Mock renames route the SCE/mman boundary of the PRODUCTION TU to the
# mocks defined in the test TU. The production source is NOT modified.
RENAMES="-DPAGE_SIZE=4096 -DPAGE_MASK=4095 \
-DsceKernelQueryMemoryProtection=cap3_host_qmp \
-DsceKernelVirtualQuery=cap3_host_vq \
-Dmprotect=cap3_host_mprotect"

echo "[run] compiling production TU (mock-renamed, x86_64 host)..."
clang++ -arch x86_64 -std=c++23 -O0 -g -Wall -Isrc -c $RENAMES \
    -o "$OBJDIR/CodePatch.host.o" src/CodePatch.cpp

echo "[run] compiling test TU..."
clang++ -arch x86_64 -std=c++23 -O0 -g -Wall -Isrc -c \
    -DPAGE_SIZE=4096 -DPAGE_MASK=4095 \
    -o "$OBJDIR/host_test_protocol.o" tests/host_test_protocol.cpp

echo "[run] linking..."
clang++ -arch x86_64 -o "$OBJDIR/cap03-host-test" \
    "$OBJDIR/CodePatch.host.o" "$OBJDIR/host_test_protocol.o"

echo "[run] running (x86_64 via Rosetta if needed)..."
set +e
"$OBJDIR/cap03-host-test"
TEST_RC=$?
set -e

echo "[run] binary exit code: $TEST_RC"

# Static P1-machinery check: the compiled (never-executed-on-host)
# kekcall_check must still contain the exact CHECK sub-op immediate
# 0xffffffff00000027 and a `syscall` instruction — in BOTH the host
# object (Mach-O, leading underscore) and the PS5-target object (ELF).
echo "[run] static check: kekcall_check disassembly (x86_64, both objects)"
OBJDUMP="$(command -v llvm-objdump || true)"
if [ -z "$OBJDUMP" ]; then
    OBJDUMP="$(ls "$HOME"/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-*/bin/llvm-objdump 2>/dev/null | head -1)"
fi
STATIC_RC=0
for SYM_OBJ in "__ZN5cap0313kekcall_checkEv:$OBJDIR/CodePatch.host.o:host" \
               "_ZN5cap0313kekcall_checkEv:build/ps5-obj/CodePatch.o:ps5"; do
    SYM="${SYM_OBJ%%:*}"; REST="${SYM_OBJ#*:}"
    OBJ="${REST%%:*}"; KIND="${REST##*:}"
    if [ ! -f "$OBJ" ]; then
        echo "[run] STATIC-NOT-RUN [$KIND] object absent; target compilation remains unverified"
        if [ "$KIND" = host ]; then STATIC_RC=1; fi
        continue
    fi
    DIS="$("$OBJDUMP" -d --disassemble-symbols="$SYM" "$OBJ" 2>/dev/null)"
    echo "$DIS" > "$OBJDIR/kekcall_check.$KIND.dis.txt"
    if echo "$DIS" | grep -q "0xFFFFFFFF00000027" && echo "$DIS" | grep -q "	syscall"; then
        echo "[run] STATIC-OK [$KIND] kekcall_check encodes CHECK sub-op 0xFFFFFFFF00000027 + syscall"
    else
        echo "[run] STATIC-FAIL [$KIND] immediate/syscall missing — see $OBJDIR/kekcall_check.$KIND.dis.txt"
        STATIC_RC=1
    fi
done

if [ "$TEST_RC" -eq 0 ] && [ "$STATIC_RC" -eq 0 ]; then
    echo "[run] HOST TEST: PASS"
    exit 0
fi
echo "[run] HOST TEST: FAIL (test=$TEST_RC static=$STATIC_RC)"
exit 1
