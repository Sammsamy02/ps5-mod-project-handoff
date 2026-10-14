#!/usr/bin/env python3
"""After a passing gate: copy the real-data manifest + dry-run plan into evidence/, write
evidence/README.md (hashes) and PATCH/SHA256SUMS. Mac-only, team folder only."""
import hashlib
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TEAM = os.path.dirname(HERE)
EV = os.path.join(TEAM, 'evidence')
W = os.path.join(HERE, 'work', 'real')


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as fh:
        for b in iter(lambda: fh.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def main():
    rec = json.load(open(os.path.join(HERE, 'output', 'tex_real.json')))
    if rec.get('result') != 'PASS':
        print('finalize: tex_real did not pass')
        return 1
    shutil.rmtree(os.path.join(EV, 'manifest'), ignore_errors=True)
    shutil.copytree(os.path.join(W, 'manifest'), os.path.join(EV, 'manifest'))
    shutil.copyfile(os.path.join(W, 'plan-sub.json'), os.path.join(EV, 'paktx-dryrun-plan-sub.json'))
    shutil.copyfile(os.path.join(TEAM, 'PATCH', 'tools', 'ps5_layout_census.json'), os.path.join(EV, 'ps5_layout_census.json'))
    shutil.copyfile(os.path.join(HERE, 'output', 'tex_real.json'), os.path.join(EV, 'tex_real.receipt.json'))
    for stale in ('paktx-dryrun-plan-sub.stderr.txt',):
        p = os.path.join(EV, stale)
        if os.path.exists(p):
            os.remove(p)
    man = json.load(open(os.path.join(EV, 'manifest', 'manifest.json')))
    plan = json.load(open(os.path.join(EV, 'paktx-dryrun-plan-sub.json')))
    lines = ['# Evidence (Mac-only, real data, read-only inputs)', '',
             '| File | sha256 | What |', '|---|---|---|']
    for rel, what in (('reparent-plan-r5-textures.json', 'plan r5 (--accept-unproven-opaque --derive-textures)'),
                      ('manifest/manifest.json', 'derived-packages manifest (file); self sha below'),
                      ('ps5_layout_census.json', 'game layout census (101 rows, IoHash-verified)'),
                      ('paktx-dryrun-plan-sub.json', 'paktx dry-run plan (sha depends on --work)'),
                      ('hair-mask-check.json', 'hair verdict (REFUSED)'),
                      ('fetch-hair-mask-textures.json', 'optional read-only fetch plan (8 chunks)'),
                      ('ps5-mode-table.md', 'per-texture PS5 layout table'),
                      ('tex_real.receipt.json', 'real dry run receipt')):
        lines.append(f'| `{rel}` | `{sha(os.path.join(EV, rel))}` | {what} |')
    lines += ['', f"manifest_sha256 (self, canonical): `{man['manifest_sha256']}`",
              f"manifest rows: {len(man['rows'])} material instances, {len(man['textures'])} derived textures",
              f"dry-run plan_sha256 (team work path): `{plan['plan_sha256']}`",
              f"post-build substituted_chunks_verified: {plan['substitution']['post_build'][0]['substituted_chunks_verified']}",
              f"tex_real: {rec['result']} {rec['items_checked']} items", '']
    open(os.path.join(EV, 'README.md'), 'w').write('\n'.join(lines))
    sums = []
    for root, _d, files in os.walk(os.path.join(TEAM, 'PATCH')):
        for f in sorted(files):
            if f == 'SHA256SUMS' or '__pycache__' in root:
                continue
            p = os.path.join(root, f)
            sums.append(f'{sha(p)}  {os.path.relpath(p, TEAM)}')
    for f in ('NOTES.md', 'APPLY.md', 'LAUNCH-CARD.md', 'REVIEW.md', 'FIXES.md', 'run_plan.sh', 'run_derive.sh',
              'run_hair_check.sh', 'per_part_table.py'):
        sums.append(f'{sha(os.path.join(TEAM, f))}  {f}')
    for f in sorted(os.listdir(HERE)):
        if f.endswith(('.py', '.sh')):
            sums.append(f'{sha(os.path.join(HERE, f))}  tests/{f}')
    open(os.path.join(TEAM, 'PATCH', 'SHA256SUMS'), 'w').write('\n'.join(sorted(sums, key=lambda l: l[66:])) + '\n')
    print('finalize: evidence + SHA256SUMS written;', man['manifest_sha256'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
