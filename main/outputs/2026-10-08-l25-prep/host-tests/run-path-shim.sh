#!/bin/sh
# run-path-shim.sh — battery-cheap host runner for lua-path-shim-test.lua.
# Agent GA, 2026-10-08 (Claude round-5 item 4).
#
# Builds a throwaway fixture tree shaped like the console modbridge root
# (/data/modbridge/roots/PPSA33286 -> $ROOT) incl. a real loadlist.txt with
# the 7-row console shape (agent-I MFA audit) plus one extra pinned row,
# runs the pure-Lua rule-contract test against it, tears the tree down.
# NO builds, NO console contact, NO engine.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TEST_FILE="$SCRIPT_DIR/lua-path-shim-test.lua"

LUA_BIN=""
for candidate in lua5.5 lua; do
    if command -v "$candidate" >/dev/null 2>&1; then
        LUA_BIN="$candidate"
        break
    fi
done
[ -n "$LUA_BIN" ] || { echo "no lua interpreter found (lua5.5/lua)"; exit 2; }

ROOT="$(mktemp -d /tmp/ga-path-shim-fixture.XXXXXX)"
trap 'rm -rf "$ROOT"' EXIT INT TERM

# ---- fixture tree (console loadlist shape: 7 rows + 1 extra pinned row) ----
mkdir -p "$ROOT/ue4ss/Mods/TownfallMiniMap/Scripts"
mkdir -p "$ROOT/ue4ss/Mods/OtherMod/Scripts"
mkdir -p "$ROOT/ue4ss/Mods/shared/UEHelpers"

cat > "$ROOT/loadlist.txt" <<'EOF'
title PPSA33286 / engine unreal / framework ue4ss-ps5 / mod TownfallItemSpawner 1.0.0
file config ue4ss/UE4SS-settings.ini 1
file config ue4ss/MemberVariableLayout.ini 1
file config ue4ss/VTableLayout.ini 1
file config ue4ss/PS5Pad.ini 1
file lua_mod ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua 1
file lua_mod ue4ss/Mods/TownfallItemSpawner/enabled.txt 0
file lua_mod ue4ss/Mods/shared/UEHelpers/UEHelpers.lua 1
file config ue4ss/Mods/TownfallMiniMap/pinned.cfg 5
EOF

printf 'x' > "$ROOT/ue4ss/UE4SS-settings.ini"
printf 'x' > "$ROOT/ue4ss/PS5Pad.ini"
printf 'enabled\n' > "$ROOT/ue4ss/Mods/TownfallMiniMap/enabled.txt"
printf '%s\n' '-- mod main' > "$ROOT/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua"
printf 'other\n' > "$ROOT/ue4ss/Mods/OtherMod/Scripts/main.lua"
printf '%s\n' '-- helpers' > "$ROOT/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua"
printf 'pinned' > "$ROOT/ue4ss/Mods/TownfallMiniMap/pinned.cfg"
printf 'round-trip-seed\n' > "$ROOT/ue4ss/Mods/TownfallMiniMap/settings.txt"
printf 'case-data\n' > "$ROOT/ue4ss/Mods/TownfallMiniMap/EXISTING.cfg"

"$LUA_BIN" "$TEST_FILE" --root "$ROOT"
