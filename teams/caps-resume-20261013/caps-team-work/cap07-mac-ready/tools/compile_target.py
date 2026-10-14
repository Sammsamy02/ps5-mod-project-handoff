"""Compile only #7 target units from genuine retained metadata, in owned output.

No build driver is imported or run. The selected NEW_UNITS/make_new_units and
rewrite_compile definitions execute unchanged. Current CHANGES and the retained
initializer dependency/overlay metadata provide real header routes; stage paths
are explicitly relocated to the owned mirror. No stub header is introduced.
"""
import sys
sys.dont_write_bytecode = True
import argparse, ast, copy, hashlib, importlib.util, json, os, shlex, subprocess
from pathlib import Path

HOME = Path(__file__).resolve().parent.parent
ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
PREFIX = Path('outputs/2026-10-08-l26-runtime')
BASE = ROOT / 'outputs/2026-10-08-l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def pin(path):
    path = Path(path)
    data = path.read_bytes()
    return {'path': str(path), 'bytes': len(data), 'sha256': digest(data)}

def extract(data, names):
    nodes = [n for n in ast.parse(data).body if
             isinstance(n, ast.FunctionDef) and n.name in names or
             isinstance(n, ast.Assign) and any(isinstance(t, ast.Name) and t.id in names for t in n.targets)]
    if len(nodes) != len(names):
        raise ValueError('selected definition census')
    return ast.fix_missing_locations(ast.Module(body=nodes, type_ignores=[]))

