#!/bin/sh
# run.sh — run the OSD toolkit host scenarios (S1..S5) under lua5.5.
# Exit code 0 iff every assertion passes. Agent HL, 2026-10-10.
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LUA="${LUA:-lua5.5}"
exec "$LUA" "$HERE/host-test.lua"
