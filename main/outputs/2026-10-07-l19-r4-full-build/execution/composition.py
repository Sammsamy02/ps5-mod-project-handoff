"""Exact147 recompilation and ordered386 composition; all packaging stays inherited."""
import copy
import concurrent.futures
import json
import os
import struct
import subprocess
import time
from pathlib import Path
from admission import OWN, ROOT, PENDING, ROLE, Refusal, require, sha, read, pin, resolved, dependencies


def write(path, data):
    path = Path(path).resolve()
    require(path.is_relative_to(OWN), 'output outside isolated R4 directory')
    path.write_text(json.dumps(data, indent=2) + '\n')


def selected_source(admission, name):
    r = admission.recipes['recipes'][name]
    return admission.records[ROLE[name]] if name in PENDING else r['final_source']


def compile_argv(admission, name, run):
    """Only the frozen source/-o/-MF slots and explicit admitted prefixes change."""
    r = admission.recipes['recipes'][name]
    argv = list(r['original_argv'])
    source = selected_source(admission, name)['path']
    prior = resolved(r['old_selected_source']['path'])
    source_indices = [i for i, token in enumerate(argv) if not token.startswith('-')
                      and resolved(token) == prior]
    require(len(source_indices) == 1, 'exact one original compile source required: ' + name)
    argv[source_indices[0]] = source
    for flag, path in (('-o', run / (name + '.o')), ('-MF', run / (name + '.d'))):
        require(argv.count(flag) == 1 and argv.index(flag) + 1 < len(argv), 'unique output slot required: ' + name)
        argv[argv.index(flag) + 1] = str(path)
    prefix = list(r['include_prefix'])
    if name in PENDING:
        profile_dirs = {Path(admission.records[ROLE[name]]['path']).parent,
                        admission.pins['profile_header'].parent,
                        Path(admission.records['NativeLayoutDeclarations.hpp']['path']).parent}
        prefix += [v for path in sorted(profile_dirs) for v in ('-I', str(path))]
    if name == 'program':
        require(not any('RC_NATIVE_LAYOUT_DECLARATIONS_HEADER' in token for token in argv),
                'preexisting compiled declaration macro refused')
        prefix += ['-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="' + str(admission.pins['generated_header']) + '"']
    return [argv[0], *prefix, *argv[1:]]


def historical_delta(base, current):
    """Measure cumulative lineage against its true pinned197-symbol baseline."""
    path = pin({'path': base['base_receipt'], 'sha256': base['base_receipt_sha256']},
               'historical composition baseline')
    previous = read(path)['undefined_delta']['current']
    measured = dict(added=sorted(current.keys() - previous.keys()),
                    removed=sorted(previous.keys() - current.keys()),
                    changed_bindings={n: [previous[n], current[n]] for n in current.keys() & previous.keys()
                                      if current[n] != previous[n]}, current_count=len(current), current=current)
    inherited = base['undefined_delta']
    require(all(measured[k] == inherited[k] for k in ('added', 'removed', 'changed_bindings')),
            'actual cumulative historical delta differs from admitted R3 lineage')
    return dict(inherited, **measured)


def sdk_nm_evidence(base, nm, command, run, env):
    """Recreate the inherited packager's required captures from actual pinned libraries."""
    import package_prx_l19
    providers = base['undefined_delta']['provider_library_hashes']
    required = sorted(set(package_prx_l19.ALL_EXTENDED_IMPORT_EVIDENCE.values()))
    libraries = {}
    # Authenticate every requested provider before the first tool invocation.
    for filename in required:
        name = filename.removeprefix('sdknm-').removesuffix('.stdout')
        matches = [p for p in providers if Path(p).name == name]
        require(len(matches) == 1, 'one inherited SDK provider pin required: ' + name)
        path = matches[0]
        libraries[filename] = pin({'path': path, 'sha256': providers[path]}, 'SDK provider/' + name)
    evidence = {}
    for filename, library in libraries.items():
        name = filename.removesuffix('.stdout')
        log = command(name, [str(nm), '-D', '--defined-only', '--format=posix', str(library)], run, env)
        path = run / filename
        path.write_text(log['stdout'])
        pin({'path': str(library), 'sha256': providers[str(library)]}, 'SDK provider after capture')
        evidence[name] = {'path': str(path), 'sha256': sha(path),
                          'provider': {'path': str(library), 'sha256': providers[str(library)]},
                          'command': {'path': str(run / (name + '.command.json')),
                                      'sha256': sha(run / (name + '.command.json'))}}
    return evidence


