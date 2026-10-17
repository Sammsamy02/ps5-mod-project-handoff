#!/bin/bash
# cleanup_for_cap12.sh -- integrator: make the #12 launch about Leon's look only.
# Removes the #8 checker (zzintake), switches the two C++ test mods off (enabled.txt + its loadlist row; the dlls/
# folders stay so they can be re-enabled), and puts the pad file back to the base (no L1+R2 row, 1974 B).
# Keeps zzpakprobe (proves Leon's objects load). Refuses while the game runs. Ends with the row-check.
set -u
EV="${1:?usage: cleanup_for_cap12.sh <evidence dir>}"
C=ps5-host-not-configured.invalid
F="ftp://$C:2121"
ROOT=/data/modbridge/roots/PPSA33286
PREP=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep
BASEPAD="$PREP/evidence/cap09-prep-20261013/PS5Pad.console.ini"
die() { echo "FAIL: $*"; exit 1; }
mkdir -p "$EV" || die mkdir
[ "$(curl -s --max-time 8 "http://$C:8084/processes_list" | grep -c eboot)" = "0" ] || die "game running"
[ "$(wc -c < "$BASEPAD" | tr -d ' ')" = "1974" ] || die "base pad size"
curl -s --max-time 12 "$F$ROOT/ue4ss/PS5Pad.ini" -o "$EV/PS5Pad.before.ini" || die "pad fetch"
grep -v -e '^L1+R2 = F10$' -e '^L1+R2 = F9$' "$EV/PS5Pad.before.ini" | cmp -s - "$BASEPAD" || die "console pad is not base + one L1+R2 row"
curl -s --max-time 12 "$F$ROOT/loadlist.txt" -o "$EV/loadlist.before.txt" || die "loadlist fetch"
python3 - "$EV" <<'EOF' || exit 1
import sys
d = sys.argv[1]
L = open(d + '/loadlist.before.txt').read().rstrip('\n').split('\n')
drop = ('ue4ss/Mods/zzintake/', 'ue4ss/Mods/NativeLifecycleProbe/enabled.txt', 'ue4ss/Mods/NativeGuiProbe/enabled.txt')
out, removed, pad = [], 0, 0
for l in L:
    p = l.split('\t')
    if len(p) == 4 and p[0] == 'file' and p[2].startswith(drop):
        removed += 1; continue
    if len(p) == 4 and p[0] == 'file' and p[2] == 'ue4ss/PS5Pad.ini':
        p[3] = '1974'; l = '\t'.join(p); pad += 1
    out.append(l)
assert removed == 4 and pad == 1, (removed, pad)
open(d + '/loadlist.after.txt', 'w').write('\n'.join(out) + '\n')
print('loadlist: removed %d rows, PS5Pad.ini re-pinned to 1974' % removed)
EOF
curl -s --max-time 20 -T "$BASEPAD" "$F$ROOT/ue4ss/PS5Pad.ini" || die "pad upload"
curl -s --max-time 20 -T "$EV/loadlist.after.txt" "$F$ROOT/loadlist.txt" || die "loadlist upload"
curl -s --max-time 12 "$F$ROOT/ue4ss/PS5Pad.ini" | cmp -s "$BASEPAD" - || die "pad readback"
curl -s --max-time 12 "$F$ROOT/loadlist.txt" | cmp -s "$EV/loadlist.after.txt" - || die "loadlist readback"
curl -s --max-time 30 -o /dev/null "$F/" \
  -Q "DELE $ROOT/ue4ss/Mods/zzintake/Scripts/main.lua" -Q "DELE $ROOT/ue4ss/Mods/zzintake/enabled.txt" \
  -Q "RMD $ROOT/ue4ss/Mods/zzintake/Scripts" -Q "RMD $ROOT/ue4ss/Mods/zzintake" \
  -Q "DELE $ROOT/ue4ss/Mods/NativeLifecycleProbe/enabled.txt" -Q "DELE $ROOT/ue4ss/Mods/NativeGuiProbe/enabled.txt" || die "delete"
L=$(curl -s --max-time 12 "$F$ROOT/ue4ss/Mods/")
echo "$L" | grep -q " zzintake$" && die "zzintake still present"
for m in NativeLifecycleProbe NativeGuiProbe; do
  curl -s --max-time 12 "$F$ROOT/ue4ss/Mods/$m/" | grep -q " enabled.txt$" && die "$m still enabled"
done
echo "PAD-READBACK-OK LOADLIST-READBACK-OK zzintake removed, C++ test mods disabled"
python3 "$PREP/harvest/prelaunch_rowcheck.py" > "$EV/rowcheck.txt" 2>&1 || die "row-check (see $EV/rowcheck.txt)"
grep VERDICT "$EV/rowcheck.txt"
echo "CLEANUP-CAP12 PASS"
