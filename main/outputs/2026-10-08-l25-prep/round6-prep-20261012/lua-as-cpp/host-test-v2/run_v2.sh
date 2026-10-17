#!/bin/sh
# run_v2.sh -- Lua-as-C++ host test, round 2 (Mac-only; lua-as-cpp track 2026-10-12).
# Builds the port's real Lua 5.4.7 sources THREE ways, one compile at a time:
#   c       -std=c11                                       (today's PS5 build)
#   cpp     C++ + -DLUA_COMPILED_AS_CPP, C++ linkage       (upstream option)
#   cppcabi C++ + -include lua_c_abi_prefix.h, C linkage   (staged proposal)
# and links three programs against each build:
#   lua_as_cpp_host_test  (round-1 test, unchanged checks)
#   replicas_test         (review replicas A/B + staged fix, coroutine yields)
#   c_caller              (plain-C caller, memory_lua.c shape)
# memcheck (strict "Pages free" >= 2 GB) runs before every compile and link.
# Exit 0 only if every expected result holds and the check count is non-zero.
set -u
HERE="$(cd "$(dirname -- "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../../.." && pwd)"
SPIKE="$WHAT/.superpowers/native-dependency-spike"
LUARAW="$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw"
PATCHED="$SPIKE/ue4ss-stdio-consumer-adaptation-1/patched"
MEMCHECK="$HERE/../tools/memcheck_strict.sh"
CC="${CC:-cc}"; CXX="${CXX:-c++}"
UPSTREAM="lapi lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc lgc linit llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio"
SOURCES=""; n=0; missing=0
for f in $UPSTREAM; do SOURCES="$SOURCES $LUARAW/src/$f.c"; n=$((n+1)); done
SOURCES="$SOURCES $PATCHED/lauxlib.c $PATCHED/liolib.c"; n=$((n+2))
for s in $SOURCES; do [ -f "$s" ] || { echo "MISSING $s"; missing=$((missing+1)); }; done
echo "lua sources: $n listed, $missing missing (expect 32, 0)"
[ "$n" -eq 32 ] && [ "$missing" -eq 0 ] || { echo "FAIL: source set incomplete"; exit 1; }
mem() { sh "$MEMCHECK" > /dev/null || { sh "$MEMCHECK"; echo "STOP: under 2 GB free, compile skipped"; exit 3; }; }
OUT="$HERE/RESULT-V2.txt"; : > "$OUT"
total_checks=0; bad=0

