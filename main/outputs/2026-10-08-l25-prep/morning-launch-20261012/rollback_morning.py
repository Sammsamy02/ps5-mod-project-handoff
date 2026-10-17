#!/usr/bin/env python3
"""rollback_morning.py — put back the pre-deploy console state.

User instruction (2026-10-12): "If any step fails, put back the old runtime and
the old mod list and stop." CONSOLE ACTION, authorised only for a failed step
of this morning's deploy (steps 2, 4, 6). Game CLOSED.

  --modlist   re-upload the pre-deploy self-test files (the 6 that step 2
              replaces; the old mod list names their exact sizes) and then the
              pre-deploy loadlist.txt; each is skipped when the console copy is
              already the old one, else uploaded and read back (sha256).
  --runtime   re-upload the known-good PRX 1e49188b (read-only frozen copy,
              fresh verify_build PASS in evidence/rollback-20261012/) with the
              proven upload shape (-Q "SITE SELF" --upload-file), then a
              decrypt-off readback: full sha256 + magic 4f153d1d.
  --dry-run   print only (default when neither flag is given).

Sources: evidence/rollback-20261012/ROLLBACK-MANIFEST.json + old/ (fetched
before step 2 and verified equal to the 11:40Z read-only check).
Prints what it checked; exit 1 on any failure or when it checked nothing.
"""
import argparse, hashlib, json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
PREP = os.path.dirname(HERE)
RB = os.path.join(PREP, 'evidence', 'rollback-20261012')
KNOWN_GOOD = '1e49188b3824d2619aad8d03601f36f5b819d7abb9f4cef58965cbe8a6f0cd9b'
PRX_REMOTE = '/data/plugins/modbridge_supplier_composition.prx'


def sha_file(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for c in iter(lambda: f.read(1 << 20), b''):
            h.update(c)
    return h.hexdigest()


def curl(args, timeout):
    r = subprocess.run(['curl', '--silent', '--show-error', '--fail', '--connect-timeout', '5',
                        '--max-time', str(timeout)] + args, capture_output=True)
    return r.returncode, r.stdout, r.stderr.decode(errors='replace').strip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--console', required=True)
    ap.add_argument('--port', default='2121')
    ap.add_argument('--modlist', action='store_true')
    ap.add_argument('--runtime', action='store_true')
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    base = 'ftp://%s:%s' % (a.console, a.port)
    man = json.load(open(os.path.join(RB, 'ROLLBACK-MANIFEST.json'), encoding='utf-8'))
    files = sorted(man['files'].items(), key=lambda kv: (kv[0] == 'loadlist.txt', kv[0]))  # loadlist last
    checked = fails = 0
    for rel, info in files:  # local copies must still be exactly what was saved
        p = os.path.join(RB, 'old', rel)
        if not os.path.isfile(p) or sha_file(p) != info['sha256']:
            print('FAIL local rollback copy missing/changed: %s' % rel)
            return 1
    prx_local = os.path.normpath(os.path.join(PREP, man['runtime']['local']))
    if sha_file(prx_local) != KNOWN_GOOD:
        print('FAIL local known-good PRX sha mismatch: %s' % prx_local)
        return 1
    if a.dry_run or not (a.modlist or a.runtime):
        for rel, info in files:
            print('WOULD restore %s (%d B, %s...) -> %s' % (rel, info['size'], info['sha256'][:12], info['remote']))
        print('WOULD restore runtime %s... -> %s, then decrypt-off readback' % (KNOWN_GOOD[:12], PRX_REMOTE))
        print('checked %d rollback files + 1 runtime (dry run, no console contact)' % len(files))
        return 0
    if a.runtime:
        rc, _, err = curl(['-Q', 'SITE SELF', '--upload-file', prx_local, base + PRX_REMOTE], 300)
        rb = os.path.join(RB, 'runtime-restore-readback.decrypt-off.prx')
        if os.path.exists(rb):
            os.remove(rb)
        rc2, _, err2 = curl(['-Q', 'SITE SELF', '-o', rb, base + PRX_REMOTE], 300)
        ok = rc == 0 and rc2 == 0 and os.path.isfile(rb) and sha_file(rb) == KNOWN_GOOD
        magic = open(rb, 'rb').read(4).hex() if os.path.isfile(rb) else '-'
        ok = ok and magic == '4f153d1d'
        print('%s runtime restore: upload rc=%d, readback rc=%d, sha256=%s, magic=%s %s' % (
            'PASS' if ok else 'FAIL', rc, rc2, sha_file(rb) if os.path.isfile(rb) else '-', magic, err or err2))
        checked += 1
        fails += not ok
    if a.modlist:
        for rel, info in files:
            p = os.path.join(RB, 'old', rel)
            url = base + info['remote']
            rc, body, _ = curl([url], 60)
            if rc == 0 and hashlib.sha256(body).hexdigest() == info['sha256']:
                print('SAME %s already the old version' % rel)
                checked += 1
                continue
            rc, _, err = curl(['-T', p, url], 120)
            rc2, body, _ = curl([url], 60)
            ok = rc == 0 and rc2 == 0 and hashlib.sha256(body).hexdigest() == info['sha256']
            print('%s restore %s (upload rc=%d, readback rc=%d) %s' % ('PASS' if ok else 'FAIL', rel, rc, rc2, err))
            checked += 1
            fails += not ok
    print('checked %d items, %d failures' % (checked, fails))
    return 1 if fails or not checked else 0


if __name__ == '__main__':
    sys.exit(main())
