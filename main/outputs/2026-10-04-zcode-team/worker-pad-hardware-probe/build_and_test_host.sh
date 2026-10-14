#!/bin/zsh
# build_and_test_host.sh — host build + synthetic-event test for the pad
# probe logic (worker-pad-hardware-probe, team 2026-10-04). Same host
# toolchain convention as the prepared module's build_and_test.sh and the
# v3 lineage host checks: Apple clang + macOS SDK (the pinned LLVM distro
# carries no C++ stdlib; these are pure C anyway). ASan+UBSan. Only files
# from THIS worker dir are compiled. The PRX itself is built by
# build_pad_probe.py with the Prospero toolchain — never by this script.
set -euo pipefail
HERE="${0:A:h}"
CXX="/usr/bin/clang"
SDK="$(xcrun --sdk macosx --show-sdk-path)"

mkdir -p "$HERE/tests/build"
rm -f "$HERE/tests/build"/pad_host_a.jsonl "$HERE/tests/build"/pad_host_b.jsonl \
      "$HERE/tests/build"/pad_host_c.jsonl "$HERE/tests/build"/pad_host_d.jsonl \
      "$HERE/tests/build"/pad_host_e.jsonl "$HERE/tests/build"/pad_host_f.jsonl

"$CXX" -std=c17 -O1 -g -fsanitize=address,undefined -isysroot "$SDK" \
  -Wall -Wextra -Wno-deprecated-declarations \
  -I"$HERE/src" \
  "$HERE/src/record_sink.c" \
  "$HERE/src/ingame_libc.c" \
  "$HERE/src/pad_source.c" \
  "$HERE/src/pad_probe_core.c" \
  "$HERE/tests/host_shim.c" \
  "$HERE/tests/pad_probe_host.c" \
  -o "$HERE/tests/build/pad_probe_host"

(cd "$HERE/tests" && "./build/pad_probe_host")
python3 "$HERE/tests/run_host_checks.py"
