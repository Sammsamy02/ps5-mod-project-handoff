#!/usr/bin/env python3
"""Focused host composition: real fixture cache -> catalog -> canonical UI pages."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    project = here.parent.parent
    tests = here / 'catalog_pages_tests'
    out = (args.output_dir or tests / 'build' / str(time.time_ns())).resolve()
    if not out.is_relative_to((tests / 'build').resolve()) or out.exists():
        parser.error('Use a fresh output within catalog_pages_tests/build')
    out.mkdir(parents=True)
    receipt = {'scope': 'host-fixture-composition', 'native_executed': False,
               'provider_enabled': False, 'installation_supported': False,
               'commands': [], 'cases': []}
    inputs = [here / name for name in ('catalog.c', 'catalog.h', 'catalog_test.h',
              'source.c', 'source.h', 'source_test.h', 'metadata.cpp', 'metadata.h',
              'pages.c', 'pages.h', 'catalog_pages_verify.py', 'pages_tests/cross_language.cjs')]
    inputs += [tests / 'caller.c', project / 'web/app.js']
    receipt['inputs'] = [{'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                         for path in inputs]

    def run(command, name):
        result = subprocess.run([str(value) for value in command], capture_output=True, timeout=60)
        (out / name).write_bytes(result.stdout + result.stderr)
        receipt['commands'].append({'exit_code': result.returncode, 'log': name})
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        if result.returncode:
            raise SystemExit((result.stdout + result.stderr).decode(errors='replace')[-8000:])
        if name.endswith('-run.log'):
            print(result.stdout.decode().strip())

    vendor = project / 'third_party'
    common = ['-O2', '-Wall', '-Wextra', '-Werror', '-I' + str(here),
              '-isystem', str(vendor), '-isystem', str(vendor / 'picosha2'),
              '-DMB_TITLE_CATALOG_HOST_TESTING=1', '-DMB_TITLE_SOURCE_HOST_TESTING=1']
    objects = []
    for source in (here / 'catalog.c', here / 'source.c', here / 'metadata.cpp',
                   here / 'pages.c', tests / 'caller.c'):
        cpp = source.suffix == '.cpp'
        obj = out / (source.stem + '.o')
        run(['/usr/bin/clang++' if cpp else '/usr/bin/clang',
             '-std=c++20' if cpp else '-std=c11', *common,
             '-c', source, '-o', obj], source.stem + '-compile.log')
        objects.append(obj)
    binary = out / 'catalog_pages'
    run(['/usr/bin/clang++', *objects, '-o', binary], 'link.log')
    node = os.environ.get('NODE') or '/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node'
    # Keep the owning source fixture within the source API's component bound.
    # macOS /tmp is a symlink; /private/tmp avoids testing through that link.
    fixture_parent = Path(tempfile.mkdtemp(prefix='modbridge-catalog-pages-', dir='/private/tmp'))
    receipt['owned_fixture_root'] = str(fixture_parent)
    fixtures = {
        'generic128': {f'Generic.Title_{n:03}': {
            'titleId': f'Generic.Title_{n:03}', 'contentVersion': '1.🔧',
            'localizedParameters': {'defaultLanguage': 'any-Locale',
                                    'any-Locale': {'titleName': f'Café 星 🧭 / " {n}'}}}
            for n in range(128)},
        'partial': {'Generic.Good': {'titleId': 'Generic.Good'},
                    'Generic.Bad': {'titleId': 17}},
        'empty': {},
    }
    for name, data in fixtures.items():
        case = out / name
        root = fixture_parent / name / 'cache'
        wire = case / 'wire'
        root.mkdir(parents=True)
        wire.mkdir()
        for title, metadata in data.items():
            folder = root / title
            folder.mkdir()
            (folder / 'param.json').write_bytes(json.dumps(metadata, ensure_ascii=False).encode())
        partial = name == 'partial'
        run([binary, root, wire, 1 if partial else 0, 1 if partial else len(data)], name + '-run.log')
        raw_pages = sorted(wire.glob('integration-page-*.json'))
        pages = [json.loads(path.read_bytes()) for path in raw_pages]
        for path, page in zip(raw_pages, pages):
            canonical = json.dumps(page, ensure_ascii=True, sort_keys=True,
                                   separators=(',', ':'), allow_nan=False).encode('ascii')
            assert path.read_bytes() == canonical, name + ': canonical Python bytes'
            assert page['snapshot']['source_scope'] == 'system_private_metadata_cache'
            assert all(item['build_fingerprint'] is None for item in page['items'])
        rows = [row for page in pages for row in page['items']]
        if partial:
            assert len(pages) == 1 and not rows and pages[0]['page'] is None
            assert pages[0]['snapshot']['status'] == 'partial'
            assert pages[0]['snapshot']['snapshot_id'] is None
            assert not (wire / 'integration-current.json').exists()
        else:
            assert {row['title_id']['value'] for row in rows} == set(data)
            assert len({row['title_key'] for row in rows}) == len(data)
            for row in rows:
                metadata = data[row['title_id']['value']]
                assert row['display_name']['value'] == metadata['localizedParameters']['any-Locale']['titleName']
                assert row['build_version']['value'] == metadata['contentVersion']
            for index, page in enumerate(pages):
                assert page['page']['index'] == index and page['page']['page_count'] == len(pages)
                assert page['page']['item_offset'] == sum(len(value['items']) for value in pages[:index])
                assert page['page']['total_count'] == len(data)
                assert (page['page']['next_cursor'] is None) == (index + 1 == len(pages))
            current = json.loads((wire / 'integration-current.json').read_bytes())
            assert current == {'schema_version': 2, 'kind': 'title_inventory_current',
                               'snapshot_id': pages[0]['snapshot']['snapshot_id'], 'current': True}
        run([node, here / 'pages_tests/cross_language.cjs', wire, project / 'web/app.js'],
            name + '-javascript-run.log')
        receipt['cases'].append({'name': name, 'metadata_rows': 1 if partial else len(data),
                                 'published_rows': len(rows), 'pages': len(pages)})
    receipt['inputs_unchanged'] = all(hashlib.sha256(Path(value['path']).read_bytes()).hexdigest() == value['sha256']
                                    for value in receipt['inputs'])
    assert receipt['inputs_unchanged'], 'Composition inputs changed during focused check'
    receipt['result'] = 'PASS'
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'output': str(out), 'native_executed': False}))


if __name__ == '__main__':
    main()
