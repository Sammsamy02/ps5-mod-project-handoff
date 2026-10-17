#!/usr/bin/env python3
"""Focused real-file config gate controls; standalone host preparation only."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
HOST = ROOT / 'outputs/ps5-mod-bridge/native-host'
TESTS = HOST / 'format/config_file_job_gate_tests'
EVIDENCE = ROOT / '.superpowers/native-dependency-spike'
BASE = ROOT / '.superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak'
MEMBER = EVIDENCE / 'unreal-archive-config-pak-composition-2/trial-1/quarantine/member-00000000.bin'
PUBLISHED = ROOT / '.superpowers/native-observations/engine-config-real-mod-intake-2026-10-01/Engine.ini'
RAR = Path('/WORKSPACE/Downloads/Engine 110 1 2026-09-29T21-36Z iFcgTCNBr.rar')
ORACLE = EVIDENCE / 'unreal-config-main-pak-trial-1/trial-1/derived.pak'
RECIPE = EVIDENCE / 'unreal-config-file-job-2/trial-1/receipt.json'
LEGACY = EVIDENCE / 'config-file-job-operation-gates-1/implementation-baseline-1/config_file_job.c'
LEGACY_SHA = '22571b18e3efc1a2a9e60b5a7b43241a008a65106c2c656cc3e13cd671eb4176'
IDENTITIES = {BASE: 'ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867',
    MEMBER: '13a7ed33a3f841c4f150f3b39a188bd6b51483a2558b9da6bd4e6f50cad58e99',
    PUBLISHED: '13a7ed33a3f841c4f150f3b39a188bd6b51483a2558b9da6bd4e6f50cad58e99',
    RAR: '24ea77e211aa95c7800035de62fde127fd514454c3f7cb9957a672a038afb906',
    ORACLE: '8bcf5eb17599e40a02575943eeb6515e7c70e669a74fc962f2d6cbd32aea8d58'}

def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''): result.update(block)
    return result.hexdigest()

def require(ok, message):
    if not ok: raise RuntimeError(message)

def run(argv, prefix):
    result = subprocess.run(argv, capture_output=True, text=True, timeout=30)
    prefix.with_suffix('.stdout').write_text(result.stdout)
    prefix.with_suffix('.stderr').write_text(result.stderr)
    return {'argv': argv, 'exit': result.returncode}, result

def verify(phase, output, cases):
    output = output.resolve(); output.relative_to(EVIDENCE.resolve())
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    require(all(digest(p) == h for p, h in IDENTITIES.items()), 'real fixture identity changed')
    require(MEMBER.read_bytes() == PUBLISHED.read_bytes(), 'selected member is not unchanged published INI')
    require(digest(LEGACY) == LEGACY_SHA, 'frozen ungated C identity changed')
    recipe = json.loads(RECIPE.read_text())['commands'][0]['argv']
    replacements = {'config_file_job.c': LEGACY if phase == 'red' else HOST / 'format/config_file_job.c',
        'host_auto_job.c': TESTS / 'driver.c',
        'config_target_selector.c': HOST / 'format/config_target_selector.c',
        'pak_stream_writer.c': HOST / 'format/pak_stream_writer.c',
        'ini_scalar_merge.c': HOST / 'format/ini_scalar_merge.c'}
    argv = [str(replacements.get(Path(arg).name, arg)) if arg.endswith('.c') else arg for arg in recipe]
    argv[1:1] = ['-I' + str(HOST / 'format')]
    argv[argv.index('-o') + 1] = str(output / 'driver')
    if phase == 'red':
        argv[1:1] = ['-DMB_CONFIG_GATE_LEGACY_BASELINE=1']
        argv.insert(argv.index('-o'), str(TESTS / 'legacy_gate.c'))
    inputs = [Path(arg) for arg in argv if arg.endswith(('.c', '.a'))]
    inputs += [RECIPE, LEGACY.with_suffix('.h'), Path(__file__).resolve(), *IDENTITIES]
    inputs += sorted((HOST / 'format').glob('*.h')) + sorted((HOST / 'pak').glob('*.h'))
    before = {str(p): digest(p) for p in dict.fromkeys(inputs)}
    compile_record, compiled = run(argv, output / 'compile')
    require(compiled.returncode == 0 and not compiled.stderr,
            'strict fixture compilation failed: ' + compiled.stderr[:500])
    commands, behaviors, passed = [compile_record], [], True
    for case in cases:
        execute, observed = run([str(output / 'driver'), str(BASE), str(MEMBER),
                                 str(output / case), case], output / (case + '-run'))
        commands.append(execute)
        lines = [line for line in observed.stdout.splitlines() if line.startswith('{')]
        require(len(lines) == 1 and not observed.stderr, 'fixture did not emit one owning receipt: ' + case)
        behavior = json.loads(lines[0]); require(behavior['case'] == case, 'case receipt mismatch')
        if phase == 'red':
            expected = (observed.returncode == 1 and behavior['failures'] > 0 and
                behavior.get('return_status') == 0 and behavior.get('result_status') == 0 and
                behavior.get('complete') == 1 and behavior.get('gate_calls') == 0 and
                behavior['output_bytes'] == 297483249 and
                digest(output / case / 'artifact.pak') == IDENTITIES[ORACLE])
        else:
            expected = observed.returncode == 0 and behavior['failures'] == 0
            if case not in ('invalid', 'legacy-v1', 'legacy-v2'):
                expected &= (behavior['return_status'] == 14 and behavior['result_status'] == 14 and
                    behavior['complete'] == 0 and behavior['gate_calls'] >= 1 and behavior['reinvoked'] == 0 and
                    behavior['live_bytes'] == 0 and behavior['allocations'] == behavior['releases'])
            if case in ('legacy-v1', 'legacy-v2', 'late'):
                identity = digest(output / case / 'artifact.pak')
                behavior['artifact_sha256'] = identity
                expected &= identity == IDENTITIES[ORACLE]
        behavior['expected_observed'] = bool(expected)
        behaviors.append(behavior); passed &= bool(expected)
    after = {p: digest(Path(p)) for p in before}
    require(before == after, 'source/dependency/fixture mutated during control')
    receipt = {'phase': phase, 'passed': passed, 'behaviors': behaviors,
        'commands': commands, 'input_sha256': before, 'inputs_unchanged': True,
        'scope': 'ordinary-host real selected published INI/captured PAK component proof; retained unified crypto only for standalone fixture',
        'console_executed': False, 'network_contact': False, 'codec_or_dependency_rebuild': False,
        'manual_modification': False, 'runtime_or_installation_claimed': False}
    (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'phase': phase, 'passed': passed, 'behaviors': behaviors,
        'receipt': str(output / 'receipt.json')}))
    return passed

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', required=True, choices=['red', 'green'])
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cases', nargs='+', choices=['admission', 'admission-v1', 'postread',
        'partial', 'late', 'invalid', 'legacy-v1', 'legacy-v2'])
    args = parser.parse_args()
    cases = args.cases or (['admission'] if args.phase == 'red' else
        ['admission', 'admission-v1', 'postread', 'partial', 'late', 'invalid', 'legacy-v1', 'legacy-v2'])
    require(not (args.phase == 'red' and any(c in ('invalid', 'legacy-v1', 'legacy-v2') for c in cases)),
            'red phase is for ignored-gate behavioral controls')
    raise SystemExit(0 if verify(args.phase, args.output, cases) else 1)
