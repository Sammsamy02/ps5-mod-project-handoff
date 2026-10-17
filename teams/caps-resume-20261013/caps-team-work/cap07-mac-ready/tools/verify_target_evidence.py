"""Verify six isolated target objects, actual shared-header routes and handoff pins."""
import sys
sys.dont_write_bytecode = True
import hashlib, json, os, subprocess
from pathlib import Path

HOME = Path(__file__).resolve().parent.parent

def pin(path):
    path = Path(path)
    data = path.read_bytes()
    return {'path': str(path.resolve()), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

def matches_pin(expected):
    actual = pin(expected['path'])
    return actual['bytes'] == expected['bytes'] and actual['sha256'] == expected['sha256']

def main():
    out = HOME / 'evidence/TARGET-VERIFICATION.json'
    if out.exists():
        raise ValueError('fresh owned verification receipt required')
    receipts = [HOME / 'target' / name / 'COMPILE.json' for name in ('attempt2', 'cache-consumers', 'program-cap10-composed-fixed')]
    loaded = [json.loads(path.read_bytes()) for path in receipts]
    baseline = loaded[0]
    llvm_config = Path(baseline['template_unit']['LLVM_CONFIG'])
    env = dict(os.environ, LLVM_CONFIG=str(llvm_config))
    wrapper = Path(baseline['compiler_wrapper']['path'])
    commands = []
    def run(argv):
        result = subprocess.run(list(map(str, argv)), env=env, capture_output=True, text=True, timeout=60)
        commands.append({'argv': list(map(str, argv)), 'exit': result.returncode})
        if result.returncode:
            raise ValueError(result.stderr)
        return result.stdout
    bindir = Path(run([wrapper.with_name('prospero-llvm-config'), '--bindir']).strip())
    compiler = bindir / 'clang++'
    version = run([wrapper, '--version'])
    target_cache = HOME / 'derived/target-ready/outputs/2026-10-08-l26-runtime/profile/units/CodeImageCache.hpp'
    cache_pin = pin(target_cache)
    results, cache_consumers, seen = [], [], set()
    header_snapshot_pins = {row['path']: row for row in baseline['audited_real_header_snapshots']}
    for receipt_path, receipt in zip(receipts, loaded):
        if receipt['stage'] != baseline['stage']:
            raise ValueError('compiled stage identity mismatch')
        if receipt['max_compiler_jobs'] != 1 or not all(row['matches'] for row in receipt['source_header_audit']):
            raise ValueError('compile budget or declared source pin audit failed')
        for result in receipt['results']:
            name = result['name']
            if name in seen or result['exit'] != 0:
                raise ValueError('duplicate or failed target unit')
            seen.add(name)
            for key in ('object', 'dependency', 'stdout', 'stderr'):
                if not matches_pin(result[key]):
                    raise ValueError('compiled artifact pin drift')
            current_dependencies = []
            for dependency in result['actual_dependencies']:
                if not matches_pin(dependency):
                    raise ValueError('actual compile dependency changed: ' + dependency['path'])
                if '/cap07-mac-ready/tests/' in dependency['path']:
                    raise ValueError('test fixture entered actual target dependency graph')
                if Path(dependency['path']).name == 'CodeImageCache.hpp':
                    if dependency != cache_pin:
                        raise ValueError('wrong shared CodeImageCache layout header')
                    current_dependencies.append(dependency)
            if name != 'pakmount':
                if len(current_dependencies) != 1:
                    raise ValueError('expected exactly one current CodeImageCache header per consumer')
                cache_consumers.append(name)
            header = run([bindir / 'llvm-readobj', '--file-header', result['object']['path']])
            if 'Machine: EM_X86_64' not in header or 'Type: Relocatable (0x1)' not in header:
                raise ValueError('expected x86-64 target ELF relocatable object')
            nm = run([bindir / 'llvm-nm', '--demangle', result['object']['path']])
            symbols = [line for line in nm.splitlines() if any(token in line for token in ('RunNativePakMountOnce(', 'PakMount::mount(', 'Cap10::invocation_observation()'))]
            results.append({'name': name, 'object': result['object'], 'dependency_count': len(result['actual_dependencies']),
                            'code_image_cache': current_dependencies, 'selected_symbols': symbols,
                            'elf_header': header, 'diagnostic_warning_count': Path(result['stderr']['path']).read_text().count('warning:')})
    if len(seen) != 6 or len(cache_consumers) != 5:
        raise ValueError('complete six-unit/five-layout-consumer verification required')
    # Ensure consumed current header snapshots and every frozen stage byte remain
    # exactly what the compiler and source rebind receipts identified.
    for path, expected in header_snapshot_pins.items():
        if not matches_pin(expected):
            raise ValueError('reviewed header snapshot changed')
    frozen = json.loads((HOME / 'evidence/FROZEN-INPUT-MANIFEST.json').read_bytes())
    for row in frozen['files']:
        actual = pin(Path(frozen['root']) / row['path'])
        if actual['bytes'] != row['bytes'] or actual['sha256'] != row['sha256']:
            raise ValueError('frozen historical packet changed')
    stage = json.loads((HOME / 'derived/target-ready/INTEGRATION.json').read_bytes())
    originals = []
    root = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
    for row in stage['originals']:
        actual = pin(root / row['path'])
        if actual['bytes'] != row['bytes'] or actual['sha256'] != row['sha256']:
            raise ValueError('live original moved since rebind: ' + row['path'])
        originals.append(actual)
    report = {'label': 'Mac-only target compile/metadata/object verification; no linked game, engine execution, PRX or console',
              'verified_compilation_receipts': [pin(path) for path in receipts], 'compiler_wrapper': pin(wrapper),
              'compiler_dispatch_wrapper': pin(wrapper.with_name('clang++')), 'compiler_executable': pin(compiler),
              'llvm_config': pin(llvm_config), 'compiler_version': version, 'commands': commands,
              'objects': results, 'cache_consumers': cache_consumers, 'shared_cache_header': cache_pin,
              'frozen_packet_files_unchanged': len(frozen['files']), 'live_originals_unchanged': originals,
              'stage_manifest': pin(HOME / 'derived/target-ready/INTEGRATION.json'), 'integration_patch': pin(HOME / 'derived/target-ready/integration.patch'),
              'limits': ['Target compilation does not prove the full next-launch game link, import/constructor/package gates, ABI at execution, allocator/delegate lifetime, pakmod root reachability or engine mount result.', 'Five retained BASE cache-layout consumers are freshly compiled here; combined build must still rebuild its complete selected consumer graph.']}
    out.write_text(json.dumps(report, indent=2) + '\n')
    print('PASS:', len(seen), 'real target objects;', len(cache_consumers), 'consumers of one exact current CodeImageCache header;', len(frozen['files']), 'frozen files unchanged;', len(originals), 'live originals unchanged')

if __name__ == '__main__':
    main()