build() { # build <mode>
    mode="$1"; dir="$HERE/build-$mode"; rm -rf "$dir"; mkdir -p "$dir"
    case $mode in
      c)       LUACC="$CC";  LUAFLAGS="-std=c11 -O1"; DEF="" ;;
      cpp)     LUACC="$CXX"; LUAFLAGS="-x c++ -std=c++23 -DLUA_COMPILED_AS_CPP -Wno-deprecated-enum-enum-conversion -O1"; DEF="-DLUA_COMPILED_AS_CPP" ;;
      cppcabi) LUACC="$CXX"; LUAFLAGS="-x c++ -std=c++23 -include $HERE/lua_c_abi_prefix.h -Wno-deprecated-enum-enum-conversion -O1"; DEF="-DHOST_LUA_IS_CPP" ;;
    esac
    objs=""; k=0
    for s in $SOURCES "$HERE/luauser_counting_stub.c"; do
        o="$dir/$(basename "$s" .c).o"; mem
        $LUACC $LUAFLAGS -I "$LUARAW/include" -c "$s" -o "$o" 2> "$o.stderr" || { cat "$o.stderr"; echo "FAIL: compile $o"; exit 1; }
        objs="$objs $o"; k=$((k+1))
    done
    w=$(cat "$dir"/*.stderr | grep -c "warning:")
    echo "[$mode] compiled $k Lua objects, $w warnings" | tee -a "$OUT"
    nm "$dir/lapi.o" | grep -E " T " | grep -c " _lua_" | sed "s/^/[$mode] lapi.o plain C lua_* exports: /" | tee -a "$OUT"
    nm "$dir/lapi.o" | grep -E " T " | grep -c " __Z.*lua_" | sed "s/^/[$mode] lapi.o C++-mangled lua_* exports: /" | tee -a "$OUT"
    echo "[$mode] ldo.o refs: $(nm -u "$dir/ldo.o" | grep -E '___cxa_throw|___gxx_personality_v0|_(set|long)jmp$' | tr -s ' \n' ' ')" | tee -a "$OUT"
    for prog in lua_as_cpp_host_test replicas_test; do
        mem
        "$CXX" -std=c++23 -O1 $DEF -I "$LUARAW/include" "$HERE/$prog.cpp" $objs -o "$dir/$prog" 2> "$dir/$prog.link.stderr" \
            || { cat "$dir/$prog.link.stderr"; echo "FAIL: link $prog ($mode)"; exit 1; }
        "$dir/$prog" > "$dir/$prog.out" 2>&1; rc=$?
        c=$(grep -cE '^  (PASS|FAIL)  ' "$dir/$prog.out"); total_checks=$((total_checks+c))
        echo "[$mode] $prog: rc=$rc checks=$c" | tee -a "$OUT"; cat "$dir/$prog.out" >> "$OUT"
        [ "$rc" -eq 0 ] && [ "$c" -gt 0 ] || bad=$((bad+1))
    done
    # plain-C caller: compile as C, link with the C++ driver (libc++ for C++ Lua)
    mem; "$CC" -std=c11 -O1 -I "$LUARAW/include" -c "$HERE/c_caller.c" -o "$dir/c_caller.o" || { echo "FAIL: compile c_caller"; exit 1; }
    mem
    if "$CXX" "$dir/c_caller.o" $objs -o "$dir/c_caller" 2> "$dir/c_caller.link.stderr"; then
        "$dir/c_caller" > "$dir/c_caller.out" 2>&1; rc=$?
        c=$(grep -cE '^  (PASS|FAIL)  ' "$dir/c_caller.out"); total_checks=$((total_checks+c))
        echo "[$mode] c_caller: linked; rc=$rc checks=$c" | tee -a "$OUT"; cat "$dir/c_caller.out" >> "$OUT"
        if [ "$mode" = cpp ]; then echo "[$mode] UNEXPECTED: C caller linked against C++-linkage Lua" | tee -a "$OUT"; bad=$((bad+1)); fi
        [ "$mode" = cpp ] || { [ "$rc" -eq 0 ] && [ "$c" -gt 0 ] || bad=$((bad+1)); }
    else
        u=$(grep -c "_lua" "$dir/c_caller.link.stderr")
        echo "[$mode] c_caller: LINK FAILED ($u undefined-symbol lines mentioning _lua*): $(grep -oE '"_lua[A-Za-z_]*"' "$dir/c_caller.link.stderr" | sort -u | tr '\n' ' ')" | tee -a "$OUT"
        total_checks=$((total_checks+1))
        if [ "$mode" = cpp ]; then echo "  PASS  cpp build: a plain-C Lua caller cannot link (expected: this is why the staged design keeps C linkage)" | tee -a "$OUT"
        else echo "  FAIL  $mode build: plain-C caller must link" | tee -a "$OUT"; bad=$((bad+1)); fi
    fi
}
{ echo "lua-as-cpp host test v2 -- $(date -u +%Y-%m-%dT%H:%M:%SZ) -- Mac-only ($(uname -sm); $($CXX --version | head -1))"; } >> "$OUT"
sh "$MEMCHECK" | tee -a "$OUT" || exit 3
build c; build cpp; build cppcabi
# exported-name identity: cppcabi must export exactly the C build's public names
for m in c cppcabi; do nm -g --defined-only "$HERE/build-$m"/*.o 2>/dev/null | awk 'NF>=3{print $3}' | grep -E '^_(lua_|luaL_|luaopen_|LuaLock|LuaUnlock)' | sort -u > "$HERE/build-$m/public-names.txt"; done
pc=$(wc -l < "$HERE/build-c/public-names.txt" | tr -d ' ')
if [ "$pc" -gt 0 ] && cmp -s "$HERE/build-c/public-names.txt" "$HERE/build-cppcabi/public-names.txt"; then
    echo "  PASS  cppcabi exports exactly the C build's $pc public Lua names (lua_*/luaL_*/luaopen_*/LuaLock*)" | tee -a "$OUT"
else echo "  FAIL  public Lua names differ between c and cppcabi ($pc in C build)" | tee -a "$OUT"; bad=$((bad+1)); fi
total_checks=$((total_checks+1))
echo "TOTAL checks=$total_checks failed_programs_or_checks=$bad" | tee -a "$OUT"
[ "$total_checks" -gt 0 ] && [ "$bad" -eq 0 ] && { echo "PASS run_v2 (all three builds)" | tee -a "$OUT"; exit 0; }
echo "FAIL run_v2" | tee -a "$OUT"; exit 1
