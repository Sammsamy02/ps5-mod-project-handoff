#!/bin/sh
# Self-test of the B6 use of the ONE gate (../gate/lock_raise_gate.py, via lock_raise_gate_b6.py) and b6_source_gate.py: they must FAIL on bad input and on
# nothing-scanned, and PASS on good input. Prints how many cases it ran; exits 1 on any surprise.
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd); D=$(dirname "$HERE")
T=$(mktemp -d /LOCAL_SCRATCH
cases=0; bad=0
expect() {  # name want_exit got_exit
  cases=$((cases+1))
  if [ "$2" = "$3" ]; then echo "  ok   $1 (exit $3)"; else echo "  BAD  $1: wanted exit $2 got $3"; bad=$((bad+1)); fi
}
G="python3 -B $D/lock_raise_gate_b6.py"
export B6_LOCK_GATE=$(CDPATH= cd -- "$D/../gate" && pwd)/lock_raise_gate.py
cat > $T/good.cpp <<'X'
void f() { b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__}; int x = 1; }
void other(const LuaMadeSimple::Lua& lua) { lua.throw_error("outside any lock"); }
X
cat > $T/raise.cpp <<'X'
void f(const LuaMadeSimple::Lua& lua) { b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__};
  lua.throw_error("boom"); }
X
cat > $T/rawraise.cpp <<'X'
void f(const LuaMadeSimple::Lua& lua) { std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};
  lua.throw_error("boom"); }
X
cat > $T/raw.cpp <<'X'
void f() { std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex}; }
void other(const LuaMadeSimple::Lua& lua) { lua.throw_error("outside any lock"); }
X
cat > $T/banned.cpp <<'X'
void f(const LuaMadeSimple::Lua& lua) { b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__}; auto mod = get_mod_ref(lua); }
void other(const LuaMadeSimple::Lua& lua) { lua.throw_error("outside any lock"); }
X
cat > $T/cxxtwin.cpp <<'X'
void f(const LuaMadeSimple::Lua& lua) { b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__}; auto mod = get_mod_ref_cxx(lua); }
void other(const LuaMadeSimple::Lua& lua) { lua.throw_error("outside any lock"); }
X
cat > $T/unknown.cpp <<'X'
void f(const LuaMadeSimple::Lua& lua) { NewLockWrap guard{LuaMod::m_thread_actions_mutex}; lua.throw_error("boom"); }
X
: > $T/empty.cpp
$G $T/good.cpp >/dev/null;               expect "tracked, clean" 0 $?
$G --require-tracked $T/good.cpp >/dev/null; expect "tracked, clean, require-tracked" 0 $?
$G $T/raise.cpp >/dev/null;              expect "raise inside tracked scope" 1 $?
$G $T/rawraise.cpp >/dev/null;           expect "raise inside raw scope" 1 $?
$G $T/raw.cpp >/dev/null;                expect "raw scope allowed without flag" 0 $?
$G --require-tracked $T/raw.cpp >/dev/null; expect "raw scope refused with --require-tracked" 1 $?
$G $T/banned.cpp >/dev/null;             expect "get_mod_ref under a tracked lock (hard mode)" 1 $?
$G $T/cxxtwin.cpp >/dev/null;            expect "get_mod_ref_cxx under a tracked lock is allowed" 0 $?
$G $T/unknown.cpp >/dev/null;            expect "unknown RAII type built from the mutex = failed gate" 2 $?
$G $T/empty.cpp >/dev/null;              expect "zero scopes = failed gate" 2 $?
$G >/dev/null;                           expect "zero files = failed gate" 2 $?
$G $T/missing.cpp >/dev/null;            expect "unreadable file = failed gate" 2 $?
python3 -B $HERE/b6_source_gate.py >/dev/null; expect "source gate on the staged tree" 0 $?
# tamper: a copy of the tree with an added include must fail the include check
cp -R $D/work $T/work; cp -R $D/base $T/base; cp $D/b6.diff $T/; mkdir -p $T/tests; cp $HERE/b6_source_gate.py $T/tests/
sed -i '' '1i\
#include <deque>
' $T/work/src/LuaMod.cpp
python3 -B $T/tests/b6_source_gate.py >/dev/null; expect "source gate with a new #include" 1 $?
cp $D/work/src/LuaMod.cpp $T/work/src/LuaMod.cpp
sed -i '' 's/b6diag::ActionsLock guard{LuaMod::m_thread_actions_mutex, __LINE__};/std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex};/' $T/work/src/LuaMod.cpp
python3 -B $T/tests/b6_source_gate.py >/dev/null; expect "source gate with raw lock scopes back" 1 $?
rm -rf $T
echo "gate self-test: cases=$cases bad=$bad"
[ "$cases" -gt 0 ] && [ "$bad" -eq 0 ]
