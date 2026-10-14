#!/bin/zsh
# run_host_test.sh — build and run the composition-5 fail-open host test.
#
# The file under test is module_entry_patched.c (byte-exact patched copy of
# outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c,
# produced by apply_patch.py). It is compiled for x86_64 (the PS5 target's
# bitness — module_entry.c contains `mov %%rsp` asm) and executed under
# Rosetta. No console, no network.
set -e
cd "$(dirname "$0")"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
OUTLOG="$TMP/UE4SS.log"
DIAGLOG="$TMP/diag_channel.log"

echo "== build =="
clang -arch x86_64 -Wall -Wextra -Werror -std=c11 -O2 \
  -I. \
  -I/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-runtime-startup/lifecycle \
  -o host_test \
  module_entry_patched.c shims.c test_main.c
echo "build OK"

echo "== scenarios =="
STATUS=0
for S in t1_channel_open_failed t2_bindings_null t3_wrong_state t4_guard_busy t5_success_silent t7_failover_refused; do
  rm -f "$OUTLOG" "$DIAGLOG"
  if ! ./host_test "$S" "$OUTLOG" "$DIAGLOG"; then
    STATUS=1
  fi
done

echo
if [ "$STATUS" -eq 0 ]; then
  echo "HOST-TEST-PASS all scenarios"
else
  echo "HOST-TEST-FAIL"
fi
exit "$STATUS"
