#!/bin/sh
# Builds the real Lua 5.4 C sources as C (setjmp/longjmp, the port's configuration) and the
# B6 host test, one compile at a time, and runs it. Skips (exit 3) when under 2 GB is free.
# Output: tests/build/RESULT.txt
set -eu
set -o pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LUARAW=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw
B="$HERE/build"
mkdir -p "$B/obj"
FREE_PAGES=$(vm_stat | awk '/Pages free/ {gsub("\\.","",$3); print $3}')
FREE_MB=$((FREE_PAGES * 16384 / 1048576))
echo "free memory: ${FREE_MB} MB"
if [ "$FREE_MB" -lt 2048 ]; then echo "SKIP: under 2 GB free"; exit 3; fi
python3 "$HERE/extract_core.py" "$B/slices"
if [ ! -f "$B/liblua.a" ]; then
  for f in "$LUARAW"/src/*.c; do
    case "$(basename "$f")" in lua.c|luac.c|luauser.c) continue;; esac
    clang -std=c11 -O1 -I"$LUARAW/include" -c "$f" -o "$B/obj/$(basename "$f" .c).o"
  done
  clang -std=c11 -c "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/host-tests/b2-error-wrapper/luauser_host_stub.c" -I"$LUARAW/include" -o "$B/obj/luauser_stub.o"
  ar rcs "$B/liblua.a" "$B"/obj/*.o
fi
clang++ -std=c++20 -O1 -g -Wall -Wextra -Werror=format -Wno-unused-function -Wno-unused-parameter \
  -I"$B/slices" -I"$LUARAW/include" "$HERE/b6_host_test.cpp" "$B/liblua.a" -o "$B/b6_host_test" -pthread
"$B/b6_host_test" | tee "$B/RESULT.txt"
clang++ -std=c++20 -O1 -g -Wall -Wextra -Werror=format -Wno-unused-function -Wno-unused-parameter -D__PROSPERO__=1 \
  -I"$B/slices" -I"$LUARAW/include" "$HERE/b6_pump_test.cpp" "$B/liblua.a" -o "$B/b6_pump_test" -pthread
"$B/b6_pump_test" | tee "$B/RESULT-pump.txt"
clang++ -std=c++20 -O1 -g -Wall -Wextra -Werror=format -Wno-unused-function -Wno-unused-parameter \
  -I"$B/slices" -I"$LUARAW/include" "$HERE/b6_shim_test.cpp" "$B/liblua.a" -o "$B/b6_shim_test"
"$B/b6_shim_test" | tee "$B/RESULT-shim.txt"
