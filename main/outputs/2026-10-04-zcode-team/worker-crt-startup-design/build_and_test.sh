#!/bin/sh
# build_and_test.sh — reproducible host build + run for the CRT startup prototype.
# Host-only C11; no engine, no Unity, no build.py involvement.
set -eu
cd "$(dirname "$0")"
CC=${CC:-cc}
OUT=crt_startup_test.bin

"$CC" -std=c11 -Wall -Wextra -Werror -pedantic -o "$OUT" crt_startup.c test_crt_startup.c
./"$OUT"
status=$?
rm -f "$OUT"
exit $status
