#!/bin/sh
# run-all.sh — Team S1 host tests (Mac-only). Builds and runs the loader unit
# tests and the Townfall byte-for-byte equivalence proof. Exit nonzero on any
# failure or zero tests.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
NEW="$HERE/../new"
rc=0

echo "== regenerate the Townfall profile from today's compiled headers =="
python3 -I "$HERE/../gen/extract_townfall.py"

echo "== loader unit tests =="
c++ -std=c++23 -Wall -I "$NEW" "$HERE/profile_loader_test.cpp" -o /tmp/s1_loader_test
/tmp/s1_loader_test || rc=1

echo "== Townfall equivalence proof =="
python3 -I "$HERE/check_equivalence.py" || rc=1

exit "$rc"
