#!/bin/sh
# Mutation check: each mutation breaks one rule in the extracted slices; the host test must
# then FAIL. Prints how many mutations were checked and exits 1 if any survived (or none ran).
set -u
FREE_PAGES=$(vm_stat | awk '/Pages free/ {gsub("\\.","",$3); print $3}')
FREE_MB=$((FREE_PAGES * 16384 / 1048576))
if [ "$FREE_MB" -lt 2048 ]; then echo "SKIP: under 2 GB free (${FREE_MB} MB)"; exit 3; fi
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
B="$HERE/build"
LUARAW=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw
[ -f "$B/liblua.a" ] || { echo "run build_and_run.sh first"; exit 1; }
python3 "$HERE/extract_core.py" "$B/slices" >/dev/null || exit 1
ran=0; survived=0
run_mut() {  # name file old new
  name=$1; file=$2; old=$3; new=$4
  d="$B/mut-$name"; rm -rf "$d"; mkdir -p "$d"; cp "$B"/slices/*.inc "$d"/
  OLD="$old" NEW="$new" python3 - "$d/$file" <<'PY' || { echo "MUTATION $name: anchor missing"; survived=$((survived+1)); return; }
import os, sys
p = sys.argv[1]; s = open(p).read(); o = os.environ['OLD']; n = os.environ['NEW']
if s.count(o) < 1: sys.exit(1)
open(p, 'w').write(s.replace(o, n, 1))
PY
  clang++ -std=c++20 -O1 -w -I"$d" -I"$LUARAW/include" "$HERE/b6_host_test.cpp" "$B/liblua.a" -o "$d/t" -pthread 2>"$d/compile.txt" || { echo "MUTATION $name: compile error (counts as killed): $(head -1 "$d/compile.txt")"; ran=$((ran+1)); return; }
  if "$d/t" >"$d/out.txt" 2>&1; then echo "MUTATION $name: SURVIVED (test still passes)"; survived=$((survived+1)); else echo "MUTATION $name: killed ($(grep -c 'FAIL' "$d/out.txt") failing lines)"; fi
  ran=$((ran+1))
}
run_mut no-depth-enter   regionB.inc 'if (LuaMadeSimple::diag_scope_enter() == 0) { holder_set(line); }' 'holder_set(line);'
run_mut no-fn-return     regionA.inc 'if (extra > 0) { diag_report(lua_state, "return", true, "", extra); }' ''
run_mut no-raise-check   regionA.inc 'if (extra > 0) { diag_report(lua_state, "raise", true, text, extra); }' ''
run_mut no-pcall-check   regionA.inc 'if (status != LUA_OK && tl_diag_depth > m_depth_in)' 'if (false)'
run_mut limiter-ignores  regionB.inc 'if (shown < limit)' 'if (true)'
run_mut base-not-set     regionA.inc 'tl_diag_frame_base = tl_diag_depth;
            ++tl_diag_call_nesting;' '++tl_diag_call_nesting;'
run_mut leak-always-logs regionB.inc 'if (last == 0 || now - last >= gap_ms)' 'if (true)'
run_mut holder-not-cleared regionB.inc 'if (LuaMadeSimple::diag_scope_depth() == 0) { holder_clear(); }' ''
run_mut no-trylock-first regionB.inc 'if (!m_mutex.try_lock()) { lock_slow(m_mutex, line); }' 'lock_slow(m_mutex, line);'
run_mut outermost-only   regionA.inc 'm_outermost(tl_diag_call_nesting == 0)' 'm_outermost(true)'
run_mut stuck-no-wait    regionB.inc 'if (since <= 0 || now - since <= kLockWaitLogMs) { continue; }' 'continue;'
echo "mutations run=$ran survived=$survived"
[ "$ran" -gt 0 ] && [ "$survived" -eq 0 ]
