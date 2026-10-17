#!/bin/sh
# Re-run the reparent plan on the integrator's real round-1..3 data (read-only inputs). Usage: run_plan.sh OUT.json [extra flags]
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
OBS=$WHAT/.superpowers/native-observations
EV=$WHAT/outputs/2026-10-08-l25-prep/evidence
F=$EV/cap12-fetched-20261013
X=$WHAT/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall
TOOLS=${TOOLS:-$(dirname "$0")/PATCH/tools}
OUT=$1; shift
exec python3 -I "$TOOLS/reparent_plan.py" --audit $EV/cap12-20261012/leon-material-audit-5hdr.json \
  --mod-utoc $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc \
  --mod-ucas $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas \
  --checked-headers $EV/cap12-20261012/round1-check.json $EV/cap12-20261012/round2-check.json $EV/cap12-20261012/round3-check.json \
  --fetched-dir $F/headers-r1 $F/headers-r2 $F/headers-r3 \
  --base-header $F/manifests/pakchunk0-ps5.container-header.bin $F/manifests/pakchunk1-ps5.container-header.bin $F/manifests/pakchunk10-ps5.container-header.bin $F/manifests/pakchunk11-ps5.container-header.bin $F/manifests/pakchunk12-ps5.container-header.bin \
  --asset-registry $X/AssetRegistry.bin \
  --library $F/manifests/Townfall_Chunk0.type8.bin $F/manifests/Global.type8.bin \
  --stinfo $X/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo $X/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo \
  "$@" --out "$OUT"
