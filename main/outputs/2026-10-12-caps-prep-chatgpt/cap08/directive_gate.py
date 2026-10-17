#!/usr/bin/env python3
"""Mac-only receipt gate, not an export decoder or runtime installer.

Receipt semantics are attestations by their named extractor. This validates
binding/integrity and required facts, not correctness of unexecuted extractors.
All failures are atomic: REVIEW_REQUIRED, zero actionable directives.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

HEX = re.compile(r'^[0-9a-f]{16}$')

def require(ok, why):
    if not ok:
        raise ValueError(why)

def checked(ref):
    require(isinstance(ref, dict), 'missing source reference')
    p = Path(ref['path'])
    require(p.is_file(), 'source file unavailable')
    b = p.read_bytes()
    require(hashlib.sha256(b).hexdigest() == ref.get('sha256'), 'source hash mismatch')
    return b

def receipt(ref, kind, pid):
    r = json.loads(checked(ref))
    require(r.get('schema') == 1 and r.get('kind') == kind, 'receipt kind/schema mismatch')
    require(isinstance(r.get('sources'), list) and len(r['sources']) > 0, 'receipt has zero sources')
    for src in r['sources']:
        checked(src)
    checked(r['extractor'])
    require(r['facts'].get('package_id') == pid, 'receipt package mismatch')
    return r['facts']

def store(ref, container):
    b = checked(ref)
    require(len(b) >= 24, 'truncated store header')
    sig, version, cid, n = struct.unpack_from('<IIQI', b)
    require(sig == 0x496f436e and version == 5, 'unsupported store signature/version')
    require(f'{cid:016x}' == container, 'store container mismatch')
    require(n > 0 and n <= (len(b)-24)//8, 'invalid/zero store count')
    pids = struct.unpack_from('<%dQ' % n, b, 20)
    require(len(set(pids)) == n, 'duplicate store package')
    size = struct.unpack_from('<I', b, 20+8*n)[0]
    start = 24+8*n
    require(size >= n*16 and start+size <= len(b), 'truncated store body')
    body = b[start:start+size]
    result = {}
    for i, pid in enumerate(pids):
        counts = []
        for field, width in ((0, 8), (8, 20)):
            at = i*16+field
            count, rel = struct.unpack_from('<ii', body, at)
            require(count >= 0, 'negative store view count')
            target = at+rel
            if count:
                require(target >= n*16 and target+count*width <= len(body), 'store view out of bounds')
            counts.append(count)
        result[f'{pid:016x}'] = {'hash_count': counts[1]}
    return result

def evaluate(classification, manifest):
    output = {'schema': 1, 'label': 'Mac-only', 'result': 'REVIEW_REQUIRED',
              'directives': [], 'checked_packages': 0, 'issues': [],
              'limits': 'Receipt facts are source-bound extractor attestations; no PS5 usability or installation approval is implied.'}
    try:
        require(manifest.get('schema') == 1, 'unknown manifest schema')
        require(json.loads(checked(manifest['classification'])) == classification, 'classification differs from bound source')
        rows = classification.get('packages')
        require(isinstance(rows, list) and len(rows) > 0, 'zero analyzed packages')
        output['checked_packages'] = len(rows)
        require(manifest.get('catalogue_complete') is True, 'complete package-store catalogue not attested')
        containers = manifest.get('containers', [])
        require(len(containers) > 0, 'zero package stores')
        stores = {}
        for c in containers:
            cid = c['container_id']
            require(HEX.fullmatch(cid) is not None and cid not in stores, 'invalid/duplicate container id')
            stores[cid] = store(c['store'], cid)
        directives, seen = [], set()
        for row in rows:
            pid = row['package_id']
            require(HEX.fullmatch(pid) is not None and pid not in seen, 'invalid/duplicate classified package')
            seen.add(pid)
            action = row.get('directive')
            # These labels are analysis hints, not overlay edits.
            if action in ('PASS', 'FLAG-MESH-PATH-BLOCKED'):
                continue
            require(action in ('SUBSTITUTE_BASE_EXPORT', 'HIDE_VIA_BASE_INVISIBLE'), 'unknown or unresolved classification')
            r = manifest.get('packages', {}).get(pid)
            require(isinstance(r, dict), 'missing package receipts: '+pid)
            cid = r.get('store_container')
            require(cid in stores and pid in stores[cid], 'UNKNOWN base store membership: '+pid)
            ident = receipt(r.get('identity'), 'identity', pid)
            require(bool(ident.get('mod_export')) and ident['mod_export'] == ident.get('base_export'), 'export identity mismatch')
            require(bool(ident.get('mod_class')) and ident['mod_class'] == ident.get('base_class'), 'class identity mismatch')
            intent = receipt(r.get('intent'), 'intent', pid)
            require(intent.get('allow_visual_change') is True, 'visual change not explicitly accepted')
            d = {'package_id': pid, 'base_container_id': cid, 'action': 'suppress_override',
                 'remove_store_override': True, 'remove_all_package_chunk_rows': True,
                 'derived_overlay_only': True, 'look_loss': True}
            if action == 'SUBSTITUTE_BASE_EXPORT':
                compat = receipt(r.get('compatibility'), 'compatibility', pid)
                require(compat.get('target_package_id') == pid and compat.get('source_class') == ident['mod_class'] and compat.get('target_class') == ident['base_class'] and compat.get('import_compatible') is True, 'base substitution import compatibility unverified')
            if action == 'HIDE_VIA_BASE_INVISIBLE':
                target = receipt(r.get('target'), 'target', pid)
                compat = receipt(r.get('compatibility'), 'compatibility', pid)
                tid, tcid = target.get('target_package_id'), target.get('target_container_id')
                require(tcid in stores and tid in stores[tcid] and stores[tcid][tid]['hash_count'] > 0, 'native target shader-store evidence absent')
                require(target.get('export_name') and re.fullmatch(r'[0-9a-f]{16}', target.get('public_export_hash','')), 'target export identity absent')
                require(compat.get('target_package_id') == tid and compat.get('target_public_export_hash') == target['public_export_hash'], 'target compatibility identity mismatch')
                require(compat.get('target_class') == target.get('class') and bool(target.get('class')) and compat.get('source_class') == ident['mod_class'], 'compatibility class mismatch')
                require(compat.get('import_compatible') is True and intent.get('hide_intent_verified') is True, 'hide intent/import compatibility unverified')
                d.update(action='repoint_derived_instance', target_package_id=tid,
                         target_container_id=tcid, target_public_export_hash=target['public_export_hash'])
            directives.append(d)
        require(len(directives) > 0, 'zero actionable packages')
        output.update(result='VALIDATED_DIRECTIVES', directives=directives)
    except (ValueError, KeyError, TypeError, OSError, struct.error, AttributeError) as e:
        output['issues'].append(str(e))
    output['actionable_count'] = len(output['directives'])
    return output

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('classification', type=Path)
    ap.add_argument('manifest', type=Path)
    ap.add_argument('--out', required=True, type=Path)
    a = ap.parse_args()
    try:
        c, m = json.loads(a.classification.read_text()), json.loads(a.manifest.read_text())
        result = evaluate(c, m)
    except (ValueError, OSError) as e:
        result = {'schema':1,'label':'Mac-only','result':'REVIEW_REQUIRED','actionable_count':0,'directives':[],'issues':[str(e)]}
    # Tool outputs are restricted to this prep folder, never an input artifact.
    dest = a.out.resolve()
    require(dest.is_relative_to(Path(__file__).resolve().parent), 'output must remain in cap08')
    require(dest not in (a.classification.resolve(), a.manifest.resolve()), 'refusing input overwrite')
    a.out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'result':result['result'],'actionable_count':result['actionable_count'],'issues':result['issues']}))
    return 0 if result['result'] == 'VALIDATED_DIRECTIVES' else 2

if __name__ == '__main__':
    raise SystemExit(main())
