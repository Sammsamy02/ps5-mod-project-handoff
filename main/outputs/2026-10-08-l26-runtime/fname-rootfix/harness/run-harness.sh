#!/bin/sh
# agent-EB harness receipt — build + run the ABI truth spec.
set -e
cd "$(dirname "$0")"
clang++ -std=c++20 -Wall -Wextra -O2 -o fname-abi-harness fname-abi-harness.cpp
./fname-abi-harness
