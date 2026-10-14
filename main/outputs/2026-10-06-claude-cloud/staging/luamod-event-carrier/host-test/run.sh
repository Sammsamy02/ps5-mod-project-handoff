#!/bin/bash
# Host test: event-carrier functions extracted verbatim from the staged LuaMod.cpp.
set -euo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
OUT="$(mktemp -d)"
python3 "$H/extract.py" "$H/../LuaMod.cpp" "$OUT/extracted.inc"
CXX="${CXX:-g++}"
EXTRA=""; command -v brew >/dev/null 2>&1 && EXTRA="-I$(brew --prefix)/include -L$(brew --prefix)/lib"
FLAGS=(-std=c++20 -O1 -g -Wall -Wextra -Wno-unused-function $EXTRA -I"$H" -I"$OUT")
if ! "$CXX" "${FLAGS[@]}" -fsanitize=address,undefined "$H/test_event_carrier.cpp" -lfmt -o "$OUT/t" 2>/dev/null; then
  echo "(sanitizers unavailable; plain build)"; "$CXX" "${FLAGS[@]}" "$H/test_event_carrier.cpp" -lfmt -o "$OUT/t"
fi
ASAN_OPTIONS=detect_leaks=0 "$OUT/t"   # fixtures are intentionally never freed
