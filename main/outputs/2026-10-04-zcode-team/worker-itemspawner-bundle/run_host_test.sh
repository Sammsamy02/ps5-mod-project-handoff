#!/bin/sh
# run_host_test.sh — worker-itemspawner-bundle, 2026-10-04.
# Compiles the host test against worker-7's vendored modforge_activation and
# runs it over THIS worker's actual bundle plus three mutated negative roots
# (scratch copies under host-test/, this worker's directory only).
# Nothing outside this directory is written; no mod Lua is executed.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
WS="/WORKSPACE/.codex/worktrees/translation-core/what"
TEAM="$WS/outputs/2026-10-04-zcode-team"
VEND="$TEAM/worker-7-installer/vendored"
PRIOR_SETTINGS="$TEAM/worker-activation-bundle/build/stage-root/ue4ss/UE4SS-settings.ini"
INTAKE="$WS/.superpowers/native-observations/item-spawner-real-mod-intake-1"
UEHELPERS="$WS/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/shared/UEHelpers/UEHelpers.lua"
TESTDIR="$HERE/host-test"

mkdir -p "$TESTDIR"

# Negative roots: scratch copies of OUR bundle with one mutation each.
rm -rf "$TESTDIR/root-badtitle" "$TESTDIR/root-badsize" "$TESTDIR/root-noll"
cp -R "$HERE/bundle" "$TESTDIR/root-badtitle"
sed -i '' 's/^title	PPSA33286$/title	PPSA00000/' "$TESTDIR/root-badtitle/loadlist.txt"
cp -R "$HERE/bundle" "$TESTDIR/root-badsize"
head -c 21414 "$TESTDIR/root-badsize/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua" \
  > "$TESTDIR/root-badsize/main.lua.tmp"
mv "$TESTDIR/root-badsize/main.lua.tmp" \
   "$TESTDIR/root-badsize/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua"
cp -R "$HERE/bundle" "$TESTDIR/root-noll"
rm "$TESTDIR/root-noll/loadlist.txt"

echo "== compile =="
cc -std=c11 -O2 -Wall -Wextra -I"$VEND" \
  "$HERE/test_activation.c" "$VEND/modforge_activation.c" \
  -o "$TESTDIR/test_activation"

echo "== run =="
"$TESTDIR/test_activation" \
  "$HERE/bundle" \
  "$INTAKE/member-00000000.bin" \
  "$INTAKE/member-00000001.bin" \
  "$UEHELPERS" \
  "$PRIOR_SETTINGS" \
  "$TESTDIR/root-badtitle" \
  "$TESTDIR/root-badsize" \
  "$TESTDIR/root-noll"
