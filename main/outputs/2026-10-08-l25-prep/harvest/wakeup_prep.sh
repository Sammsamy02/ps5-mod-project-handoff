#!/bin/bash
# WAKE-UP PREP — run when the PS5 is awake, before the user's decisive launch.
# One command completes the console state left partial when the PS5 slept:
#   PRX 5ea2c845 (verified-query build) is ALREADY UPLOADED (prep steps 1-5 passed).
#   This script: verifies daemons + no game, ensures cache-on settings,
#   uploads zzdiag v8 (auto-prober) + loadlist, arms ploader, starts klog tap.
# EVIDENCE-RETENTION (agent-HA 2026-10-10; Claude's law: nothing evidential
# lives in /tmp — see outputs/2026-10-08-l25-prep/EVIDENCE-RETENTION.md):
#   * verification readbacks (decrypt-off PRX check, settings readback) now
#     land DURABLE at outputs/2026-10-08-l25-prep/evidence/runtime/wake/;
#   * the klog tap + its stderr now land DURABLE at
#     .../evidence/runtime/l25-variant-A/ (harvest.py searches it FIRST);
#   * pinned upload sources resolve DURABLE-FIRST (evidence/ + zzdiag-v8/);
#     legacy /tmp copies are fallback only.
set -u
FTPR="ftp://ps5-host-not-configured.invalid:2121"
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
WR="$WHAT/outputs/2026-10-08-l25-prep"
EV="$WR/evidence/runtime/wake"               # durable readback receipts
TAPDIR="$WR/evidence/runtime/l25-variant-A"  # durable klog tap out-dir
mkdir -p "$EV" "$TAPDIR"
# durable-first pinned sources (byte-identical to legacy /tmp copies; shas
# pinned in evidence/tmp-preserved-20261010/MANIFEST.json)
pick() { for c in "$@"; do [ -f "$c" ] && { echo "$c"; return; }; done; echo "$1"; }
SETTINGS_CACHEON=$(pick "$WR/evidence/l25-settings-cacheon-1947.ini" /tmp/l25-settings-cacheon-1947.ini)
ZZV8_MAIN=$(pick "$WR/zzdiag-v8/main.lua" /tmp/zzdiag-v8/main.lua)
ZZV8_LOADLIST=$(pick "$WR/zzdiag-v8/loadlist-zzdiag-v8.txt" /tmp/loadlist-v8.txt)

echo "== 1. console state =="
EB=$(curl -s --max-time 8 "http://ps5-host-not-configured.invalid:8084/processes_list" | grep -c eboot || true)
[ "$EB" != "0" ] && { echo "GAME RUNNING - have user close app first"; exit 1; }
for d in kstuff.elf pldmgr.elf ftpsrv.elf klogsrv.elf; do
  C=$(curl -s --max-time 8 "http://ps5-host-not-configured.invalid:8084/processes_list" | grep -c "\"$d\"" || true)
  echo "  $d: $C"
done

echo "== 2. PRX on console (must be 5ea2c845...) =="
curl -s --max-time 15 -Q "SITE SELF" "$FTPR/data/plugins/modbridge_supplier_composition.prx" -o "$EV/wake-prx-check.prx"
echo "  magic: $(xxd -l 4 -p "$EV/wake-prx-check.prx") sha: $(shasum -a 256 "$EV/wake-prx-check.prx" | cut -c1-16)"
[ "$(shasum -a 256 "$EV/wake-prx-check.prx" | cut -c1-8)" = "5ea2c845" ] && echo "  VERIFIED-QUERY BUILD CONFIRMED" || echo "  !! MISMATCH - re-run prep_modded_launch.py first"

echo "== 3. settings (want cache-on, 1947 B) =="
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini" -o "$EV/wake-settings.ini"
SZ=$(wc -c < "$EV/wake-settings.ini" | tr -d ' ')
CACHE=$(grep bUseUObjectArrayCache "$EV/wake-settings.ini")
echo "  size=$SZ $CACHE"
if [ "$SZ" != "1947" ] || ! grep -q "bUseUObjectArrayCache=true" "$EV/wake-settings.ini"; then
  echo "  -> applying cache-on settings"
  curl -s --max-time 15 -T "$SETTINGS_CACHEON" "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"
  curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini" | cmp "$SETTINGS_CACHEON" - && echo "  CACHE-ON APPLIED"
fi

echo "== 4. zzdiag v8 (auto-prober) =="
curl -s --max-time 15 -T "$ZZV8_MAIN" "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua"
curl -s --max-time 15 -T "$ZZV8_LOADLIST" "$FTPR/data/modbridge/roots/PPSA33286/loadlist.txt"
curl -s --max-time 12 "$FTPR/data/modbridge/roots/PPSA33286/ue4ss/Mods/zzdiag/Scripts/main.lua" | cmp "$ZZV8_MAIN" - && echo "  V8 LIVE (auto-probe every 6s)"

echo "== 5. arm ploader (once; skip if resident) =="
P=$(curl -s --max-time 8 "http://ps5-host-not-configured.invalid:8084/processes_list" | grep -c payload.elf || true)
if [ "$P" = "0" ]; then
  curl -s --max-time 20 "http://ps5-host-not-configured.invalid:8084/loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fploader%2Fploader.elf" > /dev/null
  sleep 5
  P=$(curl -s --max-time 8 "http://ps5-host-not-configured.invalid:8084/processes_list" | grep -c payload.elf || true)
fi
echo "  payload.elf: $P"

echo "== 6. klog tap =="
TAPLOG=$(ls -t "$TAPDIR"/klog-*.log 2>/dev/null | head -1)
if ps aux | grep -q "[k]log_capture.py"; then
  echo "  tap already running ($TAPLOG)"
else
  nohup python3 $WHAT/outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/klog_capture.py --out-dir "$TAPDIR" --window-seconds 7200 > "$TAPDIR/wake-tap-stderr.log" 2>&1 &
  sleep 2
  echo "  tap started: $(cat "$TAPDIR/wake-tap-stderr.log" | head -1)"
fi

echo ""
echo "READY. Tell the user: LAUNCH -> load save -> get IN-WORLD (controlling character)"
echo "-> 10s -> L3+R3 once. v8 auto-reports db state every 6s; watch for 'T<n> db=v'."
