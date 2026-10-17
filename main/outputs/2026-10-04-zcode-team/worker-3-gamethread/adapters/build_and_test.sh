#!/bin/zsh
# build_and_test.sh — single small host build of worker-3's OWN files, plus a
# read-only as-is compile of the existing immutable runtime_owner.cpp and the
# header-only NativeGameThreadIdentity.hpp to exercise the REAL guards.
# Uses the pinned LLVM from BUILD_AND_SOURCE_HISTORY.txt. No target/SDK build,
# no console contact, no network.
set -euo pipefail

WHAT="/WORKSPACE/.codex/worktrees/translation-core/what"
OUT="$WHAT/outputs/2026-10-04-zcode-team/worker-3-gamethread/adapters"
# Pinned LLVM 21.1.8 was attempted first (with -isysroot "$(xcrun
# --show-sdk-path)"): its upstream headers do not compose with the Apple SDK
# toolchain (darwin _types/stdarg failures). This script builds a HOST
# BEHAVIOR TEST ONLY and produces no target object/IR, so Apple clang (which
# matches the SDK) is the correct host compiler here. Deviation recorded in
# ../REPORT.md; any target build still uses the pinned LLVM/SDK route.
CLANG="/usr/bin/clang++"

mkdir -p "$OUT/build"

"$CLANG" -std=c++20 -Wall -Wextra -O1 -g \
  -I "$OUT" \
  -I "$WHAT/.superpowers/native-dependency-spike/ue4ss-native-game-thread-1/include" \
  -I "$WHAT/outputs/ps5-mod-bridge/native-runtime-startup/bootstrap" \
  "$OUT/gamethread_seams.cpp" \
  "$OUT/seam_host_test.cpp" \
  "$WHAT/outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/runtime_owner.cpp" \
  -pthread \
  -o "$OUT/build/seam_host_test"

"$OUT/build/seam_host_test"
