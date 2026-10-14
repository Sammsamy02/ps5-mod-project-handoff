#!/bin/sh
# Per-part checker attribution on the real Leon data (read-only inputs).
# Usage: run_attribution.sh PLAN.json OUT.json OUT.md [BASE_REPORT.json]
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
OBS=$WHAT/.superpowers/native-observations
EV=$WHAT/outputs/2026-10-08-l25-prep/evidence
F=$EV/cap12-fetched-20261013
X=$WHAT/.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Townfall
TOOLS=${TOOLS:-$(cd "$(dirname "$0")" && pwd)/work/tools}
exec python3 -I "$TOOLS/attribution.py" --plan "$1" --out "$2" --md "$3" ${4:+--base-report "$4"} --tools-dir "$TOOLS" \
  --mod-utoc $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc \
  --mod-ucas $OBS/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas \
  --asset-registry $X/AssetRegistry.bin \
  --base-header $F/manifests/pakchunk0-ps5.container-header.bin $F/manifests/pakchunk1-ps5.container-header.bin $F/manifests/pakchunk10-ps5.container-header.bin $F/manifests/pakchunk11-ps5.container-header.bin $F/manifests/pakchunk12-ps5.container-header.bin \
  --script-utoc $EV/console-pulls-20261013-paks2/global.utoc --script-ucas $EV/console-pulls-20261013-paks2/global.ucas
