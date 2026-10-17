#!/usr/bin/env python3
"""Offline ordinary SDK builds. Never dispatch, deploy, or contact a console."""
import argparse
import hashlib
import ipaddress
import json
import os
import pathlib
import shutil
import shlex
import stat
import struct
import subprocess
import tempfile
import zipfile
from crypto_build import prepare_crypto
from web_assets import asset_identity, inspect_assets, write_c_assets

ROOT = pathlib.Path(__file__).resolve().parent
REPOSITORY = ROOT.parents[2]
SOURCE_INPUTS = tuple('outputs/ps5-mod-bridge/' + name for name in (
    'native-host/host.c', 'native-host/http.c', 'native-host/http.h',
    'native-host/entropy.c', 'native-host/entropy.h',
    'native-host/drbg.c', 'native-host/drbg.h', 'native-host/drbg_internal.h',
    'native-host/crypto_config.h', 'native-host/crypto_build.py',
    'native-host/llvm_selected_manifest.json',
    'native-host/core_bridge.cpp', 'native-host/core_bridge.h',
    'native-host/build.py', 'native-host/Makefile', 'native-host/web_assets.py',
    'web/index.html', 'web/app.js', 'web/style.css', 'src/dds.cpp', 'src/hash.cpp',
    'include/modbridge/dds.hpp', 'include/modbridge/core.hpp',
    'third_party/nlohmann/json.hpp', 'third_party/picosha2/picosha2.h'))
SDK_COMMIT = 'd9c9519116944a7f1c22d262012c53b80b3520b7'
SDK_SHA = 'a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb'
LLVM_VERSION = '21.1.8'
TOOL_HASHES = {
    'clang++': '8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
    'clang': '8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
    'ld.lld': '0c4381c70e4229b9988e120710de371c031cedd1d6da8f415a9aa7bac21a3620',
    'llvm-config': '39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5',
    'llvm-readelf': '73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30',
    'llvm-nm': 'a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426',
}

def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''): h.update(block)
    return h.hexdigest()

def extract_sdk(archive, destination):
    """Validate the complete ZIP first; reject links/escapes/duplicates before writes."""
    destination = pathlib.Path(destination)
    with zipfile.ZipFile(archive) as z:
        members = z.infolist()
        seen, total = set(), 0
        for member in members:
            name = member.filename
            path = pathlib.PurePosixPath(name)
            mode = member.external_attr >> 16
            if (not name or '\\' in name or path.is_absolute() or '..' in path.parts or
                    not path.parts or path.parts[0] != 'ps5-payload-sdk' or
                    str(path) in seen or (stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR))):
                raise ValueError('unsafe SDK archive member')
            seen.add(str(path)); total += member.file_size
            if total > 64 * 1024 * 1024 or member.file_size > 16 * 1024 * 1024:
                raise ValueError('SDK archive exceeds extraction bounds')
        if destination.exists():
            if destination.is_symlink(): raise ValueError('SDK destination is a symlink')
            for member in members:
                target = destination / member.filename
                if any(parent.is_symlink() for parent in [target, *target.parents] if parent != destination.parent):
                    raise ValueError('SDK extraction contains a symlink')
                if not member.is_dir() and (not target.is_file() or target.read_bytes() != z.read(member)):
                    raise ValueError('cached SDK extraction does not match verified archive')
            return destination / 'ps5-payload-sdk'
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = pathlib.Path(tempfile.mkdtemp(prefix='sdk-extract-', dir=destination.parent))
        try:
            for member in members:
                target = temporary / member.filename
                if member.is_dir(): target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(z.read(member))
                    target.chmod(0o755 if member.external_attr >> 16 & 0o111 else 0o644)
            os.rename(temporary, destination)
        finally:
            if temporary.exists(): shutil.rmtree(temporary)
    return destination / 'ps5-payload-sdk'

def profile_config(profile, address):
    if profile == 'development_lan':
        if address is None: raise ValueError('development profile requires explicit PS5 IPv4 address')
        try: ip = ipaddress.IPv4Address(address)
        except ipaddress.AddressValueError as exc: raise ValueError('invalid explicit PS5 IPv4 address') from exc
        if (not ip.is_private or ip.is_unspecified or ip.is_loopback or ip.is_multicast or
                ip.is_reserved or ip.is_link_local):
            raise ValueError('development profile requires a private unicast console IPv4 address')
        return {'profile': profile, 'address': str(ip), 'port': 8085, 'lifetime': 600, 'native': True}
    if address is not None: raise ValueError('explicit address is only valid for development_lan')
    if profile not in ('loopback', 'desktop_fixture'): raise ValueError('unknown profile')
    return {'profile': profile, 'address': '127.0.0.1', 'port': 8085,
            'lifetime': 0 if profile == 'loopback' else 600, 'native': profile != 'desktop_fixture'}

