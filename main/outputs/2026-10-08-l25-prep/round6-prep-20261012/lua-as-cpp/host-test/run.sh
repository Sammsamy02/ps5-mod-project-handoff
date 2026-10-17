#!/bin/sh
# run.sh — Lua-as-C++ host test (Mac-only; lua-as-cpp track, 2026-10-12).
# Builds the port's real Lua 5.4.7 sources TWICE (as C, then as C++ with
# -DLUA_COMPILED_AS_CPP), links lua_as_cpp_host_test.cpp against each, runs
# both, and writes RESULT-C.txt, RESULT-CPP.txt and RESULT.txt next to it.
# One compile command at a time (-j1); tools/memcheck.sh runs before each
# compile and the script stops if under 2 GB is available.
# Exit 0 only if both builds PASS and a non-zero number of checks ran.
set -u
HERE="$(cd "$(dirname -- "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../../.." && pwd)"
SPIKE="$WHAT/.superpowers/native-dependency-spike"
LUARAW="$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw"
PATCHED="$SPIKE/ue4ss-stdio-consumer-adaptation-1/patched"
MEMCHECK="$HERE/../tools/memcheck.sh"
CC="${CC:-cc}"; CXX="${CXX:-c++}"

# The same Lua sources the PS5 build compiles (B4 receipt, 33 luaraw units):
# 30 upstream files + patched lauxlib.c/liolib.c; platform_locks.c is replaced
# by luauser_counting_stub.c (same recursive-mutex semantics + a depth probe).
UPSTREAM="lapi lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc lgc linit llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio"
SOURCES=""
n=0
for f in $UPSTREAM; do SOURCES="$SOURCES $LUARAW/src/$f.c"; n=$((n+1)); done
SOURCES="$SOURCES $PATCHED/lauxlib.c $PATCHED/liolib.c"; n=$((n+2))
missing=0
for s in $SOURCES; do [ -f "$s" ] || { echo "MISSING $s"; missing=$((missing+1)); }; done
echo "lua sources: $n listed, $missing missing (expect 32, 0)"
[ "$n" -eq 32 ] && [ "$missing" -eq 0 ] || { echo "FAIL: source set incomplete"; exit 1; }

compile() { # compile <out.o> <compiler and flags...> <source>
    out="$1"; shift
    sh "$MEMCHECK" > /dev/null || { sh "$MEMCHECK"; echo "STOP: memory below 2 GB, compile skipped"; exit 3; }
    "$@" -o "$out" || { echo "FAIL: compile $out"; exit 1; }
}

build() { # build <dir> <mode: c|cpp>
    dir="$HERE/$1"; mode="$2"
    rm -rf "$dir"; mkdir -p "$dir"
    if [ "$mode" = c ]; then
        LUAFLAGS="-std=c11 -O1"; LUACC="$CC"; DEF=""
    else
        LUAFLAGS="-x c++ -std=c++23 -DLUA_COMPILED_AS_CPP -Wno-deprecated-enum-enum-conversion -O1"; LUACC="$CXX"; DEF="-DLUA_COMPILED_AS_CPP"
    fi
    objs=""; k=0
    for s in $SOURCES "$HERE/luauser_counting_stub.c"; do
        o="$dir/$(basename "$s" .c).o"
        compile "$o" $LUACC $LUAFLAGS -I "$LUARAW/include" -c "$s"
        objs="$objs $o"; k=$((k+1))
    done
    echo "$mode build: compiled $k objects"
    sh "$MEMCHECK" > /dev/null || { echo "STOP: memory below 2 GB"; exit 3; }
    "$CXX" -std=c++23 -O1 $DEF -I "$LUARAW/include" "$HERE/lua_as_cpp_host_test.cpp" $objs -o "$dir/lua_as_cpp_host_test" \
        || { echo "FAIL: link $mode"; exit 1; }
}

sh "$MEMCHECK" || exit 3
build build-c c
build build-cpp cpp

# Linkage evidence: Lua API symbols are C names in the C build, C++-mangled in the C++ build.
nm "$HERE/build-c/lapi.o" | grep -c " T _lua_" > "$HERE/build-c/lua-api-c-names.count"
nm "$HERE/build-cpp/lapi.o" | grep -c " T __Z.*lua_" > "$HERE/build-cpp/lua-api-mangled.count"
nm -u "$HERE/build-c/ldo.o" | grep -E "_(set|long)jmp$" > "$HERE/build-c/ldo-jmp-imports.txt"
nm -u "$HERE/build-cpp/ldo.o" | grep -E "___cxa_throw|___gxx_personality_v0|_(set|long)jmp$" > "$HERE/build-cpp/ldo-eh-imports.txt"

"$HERE/build-c/lua_as_cpp_host_test" > "$HERE/RESULT-C.txt" 2>&1; rc_c=$?
"$HERE/build-cpp/lua_as_cpp_host_test" > "$HERE/RESULT-CPP.txt" 2>&1; rc_cpp=$?
checks_c=$(grep -cE '^  (PASS|FAIL)  ' "$HERE/RESULT-C.txt")
checks_cpp=$(grep -cE '^  (PASS|FAIL)  ' "$HERE/RESULT-CPP.txt")
{
    echo "lua-as-cpp host test — $(date -u +%Y-%m-%dT%H:%M:%SZ) — Mac-only ($(uname -sm); $($CXX --version | head -1))"
    echo "C   build: rc=$rc_c checks=$checks_c  (ldo.o imports: $(tr '\n' ' ' < "$HERE/build-c/ldo-jmp-imports.txt"))"
    echo "C++ build: rc=$rc_cpp checks=$checks_cpp  (ldo.o imports: $(tr '\n' ' ' < "$HERE/build-cpp/ldo-eh-imports.txt"))"
    echo "lapi.o exported lua_* names: C build $(cat "$HERE/build-c/lua-api-c-names.count") plain C names; C++ build $(cat "$HERE/build-cpp/lua-api-mangled.count") C++-mangled names"
    echo "--- C build ---"; cat "$HERE/RESULT-C.txt"
    echo "--- C++ build ---"; cat "$HERE/RESULT-CPP.txt"
} > "$HERE/RESULT.txt"
cat "$HERE/RESULT.txt"
if [ "$checks_c" -eq 0 ] || [ "$checks_cpp" -eq 0 ]; then echo "FAIL: zero checks ran"; exit 1; fi
[ "$rc_c" -eq 0 ] && [ "$rc_cpp" -eq 0 ] && echo "PASS both builds ($checks_c + $checks_cpp checks)" && exit 0
echo "FAIL (rc_c=$rc_c rc_cpp=$rc_cpp)"; exit 1
