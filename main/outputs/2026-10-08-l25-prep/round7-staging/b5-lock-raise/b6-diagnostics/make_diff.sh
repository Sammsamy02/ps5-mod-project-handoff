#!/bin/sh
# Rebuilds work/ from base/ (the final-b5-applied copy) with the two apply scripts, then writes
# b6.diff (against the final-b5-applied files, paths relative to the worktree root like the B5 diff:
# apply with `patch -p1` AFTER ../sites/final-b5.diff).
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE"
python3 parts/apply_luamod.py >/dev/null
python3 parts/apply_shim.py >/dev/null
CAND=outputs/2026-10-08-l26-runtime/typescan-r4/candidate
: > b6.diff
{
echo "# B6 diagnostics (staged, not applied, not built). Applies ON TOP OF ../sites/final-b5.diff (final B5: 15 raise sites + 2 get_mod_ref sites)."
echo "# Base files: final-b5-applied copies of candidate/src/LuaMod.cpp (sha afaf5caa..., i.e. B5 only), LuaPathShim.cpp, LuaMadeSimple/LuaMadeSimple.cpp, include/LuaMadeSimple/LuaMadeSimple.hpp"
echo "# Apply from the worktree root, after final-b5.diff: patch -p1 < b6.diff"
} >> b6.diff
for rel in src/LuaMadeSimple/LuaMadeSimple.cpp include/LuaMadeSimple/LuaMadeSimple.hpp src/LuaMod.cpp src/LuaPathShim.cpp; do
  src_rel=$rel
  diff -u -L "a/$CAND/$rel" -L "b/$CAND/$rel" "base/$src_rel" "work/$src_rel" >> b6.diff || true
done
echo "b6.diff: $(wc -l < b6.diff) lines, $(grep -c '^@@' b6.diff) hunks"