def identity(config, crypto_identity=None, assets=None):
    if config['native'] and crypto_identity is None:
        raise ValueError('native identity requires verified crypto dependency identity')
    repository = ROOT.parents[2]
    sources = {name: digest(repository / name) for name in SOURCE_INPUTS}
    if assets is None:
        assets = inspect_assets(ROOT.parent / 'web')
    metadata = {'schema_version': 1, 'version': '0.1.0', 'sdk_release': 'v0.43',
                'sdk_commit': SDK_COMMIT, 'sdk_archive_sha256': SDK_SHA,
                'llvm_version': LLVM_VERSION, 'tool_sha256': TOOL_HASHES,
                'profile': config, 'source_sha256': sources,
                'web_assets': asset_identity(assets)}
    if config['native']: metadata['crypto'] = crypto_identity
    encoded = json.dumps(metadata, sort_keys=True, separators=(',', ':')).encode()
    return hashlib.sha256(encoded).hexdigest(), metadata

def elf_layout(path):
    data = pathlib.Path(path).read_bytes()
    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x01': raise ValueError('not little-endian ELF64')
    e = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    if e[2] != 62 or e[8] != 64 or e[9] != 56 or e[11] != 64 or not e[10] or not e[12]:
        raise ValueError('unsupported ELF tables')
    program_table_end = e[5] + e[9] * e[10]
    section_table_end = e[6] + e[11] * e[12]
    if max(e[8], program_table_end, section_table_end) > len(data):
        raise ValueError('ELF table out of file bounds')
    loads, section_ends = [], []
    for index in range(e[10]):
        ph = struct.unpack_from('<IIQQQQQQ', data, e[5] + index * e[9])
        if ph[0] != 1: continue
        if ph[2] + ph[5] > len(data) or ph[5] > ph[6] or not ph[7] or ph[2] % ph[7] != ph[3] % ph[7]:
            raise ValueError('invalid file-backed load')
        loads.append({'offset': ph[2], 'vaddr': ph[3], 'filesz': ph[5], 'memsz': ph[6],
                      'flags': ph[1], 'alignment': ph[7], 'file_end': ph[2] + ph[5]})
    for index in range(e[12]):
        sh = struct.unpack_from('<IIQQQQIIQQ', data, e[6] + index * e[11])
        if sh[1] == 8: continue
        if sh[4] + sh[5] > len(data): raise ValueError('ELF section out of file bounds')
        section_ends.append(sh[4] + sh[5])
    prefix = max([section_table_end, *section_ends])
    if max(e[8], program_table_end, section_table_end) > prefix:
        raise ValueError('ELF header or table exceeds loader prefix')
    if not loads or any(load['file_end'] > prefix for load in loads): raise ValueError('load exceeds loader prefix')
    if not any(load['flags'] & 1 and load['vaddr'] <= e[4] < load['vaddr'] + load['memsz'] for load in loads):
        raise ValueError('entry outside executable load')
    return {'size': len(data), 'sha256': digest(path), 'machine': 'x86_64', 'type': e[1],
            'entry': e[4], 'loads': loads, 'loader_prefix': prefix, 'section_table_offset': e[6],
            'section_count': e[12], 'elf_header_end': e[8], 'program_table_end': program_table_end,
            'section_table_end': section_table_end, 'max_load_file_end': max(load['file_end'] for load in loads)}

def verify_cpp_driver(directory):
    try:
        matches = digest(pathlib.Path(directory) / 'clang++') == TOOL_HASHES['clang']
    except OSError as exc:
        raise ValueError('cached LLVM identity mismatch: clang++') from exc
    if not matches: raise ValueError('cached LLVM identity mismatch: clang++')

