#!/bin/bash
# Focused HOST suite. Binary pin extraction is an explicit separate evidence step.
# Logs are chosen explicitly; never overwrites a previous receipt.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:?usage: run.sh NEW_LOG_PATH}
[ ! -e "$OUT" ] || { echo 'refuse existing log' >&2; exit 2; }
export TMPDIR="$HERE/tmp"
export PYTHONDONTWRITEBYTECODE=1
mkdir -p "$TMPDIR"
CXX=${CXX:-/usr/bin/clang++}
{
 python3 "$HERE/gate_no_hardcode.py"
 for name in test_pak_mount_host regressions regressions2 native_adapter producer; do
  "$CXX" -std=c++20 -Wall -Wextra -Werror -Wno-unused-function -o "$TMPDIR/$name" "$HERE/$name.cpp" "$HERE/../PATCH/RuntimePakMount.cpp"
  if [ "$name" != producer ]; then "$TMPDIR/$name"; fi
 done
 python3 "$HERE/test_producer.py"
 python3 "$HERE/test_stage.py"
 python3 "$HERE/test_native_contract.py"
 python3 "$HERE/test_build_integration.py"
 echo 'PASS focused host suite; no target compilation or console execution'
} 2>&1 | tee "$OUT"
