#!/usr/bin/env python3
"""readonly_check.py — READ-ONLY console check before the morning launch.

FTP LIST/RETR only (allowed without asking: CLAUDE.md section 1). Never
uploads, deletes, renames or creates anything on the console; never reads a
log; never contacts pldmgr (8084) or klog (3232). Run with the game CLOSED.

  1. LIST the folders the launch uses.
  2. RETR the runtime PRX with decrypt OFF (-Q "SITE SELF"): full sha256,
     size and magic against the expected build (--expect-prx, a local copy).
  3. RETR ploader.ini, the ploader lock (absent is normal) and the console
     loadlist.txt (config files, not logs).
  4. For every row of the STAGED loadlist: RETR the console copy and compare
     size + sha256 with rowcheck-pins.json. Each row is MATCH, WILL-UPLOAD
     (differs or missing, but step 2 uploads it) or PROBLEM (differs or
     missing and nothing staged replaces it).
  5. Report stale files in ueselftest/Scripts (on the console, not staged).

Everything lands in evidence/morning-readonly-<ts>/ with RECEIPT.json. The
console address is not written into the receipt. Prints how many items it
checked; exit 1 on zero checked items, a PRX mismatch, or any PROBLEM row.
"""
import argparse, datetime, hashlib, json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PREP = os.path.dirname(HERE)
UP = os.path.join(HERE, 'upload')
ROOT = '/data/modbridge/roots/PPSA33286'
PRX_REMOTE = '/data/plugins/modbridge_supplier_composition.prx'
SMALL_REMOTE = {'ploader.ini': '/data/plugins/ploader.ini',
                'ploader.lock': '/data/plugins/ploader.lock',
                'console-loadlist.txt': ROOT + '/loadlist.txt'}
LIST_DIRS = ['/data/plugins/', '/data/modbridge/', ROOT + '/', ROOT + '/ue4ss/',
             ROOT + '/ue4ss/Mods/', ROOT + '/ue4ss/Mods/ueselftest/Scripts/',
             '/data/modbridge/writable/']
STALE_DIR = 'ue4ss/Mods/ueselftest/Scripts'
SIGNED_MAGIC = '4f153d1d'


def curl(base, args, timeout):
    argv = ['curl', '--silent', '--show-error', '--fail', '--connect-timeout', '5',
            '--max-time', str(timeout)] + args
    argv = [a.replace('{BASE}', base) for a in argv]
    r = subprocess.run(argv, capture_output=True)
    return r.returncode, r.stdout, r.stderr.decode(errors='replace').strip()


