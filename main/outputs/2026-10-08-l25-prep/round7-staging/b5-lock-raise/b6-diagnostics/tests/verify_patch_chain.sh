#!/bin/sh
# Applies, in a scratch tree: candidate files -> B5 diff -> b6.diff, and proves the result is
# byte-identical to work/. Nothing in the candidate tree is touched (copies only).
# usage: verify_patch_chain.sh SCRATCH_DIR
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd); D=$(dirname "$HERE")
SCRATCH=$1; rm -rf "$SCRATCH"; mkdir -p "$SCRATCH"
WT=/WORKSPACE/.codex/worktrees/translation-core/what
CAND=outputs/2026-10-08-l26-runtime/typescan-r4/candidate
B5=$WT/outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/sites/final-b5.diff
# LuaMod.cpp comes from the PRISTINE base copy (the live candidate file may already carry final-b5); its sha is checked.
BASEC=$WT/outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/sites/base-candidate/$CAND
for rel in src/LuaMod.cpp src/LuaPathShim.cpp src/LuaMadeSimple/LuaMadeSimple.cpp include/LuaMadeSimple/LuaMadeSimple.hpp; do
  mkdir -p "$SCRATCH/$CAND/$(dirname "$rel")"
  if [ "$rel" = src/LuaMod.cpp ]; then cp "$BASEC/$rel" "$SCRATCH/$CAND/$rel"; else cp "$WT/$CAND/$rel" "$SCRATCH/$CAND/$rel"; fi
done
[ "$(shasum -a 256 "$SCRATCH/$CAND/src/LuaMod.cpp" | cut -d' ' -f1)" = 4d6bc63b7a90a8026a9c918ee6bee36c2d4eaa450220be82e8071f5427bfde9a ] || { echo "pristine base sha mismatch"; exit 1; }
cd "$SCRATCH"
patch -p1 --dry-run < "$B5" >/dev/null && patch -p1 < "$B5" >/dev/null && echo "B5 applied"
patch -p1 --dry-run < "$D/b6.diff" > dry.txt && patch -p1 < "$D/b6.diff" > applied.txt && echo "b6.diff applied after B5: $(grep -c '^patching file' applied.txt) files"
n=0; bad=0
for rel in src/LuaMod.cpp src/LuaPathShim.cpp src/LuaMadeSimple/LuaMadeSimple.cpp include/LuaMadeSimple/LuaMadeSimple.hpp; do
  n=$((n+1))
  if cmp -s "$CAND/$rel" "$D/work/$rel"; then echo "identical to work/: $rel"; else echo "DIFFERS: $rel"; bad=$((bad+1)); fi
done
grep -E 'fuzz|offset|FAILED|rej' dry.txt applied.txt || echo "no fuzz / offsets / rejects"
echo "files compared=$n differing=$bad"
[ "$n" -gt 0 ] && [ "$bad" -eq 0 ]
