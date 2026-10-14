#!/bin/sh
# run-prop-parity.sh — agent FR (Claude PRIORITY-1 parity revert, 2026-10-10)
#
# Battery-cheap host runner for prop-parity-test.lua: pure-Lua contract test
# for the userdata property-read shape restored in
# outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaUObject.cpp
# (push_nameproperty/push_textproperty/push_strproperty Get -> userdata).
# NO engine, NO console contact, NO builds — one lua5.5 process, then exit.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TEST_FILE="$SCRIPT_DIR/prop-parity-test.lua"

LUA_BIN=""
for candidate in lua5.5 lua; do
    if command -v "$candidate" >/dev/null 2>&1; then
        LUA_BIN="$candidate"
        break
    fi
done
if [ -z "$LUA_BIN" ]; then
    echo "run-prop-parity.sh: no lua interpreter found (looked for: lua5.5, lua)" >&2
    exit 2
fi

echo "interpreter: $LUA_BIN ($($LUA_BIN -v 2>&1 | head -1))"
echo "test file:   $TEST_FILE"
echo '---'
"$LUA_BIN" "$TEST_FILE"
