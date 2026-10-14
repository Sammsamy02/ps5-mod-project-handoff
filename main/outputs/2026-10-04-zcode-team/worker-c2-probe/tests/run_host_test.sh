#!/bin/bash
# run_host_test.sh — HOST LOGIC TEST runner (worker-c2-probe, team 2026-10-04).
# Builds the host harness against the REAL probe sources and validates three
# gating modes. Offline only; no console, no network.
set -euo pipefail
cd "$(dirname "$0")/.."   # worker dir

mkdir -p tests/generated tests/out
python3 tests/gen_host_config.py
python3 tests/make_patch.py tests/out/sim_patch.txt

CC="${CC:-cc}"
# -I tests/host_compat shadows <sys/user.h> with the SDK-shaped struct;
# -include host_prelude.h supplies the missing KERN_PROC_VMMAP constant.
$CC -std=c11 -g -O0 -Wall -Wextra \
    -I tests -I tests/generated -I src -I tests/host_compat \
    -include tests/host_prelude.h \
    -o tests/out/host_test \
    tests/host_test.c \
    src/record_sink.c src/ingame_libc.c src/vmmap_walk.c src/mapscan.c

echo "== mode 1: trusted map =="
tests/out/host_test trusted tests/out/trusted.jsonl tests/out/sim_patch.txt
echo "== mode 2: untrusted map =="
tests/out/host_test untrusted tests/out/untrusted.jsonl tests/out/sim_patch.txt
echo "== mode 3: covered_not_readable rodata =="
tests/out/host_test covered_not_readable tests/out/nonreadable.jsonl tests/out/sim_patch.txt

python3 tests/check_host_output.py tests/out
echo "HOST TEST COMPLETE"
