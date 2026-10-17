#!/bin/sh
# run_host_test.sh - Mac-only host test of the staged r8 title derivation.
# 1. memory guard (skip when < 2 GB available: free+inactive+speculative+purgeable)
# 2. make_fixtures.py (exits 1 on zero cases)
# 3. ONE compile (single cc invocation, -j1 by construction) of the test, which
#    #includes the STAGED patched entry (never the build's copy)
# 4. run; the test exits 1 on any failed check or zero cases
# 4b. a SECOND compile (after the first finished) of the same test with
#     -DHOST_UNBOUND_PREAD: sceKernelPread unbound (NULL); the entry must refuse
#     before any read. Exits 1 on failure or if the variant is not really NULL.
# No console contact. Writes only under this directory (fixtures/, out/).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
LINEAGE=$WHAT/outputs/2026-10-08-l26-package-A/lineage

avail_gb=$(vm_stat | awk '/Pages free|Pages inactive|Pages speculative|Pages purgeable/ {gsub("\\.","",$NF); s+=$NF} END {printf "%.2f", s*16384/1e9}')
free_gb=$(vm_stat | awk '/Pages free/ {gsub("\\.","",$NF); printf "%.2f", $NF*16384/1e9}')
echo "HOST-TEST: memory available ${avail_gb} GB (strictly free ${free_gb} GB)"
# "free" = reclaimable without paging (free+inactive+speculative+purgeable),
# cross-checked with macOS memory_pressure's system-wide free percentage.
mp_free=$(memory_pressure 2>/dev/null | awk -F': ' '/free percentage/ {gsub("%","",$2); print $2}')
echo "HOST-TEST: memory_pressure system-wide free ${mp_free:-unknown}%"
if [ "$(echo "$avail_gb < 2" | bc)" = "1" ] || [ -z "$mp_free" ] || [ "$mp_free" -lt 15 ]; then
  echo "HOST-TEST: SKIP - under 2 GB available or memory_pressure free under 15% (or unknown)"; exit 2
fi

python3 -I "$HERE/make_fixtures.py"
mkdir -p "$HERE/out"
cc -std=c99 -O1 -g -Wall -Wextra -Wno-unused-parameter \
   -I "$LINEAGE/lifecycle" -I "$LINEAGE" \
   "$HERE/test_title_derive.c" -o "$HERE/out/test_title_derive"
"$HERE/out/test_title_derive" "$HERE"
cc -std=c99 -O1 -g -Wall -Wextra -Wno-unused-parameter -DHOST_UNBOUND_PREAD \
   -I "$LINEAGE/lifecycle" -I "$LINEAGE" \
   "$HERE/test_title_derive.c" -Wl,-U,_mbt_host_unbound_pread -o "$HERE/out/test_unbound_pread"
"$HERE/out/test_unbound_pread" "$HERE"
