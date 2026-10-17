#!/bin/bash
# run-all.sh — run every staged ladder mod (12) + the TownfallItemSpawner
# console copy (13th, KNOWN-GOOD calibration subject) through runner.lua,
# in both io modes (port hypothesis: io/os absent; shim hypothesis: present).
#
# Agent FU, 2026-10-10. Mac-local Lua only; no console contact.
# Results land in runs/<name>.<io-off|io-on>.log

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
WHAT="$HERE/../../.."            # .../translation-core/what
OUT="$HERE/runs"
LUA="${LUA:-lua5.5}"
SHARED="$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/shared"
TREEB="$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods"
TREEA="$WHAT/.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1"
PREP="$WHAT/outputs/2026-10-08-l25-prep"

mkdir -p "$OUT"

run() { # run <name> <main.lua> [shared]
    local name="$1"; local main="$2"; local shared="${3:-$SHARED}"
    for mode in off on; do
        local flag=""
        [ "$mode" = on ] && flag="--with-io"
        local log="$OUT/$name.io-$mode.log"
        "$LUA" "$HERE/runner.lua" --main "$main" --name "$name" \
              --shared "$shared" $flag > "$log" 2>&1
        local v="$(grep -m1 '^verdict:' "$log" | cut -d' ' -f2-)"
        printf '%-28s io-%-3s %s\n' "$name" "$mode" "${v:-<crashed>}"
    done
}

# 13th subject FIRST: calibration law — the KNOWN-GOOD console mod must be
# LOADS-CLEAN, otherwise the stub is wrong and everything after is invalid.
run "TownfallItemSpawner-console-copy" "$PREP/evidence/townfall-main.lua.console-copy" "$TREEB/../Mods/shared"

# The 12 ladder mods (MANIFEST.md order)
run "01-LineTraceMod"            "$TREEB/LineTraceMod/Scripts/main.lua"
run "02-CheatManagerEnablerMod"  "$TREEB/CheatManagerEnablerMod/Scripts/main.lua"
run "03-ConsoleEnablerMod"       "$TREEB/ConsoleEnablerMod/Scripts/main.lua"
run "04-ConsoleCommandsMod"      "$TREEB/ConsoleCommandsMod/Scripts/main.lua"
run "05-SplitScreenMod"          "$TREEB/SplitScreenMod/Scripts/main.lua"
run "06-TownfallMiniMap"         "$TREEA/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua"
run "07-TownfallModMenu"         "$TREEA/mod-menu-v6/TownfallModMenu/Scripts/main.lua"
run "08-Keybinds"                "$TREEB/Keybinds/Scripts/main.lua"
run "09-BPML_GenericFunctions"   "$TREEB/BPML_GenericFunctions/Scripts/main.lua"
run "10-BPModLoaderMod"          "$TREEA/ue4ss-v1/ue4ss/Mods/BPModLoaderMod/Scripts/main.lua"
run "11-ActorDumperMod"          "$TREEB/ActorDumperMod/Scripts/main.lua"
run "12-jsbLuaProfilerMod"       "$TREEB/jsbLuaProfilerMod/Scripts/main.lua"

echo "raw logs: $OUT/"
