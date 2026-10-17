#!/bin/sh
# worker-createmutexa host selftest.
# Builds the PATCHED mb_win_mutex_abi.c (original + CreateMutexA shim/binder)
# against the UNCHANGED canonical mb_win_mutex.c / mb_win_threads.c on an
# x86_64 host (Apple clang ms_abi, run via Rosetta), runs the integration
# test, then emits the integration diff and a dry-run apply check.
set -eu
cd "$(dirname "$0")"
ROOT=../../../..
SERVICES="$ROOT/outputs/ps5-mod-bridge/windows-services"
EXEC="$ROOT/outputs/ps5-mod-bridge/windows-execution"
for f in "$SERVICES/mb_win_mutex.c" "$SERVICES/mb_win_threads.c" "$SERVICES/mb_win_mutex_abi.c" "$SERVICES/mb_win_mutex_abi.h"; do
  test -f "$f" || { echo "missing canonical source: $f"; exit 2; }
done

echo "== sha256 provenance (canonical inputs vs patched copies) =="
shasum -a 256 "$SERVICES/mb_win_mutex_abi.c" patched/mb_win_mutex_abi.c \
             "$SERVICES/mb_win_mutex_abi.h" patched/mb_win_mutex_abi.h \
             "$SERVICES/mb_win_mutex.c" "$SERVICES/mb_win_threads.c" | tee sha256.txt

echo "== build (x86_64 host, Rosetta run) =="
if ! clang -arch x86_64 -std=c11 -Wall -Wextra \
     -I patched -I "$SERVICES" -I "$EXEC" \
     test_createmutexa.c patched/mb_win_mutex_abi.c \
     "$SERVICES/mb_win_mutex.c" "$SERVICES/mb_win_threads.c" \
     -o run_createmutexa -pthread 2> build.log; then
  echo "first link failed; retrying with mb_win_clock.c"; tail -5 build.log
  clang -arch x86_64 -std=c11 -Wall -Wextra \
    -I patched -I "$SERVICES" -I "$EXEC" \
    test_createmutexa.c patched/mb_win_mutex_abi.c \
    "$SERVICES/mb_win_mutex.c" "$SERVICES/mb_win_threads.c" "$SERVICES/mb_win_clock.c" \
    -o run_createmutexa -pthread 2>> build.log
fi
echo "build warnings:"; wc -l < build.log

echo "== run =="
arch -x86_64 ./run_createmutexa | tee run_output.txt

echo "== emit integration diff =="
{
  diff -u \
    --label "a/outputs/ps5-mod-bridge/windows-services/mb_win_mutex_abi.c" \
    --label "b/outputs/ps5-mod-bridge/windows-services/mb_win_mutex_abi.c" \
    "$SERVICES/mb_win_mutex_abi.c" patched/mb_win_mutex_abi.c || true
  diff -u \
    --label "a/outputs/ps5-mod-bridge/windows-services/mb_win_mutex_abi.h" \
    --label "b/outputs/ps5-mod-bridge/windows-services/mb_win_mutex_abi.h" \
    "$SERVICES/mb_win_mutex_abi.h" patched/mb_win_mutex_abi.h || true
} > ../mb_win_mutex_a_integration.diff

echo "== dry-run apply check against canonical tree =="
if patch -C -p1 -d "$ROOT" < ../mb_win_mutex_a_integration.diff >/dev/null 2> patch_check.log; then
  echo "DIFF_APPLIES_CLEAN"
else
  echo "DIFF_APPLY_CHECK:"; cat patch_check.log
fi
echo "SELFTEST_DONE"
