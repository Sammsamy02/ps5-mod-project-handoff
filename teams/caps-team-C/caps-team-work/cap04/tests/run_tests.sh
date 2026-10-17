#!/bin/sh
# cap #4 host tests (Mac-only). Usage: sh tests/run_tests.sh [--no-syntax]
# Output is saved next to this script as test-output.txt. Exit status = suite status.
HERE=$(cd "$(dirname "$0")" && pwd)
PYTHONDONTWRITEBYTECODE=1 python3 -B "$HERE/test_cap04.py" "$@" 2>&1 | tee "$HERE/test-output.txt"
exit $(tail -1 "$HERE/test-output.txt" | grep -q 'SUITE: PASS' && echo 0 || echo 1)
