#!/bin/sh
# Real-data dry run of round 6 on Leon (Mac-only; read-only inputs; nothing touches the console).
# plan (invisible route v3 on the round-5 console plan) -> manifest -> paktx install --dry-run
# -> attribution -> mesh_slot_repoint on the 11 body meshes (builder check, not shipped) -> checks.
# Usage: real_leon_r6.sh OUT_DIR    (OUT_DIR must not exist)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$HERE/.." && pwd)
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
L25=$WHAT/outputs/2026-10-08-l25-prep
P5=$L25/evidence/cap12-launch-prep-20261013/reparent-plan-r5-textures.json
OUT=$1
[ -n "$OUT" ] && [ ! -e "$OUT" ] || { echo "real_leon_r6: OUT_DIR missing or exists"; exit 2; }
mkdir -p "$OUT"
export PYTHONDONTWRITEBYTECODE=1
T=$R/work/tools
echo "== plan"
TOOLS_DIR=$T ROUTE=$T/invisible_route.py sh "$R/run_invisible.sh" "$P5" "$OUT/reparent-plan-r6.json" --apply \
  --base-material-report "$OUT/base-material-report.json"
A=$(shasum -a 256 "$OUT/reparent-plan-r6.json" | cut -d' ' -f1)
B=$(shasum -a 256 "$R/evidence/plan-r6/reparent-plan-r6.json" | cut -d' ' -f1)
echo "plan sha256 $A (staged $B)"
[ "$A" = "$B" ] || { echo "real_leon_r6: FAIL plan differs from the staged evidence"; exit 1; }
echo "== manifest"
(cd "$R" && TOOLS=$T TILEREF=$L25/pak-intake sh "$R/run_derive.sh" evidence/plan-r6/reparent-plan-r6.json \
   "$OUT/manifest-r6" --accept-unproven-opaque --allow-partial)
echo "== paktx dry run"
python3 -I "$T/paktx.py" install --record "$L25/pak-overlay/records/townfall.record.json" \
  --mirror "$L25/pak-mirror-townfall-20261013" \
  --mod "$WHAT/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted" \
  --work "$OUT/leon12r6" --substitute "$OUT/manifest-r6/manifest.json" \
  --reparent-plan "$R/evidence/plan-r6/reparent-plan-r6.json" \
  --accept-decision accept-unproven-opaque --accept-decision allow-partial \
  --readers "$T" --tiling-reference "$L25/pak-intake" --dry-run > "$OUT/paktx-dry-run.txt" 2>&1 \
  || { tail -30 "$OUT/paktx-dry-run.txt"; echo "real_leon_r6: FAIL paktx dry run"; exit 1; }
grep -c "parent .* -> " "$OUT/paktx-dry-run.txt" | sed 's/^/paktx parent lines: /'
grep -c "DERIVED TEXTURE" "$OUT/paktx-dry-run.txt" | sed 's/^/paktx derived-texture lines: /'
echo "== attribution"
TOOLS=$T sh "$R/run_attribution.sh" "$R/evidence/plan-r6/reparent-plan-r6.json" "$OUT/attribution.json" "$OUT/attribution.md" \
  "$OUT/base-material-report.json"
echo "== mesh_slot_repoint (builder check on all 11 body meshes; not shipped)"
python3 -I "$HERE/real_repoint_r6.py" "$OUT/repoint"
echo "== checks"
python3 -I "$HERE/check_real_r6.py" "$OUT"
