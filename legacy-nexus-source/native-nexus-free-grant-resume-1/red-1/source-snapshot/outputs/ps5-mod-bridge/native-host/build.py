#!/usr/bin/env python3
"""Offline ordinary SDK builds. Never dispatch, deploy, or contact a console."""
import argparse
import hashlib
import ipaddress
import json
import os
import pathlib
import re
import shutil
import shlex
import stat
import struct
import subprocess
import tempfile
import zipfile
from crypto_build import prepare_crypto
from oauth_config import prepare_oauth, write_oauth_header
from web_assets import asset_identity, inspect_assets, write_c_assets

PAK_SOURCES = (
    'format/pak_archive_job.c', 'format/examples/pak_mod_union/prepare_union.c',
    'format/pak_stream_writer.c', 'pak/pak_reader.c', 'pak/pak_primary_capture.c',
    'pak/pak_secondary_integrity.c', 'pak/pak_directory_parser.c', 'pak/pak_reference_graph.c',
    'pak/pak_entry_decoder.c', 'pak/pak_physical_header.c', 'pak/pak_v3_none_reader.c',
    'archive/pak_selection.c')
CONFIG_SOURCES = ('format/config_file_job.c', 'format/ini_scalar_merge.c',
    'format/config_target_selector.c', 'archive/candidate_selector.c',
    'format/examples/archive_config_job/config_archive_scope.c')
PAK_INPUTS = (*PAK_SOURCES, *CONFIG_SOURCES,
    *(str(pathlib.PurePosixPath(path).with_suffix('.h')) for path in (*PAK_SOURCES, *CONFIG_SOURCES)),
    'archive/prepared.h', 'format/private_pak_job.cpp', 'format/private_pak_job.hpp',
    'format/private_ini_job.cpp',
    'download/format_sha1_build.py', 'pak/pak_mbedtls_config.h')
IOSTORE_SOURCES = ('archive/archive_selection.c', 'iostore/original_file_job.c',
    'iostore/toc_reader.c', 'iostore/none_range_reader.c',
    'iostore/container_header_merge.c', 'iostore/none_packer.c',
    'iostore/directory_merge.c', 'iostore/toc_overlay.c',
    'iostore/vendor/blake3/blake3.c', 'iostore/vendor/blake3/blake3_dispatch.c',
    'iostore/vendor/blake3/blake3_portable.c')
IOSTORE_INPUTS = (*IOSTORE_SOURCES,
    *(str(pathlib.PurePosixPath(path).with_suffix('.h')) for path in IOSTORE_SOURCES[:8]),
    'iostore/private_iostore_job.cpp', 'iostore/private_iostore_job.hpp',
    'iostore/schema-lock.json',
    *(('iostore/vendor/blake3/' + name) for name in (
        'blake3.h', 'blake3_impl.h', 'source-lock.json', 'README.md',
        'LICENSE_A2', 'LICENSE_A2LLVM', 'LICENSE_CC0', 'test_vectors.json')))
BLAKE3_LOCK_SHA256 = '234b9f12a320c4e4165ceaea84a24689b9d35c961baf2dccd4522ca6cadead0f'
BLAKE3_DEFINITIONS = ('BLAKE3_NO_SSE2', 'BLAKE3_NO_SSE41', 'BLAKE3_NO_AVX2',
                      'BLAKE3_NO_AVX512', 'BLAKE3_USE_NEON=0')

