#!/bin/bash
# K2 host tests (Mac-only, Team I2). No console, no runtime-tree edits, no PRX build or package.
# Compiles the SHIPPING header new/include/Platform/KbmEngineKeys.hpp together with the REAL R16 input units
# (read-only paths below) and runs:
#   1. check_table.py         key table in the shipping header vs real KeyDef enum/spellings, SDL HID, public EKeys
#   2. -Werror syntax pass    the shipping header at -Wall -Wextra -Wpedantic
#   3. test_k2 (ASan+UBSan)   unit + integration + reader reflection checks (fake reflection model)
#   4. test_threads_k2 (TSan) two real threads (game thread vs update thread) + shutdown
#   5. test_bind.lua          the K2 test mod zzkbbind (deploy copy in ../../probe/zzkbbind)
# Every step prints its counts; a step that checks nothing fails (set -e + explicit zero checks).
set -euo pipefail
T="$(cd "$(dirname "$0")" && pwd)"
K2="$(cd "$T/.." && pwd)"
KM="$(cd "$K2/.." && pwd)"
OUT="$T/out"
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
HDR="$K2/new/include/Platform/KbmEngineKeys.hpp"

echo "== inputs (read-only)"
for f in "$HDR" "$HANDLER_CPP" "$PAD_CPP" "$PAD_DIR/PS5PadSource.hpp" "$KEYDEF_HPP" "$KEYDEF_CPP" "$PORT/first/include/Input/RingBuffer.hpp"; do
  printf '  %s  %s\n' "$(shasum -a 256 "$f" | cut -c1-16)" "${f#$W/}"
done

echo "== 1. table gate (shipping header)"
python3 -I "$T/check_table.py" --keydef-hpp "$KEYDEF_HPP" --keydef-cpp "$KEYDEF_CPP" \
  --table "$HDR" --sdl "$KM/refs/sdl-2.30.0/SDL_scancode.h" | tee "$OUT/check_table.txt"

INC=(-I"$K2/new/include" -I"$T/shims" -isystem "$PORT/first/include" -isystem "$STRINGS" -isystem "$PORT/fmt/include" -isystem "$PAD_DIR")
DEFS=(-DFORCE_U16 -DFMT_HEADER_ONLY -DHAS_INPUT -DRC_INPUT_BUILD_STATIC)

echo "== 2. shipping header compiles warning-free (-Wall -Wextra -Wpedantic -Werror)"
printf '#include <Platform/KbmEngineKeys.hpp>\nint main() { return RC::Platform::Kbm::key_table().size() == 163 ? 0 : 1; }\n' > "$OUT/header_only.cpp"
"$CXX" -std=c++23 -fsyntax-only -Wall -Wextra -Wpedantic -Werror -Wno-deprecated-copy-with-user-provided-copy "${DEFS[@]}" "${INC[@]}" "$OUT/header_only.cpp"
echo "  ok new/include/Platform/KbmEngineKeys.hpp"

FLAGS=(-std=c++23 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-copy-with-user-provided-copy -Wno-sign-compare "${DEFS[@]}" "${INC[@]}")
COMMON=("$HANDLER_CPP" "$KEYDEF_CPP")

echo "== 3. unit + integration + reader reflection tests (ASan+UBSan)"
"$CXX" "${FLAGS[@]}" -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  "${COMMON[@]}" "$PAD_CPP" "$T/test_k2.cpp" -o "$OUT/test_k2"
"$OUT/test_k2" --dump-table "$OUT/keymap.tsv" | tee "$OUT/test_k2.txt"
grep -q "^tests=[1-9][0-9]* .* failures=0$" "$OUT/test_k2.txt"

echo "== 4. two-thread stress (ThreadSanitizer)"
"$CXX" "${FLAGS[@]}" -fsanitize=thread "${COMMON[@]}" "$T/test_threads_k2.cpp" -o "$OUT/test_threads_k2"
TSAN_OPTIONS="halt_on_error=1" "$OUT/test_threads_k2" | tee "$OUT/test_threads_k2.txt"

echo "== 5. K2 test mod (zzkbbind) host test"
lua "$KM/prototype/tests/test_bind.lua" "$KM/probe/zzkbbind/Scripts/main.lua" | tee "$OUT/test_bind.txt"
printf '  zzkbbind files: main.lua %s bytes sha256 %s; enabled.txt %s bytes\n' \
  "$(wc -c < "$KM/probe/zzkbbind/Scripts/main.lua" | tr -d ' ')" "$(shasum -a 256 "$KM/probe/zzkbbind/Scripts/main.lua" | cut -c1-16)" \
  "$(wc -c < "$KM/probe/zzkbbind/enabled.txt" | tr -d ' ')"

echo "== ALL PASS"
