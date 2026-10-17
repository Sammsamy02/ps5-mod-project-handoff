#!/bin/sh
# Make the derived-packages manifest (MIC rows + derived textures) from a round-5 plan on the real data
# (read-only inputs). Usage: run_derive.sh PLAN.json OUT_DIR [extra flags, e.g. --accept-unproven-opaque]
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
OBS=$WHAT/.superpowers/native-observations
EV=$WHAT/outputs/2026-10-08-l25-prep/evidence
F=$EV/cap12-fetched-20261013
TOOLS=${TOOLS:-$(cd "$(dirname "$0")" && pwd)/PATCH/tools}
# Team B's pinned oracle (texture_tiling_probe.py + tile_reference.py); after integration: L25/pak-intake
TILEREF=${TILEREF:-/WORKSPACE/.codex/worktrees/translation-core/caps-team-B/caps-team-work/cap12-layout-invisible/PATCH/tools}
PLAN=$1; OUT=$2; shift 2
exec python3 -I "$TOOLS/derive_substitutes.py" --plan "$PLAN" \
  --mod-utoc $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc \
  --mod-ucas $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas \
  --mirror $WHAT/outputs/2026-10-08-l25-prep/pak-mirror-townfall-20261013 \
  --headers $F/headers-r1 $F/headers-r2 $F/headers-r3 \
  --base-store $F/manifests/pakchunk0-ps5.container-header.bin $F/manifests/pakchunk1-ps5.container-header.bin \
     $F/manifests/pakchunk10-ps5.container-header.bin $F/manifests/pakchunk11-ps5.container-header.bin \
     $F/manifests/pakchunk12-ps5.container-header.bin \
  --material-fallback "$TOOLS" --tiling-reference "$TILEREF" "$@" --out "$OUT"
