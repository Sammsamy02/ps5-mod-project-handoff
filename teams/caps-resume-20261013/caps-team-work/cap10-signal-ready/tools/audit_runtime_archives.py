#!/usr/bin/env python3
"""Verify every actual archive member; only mini-posix may differ."""
from pathlib import Path
import argparse, hashlib, json, subprocess

BASE = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--revision', type=int, choices=[1, 2], default=1)
    args = parser.parse_args()
    suffix = '-v2' if args.revision == 2 else ''
    receipt = BASE / ('evidence/runtime-archive-audit' + suffix + '.json')
    assert not receipt.exists(), 'Preserve completed evidence.'
    build = json.loads((BASE / ('evidence/runtime-build' + suffix + '.json')).read_text())
    results = {}
    commands = []
    for mode, data in build['outputs'].items():
        original = next(Path(path) for path, digest in build['inputs_sha256'].items() if digest == data['original_archive_sha256'])
        updated = Path(data['archive'])
        ar = '/usr/bin/ar' if mode == 'host' else str(Path(json.loads((BASE.parent / 'cap10-port-closure/LOCAL-INPUTS.json').read_text())['archive_bin']) / 'llvm-ar')
        maps = []
        for name, archive in [('original', original), ('updated', updated)]:
            out = BASE / ('archive-audit' + suffix) / mode / name
            out.mkdir(parents=True)
            command = [ar, 'x', str(archive)]
            result = subprocess.run(command, cwd=out, capture_output=True, text=True)
            commands.append({'command': command, 'cwd': str(out), 'exit_code': result.returncode})
            assert result.returncode == 0, result.stderr
            maps.append({path.name: sha(path) for path in out.iterdir() if path.is_file()})
        assert maps[0].keys() == maps[1].keys()
        changed = [name for name in maps[0] if maps[0][name] != maps[1][name]]
        assert changed == ['libmini_la-mini-posix.o']
        results[mode] = {'original_archive': str(original), 'original_archive_sha256': sha(original), 'updated_archive': str(updated), 'updated_archive_sha256': sha(updated), 'member_count': len(maps[0]), 'changed_members': changed, 'unchanged_member_count': len(maps[0]) - 1, 'original_member_sha256': maps[0][changed[0]], 'updated_member_sha256': maps[1][changed[0]], 'all_other_members_byte_identical': True}
    receipt.write_text(json.dumps({'verdict': 'ONE_GENUINE_MEMBER_CHANGED_PASS', 'archives': results, 'commands': commands}, indent=2) + '\n')
    print(json.dumps(results, indent=2))

if __name__ == '__main__':
    main()
