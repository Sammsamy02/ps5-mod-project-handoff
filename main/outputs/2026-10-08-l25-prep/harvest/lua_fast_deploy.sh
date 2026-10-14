#!/bin/bash
# Lua-only fast deploy: upload a zzdiag script + update loadlist, NO PRX rebuild.
# Usage: lua_fast_deploy.sh <main.lua-path> [label]
set -u
CONSOLE=ps5-host-not-configured.invalid
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
FTPR="ftp://$CONSOLE:2121"
SCRIPT_PATH="${1:?usage: lua_fast_deploy.sh <main.lua> [label]}"
LABEL="${2:-manual-$(date +%H%M%S)}"

# Preconditions
EB=$(curl -s --max-time 8 "http://$CONSOLE:8084/processes_list" | grep -c eboot || true)
[ "$EB" != "0" ] && { echo "GAME RUNNING — close first"; exit 1; }

# Upload script
echo "== uploading $SCRIPT_PATH =="
curl -s --max-time 15 -T "$SCRIPT_PATH" "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua"
SIZE=$(wc -c < "$SCRIPT_PATH" | tr -d ' ')

# Update loadlist (fetch, modify zzdiag row, push)
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/loadlist.txt" -o /tmp/lua-fast-loadlist.txt
python3 -c "
lines = open('/tmp/lua-fast-loadlist.txt').read().rstrip('\n').split('\n')
out = []
for l in lines:
    if 'zzdiag/Scripts/main.lua' in l:
        p = l.split('\t'); p[-1] = str($SIZE); l = '\t'.join(p)
    out.append(l)
open('/tmp/lua-fast-loadlist-new.txt','w').write('\n'.join(out) + '\n')
"
curl -s --max-time 15 -T /tmp/lua-fast-loadlist-new.txt "$FTPR/data/modbridge/roots/PPSA33286/loadlist.txt"

# Verify both
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua" | cmp "$SCRIPT_PATH" - && echo "SCRIPT VERIFIED"
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/loadlist.txt" | grep "zzdiag/Scripts/main.lua.*$SIZE" && echo "LOADLIST OK"

# Arm ploader + tap
P=$(curl -s --max-time 8 "http://$CONSOLE:8084/processes_list" | grep -c payload.elf || echo 0)
[ "$P" = "0" ] && { curl -s --max-time 20 "http://$CONSOLE:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf" > /dev/null; sleep 5; }
curl -s --max-time 8 "http://$CONSOLE:8084/processes_list" | grep -c payload.elf
# EVIDENCE-RETENTION (agent-HA 2026-10-10): klog taps land DURABLE, never
# /tmp (Claude's law; harvest.py searches this dir first).
TAPDIR="$WHAT/outputs/2026-10-08-l25-prep/evidence/runtime/l25-variant-A"
mkdir -p "$TAPDIR"
nohup python3 $WHAT/outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/klog_capture.py --out-dir "$TAPDIR" --window-seconds 7200 > /dev/null 2>&1 &

echo ""
echo "=== LAUNCH NOW ($LABEL) ==="
echo "Time saved: ~15 min (no PRX rebuild)"