def prepare(stage, out, unit_names=None, program_source=None, program_source_sha256=None, extra_include=(), cap10_program=False):
    stage = Path(stage).resolve()
    out = Path(out).resolve()
    if stage.parent != HOME / 'derived' or out.parent != HOME / 'target' or out.exists():
        raise ValueError('owned stage and fresh target output required')
    spec = importlib.util.spec_from_file_location('stage_selection', HOME / 'tools/dry_run_build_selection.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    _, manifest, files, identity = module.load_stage(stage)
    if not module.audit_originals(manifest)['all_match']:
        raise ValueError('live original drift: deliberate fresh rebind required')
    staged = {str(ROOT / rel): stage / rel for rel in files}
    audited_headers = {}
    if manifest.get('dependency_rebind'):
        decision = Path(manifest['dependency_rebind']['path'])
        if decision != HOME / 'evidence/DEPENDENCY-REBIND.json' or digest(decision.read_bytes()) != manifest['dependency_rebind']['sha256']:
            raise ValueError('explicit dependency rebind receipt drift')
        for row in json.loads(decision.read_bytes())['headers']:
            snapshot = HOME / row['snapshot']
            if digest(snapshot.read_bytes()) != row['actual_sha256'] or snapshot.stat().st_size != row['bytes']:
                raise ValueError('immutable real-header snapshot drift')
            audited_headers[row['path']] = snapshot
    staged.update(audited_headers)
    def actual(path):
        return staged.get(str(Path(path)), Path(path))
    def sha(path):
        return digest(actual(path).read_bytes())
    base_bytes = BASE.read_bytes()
    base = json.loads(base_bytes)
    template = next(u for u in base['selected_units'] if u['name'] == 'unreal-unrealinitializer')
    unit_names = unit_names or ['pakmount', 'native-initializer']
    allowed = {'pakmount', 'native-initializer', 'luamod', 'program', 'native-thread-name-nativethreadnameadapter', 'native-thread-name-threadnameruntime'}
    if not unit_names or len(set(unit_names)) != len(unit_names) or set(unit_names) - allowed:
        raise ValueError('explicit nonempty unique #7/cache-consumer unit list required')
    if bool(program_source) != bool(program_source_sha256):
        raise ValueError('program source override requires its exact SHA pin')
    if cap10_program and (unit_names != ['program'] or not program_source):
        raise ValueError('#10 observer define requires only the explicitly pinned program composition')
    if program_source:
        program_source = Path(program_source).resolve()
        if not program_source.is_relative_to(HOME.parent) or digest(program_source.read_bytes()) != program_source_sha256:
            raise ValueError('program composition must be an exact owned team-stage source')
    for directory in extra_include:
        if not Path(directory).resolve().is_relative_to(HOME.parent) or not Path(directory).is_dir():
            raise ValueError('supplemental include must be an existing owned team-stage directory')
    driver = files[str(PREFIX / 'build_variants.py')]
    env = {'HERE': ROOT / PREFIX, 'Path': Path, 'copy': copy, 'sha': sha}
    exec(compile(extract(driver, {'NEW_UNITS', 'make_new_units', 'header_route_check'}), '<verified stage driver definitions>', 'exec'), env)
    new = env['make_new_units'](base['selected_units'], 1, {}, None)
    pak = next(u for u in new if u['name'] == 'pakmount')
    runtime = ROOT / PREFIX / 'build_runtime.py'
    exec(compile(extract(runtime.read_bytes(), {'rewrite_compile'}), '<current rewrite_compile>', 'exec'), env)
    changes, added, declared = [], [], []
    metadata_pins, source_audit = [], []
    for part in ('profile', 'typescan-r4', 'thread'):
        path = actual(ROOT / PREFIX / part / 'CHANGES.json')
        metadata_pins.append(pin(path))
        metadata = json.loads(path.read_bytes())
        changes.extend(metadata['files'])
        added.extend(metadata.get('added_headers', []))
        declared.extend(metadata.get('include_directories', []))
        for row in metadata['files']:
            for key in ('original', 'candidate'):
                path = actual(row[key])
                observed = pin(path)
                observed.update(expected_sha256=row[key + '_sha256'], matches=observed['sha256'] == row[key + '_sha256'])
                source_audit.append(observed)
        for path, expected in metadata.get('added_header_hashes', {}).items():
            observed = pin(actual(path))
            observed.update(expected_sha256=expected, matches=observed['sha256'] == expected)
            source_audit.append(observed)
    drifts = [p for p in source_audit if not p['matches']]
    if drifts:
        raise ValueError('current declared source/header pin drift: ' + repr(drifts))
    # Same replacement/header and include-directory construction as the runtime
    # selection function, restricted to the actual initializer dependency graph.
    replacements = {str(Path(r['original']).resolve()): r['candidate'] for r in changes}
    headers = [(Path(r['original']).name, r['original_sha256'], r['candidate']) for r in changes
               if Path(r['original']).suffix in ('.hpp', '.h', '.inc', '.inl')]
    dependency = Path(template['dependency'])
    selected_units = [(name, pak if name == 'pakmount' else template if name == 'native-initializer' else next(u for u in base['selected_units'] if u['name'] == name)) for name in unit_names]
    unitdeps = {name: [Path(p).absolute() for p in shlex.split(Path(unit['dependency']).read_text().replace('\\\n', ' '))[1:]] for name, unit in selected_units}
    deps = sorted({path for paths in unitdeps.values() for path in paths})
    inherited = {}
    overlay_inputs = []
    overlay_paths = {u['argv'][i + 1] for u in base['selected_units'] for i, flag in enumerate(u['argv']) if flag == '-ivfsoverlay'}
    for overlay in sorted(overlay_paths):
        overlay_inputs.append(pin(overlay))
        for row in json.loads(Path(overlay).read_bytes())['roots']:
            if row['type'] != 'file':
                raise ValueError('non-file retained overlay')
            name, target = row['name'], row['external-contents']
            if name in inherited and inherited[name] != target:
                rank = lambda path: next((i for i, layer in enumerate(('/2026-10-08-l25-runtime/', '/2026-10-08-l24-runtime/', '/2026-10-08-l23-runtime/')) if layer in path), 99)
                if rank(target) < rank(inherited[name]):
                    inherited[name] = target
            else:
                inherited[name] = target
    aliases = {}
    for path in deps:
        for name, expected, candidate in headers:
            if path.name == name and sha(path) == expected:
                aliases[str(path)] = candidate
                aliases[str(path.resolve())] = candidate
    for original, candidate in replacements.items():
        if Path(original).suffix in ('.hpp', '.h', '.inc', '.inl'):
            aliases[original] = candidate
    rewritten = {}
    for name, target in inherited.items():
        mapped = replacements.get(str(Path(target).resolve()), target)
        if mapped == target:
            for basename, expected, candidate in headers:
                if Path(target).name == basename and sha(target) == expected:
                    mapped = candidate
        rewritten[name] = mapped
        if mapped != target:
            aliases[name] = mapped
    merged = {**rewritten, **aliases}
    # Explicit relocation only for hash-verified #7 stage files. These routes
    # preserve originals/aliases and direct absolute inclusions as well.
    merged = {k: str(actual(v)) for k, v in merged.items()}
    merged.update({k: str(v) for k, v in staged.items()})
    include_dirs = sorted(set(declared) | {str(Path(p).parent) for p in added} |
                          {str(Path(r['candidate']).parent) for r in changes if Path(r['candidate']).suffix in ('.hpp', '.h')})
    owned_dirs = {str(Path(p).parent): str(v.parent) for p, v in staged.items()}
    relocated_dirs = [owned_dirs.get(p, p) for p in include_dirs]
    # An owned partial mirror overlays a genuine full include directory. Keep
    # that genuine directory immediately afterward for unaffected real headers.
    compile_dirs = [str(Path(directory).resolve()) for directory in extra_include]
    for before, after in zip(include_dirs, relocated_dirs):
        compile_dirs.append(after)
        if before != after:
            compile_dirs.append(before)
    out.mkdir(parents=True)
    overlay_path = out / 'headers.vfs.json'
    overlay_path.write_text(json.dumps({'version': 0, 'roots': [{'type': 'file', 'name': k, 'external-contents': v} for k, v in sorted(merged.items())]}, indent=2) + '\n')
    commands = []
    routes = []
    for name, unit in selected_units:
        source = actual(unit['source']) if name == 'pakmount' else actual(ROOT / PREFIX / 'typescan-r4/candidate/src/UnrealInitializer.cpp') if name == 'native-initializer' else actual(replacements.get(str(Path(unit['source']).resolve()), unit['source']))
        if name == 'program' and program_source:
            source = program_source
        argv = env['rewrite_compile'](unit['argv'], unit['source'], source, out / (name + '.o'), out / (name + '.d'))
        argv = [arg for i, arg in enumerate(argv) if arg != '-ivfsoverlay' and not (i and argv[i - 1] == '-ivfsoverlay')]
        argv[1:1] = ['-DMODBRIDGE_L25_ENGINE_ADD_LISTENER=1', '-ivfsoverlay', str(overlay_path), *[a for p in compile_dirs for a in ('-I', p)], '-I', str(Path(unit['source']).parent)]
        if cap10_program:
            argv[1:1] = ['-DMODBRIDGE_CAP10_ADMITTED_MONO_PORT=1']
        route_sel = {'merged_aliases': merged, 'header_specs': [(n, h, str(actual(p))) for n, h, p in headers], 'unitdeps': unitdeps}
        found, violations = env['header_route_check'](route_sel, argv)
        if violations:
            raise ValueError('actual driver header route violations: ' + repr(violations))
        routes.append({'unit': name, 'routes': found, 'violations': violations})
        commands.append({'name': name, 'source': pin(source), 'argv': list(map(str, argv))})
    receipt = {'label': 'Mac-only isolated target translation units; no game link, PRX, driver main, or console', 'stage': identity, 'base_receipt': pin(BASE), 'template_unit': template,
               'definitions': ['NEW_UNITS', 'make_new_units', 'header_route_check', 'rewrite_compile'], 'runtime_helper': pin(runtime), 'metadata_pins': metadata_pins,
               'source_header_audit': source_audit, 'retained_dependency': pin(dependency), 'retained_dependency_matches_receipt': digest(dependency.read_bytes()) == template['dependency_sha256'],
               'retained_overlay_inputs': overlay_inputs, 'header_overlay': pin(overlay_path), 'include_dirs': compile_dirs, 'header_routes': routes,
               'compiler_wrapper': pin(commands[0]['argv'][0]), 'compiler_realpath': str(Path(commands[0]['argv'][0]).with_name('clang++').resolve()),
               'audited_real_header_snapshots': [pin(p) for p in audited_headers.values()],
               'selected_dependency_metadata': [pin(unit['dependency']) for _, unit in selected_units],
               'program_composition': pin(program_source) if program_source else None,
               'max_compiler_jobs': 1, 'commands': commands, 'results': []}
    (out / 'PREPARE.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--compile', action='store_true')
    parser.add_argument('--units', help='Comma-separated explicit units; defaults to pakmount,native-initializer')
    parser.add_argument('--program-source', type=Path)
    parser.add_argument('--program-source-sha256')
    parser.add_argument('--extra-include', type=Path, action='append', default=[])
    parser.add_argument('--cap10-program', action='store_true', help='Enable the actual #10 observer composition in the program-only compile')
    args = parser.parse_args()
    receipt = prepare(args.stage, args.out, args.units.split(',') if args.units else None, args.program_source, args.program_source_sha256, args.extra_include, args.cap10_program)
    if args.compile:
        env = dict(os.environ, LLVM_CONFIG=receipt['template_unit']['LLVM_CONFIG'], CCACHE_DISABLE='1', PYTHONDONTWRITEBYTECODE='1', TMPDIR=str(args.out.resolve()))
        for command in receipt['commands']:
            result = subprocess.run(command['argv'], cwd=ROOT, env=env, capture_output=True, text=True, timeout=600)
            name = command['name']
            (args.out / (name + '.stdout')).write_text(result.stdout)
            (args.out / (name + '.stderr')).write_text(result.stderr)
            evidence = {'name': name, 'exit': result.returncode, 'stdout': pin(args.out / (name + '.stdout')), 'stderr': pin(args.out / (name + '.stderr'))}
            if result.returncode == 0:
                evidence['object'] = pin(args.out / (name + '.o'))
                evidence['dependency'] = pin(args.out / (name + '.d'))
                paths = [Path(p) for p in shlex.split((args.out / (name + '.d')).read_text().replace('\\\n', ' '))[1:]]
                evidence['actual_dependencies'] = [pin(p) for p in paths]
            receipt['results'].append(evidence)
            (args.out / 'COMPILE.json').write_text(json.dumps(receipt, indent=2) + '\n')
            print(name, 'exit', result.returncode, flush=True)
            if result.returncode:
                sys.exit(result.returncode)
    else:
        print('Prepared', len(receipt['commands']), 'genuine target unit commands; compiler not run')

if __name__ == '__main__':
    main()
