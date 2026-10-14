#!/bin/sh
# One compile at a time; refuse under 2 GB free (CLAUDE.md rule for this session).
set -eu
cd "$(dirname "$0")"
free_pages=$(vm_stat | awk '/Pages free/ {gsub("\\.","",$3); print $3}')
page=$(vm_stat | awk '/page size of/ {print $8}')
free_mb=$(( free_pages * page / 1048576 ))
echo "vm_stat free: ${free_mb} MB"
if [ "$free_mb" -lt 2048 ]; then echo "SKIP: under 2 GB free"; exit 3; fi
clang++ --version | head -1 > toolchain.txt; uname -m >> toolchain.txt
clang++ -std=c++20 -O2 -pthread -Wall -Wextra -o test_lockfix_O2 test_lockfix.cpp
clang++ -std=c++20 -O0 -g -pthread -Wall -Wextra -o test_lockfix_O0 test_lockfix.cpp
