#!/bin/zsh
# Host-side harness tests (macOS clang, no engine code, no network, no console).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
clang++ -std=c++20 -Wall -Wextra -DNATIVE_QUERY_HARNESS_HOST_TEST \
  "$HERE/../src/NativeQueryHarness.cpp" "$HERE/host_test.cpp" \
  -o "$HERE/host_test_bin" -pthread
"$HERE/host_test_bin"
