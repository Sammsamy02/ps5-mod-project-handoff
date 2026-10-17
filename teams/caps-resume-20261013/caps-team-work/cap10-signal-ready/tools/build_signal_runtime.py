#!/usr/bin/env python3
"""Compile only the genuine changed Mono unit; privately replace that archive member."""
from pathlib import Path
import argparse, difflib, hashlib, json, os, shutil, subprocess

BASE = Path(__file__).resolve().parents[1]
TEAM = BASE.parent
HOST = TEAM / 'cap10-mono-build/runtime-work'
TARGET = TEAM / 'cap10-port-closure/target-work'
local = json.loads((TEAM / 'cap10-port-closure/LOCAL-INPUTS.json').read_text())
SDK = Path(local['sdk'])
LLVM = Path(local['llvm_bin'])
ARCHIVE_TOOLS = Path(local['archive_bin'])

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--revision', type=int, choices=[1, 2], default=1)
    args = parser.parse_args()
    suffix = '-v2' if args.revision == 2 else ''
    receipt_path = BASE / ('evidence/runtime-build' + suffix + '.json')
    assert not receipt_path.exists(), 'Preserve completed evidence.'
    original_host = HOST / 'mono/mini/mini-posix.c'
    original_target = TARGET / 'mono/mini/mini-posix.c'
    assert original_host.read_bytes() == original_target.read_bytes()
    source = BASE / 'runtime-source/mono/mini/mini-posix.c'
    source.parent.mkdir(parents=True, exist_ok=True)
    text = original_host.read_text()
    old = 'g_hash_table_remove (mono_saved_signal_handlers, GINT_TO_POINTER (signo));'
    assert text.count(old) == 1
    text = text.replace(old, 'g_hash_table_steal (mono_saved_signal_handlers, GINT_TO_POINTER (signo));')
    old = '\t\tg_assert (sigaction (signo, saved_action, NULL) != -1);\n'
    assert text.count(old) == 1
    text = text.replace(old, old + '\t\tg_free (saved_action);\n')
    if args.revision == 2:
        old = '\tif (saved_handler && saved_handler->sa_handler) {\n'
        assert text.count(old) == 1
        text = text.replace(old, '\tif (saved_handler && saved_handler->sa_handler != SIG_DFL) {\n\t\tif (saved_handler->sa_handler == SIG_IGN)\n\t\t\treturn TRUE;\n')
        text = text.replace(' * was called, false otherwise.', ' * was called or its ignored disposition was respected, false otherwise.')
    source.write_text(text)
    patch = ''.join(difflib.unified_diff(original_host.read_text().splitlines(True), text.splitlines(True), fromfile='a/mono/mini/mini-posix.c', tofile='b/mono/mini/mini-posix.c'))
    patch_path = BASE / ('evidence/mono-signal-lifecycle.patch' if args.revision == 2 else 'evidence/mono-saved-signal-lifetime.patch')
    patch_path.write_text(patch)
    warnings = ['-Wall', '-Wunused', '-Wmissing-declarations', '-Wpointer-arith', '-Wno-cast-qual', '-Wwrite-strings', '-Wno-switch', '-Wno-switch-enum', '-Wno-unused-value', '-Wno-attributes', '-Qunused-arguments', '-Wno-unused-function', '-Wno-tautological-compare', '-Wno-parentheses-equality', '-Wno-self-assign', '-Wno-return-stack-address', '-Wno-constant-logical-operand', '-Wno-zero-length-array', '-Wmissing-prototypes', '-Wstrict-prototypes', '-Wnested-externs', '-Wno-format-zero-length']
    flags = ['-DHAVE_CONFIG_H', '-fvisibility=hidden', *warnings, '-O1', '-std=gnu99', '-fno-strict-aliasing', '-fwrapv', '-DMONO_DLL_EXPORT', '-g', '-Werror=return-type', '-Werror-implicit-function-declaration']
    archives = [HOST / path for path in ['mono/mini/.libs/libmini.a', 'mono/mini/.libs/libmono-ee-interp.a', 'mono/mini/.libs/libmono-dbg.a', 'mono/metadata/.libs/libmonoruntimesgen.a', 'mono/sgen/.libs/libmonosgen.a', 'mono/utils/.libs/libmonoutils.a', 'mono/eglib/.libs/libeglib.a']]
    target_archive = TARGET / 'mono/mini/.libs/libmonosgen-2.0.a'
    frozen = [original_host, original_target, *archives, target_archive, HOST / 'config.h', TARGET / 'config.h', HOST / 'mono/eglib/eglib-config.h', TARGET / 'mono/eglib/eglib-config.h']
    inputs = {str(path): sha(path) for path in frozen}
    commands = []
    logs = []
    env = {key: value for key, value in os.environ.items() if not key.startswith(('MONO_', 'DYLD_'))}
    env['LLVM_CONFIG'] = str(LLVM / 'llvm-config')
    def run(label, command, cwd=BASE):
        result = subprocess.run(command, cwd=cwd, env=env, text=True, capture_output=True)
        commands.append({'label': label, 'command': command, 'cwd': str(cwd), 'exit_code': result.returncode})
        logs.append(label + '\n' + result.stdout + result.stderr)
        (BASE / ('logs/runtime-build' + suffix + '.log')).write_text(''.join(logs))
        assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout
    outputs = {}
    for mode, work, compiler, options in [('host', HOST, '/usr/bin/clang', ['-arch', 'x86_64', '-D_THREAD_SAFE', '-DGC_MACOSX_THREADS', '-DUSE_MMAP', '-DUSE_MUNMAP', '-fno-common', '-DPIC']), ('target', TARGET, str(SDK / 'bin/prospero-clang'), ['-DHOST_BSD', '-DGC_FREEBSD_THREADS', '-D_WITH_GETLINE'])]:
        out = BASE / ('runtime-' + mode + suffix)
        out.mkdir(exist_ok=True)
        obj = out / 'libmini_la-mini-posix.o'
        command = [compiler, *options, *flags, '-I' + str(work / 'mono/mini'), '-I' + str(work), '-I' + str(work / 'mono/eglib'), '-c', str(source), '-o', str(obj)]
        run(mode + '-compile', command)
        original = archives[0] if mode == 'host' else target_archive
        archive = out / original.name
        shutil.copyfile(original, archive)
        ar = '/usr/bin/ar' if mode == 'host' else str(ARCHIVE_TOOLS / 'llvm-ar')
        before = run(mode + '-archive-before', [ar, 't', str(archive)]).splitlines()
        assert before.count(obj.name) == 1
        run(mode + '-replace-member', [ar, 'r', str(archive), str(obj)])
        run(mode + '-ranlib', ['/usr/bin/ranlib' if mode == 'host' else str(ARCHIVE_TOOLS / 'llvm-ranlib'), str(archive)])
        after = run(mode + '-archive-after', [ar, 't', str(archive)]).splitlines()
        assert before == after
        extraction = out / 'verify-member'
        extraction.mkdir(exist_ok=True)
        run(mode + '-extract-member', [ar, 'x', str(archive), obj.name], extraction)
        assert sha(extraction / obj.name) == sha(obj)
        outputs[mode] = {'object': str(obj), 'object_sha256': sha(obj), 'archive': str(archive), 'archive_sha256': sha(archive), 'original_archive_sha256': sha(original), 'archive_members_unchanged': True, 'replaced_member': obj.name, 'member_sha256_verified': True}
    host_archive = Path(outputs['host']['archive'])
    dylib = BASE / ('runtime-host' + suffix + '/libmonosgen-2.0.1.dylib')
    run('host-link', ['/usr/bin/clang', '-arch', 'x86_64', '-dynamiclib', '-Wl,-undefined', '-Wl,dynamic_lookup', '-o', str(dylib), *['-Wl,-force_load,' + str(path) for path in [host_archive, *archives[1:]]], '-framework', 'CoreFoundation', '-framework', 'Foundation', '-lz', '-lm', '-liconv', '-lpthread', '-O1', '-g', '-Wl,-bind_at_load', '-install_name', str(dylib), '-compatibility_version', '2', '-current_version', '2.0'])
    dependencies = run('host-dependencies', ['/usr/bin/otool', '-L', str(dylib)])
    (BASE / ('logs/runtime-host' + suffix + '-dependencies.log')).write_text(dependencies)
    outputs['host'].update(dylib=str(dylib), dylib_sha256=sha(dylib))
    result = {'verdict': 'GENUINE_MONO_SIGNAL_UNIT_BUILD_PASS', 'source': str(source), 'source_sha256': sha(source), 'patch_sha256': sha(patch_path), 'inputs_sha256': inputs, 'frozen_inputs_unchanged': all(sha(Path(path)) == value for path, value in inputs.items()), 'outputs': outputs, 'commands': commands, 'max_compile_jobs': 1, 'target_execution': False, 'prx_build': False, 'config_changed': False, 'layout_changed': False}
    receipt_path.write_text(json.dumps(result, indent=2) + '\n')
    assert result['frozen_inputs_unchanged']
    print(json.dumps({key: value for key, value in result.items() if key not in ['commands', 'inputs_sha256']}, indent=2))

if __name__ == '__main__':
    main()
