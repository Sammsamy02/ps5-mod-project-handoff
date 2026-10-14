#!/bin/sh
# cap07-step2 host test runner. Mac-only, no console. Runs:
#   1. the engine pin verifier (re-derives pins from the local eboot)
#   2. the no-hardcode / no-detour gate on the staged runtime unit
#   3. the C++ host unit tests (real mount unit vs a mock engine)
# Saves combined output to tests/run-output.txt. Exits non-zero on any failure.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT="$HERE/run-output.txt"
SCRATCH="${TMPDIR:-/tmp}/cap07_step2_build"
mkdir -p "$SCRATCH"

{
  echo "=== cap07-step2 run $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
  echo
  echo "--- 1. pin verifier (verify_pins.py) ---"
  python3 "$HERE/verify_pins.py"
  echo
  echo "--- 2. no-hardcode / no-detour gate (gate_no_hardcode.py) ---"
  python3 "$HERE/gate_no_hardcode.py"
  echo
  echo "--- 3. host unit tests (clang++ -std=c++20) ---"
  /usr/bin/clang++ -std=c++20 -Wall -Wextra -Werror -I"$HERE" \
    -o "$SCRATCH/t_pakmount" \
    "$HERE/test_pak_mount_host.cpp" "$HERE/../PATCH/RuntimePakMount.cpp"
  "$SCRATCH/t_pakmount"
  echo
  echo "=== ALL GATES PASS ==="
} 2>&1 | tee "$OUT"
