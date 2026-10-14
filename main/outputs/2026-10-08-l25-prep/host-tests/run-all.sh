#!/bin/sh
# run-all.sh — host-tests orchestrator (agent GA, 2026-10-08).
# No host-tests/run-all existed when the path-shim work landed (only
# run-prop-parity.sh); this runs every pure-Lua host contract test in the
# directory so future tests have one entry point.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
rc=0
"$SCRIPT_DIR/run-prop-parity.sh" || rc=1
"$SCRIPT_DIR/run-path-shim.sh" || rc=1
exit "$rc"
