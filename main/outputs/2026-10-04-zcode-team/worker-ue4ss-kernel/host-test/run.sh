#!/bin/zsh
# run.sh — build + run the worker-ue4ss-kernel host tests (2026-10-06).
# Host only: mocked kekcall transport; no console contact, no game claims.
# Toolchain: AppleClang on this Mac; -fsanitize=address,undefined per the
# team's host-1 recipe.
set -euo pipe_fail
cd "$(dirname "$0")"

CXX=${CXX:-clang++}
CXXFLAGS=( -std=c++20 -Wall -Wextra -g -fsanitize=address,undefined -fno-sanitize-recover=all -I.. )

run_one() {
  local name=$1
  echo "== build $name =="
  $CXX "${CXXFLAGS[@]}" -o "$name.out" "$name.cpp"
  echo "== run $name =="
  "./$name.out"
}

run_one facade_test
run_one pool_gap_test
run_one pad_adapter_test

echo
echo "host-test: ALL SUITES PASSED"