def run_recorded(command, commands, env):
    result = subprocess.run([str(argument) for argument in command], env=env,
                            capture_output=True, text=True)
    commands.append({'argv': [str(argument) for argument in command],
                     'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
    result.check_returncode()
    return result.stdout

def compile_objects(c_compiler, cpp_compiler, output, header, env, web_assets_source,
                    crypto=None, commands=None):
    commands = commands if commands is not None else []
    project = ROOT.parent
    common = ['-Wall', '-Wextra', '-Werror', '-Os', '-include', str(header),
              '-ffile-prefix-map=' + str(ROOT.parents[2]) + '=.',
              '-I', str(ROOT),
              '-I', str(project / 'include'), '-isystem', str(project / 'third_party'),
              '-isystem', str(project / 'third_party/picosha2')]
    objects = []
    for source, compiler, standard in (
            (ROOT / 'host.c', c_compiler, 'c11'), (ROOT / 'http.c', c_compiler, 'c11'),
            (web_assets_source, c_compiler, 'c11'),
            (ROOT / 'entropy.c', c_compiler, 'c11'),
            (ROOT / 'drbg.c', c_compiler, 'c11'),
            (ROOT / 'core_bridge.cpp', cpp_compiler, 'c++20'),
            (project / 'src/dds.cpp', cpp_compiler, 'c++20'),
            (project / 'src/hash.cpp', cpp_compiler, 'c++20')):
        obj = output / (source.stem + '.o')
        consumer = []
        if crypto and standard == 'c11':
            consumer = ['-I' + str(directory) for directory in crypto['include_dirs']]
            consumer += ['-D' + definition for definition in crypto['compile_definitions']]
            consumer += crypto['native_compile_options']
        command = [compiler, '-std=' + standard, *common, *consumer,
                   '-MD', '-MF', str(obj)+'.d', '-c', source, '-o', obj]
        run_recorded(command, commands, env)
        if crypto and source.name == 'drbg.c':
            (output / 'drbg-effective-macros.txt').write_text(run_recorded(
                [compiler, '-std=c11', *common, *consumer, '-dM', '-E', source], commands, env))
        objects.append(str(obj))
    return objects

def include_evidence(output, crypto=None, sdk=None, llvm_bin=None):
    """Retain actual per-TU header bytes; native builds reject host headers."""
    roots = [(output.resolve(), 'generated')]
    if crypto:
        roots += [(pathlib.Path(crypto['archive']).parent.resolve(), 'crypto'),
                  (sdk.resolve(), 'sdk')]
        resource = crypto['native_compile_options'][2]
        roots += [(pathlib.Path(resource).resolve(), 'llvm_resource'),
                  (llvm_bin.parent.resolve(), 'llvm_package')]
    roots += [(REPOSITORY.resolve(), 'application')]
    closure = {}
    for dependency in sorted(output.glob('*.o.d')):
        paths = shlex.split(dependency.read_text().replace('\\\n', ''))[1:]
        entries = []
        for name in dict.fromkeys(paths):
            path = pathlib.Path(name).resolve()
            origin = next((label for directory, label in roots if path.is_relative_to(directory)), 'host')
            if crypto and origin == 'host': raise ValueError('native include closure contains a host header: ' + str(path))
            entries.append({'path':str(path),'origin':origin,'sha256':digest(path)})
        closure[dependency.name] = entries
    return closure

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=('loopback', 'development_lan', 'desktop_fixture'), default='loopback')
    parser.add_argument('--ps5-address')
    parser.add_argument('--mbedtls-archive', type=pathlib.Path)
    parser.add_argument('--framework-archive', type=pathlib.Path)
    parser.add_argument('--llvm-selected-root', type=pathlib.Path)
    parser.add_argument('--output-root', type=pathlib.Path, default=ROOT / 'build/drbg')
    args = parser.parse_args(argv)
    config = profile_config(args.profile, args.ps5_address)
    if args.llvm_selected_root is not None and not config['native']:
        parser.error('--llvm-selected-root requires a native build profile')
    assets = inspect_assets(ROOT.parent / 'web')
    if config['native'] and (args.mbedtls_archive is None or args.framework_archive is None):
        parser.error('native builds require --mbedtls-archive and --framework-archive')
    subprocess.run([os.sys.executable, str(ROOT.parent / 'tools/bootstrap_deps.py')], check=True)
    output = args.output_root.resolve() / args.profile
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    crypto = None
    sdk = llvm_bin = None
    if config['native']:
        cache = ROOT.parent / 'native-probe' / 'build' / 'deps'
        archive = cache / 'sdk.zip'
        if digest(archive) != SDK_SHA or archive.stat().st_size != 8814217:
            raise ValueError('SDK release archive identity mismatch')
        llvm_bin = cache / 'llvm' / 'LLVM-21.1.8-macOS-ARM64' / 'bin'
        verify_cpp_driver(llvm_bin)
        for name, expected in TOOL_HASHES.items():
            if digest(llvm_bin / name) != expected: raise ValueError(f'cached LLVM identity mismatch: {name}')
        sdk = extract_sdk(archive, ROOT / 'build' / 'sdk-v0.43')
        env['LLVM_CONFIG'] = str(llvm_bin / 'llvm-config')
        compiler = sdk / 'bin' / 'prospero-clang'
        cpp_compiler = sdk / 'bin' / 'prospero-clang++'
        crypto = prepare_crypto('native', args.output_root.resolve() / 'dependencies',
                                args.mbedtls_archive, args.framework_archive,
                                sdk=sdk, llvm_bin=llvm_bin,
                                llvm_selected_root=args.llvm_selected_root)
        artifact = output / ('modbridge-native-host-development-lan.elf' if args.profile == 'development_lan'
                             else 'modbridge-native-host-loopback.elf')
    else:
        compiler = 'cc'
        cpp_compiler = 'c++'
        artifact = output / 'modbridge-native-host-desktop'
    build_id, metadata = identity(config, crypto['identity'] if crypto else None, assets)
    header = output / 'config.h'
    header.write_text(f'#define BUILD_ID "{build_id}"\n#define HOST_PROFILE "{config["profile"]}"\n'
                      f'#define HOST_NATIVE {int(config["native"])}\n#define HOST_ADDRESS "{config["address"]}"\n'
                      f'#define HOST_LIFETIME {config["lifetime"]}\n' +
                      (f'#define MB_CRYPTO_CONFIG_ID "{crypto["config_id"]}"\n' if crypto else ''))
    web_assets_source = output / 'web_assets_generated.c'
    write_c_assets(assets, web_assets_source)
    commands = []
    objects = compile_objects(compiler, cpp_compiler, output, header, env, web_assets_source,
                              crypto, commands)
    link = [cpp_compiler, *objects, *([crypto['archive']] if crypto else []),
            *(['-Wl,-Map,' + str(output / 'link.map')] if crypto else []), '-o', artifact]
    run_recorded(link, commands, env)
    manifest = {**metadata, 'build_id': build_id, 'artifact': artifact.name,
                'artifact_sha256': digest(artifact), 'artifact_size': artifact.stat().st_size,
                'owned_launch_contract': 'socket_spawn_only', 'console_executed': False}
    manifest['commands'] = commands
    manifest['object_sha256'] = {pathlib.Path(path).name: digest(path) for path in objects}
    manifest['include_closure'] = include_evidence(output, crypto, sdk, llvm_bin)
    if config['native']:
        manifest['crypto_descriptor'] = crypto
        attribution = output / 'attribution'
        attribution.mkdir(exist_ok=True)
        dependency = pathlib.Path(crypto['archive']).parent
        for tree, name in (('mbedtls', 'MBEDTLS-LICENSE'), ('framework', 'FRAMEWORK-LICENSE')):
            license_path = next((dependency / 'sources' / tree).glob('*/LICENSE'))
            shutil.copyfile(license_path, attribution / name)
        manifest['attribution_sha256'] = {path.name: digest(path) for path in attribution.iterdir()}
        manifest['sdk_source_reference'] = 'https://github.com/ps5-payload-dev/sdk/tree/' + SDK_COMMIT
        manifest['sdk_license_obligation'] = 'GPL-3.0-or-later runtime; preserve SDK/FreeBSD/vendor notices with distribution'
        manifest['sdk_release_license_file'] = 'standalone license file absent from pinned release; full notice packaging handled separately'
        manifest['checkpoint_scope'] = 'ephemeral owned startup only; no hard syscall deadline, TLS, DNS, thread, retained RNG or installation'
        manifest['elf'] = elf_layout(artifact)
        for tool, arguments, name in (('llvm-readelf', ['-h', '-l', '-d'], 'readelf.txt'),
                                     ('llvm-nm', ['--dynamic', '--undefined-only'], 'imports.txt'),
                                     ('llvm-readelf', ['-S'], 'sections.txt'),
                                     ('llvm-nm', [], 'symbols.txt')):
            (output / name).write_text(run_recorded([llvm_bin / tool, *arguments, artifact], commands, env))
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    result = {'artifact': str(artifact), 'build_id': build_id,
              'artifact_sha256': manifest['artifact_sha256'], 'size': manifest['artifact_size']}
    print(json.dumps(result))
    return result

if __name__ == '__main__': main()
