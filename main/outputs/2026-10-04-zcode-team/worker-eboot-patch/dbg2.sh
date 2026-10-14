#!/usr/bin/env bash
set -uo pipefail
TEAM="$(dirname "$(cd "$(dirname "$0")" && pwd)")"
E1="$TEAM/worker-early-load/eboot-earlyload-A1.bin"
E2="$TEAM/worker-early-load/eboot-earlyload-A2.bin"
echo "hash A1..."
g="$(/usr/bin/shasum -a 256 "$E1" | awk '{print $1}')"
echo "A1=$g"
echo "after shasum: -f A2 -> $([[ -f "$E2" ]] && echo yes || echo no)"
