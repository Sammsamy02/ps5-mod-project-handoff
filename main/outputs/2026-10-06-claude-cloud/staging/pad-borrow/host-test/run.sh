#!/bin/bash
# Host test for the borrowed-handle pad backend. Needs clang++ (C++20).
set -euo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
OUT="$(mktemp -d)"
CXX="${CXX:-clang++}"
SRCS=("$H/port/PS5PadSource.cpp" "$H/../port/PS5PadNative.borrow.cpp" "$H/test_pad_borrow.cpp")
FLAGS=(-std=c++20 -O1 -g -Wall -Wextra -D__PROSPERO__ -I"$H/shims" -I"$H/port")
# ASan+UBSan when the toolchain ships the runtimes; plain build otherwise.
if ! "$CXX" "${FLAGS[@]}" -fsanitize=address,undefined "${SRCS[@]}" -o "$OUT/t" 2>/dev/null; then
  echo "(sanitizer runtime unavailable for $CXX; building without sanitizers)"
  "$CXX" "${FLAGS[@]}" "${SRCS[@]}" -o "$OUT/t"
fi
"$OUT/t"
