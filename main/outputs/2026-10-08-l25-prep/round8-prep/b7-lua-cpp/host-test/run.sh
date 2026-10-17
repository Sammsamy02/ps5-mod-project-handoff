#!/bin/sh
# run.sh -- B7 host test (Mac-only; round 8, b7-lua-cpp track). No console contact.
# 1. extracts the REAL runtime code (extract_real.py, read-only on the candidate and staged trees)
# 2. builds the port's real Lua 5.4.7 sources TWO ways, one compile at a time:
#      c        -std=c11                                  (today's PS5 build)
#      cppcabi  C++ + -include lua_c_abi_prefix.h         (staged B7: throw/catch, C names)
# 3. links b7_host_test.cpp against each and runs it; compares the "T:" transcript lines across builds.
# memcheck_strict (vm_stat "Pages free" >= 2 GB) runs before every compile/link; exit 3 = skipped.
# Exit 0 only if both runs pass, the transcripts agree where they must, and the check count is > 0.
set -u
HERE="$(cd "$(dirname -- "$0")" && pwd)"
WHAT="$(cd "$HERE/../../../../.." && pwd)"
SPIKE="$WHAT/.superpowers/native-dependency-spike"
LUARAW="$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw"
PATCHED="$SPIKE/ue4ss-stdio-consumer-adaptation-1/patched"
CC="${CC:-cc}"; CXX="${CXX:-c++}"
mem() { sh "$HERE/memcheck_strict.sh" > /dev/null || { sh "$HERE/memcheck_strict.sh"; echo "STOP: under 2 GB free, compile skipped"; exit 3; }; }
UPSTREAM="lapi lbaselib lcode lcorolib lctype ldblib ldebug ldo ldump lfunc lgc linit llex lmathlib lmem loadlib lobject lopcodes loslib lparser lstate lstring lstrlib ltable ltablib ltm lundump lutf8lib lvm lzio"
SOURCES=""; n=0; missing=0
for f in $UPSTREAM; do SOURCES="$SOURCES $LUARAW/src/$f.c"; n=$((n+1)); done
SOURCES="$SOURCES $PATCHED/lauxlib.c $PATCHED/liolib.c"; n=$((n+2))
for s in $SOURCES; do [ -f "$s" ] || { echo "MISSING $s"; missing=$((missing+1)); }; done
echo "lua sources: $n listed, $missing missing (expect 32, 0)"
[ "$n" -eq 32 ] && [ "$missing" -eq 0 ] || { echo "FAIL: source set incomplete"; exit 1; }
[ "$(shasum -a 256 "$HERE/lua_c_abi_prefix.h" | cut -d' ' -f1)" = "96b3f261366d8a131627a265463d0d5b9cce6851da406eb8ca9685387fc638ea" ] || { echo "FAIL: prefix header sha drift"; exit 1; }
python3 -I "$HERE/../tools/make_b7_runtime_patch.py" --base candidate > "$HERE/make_patch.log" 2>&1 || { cat "$HERE/make_patch.log"; echo "FAIL: staged patch generation"; exit 1; }
python3 -I "$HERE/extract_real.py" > "$HERE/extract_real.log" 2>&1 || { cat "$HERE/extract_real.log"; echo "FAIL: extract_real"; exit 1; }
tail -1 "$HERE/extract_real.log"
OUT="$HERE/RESULT.tmp"; : > "$OUT"   # promoted to RESULT.txt only when the run completes (a memory SKIP, exit 3, keeps the last result)
{ echo "b7 host test -- $(date -u +%Y-%m-%dT%H:%M:%SZ) -- Mac-only ($(uname -sm); $($CXX --version | head -1))"; sh "$HERE/memcheck_strict.sh"; } | tee -a "$OUT"

build() { # build <mode>
    mode="$1"; dir="$HERE/build-$mode"; rm -rf "$dir"; mkdir -p "$dir"
    case $mode in
      c)       LUACC="$CC";  LUAFLAGS="-std=c11 -O1"; DEF="" ;;
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
    mem
    "$CXX" -std=c++23 -O1 $DEF -I "$LUARAW/include" -I "$HERE" "$HERE/b7_host_test.cpp" $objs -o "$dir/b7_host_test" 2> "$dir/link.stderr" \
        || { cat "$dir/link.stderr"; echo "FAIL: compile/link b7_host_test ($mode)"; exit 1; }
    w2=$(grep -c "warning:" "$dir/link.stderr")
    echo "[$mode] b7_host_test built, $w2 warnings" | tee -a "$OUT"
    "$dir/b7_host_test" > "$dir/b7_host_test.out" 2>&1; rc=$?
    c=$(grep -cE '^  (PASS|FAIL)  ' "$dir/b7_host_test.out")
    echo "[$mode] b7_host_test: rc=$rc checks=$c" | tee -a "$OUT"
    cat "$dir/b7_host_test.out" >> "$OUT"
    grep '^T: ' "$dir/b7_host_test.out" | sort > "$dir/transcript.txt"
    total_checks=$((total_checks+c))
    [ "$rc" -eq 0 ] && [ "$c" -gt 0 ] || bad=$((bad+1))
}
total_checks=0; bad=0
build c; build cppcabi

echo "" | tee -a "$OUT"
# transcript lines that must be identical across the two Lua builds: the Lua-visible error text and statuses
for key in "b2.cur." "b2.fixed." "destructors" "uf.fixed." "child.nested-protected" "child.raw-wrapped"; do
    grep "^T: $key" "$HERE/build-c/transcript.txt" | sort > "$HERE/build-c/t.key"; grep "^T: $key" "$HERE/build-cppcabi/transcript.txt" | sort > "$HERE/build-cppcabi/t.key"
    na=$(wc -l < "$HERE/build-c/t.key" | tr -d ' ')
    total_checks=$((total_checks+1))
    if [ "$na" -gt 0 ] && cmp -s "$HERE/build-c/t.key" "$HERE/build-cppcabi/t.key"; then
        echo "  PASS  cross-build transcript '$key' identical ($na lines)" | tee -a "$OUT"
    else
        echo "  FAIL  cross-build transcript '$key' differs or is empty (lines in C build: $na)" | tee -a "$OUT"; bad=$((bad+1))
        diff "$HERE/build-c/t.key" "$HERE/build-cppcabi/t.key" | head -10 | tee -a "$OUT"
    fi
done
echo "TOTAL checks=$total_checks failed_programs_or_checks=$bad" | tee -a "$OUT"
[ "$total_checks" -gt 0 ] && [ "$bad" -eq 0 ] && { echo "PASS run.sh (Lua as C and Lua as C++)" | tee -a "$OUT"; mv "$OUT" "$HERE/RESULT.txt"; exit 0; }
echo "FAIL run.sh" | tee -a "$OUT"; mv "$OUT" "$HERE/RESULT-FAILED.txt"; exit 1