ROOT = pathlib.Path(__file__).resolve().parent
REPOSITORY = ROOT.parents[2]
SOURCE_INPUTS = tuple('outputs/ps5-mod-bridge/' + name for name in (
    'native-host/host.c', 'native-host/http.c', 'native-host/http.h',
    'native-host/entropy.c', 'native-host/entropy.h',
    'native-host/drbg.c', 'native-host/drbg.h', 'native-host/drbg_internal.h',
    'native-host/crypto_config.h', 'native-host/crypto_build.py',
    'native-host/download/dependency_build.py',
    'native-host/download/curl_download.c', 'native-host/download/curl_download.h',
    'native-host/download/runtime.h', 'native-host/download/provider_transfer.h',
    'native-host/download/owner.c', 'native-host/download/owner.h',
    'native-host/download/ps5_entropy.c', 'native-host/download/ps5_entropy.h',
    'native-host/archive/private/file_sink.c', 'native-host/archive/private/file_sink.h',
    'native-host/dependency-patches/curl-8.18-init-failure.patch',
    'native-host/llvm_selected_manifest.json',
    'native-host/core_bridge.cpp', 'native-host/core_bridge.h',
    'native-host/nexus_client.cpp', 'native-host/nexus_client.h',
    'native-host/nexus_download.cpp', 'native-host/nexus_download.h',
    'native-host/auth.cpp', 'native-host/auth.h', 'native-host/oauth_code.cpp',
    'native-host/oauth_code.h', 'native-host/oauth_config.py',
    'native-host/jobs.cpp', 'native-host/jobs.h', 'native-host/analysis.cpp', 'native-host/analysis.h',
    'native-host/archive/dependency_build.py', 'native-host/archive/link_layout.py', 'native-host/archive/intake.c', 'native-host/archive/intake.h',
    'native-host/archive/groups.c', 'native-host/archive/groups.h',
    'native-host/archive/private/fd_io.c', 'native-host/archive/private/fd_io.h',
    'native-host/archive/private/posix_compat.c', 'native-host/archive/private/posix_compat.h',
    'native-host/download/oauth_code_transfer.h',
    'native-host/nexus_bridge.cpp', 'native-host/nexus_bridge.h',
    'native-host/build.py', 'native-host/Makefile', 'native-host/web_assets.py',
    'web/index.html', 'web/app.js', 'web/style.css', 'src/dds.cpp', 'src/hash.cpp',
    'include/modbridge/dds.hpp', 'include/modbridge/core.hpp',
    'src/nexus.cpp', 'include/modbridge/nexus.hpp',
    'include/modbridge/nexus_requirements_query.h',
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

def job_config(directory):
    if directory is None: return {'enabled': False}
    text = str(directory)
    if (len(text) > 1024 or not re.fullmatch(r'/(?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+', text)
            or any(part in ('.', '..') for part in text.split('/')[1:])):
        raise ValueError('job directory must be an explicit canonical absolute path')
    return {'enabled': True, 'directory': text, 'storage': 'process_lifetime',
            'execution': 'serialized_after_response', 'installation_supported': False}

def pak_config(profile):
    if profile is None: return {'enabled': False}
    path = pathlib.Path(profile).resolve(strict=True)
    value = json.loads(path.read_text())
    if not isinstance(value, dict) or set(value) != {'base_path', 'base_bytes', 'base_sha256', 'nexus_game', 'timeout_ms'}:
        raise ValueError('PAK profile requires exactly captured base path/bytes/SHA256, Nexus source game and timeout')
    if not isinstance(value['base_path'], str): raise ValueError('invalid captured base path')
    job_config(value['base_path'])  # same canonical absolute path grammar
    if type(value['base_bytes']) is not int or not 0 < value['base_bytes'] <= 512 * 1024 * 1024:
        raise ValueError('captured base size exceeds preparation limit')
    if not isinstance(value['base_sha256'], str) or not re.fullmatch('[0-9a-f]{64}', value['base_sha256']):
        raise ValueError('invalid captured base SHA256')
    if not isinstance(value['nexus_game'], str) or not re.fullmatch('[a-z0-9][a-z0-9_-]{0,79}', value['nexus_game']):
        raise ValueError('invalid explicit Nexus source binding')
    if type(value['timeout_ms']) is not int or not 0 < value['timeout_ms'] <= 300000:
        raise ValueError('invalid PAK preparation timeout')
    return {'enabled': True, **value, 'profile_sha256': digest(path),
            'scope': 'trusted_immutable_captured_base_private_preparation',
            'installation_supported': False, 'runtime_supported': False}

def iostore_config(profile):
    if profile is None: return {'enabled': False}
    path = pathlib.Path(profile).resolve(strict=True)
    value = json.loads(path.read_text())
    fields = {'base_toc_path', 'base_toc_bytes', 'base_toc_sha256',
              'base_header_path', 'base_header_bytes', 'base_header_sha256',
              'observed_base_ucas_bytes', 'nexus_game', 'timeout_ms'}
    if not isinstance(value, dict) or set(value) != fields:
        raise ValueError('IoStore profile requires exactly captured TOC/header path/bytes/SHA256, observed UCAS bytes, Nexus source game and timeout')
    for name, cap in (('base_toc', 256 * 1024 * 1024), ('base_header', 64 * 1024 * 1024)):
        if not isinstance(value[name + '_path'], str):
            raise ValueError('invalid captured ' + name + ' path')
        job_config(value[name + '_path'])
        if type(value[name + '_bytes']) is not int or not 0 < value[name + '_bytes'] <= cap:
            raise ValueError('captured ' + name + ' size exceeds preparation limit')
        if not isinstance(value[name + '_sha256'], str) or not re.fullmatch('[0-9a-f]{64}', value[name + '_sha256']):
            raise ValueError('invalid captured ' + name + ' SHA256')
    if value['base_toc_path'] == value['base_header_path']:
        raise ValueError('captured TOC and header paths must be distinct')
    if type(value['observed_base_ucas_bytes']) is not int or not 0 < value['observed_base_ucas_bytes'] <= (1 << 63) - 1:
        raise ValueError('invalid observed base UCAS size')
    if not isinstance(value['nexus_game'], str) or not re.fullmatch('[a-z0-9][a-z0-9_-]{0,79}', value['nexus_game']):
        raise ValueError('invalid explicit Nexus source binding')
    if type(value['timeout_ms']) is not int or not 0 < value['timeout_ms'] <= 300000:
        raise ValueError('invalid IoStore preparation timeout')
    return {'enabled': True, **value, 'profile_sha256': digest(path),
            'scope': 'trusted_immutable_captured_metadata_private_preparation',
            'installation_supported': False, 'runtime_supported': False}

def preparation_enabled(config):
    return any(config.get(name, {}).get('enabled', False)
               for name in ('pak_preparation', 'iostore_preparation'))

def portable_blake3_identity():
    directory = ROOT / 'iostore/vendor/blake3'
    lock_path = directory / 'source-lock.json'
    if digest(lock_path) != BLAKE3_LOCK_SHA256:
        raise ValueError('pinned portable BLAKE3 source lock mismatch')
    lock = json.loads(lock_path.read_text())
    files = {}
    for row in lock['files']:
        path = directory / row['path']
        actual = digest(path)
        if path.stat().st_size != row['bytes'] or actual != row['sha256']:
            raise ValueError('pinned portable BLAKE3 input mismatch: ' + row['path'])
        files[row['path']] = {'bytes': row['bytes'], 'sha256': actual, 'upstream_url': row['url']}
    return {'project': lock['project'], 'tag': lock['tag'], 'commit': lock['commit'],
            'backend': 'portable C only', 'source_lock_sha256': BLAKE3_LOCK_SHA256,
            'compile_definitions': list(BLAKE3_DEFINITIONS), 'files': files,
            'licenses': ['LICENSE_A2', 'LICENSE_A2LLVM', 'LICENSE_CC0'],
            'target_executed': False}

def identity(config, crypto_identity=None, assets=None):
    if config['native'] and crypto_identity is None:
        raise ValueError('native identity requires verified crypto dependency identity')
    repository = ROOT.parents[2]
    sources = {name: digest(repository / name) for name in SOURCE_INPUTS}
    if preparation_enabled(config):
        sources.update({'outputs/ps5-mod-bridge/native-host/' + name: digest(ROOT / name)
                        for name in (*PAK_INPUTS, *IOSTORE_INPUTS)})
    if assets is None:
        assets = inspect_assets(ROOT.parent / 'web')
    metadata = {'schema_version': 1, 'version': '0.1.0', 'sdk_release': 'v0.43',
                'sdk_commit': SDK_COMMIT, 'sdk_archive_sha256': SDK_SHA,
                'llvm_version': LLVM_VERSION, 'tool_sha256': TOOL_HASHES,
                'profile': config, 'source_sha256': sources,
                'web_assets': asset_identity(assets)}
    if crypto_identity is not None: metadata['crypto'] = crypto_identity
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
                    crypto=None, commands=None, *, download=False, native=False, analysis=None,
                    sdk=None, pak=None):
    commands = commands if commands is not None else []
    project = ROOT.parent
    common = ['-Wall', '-Wextra', '-Werror', '-Os', '-include', str(header),
              '-ffile-prefix-map=' + str(ROOT.parents[2]) + '=.',
              '-I', str(ROOT),
              '-I', str(project / 'include'), '-isystem', str(project / 'third_party'),
              '-isystem', str(project / 'third_party/picosha2')]
    objects = []
    sources = [
            (ROOT / 'host.c', c_compiler, 'c11'), (ROOT / 'http.c', c_compiler, 'c11'),
            (web_assets_source, c_compiler, 'c11'),
            (ROOT / 'core_bridge.cpp', cpp_compiler, 'c++20'),
            (project / 'src/dds.cpp', cpp_compiler, 'c++20'),
            (project / 'src/hash.cpp', cpp_compiler, 'c++20')]
    if download:
        sources += [(ROOT / path, c_compiler, 'c11') for path in
                    ('download/owner.c', 'download/curl_download.c', 'archive/private/file_sink.c')]
        sources += [(ROOT / 'nexus_client.cpp', cpp_compiler, 'c++20'),
                    (ROOT / 'nexus_download.cpp', cpp_compiler, 'c++20'),
                    (ROOT / 'oauth_code.cpp', cpp_compiler, 'c++20'),
                    (ROOT / 'auth.cpp', cpp_compiler, 'c++20'),
                    (ROOT / 'nexus_bridge.cpp', cpp_compiler, 'c++20'),
                    (project / 'src/nexus.cpp', cpp_compiler, 'c++20')]
        if native:
            sources.append((ROOT / 'download/ps5_entropy.c', c_compiler, 'c11'))
    else:
        sources += [(ROOT / 'entropy.c', c_compiler, 'c11'),
                    (ROOT / 'drbg.c', c_compiler, 'c11')]
    if analysis:
        sources += [(ROOT / path, c_compiler, 'c11') for path in
                    ('archive/intake.c', 'archive/groups.c', 'archive/private/fd_io.c',
                     'archive/private/posix_compat.c')]
        sources += [(ROOT / 'analysis.cpp', cpp_compiler, 'c++20'),
                    (ROOT / 'jobs.cpp', cpp_compiler, 'c++20')]
    if pak:
        sources += [(ROOT / path, c_compiler, 'c11') for path in (*PAK_SOURCES, *CONFIG_SOURCES, *IOSTORE_SOURCES)]
        sources += [(ROOT / path, cpp_compiler, 'c++20') for path in
                    ('format/private_pak_job.cpp', 'format/private_ini_job.cpp', 'iostore/private_iostore_job.cpp')]
    for source, compiler, standard in sources:
        obj = output / (source.stem + '.o')
        consumer = []
        selected = pak if pak and source.is_relative_to(ROOT) and str(source.relative_to(ROOT)) in PAK_SOURCES else crypto
        if selected and (standard == 'c11' or source.name in ('jobs.cpp', 'private_pak_job.cpp', 'private_ini_job.cpp', 'private_iostore_job.cpp')):
            if native and standard == 'c++20':
                if sdk is None:
                    raise ValueError('native C++ crypto consumers require the verified SDK root')
                # -nostdinc removes the SDK wrapper's implicit C include path.
                # Keep libc++'s C shims before C/resource headers as it requires.
                consumer += ['-isystem', str(sdk / 'target/include/c++/v1'),
                             '-isystem', str(sdk / 'target/include')]
            consumer += ['-I' + str(directory) for directory in selected['include_dirs']]
            consumer += ['-D' + definition for definition in selected['compile_definitions']]
            consumer += selected['native_compile_options']
            for include in selected.get('forced_includes', []):
                consumer += ['-include', str(include)]
        if analysis:
            consumer += analysis['cflags']
        if pak:
            consumer += ['-I' + str(ROOT / directory) for directory in ('archive', 'pak', 'format', 'iostore', 'iostore/vendor/blake3')]
            if not native and os.sys.platform == 'darwin' and source.name == 'original_file_job.c':
                # Match the proven host FD job's POSIX/BSD header contract.
                consumer += ['-D_DARWIN_C_SOURCE=1']
            if source.parent == ROOT / 'iostore/vendor/blake3':
                consumer += ['-D' + definition for definition in BLAKE3_DEFINITIONS]
                if source.name == 'blake3_dispatch.c': consumer += ['-Wno-unused-function']
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
    if crypto and sdk is not None:
        roots += [(pathlib.Path(crypto['archive']).parent.resolve(), 'crypto'),
                  (sdk.resolve(), 'sdk')]
        resource = crypto.get('resource_dir') or crypto['native_compile_options'][2]
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
            if sdk is not None and origin == 'host': raise ValueError('native include closure contains a host header: ' + str(path))
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
    parser.add_argument('--download-profile', type=pathlib.Path,
                        help='explicit retained matching curl/MbedTLS profile; selects the download runtime')
    parser.add_argument('--oauth-config', type=pathlib.Path,
                        help='explicit public registered OAuth client/callback/CA settings for this build')
    parser.add_argument('--curl-source', type=pathlib.Path,
                        help='explicit retained pinned curl 8.18 source for the existing backport')
    parser.add_argument('--archive-profile', type=pathlib.Path,
                        help='explicit matching retained archive/codecs descriptor for download jobs')
    parser.add_argument('--job-directory',
                        help='existing app-owned private scratch directory; enables process-lifetime jobs')
    parser.add_argument('--pak-profile', type=pathlib.Path,
                        help='explicit captured-base profile for private PAK preparation; never a game destination')
    parser.add_argument('--iostore-profile', type=pathlib.Path,
                        help='explicit captured TOC/header profile for private IoStore preparation; never a game destination')
    parser.add_argument('--output-root', type=pathlib.Path, default=ROOT / 'build/drbg')
    args = parser.parse_args(argv)
    config = profile_config(args.profile, args.ps5_address)
    download = args.download_profile is not None
    jobs = args.archive_profile is not None
    if jobs != (args.job_directory is not None):
        parser.error('jobs require both --archive-profile and --job-directory')
    if jobs and (not download or args.oauth_config is None):
        parser.error('jobs require the download runtime and explicit --oauth-config')
    if args.pak_profile is not None and not jobs:
        parser.error('private PAK preparation requires configured archive jobs')
    if args.iostore_profile is not None and not jobs:
        parser.error('private IoStore preparation requires configured archive jobs')
    try:
        config['jobs'] = job_config(args.job_directory)
        config['pak_preparation'] = pak_config(args.pak_profile)
        config['iostore_preparation'] = iostore_config(args.iostore_profile)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    if download != (args.curl_source is not None):
        parser.error('download builds require both --download-profile and --curl-source')
    if download and (args.mbedtls_archive is not None or args.framework_archive is not None or
                     args.llvm_selected_root is not None):
        parser.error('select the download profile or legacy crypto archives, not both')
    if args.llvm_selected_root is not None and not config['native']:
        parser.error('--llvm-selected-root requires a native build profile')
    if args.oauth_config is not None and not download:
        parser.error('--oauth-config requires the download runtime')
    try:
        oauth = prepare_oauth(args.oauth_config, config)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    config['oauth'] = oauth.identity
    if jobs and not oauth.identity['configured']:
        parser.error('jobs require configured OAuth registration and trust')
    assets = inspect_assets(ROOT.parent / 'web')
    if config['native'] and not download and (args.mbedtls_archive is None or args.framework_archive is None):
        parser.error('native builds require --mbedtls-archive and --framework-archive')
    subprocess.run([os.sys.executable, str(ROOT.parent / 'tools/bootstrap_deps.py')], check=True)
    output = args.output_root.resolve() / (args.profile + ('-download' if download else ''))
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    crypto = analysis = pak = blake3 = None
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
        if not download:
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
    if download:
        from download.dependency_build import prepare_download
        crypto = prepare_download('native' if config['native'] else 'host',
                                  args.output_root.resolve() / 'dependencies-download',
                                  args.download_profile, args.curl_source,
                                  sdk=sdk, llvm_bin=llvm_bin)
        if not config['native']:
            compiler, cpp_compiler = '/usr/bin/clang', '/usr/bin/clang++'
        artifact = artifact.with_name(artifact.stem + '-download' + artifact.suffix)
        for key in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'OBJC_INCLUDE_PATH',
                    'LIBRARY_PATH', 'CFLAGS', 'CPPFLAGS', 'CXXFLAGS', 'LDFLAGS'):
            env.pop(key, None)
        config['download_runtime'] = 'curl_8_18_exclusive_owner'
    if jobs:
        from archive.dependency_build import prepare_archive
        analysis = prepare_archive('native' if config['native'] else 'host',
                                   args.archive_profile, sdk=sdk, llvm_bin=llvm_bin)
        config['jobs']['archive_dependency'] = analysis['identity']
    if preparation_enabled(config):
        from download.format_sha1_build import prepare_format_sha1
        pak = prepare_format_sha1('native' if config['native'] else 'host',
                                 args.output_root.resolve() / 'dependencies-format-sha1',
                                 args.download_profile, sdk=sdk, llvm_bin=llvm_bin)
        for name in ('pak_preparation', 'iostore_preparation'):
            if config[name]['enabled']: config[name]['hash_dependency'] = pak['identity']
        blake3 = portable_blake3_identity()
        config['portable_blake3_dependency'] = blake3
    build_id, metadata = identity(config, crypto['identity'] if crypto else None, assets)
    write_oauth_header(oauth, output / 'oauth_config.h')
    header = output / 'config.h'
    header.write_text('#include "oauth_config.h"\n' + f'#define BUILD_ID "{build_id}"\n#define HOST_PROFILE "{config["profile"]}"\n'
                      f'#define HOST_NATIVE {int(config["native"])}\n#define HOST_ADDRESS "{config["address"]}"\n'
                      f'#define HOST_LIFETIME {config["lifetime"]}\n' +
                      f'#define MB_HOST_DOWNLOAD {int(download)}\n' +
                      f'#define MB_HOST_JOBS {int(jobs)}\n' +
                      f'#define MB_HOST_PAK_PREPARATION {int(pak is not None)}\n' +
                      f'#define MB_HOST_PAK_PROFILE {int(config["pak_preparation"]["enabled"])}\n' +
                      f'#define MB_HOST_IOSTORE_PROFILE {int(config["iostore_preparation"]["enabled"])}\n' +
                      (f'#define MB_HOST_JOB_DIRECTORY {json.dumps(config["jobs"]["directory"])}\n' if jobs else '') +
                      (f'#define MB_CRYPTO_CONFIG_ID "{crypto["config_id"]}"\n' if crypto else ''))
    if config['pak_preparation']['enabled']:
        pc = config['pak_preparation']
        with header.open('a') as stream:
            stream.write(f'#define MB_HOST_PAK_BASE_PATH {json.dumps(pc["base_path"])}\n'
                         f'#define MB_HOST_PAK_BASE_BYTES UINT64_C({pc["base_bytes"]})\n'
                         f'#define MB_HOST_PAK_NEXUS_GAME {json.dumps(pc["nexus_game"])}\n'
                         f'#define MB_HOST_PAK_TIMEOUT_MS {pc["timeout_ms"]}u\n'
                         '#define MB_HOST_PAK_BASE_SHA256 {' +
                         ','.join(str(x) for x in bytes.fromhex(pc['base_sha256'])) + '}\n')
    if config['iostore_preparation']['enabled']:
        ic = config['iostore_preparation']
        with header.open('a') as stream:
            for name in ('base_toc', 'base_header'):
                macro = 'MB_HOST_IOSTORE_' + name.upper()
                stream.write(f'#define {macro}_PATH {json.dumps(ic[name + "_path"])}\n'
                             f'#define {macro}_BYTES UINT64_C({ic[name + "_bytes"]})\n'
                             f'#define {macro}_SHA256 {{' +
                             ','.join(str(x) for x in bytes.fromhex(ic[name + '_sha256'])) + '}\n')
            stream.write(f'#define MB_HOST_IOSTORE_BASE_UCAS_BYTES UINT64_C({ic["observed_base_ucas_bytes"]})\n'
                         f'#define MB_HOST_IOSTORE_NEXUS_GAME {json.dumps(ic["nexus_game"])}\n'
                         f'#define MB_HOST_IOSTORE_TIMEOUT_MS {ic["timeout_ms"]}u\n')
    web_assets_source = output / 'web_assets_generated.c'
    write_c_assets(assets, web_assets_source)
    commands = list(crypto.get('commands', [])) if download else []
    link_profile = None
    if config['native']:
        from archive.link_layout import write_linker_script, validate_dynamic_layout
        link_profile = write_linker_script(sdk, output)
    try:
        objects = compile_objects(compiler, cpp_compiler, output, header, env, web_assets_source,
                                  crypto, commands, download=download, native=config['native'],
                                  analysis=analysis, sdk=sdk, pak=pak)
        archives = crypto.get('link_archives', [crypto['archive']]) if crypto else []
        if analysis: archives = [*analysis['link_archives'], *archives]
        if pak: archives = [pak['archive'], *archives]
        map_flag = '-Wl,-Map,' if config['native'] else '-Wl,-map,'
        link = [cpp_compiler, *objects, *archives,
                *([map_flag + str(output / 'link.map')] if crypto else []), '-o', artifact]
        if link_profile: link += ['-Wl,-T,' + link_profile['caller_script']]
        if download:
            link += crypto.get('link_options', [])
            if config['native']:
                # Retain callable clients for the future dispatcher under section
                # collection. Neither symbol creates an HTTP endpoint or session.
                link += ['-Wl,--undefined=mb_curl_download_v1',
                         '-Wl,--undefined=mb_nexus_client_request_v1',
                         '-Wl,--undefined=mb_nexus_resolve_download_v1',
                         '-Wl,--undefined=mb_nexus_auth_resolve_download_v1']
                if pak: link += ['-Wl,--undefined=mb_nexus_jobs_configure_iostore_v1']
        run_recorded(link, commands, env)
        if link_profile: link_profile['dynamic_layout'] = validate_dynamic_layout(artifact)
    finally:
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    manifest = {**metadata, 'build_id': build_id, 'artifact': artifact.name,
                'artifact_sha256': digest(artifact), 'artifact_size': artifact.stat().st_size,
                'owned_launch_contract': 'socket_spawn_only', 'console_executed': False}
    manifest['commands'] = commands
    if link_profile: manifest['link_profile'] = link_profile
    manifest['object_sha256'] = {pathlib.Path(path).name: digest(path) for path in objects}
    manifest['include_closure'] = include_evidence(output, crypto, sdk, llvm_bin)
    if analysis: manifest['archive_descriptor'] = analysis
    if pak: manifest['format_sha1_descriptor'] = pak
    if blake3: manifest['portable_blake3_descriptor'] = blake3
    if download:
        manifest['download_descriptor'] = crypto
        attribution = output / 'attribution'
        attribution.mkdir(exist_ok=True)
        for name, source in crypto['attributions'].items():
            shutil.copyfile(source, attribution / name)
        if analysis:
            for name, notice in analysis['notices'].items():
                shutil.copyfile(notice['path'], attribution / name)
        if blake3:
            for name in blake3['licenses']:
                shutil.copyfile(ROOT / 'iostore/vendor/blake3' / name, attribution / ('BLAKE3-' + name))
        manifest['attribution_sha256'] = {path.name: digest(path) for path in attribution.iterdir()}
        manifest['checkpoint_scope'] = ('exclusive process curl/PSA ownership; PKCE/account/catalog routes require compiled registration/trust; credentials process-lifetime; '
            + ('session-scoped serial download/scan jobs selected with private scratch directory, eight process-lifetime records; ' if jobs else 'download jobs unavailable; ')
            + 'installation unavailable; console unverified; cooperative budgets, no hard syscall/parser interruption deadline')
    elif config['native']:
        manifest['crypto_descriptor'] = crypto
        attribution = output / 'attribution'
        attribution.mkdir(exist_ok=True)
        dependency = pathlib.Path(crypto['archive']).parent
        for tree, name in (('mbedtls', 'MBEDTLS-LICENSE'), ('framework', 'FRAMEWORK-LICENSE')):
            license_path = next((dependency / 'sources' / tree).glob('*/LICENSE'))
            shutil.copyfile(license_path, attribution / name)
        manifest['attribution_sha256'] = {path.name: digest(path) for path in attribution.iterdir()}
        manifest['checkpoint_scope'] = 'ephemeral owned startup only; no hard syscall deadline, TLS, DNS, thread, retained RNG or installation'
    if config['native']:
        manifest['sdk_source_reference'] = 'https://github.com/ps5-payload-dev/sdk/tree/' + SDK_COMMIT
        manifest['sdk_license_obligation'] = 'GPL-3.0-or-later runtime; preserve SDK/FreeBSD/vendor notices with distribution'
        manifest['sdk_release_license_file'] = 'standalone license file absent from pinned release; full notice packaging handled separately'
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
