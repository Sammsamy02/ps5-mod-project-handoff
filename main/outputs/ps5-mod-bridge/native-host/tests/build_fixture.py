"""Separate actual C/C++ compilation for development-only ABI/host fixtures."""
import pathlib
import subprocess
import json
import hashlib
import tempfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
PROJECT = ROOT.parent

def compile_fixture(output, c_sources=(), cpp_sources=(), defines=(), sanitize=False,
                    include_dirs=(), libraries=(), link_options=(), source_defines=None):
    output = pathlib.Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    common = ['-Wall', '-Wextra', '-Werror', '-O1', '-g', '-I' + str(ROOT),
              '-I' + str(PROJECT / 'include'), '-isystem', str(PROJECT / 'third_party'),
              '-isystem', str(PROJECT / 'third_party/picosha2')]
    if sanitize: common += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    common += ['-I' + str(directory) for directory in include_dirs]
    objects = []
    for index, (source, compiler, standard) in enumerate(
            [(s, 'cc', 'c11') for s in c_sources] + [(s, 'c++', 'c++20') for s in cpp_sources]):
        obj = output.parent / (output.name + str(index) + '.o')
        extra = (source_defines or {}).get(pathlib.Path(source).name, ())
        subprocess.run([compiler, '-std=' + standard, *common, *defines, *extra,
                        '-c', str(source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    subprocess.run(['c++', *common, *objects, *(str(path) for path in libraries),
                    *link_options, '-o', str(output)], check=True)
    return output

def core_sources():
    return [ROOT / 'core_bridge.cpp', PROJECT / 'src/dds.cpp', PROJECT / 'src/hash.cpp']

def instrument_crypto_fixture(descriptor):
    """Replay the identified host recipe with ASan/UBSan in a new owned tree.
    No vendor/config byte changes and no cache/output overwrite. Both fixture
    and all 81 real library TUs are instrumented, rather than only the adapter.
    """
    cache = pathlib.Path(descriptor['archive']).parent
    evidence = ROOT / 'build/drbg/task2-evidence/sanitizer-builds'
    evidence.mkdir(parents=True, exist_ok=True)
    output = pathlib.Path(tempfile.mkdtemp(prefix='host-', dir=evidence))
    original = json.loads((cache / 'evidence/commands.json').read_text())
    commands, objects, source_hashes = [], [], {}
    for recorded in original:
        command = recorded['argv']
        if '-c' not in command: continue
        old_object = pathlib.Path(command[command.index('-o') + 1])
        old_cache = old_object.parent.parent
        command = [arg.replace(str(old_cache), str(cache)) for arg in command]
        source = pathlib.Path(command[command.index('-c') + 1])
        obj = output / old_object.name
        command[command.index('-o') + 1] = str(obj)
        command[command.index('-MF') + 1] = str(obj) + '.d'
        command[command.index('-c'):command.index('-c')] = [
            '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run(command, capture_output=True, text=True)
        commands.append({'argv':command,'returncode':result.returncode,
                         'stdout':result.stdout,'stderr':result.stderr})
        result.check_returncode()
        objects.append(str(obj))
        source_hashes[source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
    if len(objects) != 81: raise ValueError('fixture requires the exact 81-unit host crypto recipe')
    archive = output / 'libmbedcrypto-sanitized.a'
    command = ['/usr/bin/ar', 'rcs', str(archive), *objects]
    result = subprocess.run(command, capture_output=True, text=True)
    commands.append({'argv':command,'returncode':result.returncode,
                     'stdout':result.stdout,'stderr':result.stderr})
    result.check_returncode()
    manifest = {'scope':'owned sanitizer fixture only; unmodified vendor/config bytes',
                'config_id':descriptor['config_id'], 'base_archive_sha256':descriptor['identity']['archive_sha256'],
                'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
                'source_sha256':source_hashes,
                'object_sha256':{pathlib.Path(obj).name:hashlib.sha256(pathlib.Path(obj).read_bytes()).hexdigest() for obj in objects},
                'commands':commands}
    (output / 'manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
    return archive
