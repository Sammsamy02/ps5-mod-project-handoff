#!/bin/sh
# Hair (special-model) derived-mask check on the real data (read-only inputs).
# Usage: run_hair_check.sh PLAN.json OUT.json FETCH_PLAN.json [--mask-chunks DIR]
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
EV=$WHAT/outputs/2026-10-08-l25-prep/evidence
F=$EV/cap12-fetched-20261013
X=$WHAT/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall
TOOLS=${TOOLS:-$(cd "$(dirname "$0")" && pwd)/PATCH/tools}
PLAN=$1; OUT=$2; FP=$3; shift 3
exec python3 -I "$TOOLS/hair_mask_check.py" --plan "$PLAN" \
  --checked-headers $EV/cap12-20261012/round1-check.json $EV/cap12-20261012/round2-check.json $EV/cap12-20261012/round3-check.json \
  --fetched-dir $F/headers-r1 $F/headers-r2 $F/headers-r3 \
  --asset-registry $X/AssetRegistry.bin --mirror $WHAT/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013 \
  --fetch-plan-out "$FP" --out "$OUT" "$@"
