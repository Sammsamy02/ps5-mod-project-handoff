#!/bin/sh
# Team J: re-create every evidence file of this folder from the game's own files and the unchanged
# PC mod files (Mac-only, read-only on every input; writes only into ./evidence and ./fetch-plans).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
W=/WORKSPACE/.codex/worktrees/translation-core/what
P=$W/outputs/2026-10-08-l25-prep
M=$P/pak-mirror-townfall-20261013
G=$P/evidence/console-pulls-20261013-paks2
F=$P/evidence/cap08-base-fetched-20261013
LEON=$W/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P
Q=$W/.superpowers/native-dependency-spike/iostore-three-archive-composition-1
XT=$W/.superpowers/native-dependency-spike/unreal-config-pak-composition-1/trial-1/independent-extraction
REG=$XT/Townfall/AssetRegistry.bin
REPAK=$W/.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/build/aarch64-apple-darwin/release/repak
DEC=/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/PATCH/tools
SAMPLES=${SAMPLES:-}   # optional: directory with the fetched F1 samples (fetch-plans/F1-type-samples.json)
E=$HERE/evidence
mkdir -p "$E"
PY="python3 -I"
TOCS="$M/pakchunk0-ps5.utoc $M/pakchunk1-ps5.utoc $M/pakchunk10-ps5.utoc $M/pakchunk11-ps5.utoc $M/pakchunk12-ps5.utoc"
HDRS="$W/.superpowers/native-observations/townfall-original-container-header-1/container-header.bin \
 $F/pakchunk1-ps5/87608d0ea4cfc0f400000006.bin $F/pakchunk10-ps5/5b160bf69b8b988d00000006.bin \
 $F/pakchunk11-ps5/f1635ac7b843b33400000006.bin $F/pakchunk12-ps5/d656cdde5c61998000000006.bin"
EXTRA_PS5=""
[ -n "$SAMPLES" ] && EXTRA_PS5="--ps5-dir $SAMPLES"

# 0. the console build's shader format-platform, from the game's own pak entry names (ShaderTypeInfo-*.stinfo)
TARGET=$("$REPAK" list "$G/pakchunk11-ps5.pak" | sed -n 's/.*ShaderTypeInfo-.*-\([A-Z0-9_]*-[A-Z0-9_]*\)\.stinfo$/\1/p' | sort -u)
[ "$(printf '%s\n' "$TARGET" | grep -c .)" = 1 ] || { echo "run_all: FAIL (no single shader target in the game's paks: '$TARGET')"; exit 1; }
echo "target (from the game's .stinfo entry): $TARGET" | tee "$E/target.txt"

# 1. PC vs console, same class (evidence for the per-type verdicts)
$PY "$HERE/tools/pair_compare.py" \
  --pc "$LEON.utoc" "$LEON.ucas" \
  --pc "$Q/quarantine-2/member-00000001.bin" "$Q/quarantine-2/member-00000000.bin" \
  --pc "$Q/quarantine-1/member-00000007.bin" "$Q/quarantine-1/member-00000006.bin" \
  --ps5-dir "$F" --ps5-dir "$P/evidence/cap12-layout-fetched-20261013" $EXTRA_PS5 \
  --base-toc $TOCS --script-utoc "$G/global.utoc" --script-ucas "$G/global.ucas" --target "$TARGET" \
  --hash-receipt "$E/ps5-sample-iohash-receipt.json" \
  --mesh-decoder "$DEC" --out "$E/pair-compare.json" | tee "$E/pair-compare.txt"

# 2. classify each PC mod (and the game's own DLC container as a control sample)
cls() {
  $PY "$HERE/tools/classify_mod.py" --mod "$1" "$2" "$3" \
    --script-utoc "$G/global.utoc" --script-ucas "$G/global.ucas" --base-toc $TOCS --base-header $HDRS \
    --registry "$REG" --pair-evidence "$E/pair-compare.json" --target "$TARGET" \
    --title "$4" --out "$E/$5.json" --md "$E/$5.md"
}
cls "$LEON.utoc" "$LEON.ucas" "$LEON.pak" "Leon (PC outfit mod)" classify-leon | tee "$E/classify.txt"
cls "$Q/quarantine-2/member-00000001.bin" "$Q/quarantine-2/member-00000000.bin" "$Q/quarantine-2/member-00000002.bin" \
  "Third-person view + character (PC mod)" classify-thirdperson | tee -a "$E/classify.txt"
cls "$Q/quarantine-1/member-00000007.bin" "$Q/quarantine-1/member-00000006.bin" "$Q/quarantine-1/member-00000005.bin" \
  "Movement speed 2x (PC data-table mod)" classify-movespeed | tee -a "$E/classify.txt"
cls "$M/pakchunk10-ps5.utoc" "$M/pakchunk10-ps5.ucas" - \
  "Control: the game's own console DLC container" classify-control-game-pakchunk10 | tee -a "$E/classify.txt"

# 3. fetch plan for console samples of the classes without evidence yet (read-only; the integrator runs it).
#    First rows: the game's own copies of the assets the non-Leon PC samples replace (taken from the reports).
MUST=$($PY -c 'import json,sys
for f in sys.argv[1:]:
    for p in json.load(open(f))["packages"]:
        if p.get("overrides_game"): print(p["package"])' "$E/classify-movespeed.json" "$E/classify-thirdperson.json" | sort -u)
$PY "$HERE/tools/make_sample_plan.py" plan --toc "$M/pakchunk0-ps5.utoc" --registry "$REG" --must $MUST \
  --out "$HERE/fetch-plans/F1-type-samples.json" | tee "$E/fetch-plan.txt"

# 4. localization / text files: format of the game's own console copies (classic paks)
{
  echo "== game text paks (pakchunk1-ps5.pak) =="; "$REPAK" list "$G/pakchunk1-ps5.pak"
  for f in Townfall/Content/Townfall/Dialoc/Dialoc_Base.dloc Townfall/Content/PS5/Dialoc/Dialoc_PS5.dloc; do
    echo "== $f: size and first bytes =="
    "$REPAK" get "$G/pakchunk1-ps5.pak" "$f" | wc -c
    { "$REPAK" get "$G/pakchunk1-ps5.pak" "$f" 2>/dev/null || true; } | head -c 96 | od -c | head -4
  done
  echo "== Engine.locres magic (game's console main pak, earlier extraction) =="
  head -c 17 "$XT/Engine/Content/Localization/Engine/en/Engine.locres" | od -An -tx1
  echo "expected FTextLocalizationResource magic GUID: 0e 14 74 75 67 4a 03 fc 4a 15 90 9d c3 37 7f 1b"
} > "$E/localization-check.txt" 2>&1
echo "run_all: DONE"