class Composer:
    def __init__(self, admission, invoke=None):
        self.admission = admission
        self.invoke = invoke or self._invoke

    @staticmethod
    def _invoke(argv, env):
        return subprocess.run(list(map(str, argv)), cwd=ROOT, env=env, capture_output=True, text=True)

    def command(self, name, argv, run, env):
        start = time.monotonic()
        result = self.invoke(argv, env)
        log = {'name': name, 'argv': list(map(str, argv)), 'exit': result.returncode,
               'seconds': round(time.monotonic() - start, 3), 'stdout': result.stdout, 'stderr': result.stderr}
        write(run / (name + '.command.json'), log)
        require(result.returncode == 0, 'command failed: ' + name + ':' + result.stderr[-600:])
        return log

    def compose(self, run, pass_number):
        a = self.admission
        # Repeat all pins before invoking even the first actual compiler.
        a.validate()
        require(type(pass_number) is int and pass_number in (1, 2), 'explicit pass1/pass2 required')
        run = Path(run).resolve()
        require(run.is_relative_to(OWN / 'runs') and not run.exists(), 'fresh isolated composition directory required')
        run.mkdir(parents=True)
        import prep_guard
        env = prep_guard.local_environment()
        selected = copy.deepcopy(a.selected)
        fresh, reused, calls, walks = [], [], [], {}
        compile_names = [n for n in a.base['runtime_object_order'] if n in a.recipes['recipes']
                         and not (pass_number == 1 and a.recipes['recipes'][n]['pass1_reusable'])]
        # Only two compiler processes, with distinct object/.d/log outputs. Link order
        # and receipt order are still the exact frozen order, independent of completion.
        def compile_one(name):
            r = a.recipes['recipes'][name]
            argv = compile_argv(a, name, run)
            env_unit = dict(env, LLVM_CONFIG=r['LLVM_CONFIG'])
            return self.command('compile-' + name, argv, run, env_unit)

        logs = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures = {name: pool.submit(compile_one, name) for name in compile_names}
            try:
                for name in compile_names:
                    logs[name] = futures[name].result()
            except BaseException:
                for future in futures.values():
                    future.cancel()
                raise
        for name in a.base['runtime_object_order']:
            if name not in a.recipes['recipes']:
                continue
            r = a.recipes['recipes'][name]
            old = selected[name]
            source = selected_source(a, name)
            if pass_number == 1 and r['pass1_reusable']:
                obj = pin(r['pass1_object'], 'accepted pass1 object/' + name)
                dep = pin(r['pass1_dependency'], 'accepted pass1 dependency/' + name)
                argv = r['pass1_argv']
                reused.append(name)
                elapsed = 0
            else:
                argv = compile_argv(a, name, run)
                obj, dep = run / (name + '.o'), run / (name + '.d')
                log = logs[name]
                elapsed = log['seconds']
                calls.append({k: log[k] for k in ('name', 'argv', 'exit', 'seconds')})
                fresh.append(name)
            require(obj.is_file() and dep.is_file(), 'missing object/dependency pair: ' + name)
            walks[name] = a.check_dependencies(name, dep)
            # Program's actual argv and actual .d must both demonstrate selection.
            if name == 'program':
                wanted = '-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="' + str(a.pins['generated_header']) + '"'
                require(argv.count(wanted) == 1, 'Program macro selection missing/conflicting')
            selected[name] = dict(old, source=source['path'], source_sha256=source['sha256'],
                                 object=str(obj), object_sha256=sha(obj), dependency_file=str(dep),
                                 dependency_sha256=sha(dep), argv=argv, exit=0, seconds=elapsed,
                                 LLVM_CONFIG=r['LLVM_CONFIG'], r4_owner_header_selected=True,
                                 r4_pass1_reused=name in reused)
            if name == 'luamod':
                require(sha(obj) == a.recipes['original_luamod']['object_sha256'],
                        'original exact LuaMod comparator drift')
        require(len(fresh) == (3 if pass_number == 1 else 147) and len(reused) == (144 if pass_number == 1 else 0),
                'actual147 fresh/reuse count mismatch')
        ordered = []
        for slot in a.substitutions['input_slots']:
            name = slot['replace_from_affected_recipe']
            ordered.append(selected[name]['object'] if name else slot['old_input']['path'])
        require(len(ordered) == 386 and len(set(ordered)) == 386, 'exact386 unique ordered inputs required')
        hashes = {p: sha(p) for p in ordered}
        write(run / 'input-hashes.json', hashes)
        target = run / 'coherent-tick-file-witness-runtime.o'
        link = [a.data['linker_invocation'], '-m', 'elf_x86_64', '-r', *ordered,
                '-Map=' + str(run / 'runtime.map'), '-o', str(target)]
        log = self.command('link', link, run, env)
        calls.append({k: log[k] for k in ('name', 'argv', 'exit', 'seconds')})
        raw = target.read_bytes()[:20]
        require(raw[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<HH', raw, 16) == (1, 62),
                'ELF64 x86_64 relocatable required')
        nm = self.command('undefined', [str(a.pins['nm']), '--extern-only', '--undefined-only', '--format=posix', str(target)], run, env)
        current = {v[0]: v[1] for v in (line.split() for line in nm['stdout'].splitlines()) if len(v) >= 2}
        previous = a.base['undefined_delta']['current']
        delta = dict(added=sorted(current.keys() - previous.keys()), removed=sorted(previous.keys() - current.keys()),
                     changed_bindings={n: [previous[n], current[n]] for n in current.keys() & previous.keys()
                                       if current[n] != previous[n]}, current_count=len(current), current=current)
        require(not delta['added'] and not delta['removed'] and not delta['changed_bindings'],
                'new raw composition undefined delta vs actual R3 refused')
        exports = self.command('exports', [str(a.pins['nm']), '--extern-only', '--defined-only', '--format=posix', str(target)], run, env)
        defined = {v[0]: v[1] for v in (line.split() for line in exports['stdout'].splitlines()) if len(v) >= 2}
        rust = read(a.base['rust_receipt'])
        pin({'path': a.base['rust_receipt'], 'sha256': a.base['rust_receipt_sha256']}, 'retained Rust receipt')
        for name, kind in rust['public_exports'].items():
            require(defined.get(name) == kind, 'lost actual Rust ABI export: ' + name)
        pin({'path': a.base['host_receipt'], 'sha256': a.base['host_receipt_sha256']}, 'retained host receipt')
        require(read(a.base['host_receipt'])['status'] == 'OWNED_CPP_FILE_FALLBACK_HOST_PASS', 'retained host connection missing')
        # Rehash all admitted source dependencies and actual link inputs after execution.
        for p, h in {**a.allowed, **hashes, **a.tool_pins}.items():
            pin({'path': p, 'sha256': h}, 'final source/input/tool pin')
        a.validate_tool_aliases()
        for name in a.recipes['recipes']:
            a.check_dependencies(name, selected[name]['dependency_file'])
        inherited_delta = historical_delta(a.base, current)
        sdk_evidence = sdk_nm_evidence(a.base, a.pins['nm'], self.command, run, env)
        receipt = dict(a.base, status='NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED',
                       r4_recomposition_base_receipt=a.recipes['base_receipt'],
                       r4_sdk_provider_evidence=sdk_evidence,
                       runtime_object=str(target), runtime_sha256=sha(target), runtime_bytes=target.stat().st_size,
                       runtime_compilation_units=369, freshly_compiled_units=fresh, retained_runtime_units=369-len(fresh),
                       selected_runtime_units=selected, undefined_delta=inherited_delta, source_hashes=a.allowed,
                       commands=calls, runtime_object_order=list(a.base['runtime_object_order']),
                       reused_composed_runtime_object=False, linked_prx=False, target_executed=False, console_contact=False,
                       r4_execution_admission={'path': str(a.path), 'sha256': a.digest},
                       r4_parent_go={'path': str(a.go_path), 'sha256': a.go_digest},
                       r4_affected_units=147, r4_pass_number=pass_number, r4_reused_accepted_pass1_units=reused,
                       r4_actual_undefined_delta_vs_r3=delta,
                       scope='LOCAL R4 exact147 affected composition; inherited packaging gates still required')
        write(run / 'receipt.json', receipt)
        write(run / 'undefined-delta.json', inherited_delta)
        write(run / 'R4-UNDEFINED-VS-R3.json', delta)
        write(run / 'R4-DEPENDENCIES.json', walks)
        write(run / 'R4-COMPOSITION.json', dict({'pass': True}, affected=147, fresh=len(fresh), reused=len(reused),
                                               inputs=386, selected=369, runtime_sha256=sha(target),
                                               admission_sha256=a.digest, compile_concurrency=2,
                                               linked_prx=False, console_contact=False))
        print(json.dumps({'r4_pass': pass_number, 'fresh': len(fresh), 'reused': len(reused), 'inputs': 386,
                          'runtime_sha256': sha(target)}, sort_keys=True), flush=True)
        return run
