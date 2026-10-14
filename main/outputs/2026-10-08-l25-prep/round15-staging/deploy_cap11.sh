#!/bin/bash
# deploy_cap11.sh -- integrator: put the #11 test mod on the console (cap11 LAUNCH-CARD steps 2-4).
# The #9 probe stays installed (coexistence); its F9 pad row is replaced by the #11 F10 row (one free chord).
# Writes: Mods/NativeGuiProbe/dlls/main.prx (+ decrypt-off readback), Mods/NativeGuiProbe/enabled.txt,
# ue4ss/PS5Pad.ini (base + L1+R2 = F10), loadlist (+1 enabled.txt row, PS5Pad.ini size re-pinned). Never a row for
# main.prx. Refuses if the game runs, if the mod PRX sha/magic/verify_build is not exactly the reviewed file, or if
# the console pad file is not the expected base. Ends with the row-check. Evidence -> $1 (a new dir).
set -u
EV="${1:?usage: deploy_cap11.sh <new evidence dir>}"
C=ps5-host-not-configured.invalid
F="ftp://$C:2121"
ROOT=/data/modbridge/roots/PPSA33286
D=/WORKSPACE/.codex/worktrees/translation-core/caps-team-D/caps-team-work/cap09-cap11/cap11
PREP=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep
PRX="$D/build-out/NativeGuiProbe/final-pass-1/main.prx"
WANT=ba4675085c601a6ce8dbaafdc4cb62e244344c8484f23d080918c4d72cc0b5ad
PAD_NEW="$D/PATCH/keymap/proposed/PS5Pad.cap11.ini"
die() { echo "FAIL: $*"; exit 1; }
mkdir -p "$EV" || die "mkdir"
[ "$(curl -s --max-time 8 "http://$C:8084/processes_list" | grep -c eboot)" = "0" ] || die "game running"
[ "$(shasum -a 256 "$PRX" | cut -d' ' -f1)" = "$WANT" ] || die "mod PRX sha is not the reviewed file"
[ "$(xxd -l4 -p "$PRX")" = "4f153d1d" ] || die "mod PRX magic"
bash "$PREP/harvest/verify_build.sh" "$PRX" --manifest "$D/build-out/verify-manifest-NativeGuiProbe.json" \
  "$D/build-out/NativeGuiProbe/final-pass-1" > "$EV/verify_build-mod.txt" 2>&1 || die "verify_build on the mod PRX"
grep -q "result=PASS" "$EV/verify_build-mod.txt" || die "verify_build did not print PASS"
# pad file: console copy must equal the proposed file minus its one new row
curl -s --max-time 12 "$F$ROOT/ue4ss/PS5Pad.ini" -o "$EV/PS5Pad.before.ini" || die "pad fetch"
grep -v '^L1+R2 = F10$' "$PAD_NEW" | cmp -s - <(grep -v '^L1+R2 = F9$' "$EV/PS5Pad.before.ini") || die "console PS5Pad.ini is not the expected base (+ optional F9 row)"
curl -s --max-time 12 "$F$ROOT/loadlist.txt" -o "$EV/loadlist.before.txt" || die "loadlist fetch"
PADSZ=$(wc -c < "$PAD_NEW" | tr -d ' ')
python3 - "$EV" "$PADSZ" <<'EOF' || exit 1
import sys
d, padsz = sys.argv[1], sys.argv[2]
L = open(d + '/loadlist.before.txt').read().rstrip('\n').split('\n')
assert not any('NativeGuiProbe' in l for l in L), 'row already present'
n = 0
for i, l in enumerate(L):
    p = l.split('\t')
    if len(p) == 4 and p[0] == 'file' and p[2] == 'ue4ss/PS5Pad.ini':
        p[3] = padsz; L[i] = '\t'.join(p); n += 1
assert n == 1, 'PS5Pad.ini row count %d' % n
L.append('file\tconfig\tue4ss/Mods/NativeGuiProbe/enabled.txt\t0')
open(d + '/loadlist.after.txt', 'w').write('\n'.join(L) + '\n')
print('loadlist: PS5Pad.ini row re-pinned to %s, 1 enabled.txt row added' % padsz)
EOF
: > "$EV/enabled.txt"
curl -s --max-time 120 --ftp-create-dirs -Q "SITE SELF" -T "$PRX" "$F$ROOT/ue4ss/Mods/NativeGuiProbe/dlls/main.prx" || die "prx upload"
curl -s --max-time 20 -T "$EV/enabled.txt" "$F$ROOT/ue4ss/Mods/NativeGuiProbe/enabled.txt" || die "enabled upload"
curl -s --max-time 20 -T "$PAD_NEW" "$F$ROOT/ue4ss/PS5Pad.ini" || die "pad upload"
curl -s --max-time 20 -T "$EV/loadlist.after.txt" "$F$ROOT/loadlist.txt" || die "loadlist upload"
curl -s --max-time 120 -Q "SITE SELF" "$F$ROOT/ue4ss/Mods/NativeGuiProbe/dlls/main.prx" -o "$EV/main.prx.readback" || die "prx readback"
[ "$(shasum -a 256 "$EV/main.prx.readback" | cut -d' ' -f1)" = "$WANT" ] || die "prx readback sha"
[ "$(xxd -l4 -p "$EV/main.prx.readback")" = "4f153d1d" ] || die "prx readback magic"
echo "PRX-READBACK-OK sha=$WANT magic=4f153d1d (decrypt off)"; rm -f "$EV/main.prx.readback"
curl -s --max-time 12 "$F$ROOT/ue4ss/PS5Pad.ini" | cmp -s "$PAD_NEW" - || die "pad readback"
curl -s --max-time 12 "$F$ROOT/loadlist.txt" | cmp -s "$EV/loadlist.after.txt" - || die "loadlist readback"
echo "PAD-READBACK-OK LOADLIST-READBACK-OK"
python3 "$PREP/harvest/prelaunch_rowcheck.py" > "$EV/rowcheck.txt" 2>&1 || die "row-check (see $EV/rowcheck.txt)"
grep VERDICT "$EV/rowcheck.txt"
echo "DEPLOY-CAP11 PASS"
