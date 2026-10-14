#!/bin/bash
# Team I2 host tests (Mac-only). No console, no runtime tree edits, no builds of
# the PRX. Compiles the prototype together with the REAL R16 input units
# (read-only include/source paths below) and runs:
#   1. tests/check_table.py   table vs real KeyDef enum, SDL (HID), public EKeys
#   2. -Werror syntax pass    our three sources
#   3. test_kbm               unit + integration tests (real Handler, real PS5PadSource)
#   4. test_threads (TSan)    two-thread stress (game thread vs update thread)
#   5. tests/test_probe.lua   host test of the console Lua probe (zzkbprobe)
#      tests/test_bind.lua    host test of the K2 bind-check mod (zzkbbind)
# Every step prints its counts; a step that checks nothing fails.
set -euo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
K="$(cd "$H/.." && pwd)"
OUT="$H/out"
mkdir -p "$OUT"
CXX="${CXX:-clang++}"

W=/WORKSPACE/.codex/worktrees/translation-core/what
PORT="$W/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port"
STRINGS="$W/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include"
HANDLER_CPP="$W/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/input-handler-coexist/Handler.cpp"
PAD_DIR="$W/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port"
PAD_CPP="$W/outputs/2026-10-08-l22-runtime/candidate/PS5PadSource.cpp"
KEYDEF_HPP="$PORT/first/include/Input/KeyDef.hpp"
KEYDEF_CPP="$PORT/first/src/Input/KeyDef.cpp"

echo "== inputs (read-only)"
for f in "$HANDLER_CPP" "$PAD_CPP" "$PAD_DIR/PS5PadSource.hpp" "$KEYDEF_HPP" "$KEYDEF_CPP"; do
  shasum -a 256 "$f" | awk '{print "  " substr($1,1,16) "  "}' | tr -d '\n'; echo "${f#$W/}"
done

echo "== 1. table gate"
python3 -I "$H/tests/check_table.py" --keydef-hpp "$KEYDEF_HPP" --keydef-cpp "$KEYDEF_CPP" \
  --table "$H/src/KeyboardMouseMap.cpp" --sdl "$K/refs/sdl-2.30.0/SDL_scancode.h" | tee "$OUT/check_table.txt"

# FORCE_U16 = the port's owned host profile (2-byte CharType); FMT header-only.
# R16 headers are -isystem (their own warnings are not ours); our code is -Wall -Wextra -Werror.
FLAGS=(-std=c++23 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-copy-with-user-provided-copy -Wno-sign-compare
       -DFORCE_U16 -DFMT_HEADER_ONLY -DHAS_INPUT -DRC_INPUT_BUILD_STATIC
       -I"$H/include" -I"$H/tests/shims" -isystem "$PORT/first/include" -isystem "$STRINGS" -isystem "$PORT/fmt/include" -isystem "$PAD_DIR")
echo "== 2. our sources compile warning-free (-Werror)"
for f in "$H/src/KeyboardMouseMap.cpp" "$H/src/EngineKeySource.cpp" "$H/src/CompositeInputSource.cpp"; do
  "$CXX" -std=c++23 -fsyntax-only -Wall -Wextra -Wpedantic -Werror -Wno-deprecated-copy-with-user-provided-copy -DFORCE_U16 -DHAS_INPUT -DRC_INPUT_BUILD_STATIC \
    -I"$H/include" -I"$H/tests/shims" -isystem "$PORT/first/include" -isystem "$STRINGS" "$f" && echo "  ok ${f#$H/}"
done
COMMON=("$H/src/KeyboardMouseMap.cpp" "$H/src/EngineKeySource.cpp" "$H/src/CompositeInputSource.cpp"
        "$HANDLER_CPP" "$KEYDEF_CPP")

echo "== 3. unit + integration tests (ASan+UBSan)"
"$CXX" "${FLAGS[@]}" -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  "${COMMON[@]}" "$PAD_CPP" "$H/tests/test_kbm.cpp" -o "$OUT/test_kbm"
"$OUT/test_kbm" --dump-table "$OUT/keymap.tsv" | tee "$OUT/test_kbm.txt"
echo "  table rows dumped: $(($(wc -l < "$OUT/keymap.tsv") - 1)) -> out/keymap.tsv"

echo "== 4. two-thread stress (ThreadSanitizer)"
"$CXX" "${FLAGS[@]}" -fsanitize=thread "${COMMON[@]}" "$H/tests/test_threads.cpp" -o "$OUT/test_threads"
TSAN_OPTIONS="halt_on_error=1" "$OUT/test_threads" | tee "$OUT/test_threads.txt"

echo "== 5. Lua probe + bind-check host tests"
lua "$H/tests/test_probe.lua" "$K/probe/zzkbprobe/Scripts/main.lua" | tee "$OUT/test_probe.txt"
lua "$H/tests/test_bind.lua" "$K/probe/zzkbbind/Scripts/main.lua" | tee "$OUT/test_bind.txt"

echo "== ALL PASS"
