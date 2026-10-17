#!/usr/bin/env python3
"""One-PRX delivery/rollback helper. Default is HOLD with zero console contact.
Execution requires a separate user OK in the conversation and explicit --execute.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

REMOTE = '/data/plugins/modbridge_supplier_composition.prx'
URL = 'ftp://ps5-host-not-configured.invalid:2121/' + REMOTE
SIGNED_MAGICS = {'4f153d1d', '5414f5ee'}


def metadata(path, content=None):
    path = Path(path).resolve()
    data = path.read_bytes() if content is None else content
    return {'path': str(path), 'sha256': hashlib.sha256(data).hexdigest(),
            'bytes': len(data), 'magic': data[:4].hex()}


def valid_hash(value):
    if not isinstance(value, str) or re.fullmatch('[0-9a-f]{64}', value) is None:
        raise ValueError('expected a lowercase SHA-256 digest')
    return value


def checked_source(record, base):
    path = Path(record['path'])
    if not path.is_absolute(): path = base / path
    data = path.read_bytes()
    actual = metadata(path, data)
    if actual['sha256'] != valid_hash(record['sha256']) or actual['bytes'] != record['bytes']:
        raise ValueError('local source/backup hash or size does not match its receipt')
    if actual['magic'] not in SIGNED_MAGICS:
        raise ValueError('source/backup is not a signed SELF/SPRX container; unsigned ELF refused')
    return data, actual


def frozen_json(path, value):
    with path.open('x') as stream: json.dump(value, stream, indent=2); stream.write('\n')
    path.chmod(0o444)


def curl_call(args, receipt, curl):
    argv = [curl, '--disable', '--fail', '--silent', '--show-error',
            '--connect-timeout', '10', '--max-time', '600', '--noproxy', '*',
            '--ftp-method', 'nocwd', '--user', 'anonymous:anonymous', *args, URL]
    event = {'argv': argv, 'returncode': None}
    receipt['steps'].append(event)
    result = subprocess.run(argv, text=True, capture_output=True, timeout=610)
    event.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
    if result.returncode:
        raise RuntimeError('curl failed; no automatic retry or rollback is attempted')


def raw_get(destination, receipt, curl):
    headers = destination.with_suffix(destination.suffix + '.headers')
    curl_call(['--quote', 'SITE SELF', '--dump-header', str(headers),
               '--output', str(destination)], receipt, curl)
    if '226 SELF transfer mode disabled' not in headers.read_text(errors='replace'):
        raise RuntimeError('same-connection decrypt-OFF acknowledgement missing; bytes not trusted')
    destination.chmod(0o444)
    headers.chmod(0o444)
    return metadata(destination)


def execute(args, data, source, expected_preimage):
    args.jobs.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    job = args.jobs / (args.phase + '-' + stamp + '-' + uuid.uuid4().hex)
    job.mkdir(exist_ok=False)
    receipt = {'schema': 1, 'phase': args.phase, 'remote_path': REMOTE,
               'status': 'started', 'steps': [], 'source': source,
               'started_utc': stamp, 'job_dir': str(job.resolve())}
    try:
        replacement = job / 'replacement.prx'
        with replacement.open('xb') as stream: stream.write(data)
        replacement.chmod(0o444)
        receipt['replacement'] = metadata(replacement)
        receipt['preimage'] = raw_get(job / 'preimage.prx', receipt, args.curl)
        if expected_preimage and receipt['preimage']['sha256'] != expected_preimage:
            raise RuntimeError('remote preimage hash differs from approved manifest; upload refused')
        if args.phase == 'upload' and receipt['preimage']['magic'] not in SIGNED_MAGICS:
            raise RuntimeError('remote backup is not a signed container; upload refused')
        # This immutable checkpoint exists before the sole remote mutation.
        frozen_json(job / 'backup.json', {'schema': 1, 'phase': args.phase,
                    'remote_path': REMOTE, 'preimage': receipt['preimage'],
                    'replacement': receipt['replacement'], 'created_utc': stamp})
        if metadata(replacement) != receipt['replacement']:
            raise RuntimeError('local replacement changed before upload; refused')
        curl_call(['--upload-file', str(replacement)], receipt, args.curl)
        receipt['postimage'] = raw_get(job / 'postimage.prx', receipt, args.curl)
        if any(receipt['postimage'][k] != receipt['replacement'][k] for k in ('sha256', 'bytes', 'magic')):
            raise RuntimeError('post-upload readback does not match; stopped without retry or automatic rollback')
        receipt['status'] = 'verified'
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        receipt['status'] = 'failed'
        receipt['error'] = str(error)
    finally:
        receipt['finished_utc'] = datetime.now(timezone.utc).isoformat()
        frozen_json(job / 'receipt.json', receipt)
    print(json.dumps({'status': receipt['status'], 'phase': args.phase,
                      'receipt': str((job / 'receipt.json').resolve()),
                      'backup_receipt': str((job / 'backup.json').resolve())}))
    return 0 if receipt['status'] == 'verified' else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', nargs='?', choices=('plan', 'upload', 'rollback'), default='plan')
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--backup-receipt', type=Path)
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--jobs', type=Path, default=Path(__file__).resolve().parent / 'jobs')
    parser.add_argument('--curl', default='/usr/bin/curl', help='curl executable; tests use a local stub')
    args = parser.parse_args(argv)
    try:
        if args.phase == 'plan':
            if args.execute: raise ValueError('select upload or rollback explicitly')
            print(json.dumps({'status': 'HOLD', 'remote_path': REMOTE,
                              'next': 'Separate user OK, then an explicit phase and --execute are required.'}))
            return 0
        document = args.manifest if args.phase == 'upload' else args.backup_receipt
        if document is None: raise ValueError('upload requires --manifest; rollback requires --backup-receipt')
        doc = json.loads(document.read_text())
        if doc.get('schema') != 1 or doc.get('remote_path') != REMOTE:
            raise ValueError('receipt/manifest schema or exact single remote path mismatch')
        record = doc['candidate'] if args.phase == 'upload' else doc['preimage']
        data, source = checked_source(record, document.resolve().parent)
        expected = valid_hash(doc['expected_remote_sha256']) if args.phase == 'upload' else None
        if not args.execute:
            print(json.dumps({'status': 'HOLD', 'phase': args.phase, 'remote_path': REMOTE,
                              'source': source, 'expected_remote_sha256': expected,
                              'plan': ['fresh decrypt-OFF backup', 'one PRX upload', 'decrypt-OFF hash verification'],
                              'note': 'No console contact. Execute only after separate user OK.'}))
            return 0
        return execute(args, data, source, expected)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print('REFUSED: ' + str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
