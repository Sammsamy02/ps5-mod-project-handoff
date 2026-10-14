#!/usr/bin/env python3
"""deploy_launch2.py — deploy the staged launch-2 files (upload/) to the console.

CONSOLE ACTION for --backup-and-upload / --restore / --delete-stale: run ONLY
with the user's explicit OK, game CLOSED. --dry-run (default) and --list-stale
make no writes.

  --dry-run             print every step, contact nothing
  --backup-and-upload   1) RETR every target's current console copy into
                           evidence/rollback-launch2-<ts>/ (missing = noted)
                        2) create missing dirs level by level (MKD <abs dir>,
                           proven form; verified by LIST)
                        3) upload each changed file (skip identical), readback
                           sha256; loadlist.txt LAST and only if all passed
  --restore DIR         put back the backed-up copies from DIR (loadlist last);
                        files that did not exist before are left in place (they
                        are not in the old loadlist, so MFA never opens them)
  --list-stale          LIST the Scripts dirs of the mods we own/stage and report
                        files not in the staged set (read-only)
Plain FTP (text files only). Exit 1 on any failure or zero files.
"""
import argparse, hashlib, json, os, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
P = os.path.dirname(HERE)
UP = HERE + '/upload'
ROOT = '/data/modbridge/roots/PPSA33286'
MIRROR_DIRS = ['ue4ss/Mods/ueselftest/Scripts', 'ue4ss/Mods/zzdiag/Scripts', 'ue4ss/Mods/SpeedHackMod/Scripts']


def staged():
    out = []
    for dp, _, fs in os.walk(UP):
        for f in fs:
            out.append((os.path.relpath(os.path.join(dp, f), UP), os.path.join(dp, f)))
    return sorted(out, key=lambda x: (x[0] == 'loadlist.txt', x[0]))


def curl(args, timeout=120):
    r = subprocess.run(['curl', '-s', '--fail', '--connect-timeout', '5', '--max-time', str(timeout)] + args, capture_output=True)
    return r.returncode, r.stdout


def sha(b):
    return hashlib.sha256(b).hexdigest()


def names(body):
    fs = []
    for ln in body.decode(errors='replace').splitlines():
        p = ln.split(None, 8)
        if len(p) == 9 and not ln.startswith('d') and p[8] not in ('.', '..'):
            fs.append(p[8])
    return fs


def ensure_dirs(base, rel):
    parts = rel.split('/')[:-1]
    for i in range(1, len(parts) + 1):
        d = ROOT + '/' + '/'.join(parts[:i])
        rc, _ = curl([base + d + '/'], 30)
        if rc == 0:
            continue
        if rc not in (9, 78):
            return 'LIST %s rc=%d' % (d, rc)
        curl(['-Q', 'MKD ' + d, '-o', os.devnull, base + '/'], 30)
        rc, _ = curl([base + d + '/'], 30)
        if rc != 0:
            return 'MKD %s failed (LIST after rc=%d)' % (d, rc)
        print('MKD  %s (verified)' % d)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--console', required=True)
    ap.add_argument('--port', default='2121')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--backup-and-upload', action='store_true')
    ap.add_argument('--restore', default=None)
    ap.add_argument('--list-stale', action='store_true')
    a = ap.parse_args()
    base = 'ftp://%s:%s' % (a.console, a.port)
    files = staged()
    if not files:
        print('FAIL: zero staged files'); return 1
    if a.dry_run or not (a.backup_and_upload or a.restore or a.list_stale):
        for rel, p in files:
            print('WOULD backup console %s, create missing dirs, upload %d B if different, readback%s' % (
                rel, os.path.getsize(p), ' (LAST, only if all passed)' if rel == 'loadlist.txt' else ''))
        print('WOULD LIST %s and report stale files' % ', '.join(MIRROR_DIRS))
        print('checked %d staged files (dry run, no console contact)' % len(files)); return 0
    fails = 0
    if a.restore:
        man = json.load(open(os.path.join(a.restore, 'BACKUP.json')))
        for rel in sorted(man, key=lambda r: (r == 'loadlist.txt', r)):
            if not man[rel]['existed']:
                print('KEEP %s (did not exist before; not in the old loadlist)' % rel); continue
            p = os.path.join(a.restore, rel)
            rc, _ = curl(['-T', p, base + ROOT + '/' + rel])
            rc2, body = curl([base + ROOT + '/' + rel])
            ok = rc == 0 and rc2 == 0 and sha(body) == man[rel]['sha256']
            print('%s restore %s' % ('PASS' if ok else 'FAIL', rel)); fails += not ok
        print('restore: %d failures' % fails); return 1 if fails else 0
    if a.backup_and_upload:
        bdir = P + '/evidence/rollback-launch2-' + time.strftime('%Y%m%dT%H%M%SZ', time.gmtime())
        man = {}
        for rel, _ in files:
            rc, body = curl([base + ROOT + '/' + rel])
            man[rel] = {'existed': rc == 0, 'sha256': sha(body) if rc == 0 else None, 'size': len(body) if rc == 0 else None}
            if rc == 0:
                os.makedirs(os.path.dirname(os.path.join(bdir, rel)), exist_ok=True)
                open(os.path.join(bdir, rel), 'wb').write(body)
        os.makedirs(bdir, exist_ok=True)
        json.dump(man, open(bdir + '/BACKUP.json', 'w'), indent=1)
        print('backup: %d files read (%d existed) -> %s' % (len(man), sum(v['existed'] for v in man.values()), os.path.relpath(bdir, P)))
        for rel, p in files:
            local = open(p, 'rb').read()
            if rel == 'loadlist.txt' and fails:
                print('SKIP loadlist.txt: %d earlier failure(s); console keeps its old loadlist' % fails); fails += 1; continue
            if man[rel]['existed'] and man[rel]['sha256'] == sha(local):
                print('SAME %s' % rel); continue
            err = ensure_dirs(base, rel)
            if err:
                print('FAIL ' + err); fails += 1; continue
            rc, _ = curl(['-T', p, base + ROOT + '/' + rel])
            rc2, body = curl([base + ROOT + '/' + rel])
            ok = rc == 0 and rc2 == 0 and sha(body) == sha(local)
            print('%s %s (%d B)' % ('PASS readback' if ok else 'FAIL', rel, len(local))); fails += not ok
    if a.list_stale or a.backup_and_upload:
        for d in MIRROR_DIRS:
            rc, body = curl([base + ROOT + '/' + d + '/'], 30)
            mine = {os.path.basename(r) for r, _ in files if r.startswith(d + '/')}
            if d.endswith('ueselftest/Scripts') or d.endswith('zzdiag/Scripts'):
                mine |= {os.path.basename(x) for x in os.listdir(HERE + '/fixture/' + d)}
            remote = names(body) if rc == 0 else []
            stale = [n for n in remote if n not in mine]
            print('LIST %s: rc=%d %d files, stale: %s' % (d, rc, len(remote), ', '.join(stale) or '-'))
    print('checked %d staged files, %d failures' % (len(files), fails))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
