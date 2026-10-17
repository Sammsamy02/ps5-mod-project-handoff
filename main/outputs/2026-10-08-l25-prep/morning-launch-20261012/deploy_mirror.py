#!/usr/bin/env python3
"""deploy_mirror.py — exact-mirror deploy of OUR diagnostic files (self-test +
loadlist) staged in ./upload/, for the morning Townfall launch.

CONSOLE ACTION. Run ONLY with the user's explicit OK for this step, with the
game CLOSED. Never touches mod files we don't own, executables, or settings.

  --dry-run         print every command, contact nothing (default when no flag)
  --upload          for each staged file: read the console copy; upload only
                    when it differs, then read it back and compare sha256.
                    loadlist.txt goes LAST and only when every other file
                    passed (a half-done deploy never gets a new loadlist).
  --list-stale      list remote ueselftest/Scripts and report files that are NOT
                    in the staged mirror (read-only)
  --delete-stale    delete those stale files (separate explicit OK required)

Plain FTP for text files (no SITE SELF needed: only .lua/.txt here). Prints how
many files it checked; exits 1 on any failure or when it checked zero files.

2026-10-12 live fix: this ftpsrv refuses NLST (curl --list-only rc 19, seen in
the morning read-only check), so listings parse the LIST long format.
"""
import argparse, hashlib, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
UP = os.path.join(HERE, 'upload')
ROOT = '/data/modbridge/roots/PPSA33286'
STALE_DIR = 'ue4ss/Mods/ueselftest/Scripts'
LOADLIST = 'loadlist.txt'


def staged():
    out = []
    for dp, _, fs in os.walk(UP):
        for f in fs:
            p = os.path.join(dp, f)
            out.append((os.path.relpath(p, UP), p))
    # loadlist last: it is only uploaded once every file it names is in place
    return sorted(out, key=lambda rp: (rp[0] == LOADLIST, rp[0]))


def curl(args, timeout=120):
    r = subprocess.run(['curl', '-s', '--fail', '--connect-timeout', '5', '--max-time', str(timeout)] + args,
                       capture_output=True)
    return r.returncode, r.stdout


def parse_list(body):
    """LIST long format -> (files, dirs) names, '.'/'..' dropped."""
    files, dirs = [], []
    for ln in body.decode(errors='replace').splitlines():
        parts = ln.split(None, 8)
        if len(parts) < 9 or parts[8] in ('.', '..'):
            continue
        (dirs if ln.startswith('d') else files).append(parts[8])
    return files, dirs


def sha(b):
    return hashlib.sha256(b).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--console', required=True)
    ap.add_argument('--port', default='2121')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--upload', action='store_true')
    ap.add_argument('--list-stale', action='store_true')
    ap.add_argument('--delete-stale', action='store_true')
    a = ap.parse_args()
    base = 'ftp://%s:%s' % (a.console, a.port)
    files = staged()
    if not files:
        print('FAIL: zero staged files in ' + UP)
        return 1
    live = a.upload or a.list_stale or a.delete_stale
    if not live or a.dry_run:
        for rel, p in files:
            print('WOULD read console %s; if it differs upload %s (%d B) -> %s%s/%s, then readback sha256 compare%s' % (
                rel, rel, os.path.getsize(p), base, ROOT, rel, ' (LAST, only if all others passed)' if rel == LOADLIST else ''))
        print('WOULD LIST %s%s/%s/ and report files not in the mirror' % (base, ROOT, STALE_DIR))
        print('checked %d staged files (dry run, no console contact)' % len(files))
        return 0
    fails = uploaded = same = 0
    if a.upload:
        for rel, p in files:
            url = '%s%s/%s' % (base, ROOT, rel)
            local = open(p, 'rb').read()
            if rel == LOADLIST and fails:
                print('SKIP %s: %d earlier file(s) failed, console keeps its old loadlist' % (rel, fails))
                fails += 1
                continue
            rc, body = curl([url])
            if rc == 0 and sha(body) == sha(local):
                print('SAME %s (%d B) already identical on console, not uploaded' % (rel, len(local)))
                same += 1
                continue
            rc, _ = curl(['-T', p, url])
            if rc != 0:
                print('FAIL upload %s rc=%d' % (rel, rc)); fails += 1; continue
            rc, body = curl([url])
            ok = rc == 0 and sha(body) == sha(local)
            print('%s %s (%d B)' % ('PASS readback' if ok else 'FAIL readback', rel, len(body)))
            fails += not ok
            uploaded += ok
        print('upload: %d uploaded + verified, %d already identical, %d failed' % (uploaded, same, fails))
    stale = []
    if a.list_stale or a.delete_stale:
        rc, body = curl(['%s%s/%s/' % (base, ROOT, STALE_DIR)])
        if rc != 0:
            print('FAIL listing %s rc=%d' % (STALE_DIR, rc)); return 1
        remote, _ = parse_list(body)
        mine = {os.path.basename(rel) for rel, _ in files if rel.startswith(STALE_DIR + '/')}
        stale = [n for n in remote if n not in mine]
        print('remote %s: %d files, %d in mirror, %d stale: %s' % (STALE_DIR, len(remote), len(remote) - len(stale), len(stale), ', '.join(stale) or '-'))
        if not remote:
            print('FAIL: remote listing is empty (game running? FTP reads return empty under load)'); return 1
    if a.delete_stale:
        for n in stale:
            rc, _ = curl(['-Q', 'DELE %s/%s/%s' % (ROOT, STALE_DIR, n), '%s/' % base])
            print('%s delete %s' % ('PASS' if rc == 0 else 'FAIL', n)); fails += rc != 0
    print('checked %d staged files, %d failures' % (len(files), fails))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
