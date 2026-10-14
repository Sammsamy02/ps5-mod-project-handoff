#!/bin/sh
# run_all.sh — worker-lua-mod-smoke reproducible suite
# Usage: sh run_all.sh   (cwd = this harness dir)
# Bin/lua is Lua 5.4.7 compiled from the port's own LuaRaw deps
# (ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw)
# with lua_lock_shim.c replacing the Windows-only luauser.c lock hooks.
set -e
cd "$(dirname "$0")"
[ -x bin/lua ] || { echo "bin/lua missing — see REPORT.md build line"; exit 2; }
for p in env load f7-nosave f7-gate f7-save gt-gate; do
    echo "########## $p"
    ./bin/lua run_phases.lua "$p"
    echo "RC=$?"
done