def sha_file(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def magic_file(p):
    with open(p, 'rb') as f:
        return f.read(4).hex()


def staged_rows():
    rows = []
    with open(os.path.join(UP, 'loadlist.txt'), encoding='utf-8') as f:
        for ln in f.read().splitlines():
            parts = ln.split('\t')
            if parts and parts[0] == 'file' and len(parts) == 4:
                rows.append({'role': parts[1], 'path': parts[2], 'size': int(parts[3])})
    return rows


def staged_upload_set():
    out = set()
    for dp, _, fs in os.walk(UP):
        for f in fs:
            out.add(os.path.relpath(os.path.join(dp, f), UP))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--console', required=True)
    ap.add_argument('--port', default='2121')
    ap.add_argument('--expect-prx', required=True, help='local copy of the PRX expected on the console')
    ap.add_argument('--out-dir', default=None)
    a = ap.parse_args()
    base = 'ftp://%s:%s' % (a.console, a.port)
    ts = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    out = a.out_dir or os.path.join(PREP, 'evidence', 'morning-readonly-' + ts)
    spool = os.path.join(out, 'spool')
    os.makedirs(spool, exist_ok=True)
    rec = {'utc': ts, 'kind': 'read-only console check (FTP LIST/RETR only)', 'listings': {},
           'prx': {}, 'small_files': {}, 'rows': [], 'stale': None, 'problems': []}
    checked = 0

    # 1. listings
    for d in LIST_DIRS:
        rc, body, err = curl(base, ['{BASE}' + d], 30)
        name = d.strip('/').replace('/', '_') or 'root'
        txt = body.decode(errors='replace')
        with open(os.path.join(out, 'LIST-%s.txt' % name), 'w', encoding='utf-8') as f:
            f.write(txt)
        lines = [l for l in txt.splitlines() if l.strip()]
        rec['listings'][d] = {'rc': rc, 'entries': len(lines), 'err': err[:200]}
        print('LIST %-52s rc=%-2d entries=%d%s' % (d, rc, len(lines), ('  (' + err[:80] + ')') if rc else ''))
        checked += 1

    # 2. PRX, decrypt OFF
    exp = {'sha256': sha_file(a.expect_prx), 'size': os.path.getsize(a.expect_prx), 'magic': magic_file(a.expect_prx)}
    dest = os.path.join(spool, 'console-prx.decrypt-off.bin')
    rc, _, err = curl(base, ['-Q', 'SITE SELF', '-o', dest, '{BASE}' + PRX_REMOTE], 300)
    got = {'rc': rc, 'err': err[:200]}
    if rc == 0 and os.path.isfile(dest):
        got.update({'sha256': sha_file(dest), 'size': os.path.getsize(dest), 'magic': magic_file(dest)})
    prx_ok = (rc == 0 and got.get('sha256') == exp['sha256'] and got.get('size') == exp['size']
              and got.get('magic') == SIGNED_MAGIC == exp['magic'])
    rec['prx'] = {'remote': PRX_REMOTE, 'expected': exp, 'console': got, 'verdict': 'PASS' if prx_ok else 'FAIL'}
    print('PRX  %s  console sha256=%s size=%s magic=%s  expected %s... (%s B)' % (
        'PASS' if prx_ok else 'FAIL', got.get('sha256', '-'), got.get('size', '-'), got.get('magic', '-'),
        exp['sha256'][:16], exp['size']))
    if not prx_ok:
        rec['problems'].append('PRX on console is not the expected build (or could not be read)')
    checked += 1

    # 3. small config files
    for label, remote in SMALL_REMOTE.items():
        dest = os.path.join(spool, label)
        if os.path.exists(dest):
            os.remove(dest)
        rc, _, err = curl(base, ['-o', dest, '{BASE}' + remote], 30)
        info = {'remote': remote, 'rc': rc}
        if rc == 0 and os.path.isfile(dest):
            info.update({'sha256': sha_file(dest), 'size': os.path.getsize(dest)})
        rec['small_files'][label] = info
        print('RETR %-22s rc=%-2d %s' % (label, rc, ('size=%d sha256=%s' % (info['size'], info['sha256'][:16])) if 'size' in info else '(absent / not readable)'))
        checked += 1

    # 4. every staged row vs pins
    pins = json.load(open(os.path.join(HERE, 'rowcheck-pins.json'), encoding='utf-8'))
    upload = staged_upload_set()
    rows = staged_rows()
    counts = {'MATCH': 0, 'WILL-UPLOAD': 0, 'PROBLEM': 0}
    for row in rows:
        p = row['path']
        pin = pins.get(p)
        dest = os.path.join(spool, 'rows', p)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        if os.path.exists(dest):
            os.remove(dest)
        rc, _, err = curl(base, ['-o', dest, '{BASE}' + ROOT + '/' + p], 60)
        have = None
        if rc == 0 and os.path.isfile(dest):
            have = {'size': os.path.getsize(dest), 'sha256': sha_file(dest)}
        same = bool(pin) and have is not None and have['size'] == pin['size'] == row['size'] and have['sha256'] == pin['sha256']
        if same:
            verdict = 'MATCH'
        elif p in upload:
            verdict = 'WILL-UPLOAD'
        else:
            verdict = 'PROBLEM'
            rec['problems'].append('row %s: console copy %s, nothing staged replaces it' % (
                p, 'missing' if have is None else 'differs from pin'))
        counts[verdict] += 1
        rec['rows'].append({'path': p, 'role': row['role'], 'declared_size': row['size'], 'pin': pin,
                            'console': have, 'rc': rc, 'verdict': verdict})
        checked += 1
    for v in rec['rows']:
        if v['verdict'] != 'MATCH':
            c = v['console']
            print('ROW  %-11s %s  (console %s)' % (v['verdict'], v['path'],
                  'missing' if c is None else '%d B %s' % (c['size'], c['sha256'][:12])))
    print('ROWS %d checked: %d MATCH, %d WILL-UPLOAD, %d PROBLEM' % (
        len(rows), counts['MATCH'], counts['WILL-UPLOAD'], counts['PROBLEM']))
    if not rows:
        rec['problems'].append('zero staged rows parsed')

    # loadlist itself
    ll = rec['small_files'].get('console-loadlist.txt', {})
    rec['loadlist'] = {'console_sha256': ll.get('sha256'), 'staged_sha256': pins['loadlist.txt']['sha256'],
                       'same': ll.get('sha256') == pins['loadlist.txt']['sha256']}
    print('LOADLIST console %s staged (staged one is uploaded in step 2)' % ('==' if rec['loadlist']['same'] else '!='))

    # 5. stale files in our self-test folder
    # LIST long format: this ftpsrv refuses NLST (curl --list-only rc 19, seen live 2026-10-12)
    rc, body, err = curl(base, ['{BASE}' + ROOT + '/' + STALE_DIR + '/'], 30)
    remote = []
    for ln in body.decode(errors='replace').splitlines():
        parts = ln.split(None, 8)
        if len(parts) == 9 and not ln.startswith('d') and parts[8] not in ('.', '..'):
            remote.append(parts[8])
    mine = {os.path.basename(r) for r in upload if r.startswith(STALE_DIR + '/')}
    stale = sorted(n for n in remote if n not in mine)
    rec['stale'] = {'rc': rc, 'remote_count': len(remote), 'stale': stale}
    print('STALE %s: %d on console, %d stale: %s' % (STALE_DIR, len(remote), len(stale), ', '.join(stale) or '-'))
    if rc != 0 or not remote:
        rec['problems'].append('self-test folder listing empty or failed (rc=%d)' % rc)
    checked += 1

    rec['checked_items'] = checked
    rec['row_counts'] = counts
    rec['verdict'] = 'PASS' if checked and not rec['problems'] else 'FAIL'
    with open(os.path.join(out, 'RECEIPT.json'), 'w', encoding='utf-8') as f:
        json.dump(rec, f, indent=1)
    print('checked %d items; problems: %d -> %s' % (checked, len(rec['problems']), rec['verdict']))
    for p in rec['problems']:
        print('  PROBLEM: ' + p)
    print('evidence: ' + os.path.relpath(out, PREP))
    return 0 if rec['verdict'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
