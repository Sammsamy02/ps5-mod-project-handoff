#!/usr/bin/env python3
"""Bounded standalone owned PAK gate controls; no console/account/publication.

The retained unified SHA1/SHA256 library is intentionally used only for this
isolated legacy fixture. It is not the application's selected curl profile.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
HOST = ROOT / 'outputs/ps5-mod-bridge/native-host'
TESTS = HOST / 'format/pak_prepare_gate_tests'
EVIDENCE = ROOT / '.superpowers/native-dependency-spike'
BASE = ROOT / '.superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak'
ARCHIVE = ROOT / '.superpowers/native-observations/performance-pak-real-mod-intake-1/original.zip'
RECIPE = EVIDENCE / 'pak-archive-job-1/build.json'
BASE_SHA = 'ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867'
ARCHIVE_SHA = '27db7e1b6ea2bf5b574a68a1fa6d7d89ccd5795fd11ac98b4549c2e69132e06e'
TARGET = hashlib.sha256(b'owned offline PAK gate controls; no target discovery').hexdigest()
GATED_SOURCES = [HOST / 'format/pak_archive_job.c',
                 HOST / 'format/examples/pak_mod_union/prepare_union.c',
                 HOST / 'format/pak_stream_writer.c']
LEGACY = EVIDENCE / 'pak-private-preparation-operation-gate-1/implementation-baseline-1'
LEGACY_SHA = ['eb66cce9dcce8a311cdd1156e6889f3bb510e8d9b725f6387a86547e30bf6ced',
              'd36338846dffad4319e1ec341890dd4a2025f39bf0fa81a675beeef4a6b29dcc',
              'b3687b738a6afab6f6df6a24c8f4e027884514b6f8ac29f6879b7ae54fcd65bd']


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            result.update(block)
    return result.hexdigest()


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def run(command, prefix, timeout=30):
    done = subprocess.run(command, capture_output=True, timeout=timeout,
                          text=True, encoding='utf-8', errors='strict')
    prefix.with_suffix('.stdout').write_text(done.stdout)
    prefix.with_suffix('.stderr').write_text(done.stderr)
    return {'argv': command, 'exit': done.returncode,
            'stdout': str(prefix.with_suffix('.stdout')),
            'stderr': str(prefix.with_suffix('.stderr'))}, done


def verify(phase, output, cases):
    output = output.resolve()
    output.relative_to(EVIDENCE.resolve())
    output.mkdir(parents=True, mode=0o700, exist_ok=False)
    recipe = json.loads(RECIPE.read_text())
    if isinstance(recipe, list):
        recipe = recipe[0]
    original = recipe['argv']
    require(digest(BASE) == BASE_SHA and digest(ARCHIVE) == ARCHIVE_SHA,
            'retained input identity changed')
    tracked_inputs = [Path(arg) for arg in original
                      if arg.endswith(('.c', '.a')) and
                      not (phase == 'red' and Path(arg) in GATED_SOURCES)]
    tracked_inputs += [RECIPE, TESTS / 'driver.c', TESTS / 'legacy_gate.c',
                       Path(__file__).resolve(), BASE, ARCHIVE]
    for directory in [HOST / 'archive', HOST / 'pak', HOST / 'format']:
        tracked_inputs += sorted(directory.rglob('*.h'))
    frozen = {}
    if phase == 'red':
        for source, expected in zip(GATED_SOURCES, LEGACY_SHA):
            relative = source.relative_to(HOST)
            copy = output / 'legacy-source' / relative
            copy.parent.mkdir(parents=True, exist_ok=True)
            baseline = LEGACY / source.name
            require(digest(baseline) == expected, 'frozen legacy C identity changed')
            content = baseline.read_bytes()
            copy.write_bytes(content)
            frozen[str(source)] = {'path': str(copy), 'sha256': digest(copy)}
            tracked_inputs += [baseline, copy]
            header = source.with_suffix('.h')
            header_copy = copy.with_suffix('.h.original')
            header_bytes = subprocess.run(
                ['git', 'show', 'a7c8229e2bcb55c80b089d65fc273a3a4b886ec8:' +
                 str(header.relative_to(ROOT))], cwd=ROOT,
                capture_output=True, check=True).stdout
            header_copy.write_bytes(header_bytes)
            tracked_inputs.append(header_copy)
    before = {str(path): digest(path) for path in dict.fromkeys(tracked_inputs)}
    command = []
    for arg in original:
        if arg == str(HOST / 'format/examples/pak_mod_union/archive_merge.c'):
            command.append(str(TESTS / 'driver.c'))
        elif arg in frozen:
            command.append(frozen[arg]['path'])
        else:
            command.append(arg)
    binary = output / 'gate-driver'
    command[command.index('-o') + 1] = str(binary)
    command[1:1] = ['-I' + str(HOST / 'format/examples/pak_mod_union')]
    if phase == 'red':
        command[1:1] = ['-DMB_GATE_LEGACY_BASELINE=1']
        command.insert(command.index('-o'), str(TESTS / 'legacy_gate.c'))
    build, compiled = run(command, output / 'build')
    require(compiled.returncode == 0 and not compiled.stderr,
            'strict standalone fixture compilation failed: ' + compiled.stderr[:400])
    commands, results = [build], []
    passed = True
    for case in cases:
        execute, observed = run([str(binary), str(BASE), str(ARCHIVE), ARCHIVE_SHA,
                                TARGET, str(output / case), case], output / (case + '-run'))
        commands.append(execute)
        lines = [line for line in observed.stdout.splitlines() if line.startswith('{')]
        require(len(lines) == 1, 'driver did not emit one owning receipt: ' + case)
        result = json.loads(lines[0])
        require(result['case'] == case and not observed.stderr,
                'fixture case mismatch or unexpected diagnostics')
        if phase == 'red' and case not in ('legacy', 'default'):
            expected = (observed.returncode == 1 and result['failures'] > 0 and
                        result.get('return_status') == 0 and result.get('complete') == 1 and
                        result.get('gate_calls') == 0)
            if case == 'admission':
                expected &= (result['acquire_calls'] == 1 and result['output_bytes'] == 297439679)
        else:
            expected = observed.returncode == 0 and result['failures'] == 0
            if case not in ('legacy', 'default', 'invalid'):
                expected &= (result['return_status'] == 12 and result['result_status'] == 12 and
                             result['complete'] == 0 and result['gate_calls'] >= 1 and
                             result['reinvoked'] == 0)
        result['expected_observed'] = bool(expected)
        results.append(result)
        passed &= bool(expected)
    after = {path: digest(Path(path)) for path in before}
    unchanged = before == after
    require(unchanged, 'fixture/product/input changed during control')
    receipt = {'phase': phase, 'passed': passed, 'behaviors': results,
               'commands': commands, 'input_sha256': before,
               'inputs_unchanged': unchanged, 'frozen_legacy_sources': frozen,
               'binary_sha256': digest(binary),
               'crypto_scope': 'standalone retained unified SHA1/SHA256; not app',
               'console_executed': False, 'account_or_network_contact': False,
               'runtime_or_installation_claimed': False}
    (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'phase': phase, 'passed': passed, 'behaviors': results,
                      'receipt': str(output / 'receipt.json')}))
    return passed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', required=True, choices=['red', 'green'])
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cases', nargs='+', choices=['admission', 'materialization',
        'partial', 'cpu', 'cleanup', 'legacy', 'default', 'invalid'])
    args = parser.parse_args()
    cases = args.cases or (['admission', 'cpu', 'cleanup'] if args.phase == 'red' else
        ['admission', 'materialization', 'partial', 'cpu', 'cleanup', 'invalid', 'legacy', 'default'])
    raise SystemExit(0 if verify(args.phase, args.output, cases) else 1)
