#!/usr/bin/env python3
"""Stage the morning Townfall launch (Mac-only; NO console contact).

Builds, from local files only:
  loadlist.txt        the new loadlist: the deployed acceptance list with the
                      self-test rows at their new sizes + the minimap rows
  rowcheck-pins.json  every row's exact size + sha256 (prelaunch_rowcheck --expect)
  upload/             the files that must be uploaded (self-test mirror + loadlist)
  STAGE-RECEIPT.json  what was staged, with the console prefixes it was checked against

Every UNCHANGED row's local source is checked against the sha256 prefix read
from the console during step 0 (2026-10-12 02:55 UTC, read-only FTP), so the
pins describe what is really deployed. Any mismatch stops the staging.
"""
import hashlib, json, os, shutil, sys

W = '/WORKSPACE/.codex/worktrees/translation-core/what'
P = W + '/outputs/2026-10-08-l25-prep'
OUT = P + '/morning-launch-20261012'
RB = P + '/evidence/rowcheck/rowcheck-20261012T002334Z-20103/readback'
RBA = P + '/evidence/runtime/l25-variant-A/rowcheck/readback'
MM = W + '/.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/minimap-v3.1/ue4ss/Mods/TownfallMiniMap/Scripts/main.lua'
ST = P + '/ueselftest'
BASE_LOADLIST = P + '/launch-manifests/acceptance-noMM-loadlist.txt'   # == console copy (sha c1e71515, step 0)

# console sha256 prefixes observed in step 0 (read-only FTP) for unchanged rows
CONSOLE = {
    'ue4ss/UE4SS-settings.ini': 'b980cea75c46a390',
    'ue4ss/PS5Pad.ini': '957d3e25e3c4802a',
    'ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua': 'f586043a1ca57081',
    'ue4ss/Mods/shared/UEHelpers/UEHelpers.lua': '5a8f6609e555eee4',
    'ue4ss/Mods/zzdiag/Scripts/main.lua': 'a965dc97b19428dc',
    'ue4ss/Mods/CheatManagerEnablerMod/Scripts/main.lua': '0b70465317ef3fce',
    'ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua': 'b31feb8ffb8adac1',
    'ue4ss/Mods/InfiniteStamina/Scripts/main.lua': '7667fed7e4d7b4cf',
    'ue4ss/Mods/TownfallMiniMap/Scripts/main.lua': 'd8f8d64f84d7e093',
}
SOURCE = {
    'ue4ss/UE4SS-settings.ini': RB + '/ue4ss__UE4SS-settings.ini',
    'ue4ss/MemberVariableLayout.ini': RB + '/ue4ss__MemberVariableLayout.ini',
    'ue4ss/VTableLayout.ini': RB + '/ue4ss__VTableLayout.ini',
    'ue4ss/PS5Pad.ini': P + '/evidence/rowcheck/rowcheck-20261011T205020Z-17385/readback/ue4ss__PS5Pad.ini',
    'ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua': P + '/evidence/townfall-main.lua.console-copy',
    'ue4ss/Mods/shared/UEHelpers/UEHelpers.lua': RB + '/ue4ss__Mods__shared__UEHelpers__UEHelpers.lua',
    'ue4ss/Mods/zzdiag/Scripts/main.lua': P + '/zzdiag-v11/main.lua',
    'ue4ss/Mods/CheatManagerEnablerMod/Scripts/main.lua': RBA + '/ue4ss__Mods__CheatManagerEnablerMod__Scripts__main.lua',
    'ue4ss/Mods/ConsoleEnablerMod/Scripts/main.lua': RB + '/ue4ss__Mods__ConsoleEnablerMod__Scripts__main.lua',
    'ue4ss/Mods/InfiniteStamina/Scripts/main.lua': RBA + '/ue4ss__Mods__InfiniteStamina__Scripts__main.lua',
    'ue4ss/Mods/TownfallMiniMap/Scripts/main.lua': MM,
}
SELFTEST = ['main'] + ['part%d' % i for i in range(2, 13)]
for n in SELFTEST:
    SOURCE['ue4ss/Mods/ueselftest/Scripts/%s.lua' % n] = ST + '/%s.lua' % n
MINIMAP_ROWS = ['ue4ss/Mods/TownfallMiniMap/Scripts/main.lua', 'ue4ss/Mods/TownfallMiniMap/enabled.txt']


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def main():
    rows, header = [], []
    for ln in open(BASE_LOADLIST).read().splitlines():
        parts = ln.split('\t')
        (rows if parts[0] == 'file' else header).append(parts)
    have = {r[2] for r in rows}
    for path in MINIMAP_ROWS:
        if path in have:
            sys.exit('minimap row already present: ' + path)
    rows.append(['file', 'lua_mod', MINIMAP_ROWS[0], '0'])
    rows.append(['file', 'config', MINIMAP_ROWS[1], '0'])
    pins, problems, upload = {}, [], []
    for r in rows:
        path = r[2]
        if path.endswith('enabled.txt'):
            src = None
            size, digest = 0, hashlib.sha256(b'').hexdigest()
        else:
            src = SOURCE.get(path)
            if not src or not os.path.isfile(src):
                problems.append('no local source for row: ' + path)
                continue
            size, digest = os.path.getsize(src), sha(src)
        if path in CONSOLE and not digest.startswith(CONSOLE[path]):
            problems.append('local source does not match the deployed console file: %s (%s vs %s)' % (path, digest[:16], CONSOLE[path]))
        r[3] = str(size)
        pins[path] = {'size': size, 'sha256': digest}
        if path.startswith('ue4ss/Mods/ueselftest/Scripts/'):
            upload.append((path, src))
    if problems:
        print('STAGING FAIL:')
        for x in problems:
            print('  ' + x)
        return 1
    if not rows:
        print('STAGING FAIL: zero rows')
        return 1
    text = '\n'.join('\t'.join(h) for h in header) + '\n' + '\n'.join('\t'.join(r) for r in rows) + '\n'
    os.makedirs(OUT + '/upload/ue4ss/Mods/ueselftest/Scripts', exist_ok=True)
    open(OUT + '/loadlist.txt', 'w').write(text)
    shutil.copyfile(OUT + '/loadlist.txt', OUT + '/upload/loadlist.txt')
    for path, src in upload:
        shutil.copyfile(src, OUT + '/upload/' + path)
    pins['loadlist.txt'] = {'size': len(text.encode()), 'sha256': hashlib.sha256(text.encode()).hexdigest()}
    json.dump(pins, open(OUT + '/rowcheck-pins.json', 'w'), indent=1, sort_keys=True)
    receipt = {'staged_utc': __import__('time').strftime('%Y-%m-%dT%H:%M:%SZ', __import__('time').gmtime()),
               'console_contact': False, 'rows': len(rows), 'upload_files': len(upload) + 1,
               'loadlist_sha256': pins['loadlist.txt']['sha256'], 'loadlist_bytes': pins['loadlist.txt']['size'],
               'checked_against_console_step0_prefixes': CONSOLE,
               'minimap_already_on_console': 'yes (step 0 listing: Scripts/main.lua 196743 B sha d8f8d64f..., enabled.txt 0 B)'}
    json.dump(receipt, open(OUT + '/STAGE-RECEIPT.json', 'w'), indent=1)
    print('STAGED: %d rows (%d self-test files + loadlist to upload); loadlist %d B sha %s' % (
        len(rows), len(upload), receipt['loadlist_bytes'], receipt['loadlist_sha256'][:16]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
