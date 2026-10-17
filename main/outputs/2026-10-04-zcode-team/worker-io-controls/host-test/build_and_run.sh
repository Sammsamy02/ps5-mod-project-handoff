#!/bin/sh
# worker-io-controls host test — build + run.
#   Part 1: test_io_controls — compiles the staged bridge's HOST-TEST-EXTRACT
#           region VERBATIM (extract_region.sh) against mocked sceKernel*/
#           stdio: all A/B/C/D pass/fail paths, per-operation errno
#           freshness, D-skip-on-B-fail, canary nonce stale-rejection.
#   Part 2: test_rescue_path — compiles the STAGED SeedValidation.hpp:
#           NoneId==0 semantics, the rescuable-class guard, R-4 capture
#           decode, terminal-evidence class.
# Tees both logs; propagates the verdict.
set -e
cd "$(dirname "$0")"
CXX="${CXX:-c++}"
echo "compiler: $($CXX --version | head -1)"

sh ./extract_region.sh
$CXX -std=c++17 -Wall -Wextra -O1 -o test_io_controls test_io_controls.cpp
$CXX -std=c++17 -Wall -Wextra -O1 -I ../staging -o test_rescue_path test_rescue_path.cpp

set +e
./test_io_controls 2>&1 | tee host-test-run.log
rc1=$?
./test_rescue_path 2>&1 | tee rescue-path-run.log
rc2=$?
set -e
echo "part1 exit=$rc1 part2 exit=$rc2"
exit $((rc1 + rc2))
