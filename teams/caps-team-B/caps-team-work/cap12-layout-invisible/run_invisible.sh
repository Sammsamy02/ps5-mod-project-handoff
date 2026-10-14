#!/bin/sh
# Run the invisible route on the real Leon data (read-only inputs). Usage: run_invisible.sh PLAN_IN.json OUT.json [--apply]
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
OBS=$WHAT/.superpowers/native-observations
EV=$WHAT/outputs/2026-10-08-l25-prep/evidence
F=$EV/cap12-fetched-20261013
X=$WHAT/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS_DIR=${TOOLS_DIR:-/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-fulllook/PATCH/tools}
IN=$1; OUT=$2; shift 2
exec python3 -I "$HERE/PATCH/tools/invisible_route.py" --tools-dir "$TOOLS_DIR" --plan "$IN" \
  --audit $EV/cap12-20261012/leon-material-audit-5hdr.json \
  --mod-utoc $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc \
  --mod-ucas $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas \
  --checked-headers $EV/cap12-20261012/round1-check.json $EV/cap12-20261012/round2-check.json $EV/cap12-20261012/round3-check.json \
  --fetched-dir $F/headers-r1 $F/headers-r2 $F/headers-r3 \
  --base-header $F/manifests/pakchunk0-ps5.container-header.bin $F/manifests/pakchunk1-ps5.container-header.bin $F/manifests/pakchunk10-ps5.container-header.bin $F/manifests/pakchunk11-ps5.container-header.bin $F/manifests/pakchunk12-ps5.container-header.bin \
  --asset-registry $X/AssetRegistry.bin \
  --library $F/manifests/Townfall_Chunk0.type8.bin $F/manifests/Global.type8.bin \
  --stinfo $X/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo $X/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo \
  --script-utoc $EV/console-pulls-20261013-paks2/global.utoc --script-ucas $EV/console-pulls-20261013-paks2/global.ucas \
  "$@" --out "$OUT"
