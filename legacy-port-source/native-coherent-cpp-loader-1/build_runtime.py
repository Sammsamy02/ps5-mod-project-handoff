#!/usr/bin/env python3
"""Recompile the native C++ loader and every recorded header consumer.

Uses an existing coherent pad composition's per-unit inputs, never its composite
object. The native API is compiled/linked only; no target program is executed.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

def read(p): return json.loads(Path(p).read_text())
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p, value): Path(p).write_text(json.dumps(value, indent=2) + '\n')
def check(p, expected):
    if not Path(p).is_file() or sha(p) != expected:
        raise ValueError('Missing or changed input: ' + str(p))
def dependencies(unit):
    return {str(Path(p).resolve()) for p in Path(unit['dependency_file']).read_text().replace('\\\n', ' ').split()[1:] if Path(p).is_file()}
def run(argv, out, name, env=None):
    result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, env=env)
    (out / (name + '.stdout')).write_text(result.stdout)
    (out / (name + '.stderr')).write_text(result.stderr)
    save(out / (name + '.command.json'), {'argv': list(map(str, argv)), 'exit': result.returncode})
    if result.returncode:
        raise RuntimeError(name + ' failed: ' + result.stderr[-4000:])
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-receipt', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    plan = read(HERE / 'plan.json')
    base = read(args.base_receipt)
    if base['runtime_object_order'] != plan['base_runtime_order']:
        raise ValueError('Base runtime order differs from source contract')
    if {n: u['source_sha256'] for n, u in base['selected_runtime_units'].items()} != plan['base_runtime_source_hashes']:
        raise ValueError('Base runtime sources differ from source contract')
    if {n: u['source_sha256'] for n, u in base['bootstrap_units'].items()} != plan['base_bootstrap_source_hashes']:
        raise ValueError('Base bootstrap sources differ from source contract')
    sealed = ROOT / '.superpowers/native-dependency-spike/native-coherent-runtime-1'
    spec = importlib.util.spec_from_file_location('coherent_build', sealed / 'build_runtime.py')
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    pins = {str(Path(p).resolve()): h for p, h in build.validate_inputs().items()}
    pad_plan = ROOT / '.superpowers/native-dependency-spike/native-coherent-pad-runtime-1/overlay-plan.json'
    check(pad_plan, base['overlay_plan_sha256'])
    for row in read(pad_plan)['sources']:
        p = args.base_receipt.parent / 'source' / Path(row['path']).name
        check(p, row['sha256'])
        pins[str(p.resolve())] = row['sha256']
    units = base['selected_runtime_units']
    boot = base['bootstrap_units']
    for unit in [*units.values(), *boot.values()]:
        check(unit['source'], unit['source_sha256'])
        check(unit['object'], unit['object_sha256'])
        if dependencies(unit) - pins.keys():
            raise ValueError('Unpinned existing dependency: ' + unit['name'])
    old_header = str((sealed / 'source/port/ue4ss/include/Platform/CppModModule.hpp').resolve())
    consumers = [name for name, unit in units.items() if old_header in dependencies(unit)]
    if set(consumers) != set(plan['header_consumers']):
        raise ValueError('Recorded header consumers changed')
    if any(old_header in dependencies(unit) for unit in boot.values()):
        raise ValueError('Unexpected bootstrap header consumer')
    base_inputs = read(args.base_receipt.parent / 'input-hashes.json')
    for p, h in base_inputs.items(): check(p, h)
    for row in plan['overlay_files']: check(ROOT / row['path'], row['sha256'])
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = out / 'source'
    for row in plan['overlay_files']:
        target = source / row['destination']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / row['path'], target)
        pins[str(target)] = row['sha256']
    selected = dict(units)
    for name in consumers:
        old = units[name]
        src = source / 'CppModModule.cpp' if name == 'cppmod-module' else Path(old['source'])
        obj, dep = out / (name + '.o'), out / (name + '.d')
        argv = old['argv'].copy()
        argv[argv.index(old['source'])] = str(src)
        argv[argv.index('-o') + 1] = str(obj)
        argv[argv.index('-MF') + 1] = str(dep)
        argv[1:1] = ['-I', str(source)]
        llvm_config = old.get('LLVM_CONFIG', str(build.LLVM / 'llvm-config'))
        run(argv, out, name, dict(os.environ, LLVM_CONFIG=llvm_config))
        fresh = {**old, 'source': str(src), 'source_sha256': sha(src), 'object': str(obj), 'object_sha256': sha(obj), 'dependency_file': str(dep), 'argv': argv, 'LLVM_CONFIG': llvm_config, 'exit': 0}
        if dependencies(fresh) - pins.keys():
            raise ValueError('Unpinned new dependency: ' + name)
        if str(source / 'Platform/CppModModule.hpp') not in dependencies(fresh):
            raise ValueError('New header was not selected: ' + name)
        for p in dependencies(fresh): check(p, pins[p])
        selected[name] = fresh
    # Preserve every base linker input and its position; replace only the two
    # recorded consumer objects. input-hashes was written in linker input order.
    replacements = {units[name]['object']: selected[name]['object'] for name in consumers}
    inputs = [replacements.get(p, p) for p in base_inputs]
    if base['runtime_object'] in inputs:
        raise ValueError('Composed runtime object must never be an input')
    hashes = {p: sha(p) for p in inputs}
    save(out / 'input-hashes.json', hashes)
    target = out / 'coherent-cpp-loader-runtime.o'
    run([build.LLVM / 'ld.lld', '-m', 'elf_x86_64', '-r', *inputs, '-Map=' + str(out / 'runtime.map'), '-o', target], out, 'link')
    for p, h in hashes.items(): check(p, h)
    for p, h in pins.items(): check(p, h)
    for row in plan['overlay_files']: check(ROOT / row['path'], row['sha256'])
    symbols = run([build.LLVM / 'llvm-nm', '--extern-only', '--undefined-only', '--format=posix', target], out, 'undefined')
    current = {line.split()[0]: line.split()[1] for line in symbols.stdout.splitlines() if line.strip()}
    previous = base['undefined_delta']['current']
    delta = {'added': sorted(current.keys() - previous.keys()), 'removed': sorted(previous.keys() - current.keys()), 'changed_bindings': {n: [previous[n], current[n]] for n in current.keys() & previous.keys() if current[n] != previous[n]}, 'current_count': len(current), 'current': current}
    if any(name not in current for name in plan['required_native_imports']):
        raise ValueError('Native module API import is absent from composed runtime')
    receipt = {'status': 'NATIVE_CPP_LOADER_RELOCATABLE_COMPOSED', 'base_receipt': str(args.base_receipt.resolve()), 'base_receipt_sha256': sha(args.base_receipt), 'plan_sha256': sha(HERE / 'plan.json'), 'runtime_object': str(target), 'runtime_sha256': sha(target), 'runtime_bytes': target.stat().st_size, 'runtime_compilation_units': len(selected), 'freshly_compiled_units': consumers, 'retained_runtime_units': len(selected) - len(consumers), 'runtime_object_order': base['runtime_object_order'], 'selected_runtime_units': selected, 'bootstrap_units': boot, 'retained_fixed_dependencies': base['retained_fixed_dependencies'], 'undefined_delta': delta, 'retained_claude_activation': True, 'reused_composed_runtime_object': False, 'linked_prx': False, 'target_executed': False, 'console_contact': False}
    save(out / 'undefined-delta.json', delta)
    save(out / 'receipt.json', receipt)
    print(json.dumps({k: receipt[k] for k in ['status', 'runtime_bytes', 'runtime_sha256', 'freshly_compiled_units', 'retained_runtime_units']}))
    print(json.dumps({k: delta[k] for k in ['added', 'removed', 'changed_bindings', 'current_count']}))

if __name__ == '__main__': main()
