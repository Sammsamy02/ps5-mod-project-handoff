#!/usr/bin/env bash
set -uo pipefail
TEAM="$(dirname "$(cd "$(dirname "$0")" && pwd)")"
E2="$TEAM/worker-early-load/eboot-earlyload-A2.bin"
echo "TEAM=[$TEAM]"
echo "E2=[$E2]"
echo "test -f: $(test -f "$E2" && echo yes || echo no)"
echo "[[ -f ]]: $([[ -f "$E2" ]] && echo yes || echo no)"
ls -la "$E2" 2>&1 | head -1
stat -f "stat: %z bytes" "$E2" 2>&1
