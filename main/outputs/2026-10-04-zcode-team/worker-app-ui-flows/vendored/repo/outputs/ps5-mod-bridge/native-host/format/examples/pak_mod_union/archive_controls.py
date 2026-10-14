#!/usr/bin/env python3
"""Exercise ZIP-to-PAK preparation using authored tiny PAK fixtures."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def main():
    if len(sys.argv) != 5:
        raise SystemExit('usage: archive_controls.py ARCHIVE_MERGER FIXTURE_BASE FIXTURE_MOD FRESH_DIRECTORY')
    merger, base, mod = (Path(p).resolve(strict=True) for p in sys.argv[1:4])
    directory = Path(sys.argv[4]).resolve()
    directory.mkdir(exist_ok=False)
    before_base, before_mod = base.read_bytes(), mod.read_bytes()
    token = hashlib.sha256(b'authored archive caller controls').hexdigest()
    receipts = []

    def archive(name, entries):
        path = directory / (name + '.zip')
        with zipfile.ZipFile(path, 'x', compression=zipfile.ZIP_DEFLATED) as z:
            for filename, data in entries:
                z.writestr(filename, data)
        return path

    def run(name, source, component=False):
        quarantine = directory / (name + '-quarantine')
        quarantine.mkdir(mode=0o700)
        output = directory / (name + '.pak')
        raw = source.read_bytes()
        args = [str(merger), str(base), str(source), str(len(raw)),
                hashlib.sha256(raw).hexdigest(), token, str(quarantine), str(output)]
        if component:
            args.append('--pak-component-only')
        p = subprocess.run(args, capture_output=True, text=True, timeout=30)
        require(bool(p.stdout), name + ': no structured result: ' + p.stderr)
        result = json.loads(p.stdout)
        require(source.read_bytes() == raw, name + ': archive changed')
        require(not result.get('installation_supported') and not result.get('runtime_supported'),
                name + ': unsupported installation/runtime claim')
        receipts.append({'case': name, 'exit': p.returncode, 'result': result, 'stderr': p.stderr})
        return p.returncode, result, output, quarantine

    single = archive('single', [('Arbitrary Wrapper/Another Name.PAK', before_mod)])
    code, result, single_output, _ = run('single-default', single)
    require(code == 0 and result['artifact_complete'] and result['member_count'] == 1,
            'single PAK failed automatic discovery')
    require(result['retained_extra_count'] == 0, 'single PAK has unexpected extras')

    members = [('Other Wrapper/Completely Different.pak', before_mod),
               ('Unrelated/Engine.ini', b'[Settings]\nUnknown=1\n'),
               ('README.txt', b'\x00\xffbinary, not assumed documentation')]
    mixed = archive('mixed', members)
    code, result, output, quarantine = run('mixed-default', mixed)
    require(code != 0 and not result['artifact_complete'] and not result['preparation_ready'],
            'default silently accepted extra files')
    require(result['retained_extra_count'] == 2 and result['member_count'] == 3,
            'default lost extra regular rows')
    require(not output.exists() and not list(quarantine.iterdir()), 'default refusal wrote artifacts')
    code, result, output, quarantine = run('mixed-component', mixed, True)
    require(code == 0 and result['artifact_complete'] and not result['whole_bundle_handled'],
            'component mode incorrectly claimed whole bundle or failed')
    require(output.read_bytes() == single_output.read_bytes(), 'archive wrapper changed prepared bytes')
    require(result['member_count'] == 3 and result['retained_extra_count'] == 2,
            'component mode dropped regular rows')
    expected = dict(members)
    for row in result['members']:
        require(row['logical_name'] in expected, 'unexpected member name')
        name = row['retained_name']
        require(name.startswith('member-') and '/' not in name and '\\' not in name,
                'member path used as a filesystem destination')
        data = (quarantine / name).read_bytes()
        require(data == expected.pop(row['logical_name']), 'retained member changed')
        require(hashlib.sha256(data).hexdigest() == row['sha256'], 'member digest mismatch')
    require(not expected, 'regular member omitted from report')

    ambiguous = archive('ambiguous', [('Choice A.pak', before_mod), ('Choice B.pak', before_mod)])
    code, result, output, _ = run('ambiguous-component', ambiguous, True)
    require(code != 0 and not result['preparation_ready'] and not output.exists(),
            'multiple standalone PAKs selected arbitrarily')
    incomplete = archive('incomplete', [('container.pak', before_mod), ('container.utoc', b'incomplete')])
    code, result, output, _ = run('incomplete-component', incomplete, True)
    require(code != 0 and not result['preparation_ready'] and not output.exists(),
            'incomplete IoStore set downgraded to standalone PAK')
    require(base.read_bytes() == before_base and mod.read_bytes() == before_mod, 'input PAK changed')
    record = {'status': 'pass', 'cases': receipts, 'source_inputs_unchanged': True,
              'console_actions': False}
    (directory / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps({'status': 'pass', 'cases': len(receipts), 'console_actions': False}))


if __name__ == '__main__':
    main()
