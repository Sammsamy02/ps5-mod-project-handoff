#!/usr/bin/env python3
"""Build only the pinned crypto archive, offline, with a verified cache closure.

Native callers provide existing verified SDK/LLVM paths. Additional llvm-ar and
resource-header bytes come exclusively from that LLVM cache's pinned archive.
No historical cache, vendor source, account, device, or network is modified.
"""
import argparse
import gzip
import hashlib
import io
import json
import os
import pathlib
import re
import shlex
import shutil
import stat
import struct
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent
CONFIG_HEADER = ROOT / 'crypto_config.h'
SOURCE_COMMIT = '068ff080b369adfac81509f9b57b2afabaf82dc5'
SOURCE_SHA = 'ca6bd316bbec49ef20088f39b8755fcaec0b7e45781506de55b17cd191ae6937'
FRAMEWORK_COMMIT = 'dde0c4a0e448a0552f18817dcea633bb851fd288'
FRAMEWORK_SHA = '3b0d4864fa877dc9165b90ff3d5b68505c6176c0b22d317d99fe8c89a993e9fd'
SOURCE_ROOT = 'mbedtls-' + SOURCE_COMMIT
FRAMEWORK_ROOT = 'mbedtls-framework-' + FRAMEWORK_COMMIT
SDK_SHA = 'a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb'
SDK_COMMIT = 'd9c9519116944a7f1c22d262012c53b80b3520b7'
LLVM_SHA = 'b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669'
LLVM_PACKAGE = 'LLVM-21.1.8-macOS-ARM64'
LLVM_SELECTED_MANIFEST_PATH = ROOT / 'llvm_selected_manifest.json'
LLVM_SELECTED_MANIFEST_SHA256 = '7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896'
LLVM_SELECTED_DESCRIPTOR_SHA256 = '356e2aaea2485b82b6866383a12357b81b4d2e5fca6c372e8bd127bd620861b2'
LLVM_SELECTED_MAX_FILES = 299
LLVM_SELECTED_MAX_MEMBER_BYTES = 40 * 1024**2
LLVM_SELECTED_MAX_TOTAL_BYTES = 64 * 1024**2
LLVM_SELECTED_MANIFEST_MAX_BYTES = 64 * 1024
TOOL_HASHES = {
    'clang': '8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
    'llvm-config': '39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5',
    'llvm-readelf': '73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30',
    'llvm-nm': 'a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426',
}
# This exact ordered list is checked against the pinned library/CMakeLists.txt.
CRYPTO_SOURCES = tuple('''aes.c aesni.c aesce.c aria.c asn1parse.c asn1write.c
base64.c bignum.c bignum_core.c bignum_mod.c bignum_mod_raw.c block_cipher.c
camellia.c ccm.c chacha20.c chachapoly.c cipher.c cipher_wrap.c constant_time.c
cmac.c ctr_drbg.c des.c dhm.c ecdh.c ecdsa.c ecjpake.c ecp.c ecp_curves.c
entropy.c entropy_poll.c error.c gcm.c hkdf.c hmac_drbg.c lmots.c lms.c md.c md5.c
memory_buffer_alloc.c nist_kw.c oid.c padlock.c pem.c pk.c pk_ecc.c pk_wrap.c
pkcs12.c pkcs5.c pkparse.c pkwrite.c platform.c platform_util.c poly1305.c
psa_crypto.c psa_crypto_aead.c psa_crypto_cipher.c psa_crypto_client.c
psa_crypto_driver_wrappers_no_static.c psa_crypto_ecp.c psa_crypto_ffdh.c
psa_crypto_hash.c psa_crypto_mac.c psa_crypto_pake.c psa_crypto_rsa.c
psa_crypto_random.c psa_crypto_se.c psa_crypto_slot_management.c
psa_crypto_storage.c psa_its_file.c psa_util.c ripemd160.c rsa.c rsa_alt_helpers.c
sha1.c sha256.c sha512.c sha3.c threading.c timing.c version.c version_features.c'''.split())
DEFINITIONS = ['MBEDTLS_USER_CONFIG_FILE="crypto_config.h"']
EFFECTIVE_HEADER = b'''/* Generated from the owned build recipe; include in every Mbed consumer. */
#ifndef MODBRIDGE_CRYPTO_EFFECTIVE_H
#define MODBRIDGE_CRYPTO_EFFECTIVE_H
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#define MODBRIDGE_CRYPTO_CHECK_EFFECTIVE
#include "crypto_config.h"
#undef MODBRIDGE_CRYPTO_CHECK_EFFECTIVE
#endif
'''
PROBE_SOURCE = b'/* Owned public-header macro/compile probe. */\n'
_LLVM_SELECTION = {}


def digest_bytes(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    result = hashlib.sha256()
    with pathlib.Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def _json(data):
    return json.dumps(data, sort_keys=True, separators=(',', ':')).encode()


def _verified_archive(path, expected, size=None):
    path = pathlib.Path(path)
    if path.is_symlink() or not path.is_file():
        raise ValueError('archive missing or not a regular file')
    if (size is not None and path.stat().st_size != size) or digest(path) != expected:
        raise ValueError('archive identity mismatch: ' + path.name)
    return path.resolve()


def _safe_name(name, expected_root):
    path = pathlib.PurePosixPath(name)
    if (not name or '\x00' in name or '\\' in name or path.is_absolute() or
            '..' in path.parts or not path.parts or path.parts[0] != expected_root):
        raise ValueError('unsafe archive member: ' + repr(name))
    return str(path)


def _check_nodes(nodes, name, directory):
    if name in nodes:
        raise ValueError('duplicate normalized archive member: ' + name)
    parts = pathlib.PurePosixPath(name).parts
    if any(nodes.get('/'.join(parts[:i])) is False for i in range(1, len(parts))):
        raise ValueError('archive file is parent of another member')
    if not directory and any(other.startswith(name + '/') for other in nodes):
        raise ValueError('archive file replaces a member directory')
    nodes[name] = directory


def _read_source_archive(archive, expected, expected_root, *, max_members=10000,
                         max_member_bytes=32 * 1024**2, max_total_bytes=128 * 1024**2):
    archive = _verified_archive(archive, expected)
    files, nodes, total = {}, {}, 0
    try:
        # Validate physical TAR records BEFORE tarfile consumes hidden PAX
        # records. Expansion, metadata, padding, counts and payload sizes all
        # have finite bounds, including metadata tarfile does not yield.
        if any(not isinstance(value, int) or value < 0 for value in
               (max_members, max_member_bytes, max_total_bytes)):
            raise ValueError('invalid finite archive extraction bounds')
        raw_limit = max_total_bytes + max_members * 1024 + 10240
        with gzip.open(archive, 'rb') as stream:
            payload = stream.read(raw_limit + 1)
        if len(payload) > raw_limit:
            raise ValueError('archive decompression exceeds finite bounds')
        offset, records, raw_total = 0, 0, 0
        while offset < len(payload):
            header = payload[offset:offset + 512]
            if len(header) != 512:
                raise ValueError('invalid TAR header length')
            if header == bytes(512):
                if any(payload[offset:]):
                    raise ValueError('unexpected bytes after TAR end marker')
                break
            record = tarfile.TarInfo.frombuf(header, 'utf-8', 'surrogateescape')
            records += 1
            raw_total += record.size
            if (records > max_members or record.size < 0 or
                    record.size > max_member_bytes or raw_total > max_total_bytes):
                raise ValueError('raw archive metadata/payload exceeds finite bounds')
            offset += 512 + (record.size + 511) // 512 * 512
            if offset > len(payload):
                raise ValueError('truncated raw TAR member')
        with tarfile.open(fileobj=io.BytesIO(payload), mode='r:') as incoming:
            for member in incoming:
                name = _safe_name(member.name, expected_root)
                if not (member.isfile() or member.isdir()):
                    raise ValueError('archive links and special members are forbidden')
                _check_nodes(nodes, name, member.isdir())
                total += member.size
                if (len(nodes) > max_members or member.size < 0 or
                        member.size > max_member_bytes or total > max_total_bytes):
                    raise ValueError('archive extraction exceeds finite bounds')
                if member.isfile():
                    with incoming.extractfile(member) as stream:
                        data = stream.read(max_member_bytes + 1)
                    if len(data) != member.size:
                        raise ValueError('archive member length mismatch')
                    files[name] = data
    except (tarfile.TarError, OSError) as error:
        raise ValueError('invalid source archive') from error
    if not files:
        raise ValueError('archive contains no source files')
    return files


def _tree_hashes(directory):
    directory = pathlib.Path(directory)
    if directory.is_symlink() or not directory.is_dir():
        raise ValueError('cache directory missing or symlink')
    result = {}
    for path in directory.rglob('*'):
        if path.is_symlink() or not (path.is_dir() or path.is_file()):
            raise ValueError('cache contains a symlink or special file')
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = digest(path)
    return result


def _check_tree(directory, expected):
    if _tree_hashes(directory) != expected:
        raise ValueError('cache bytes or file closure do not match verified inputs')


def _write_files(directory, files, executable=()):
    for name, data in files.items():
        path = pathlib.Path(directory) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(0o755 if name in executable else 0o644)


def extract_source_archive(archive, destination, expected_sha256, expected_root, **limits):
    """Validate every TAR member before writing, or verify the complete cache."""
    files = _read_source_archive(archive, expected_sha256, expected_root, **limits)
    destination = pathlib.Path(destination)
    expected = {name: digest_bytes(data) for name, data in files.items()}
    if destination.exists() or destination.is_symlink():
        _check_tree(destination, expected)
    else:
        destination.parent.mkdir(parents=True, exist_ok=True)
        staging = pathlib.Path(tempfile.mkdtemp(prefix='crypto-source-', dir=destination.parent))
        try:
            _write_files(staging, files)
            staging.rename(destination)
        finally:
            if staging.exists():
                shutil.rmtree(staging)
    return destination.resolve() / expected_root


def configuration_identity(overlay):
    """Host and native share this semantic configuration, independent of paths."""
    recipe = {'source_archive_sha256': SOURCE_SHA, 'framework_archive_sha256': FRAMEWORK_SHA,
              'overlay_sha256': digest_bytes(overlay), 'effective_header_sha256': digest_bytes(EFFECTIVE_HEADER),
              'recipe_sha256': digest(pathlib.Path(__file__)), 'sources': CRYPTO_SOURCES,
              'definitions': DEFINITIONS, 'c_standard': 'c11', 'optimization': 'Os'}
    return digest_bytes(_json(recipe))


def _safe_llvm_link(name, target, hard):
    if not target or '\x00' in target or '\\' in target or pathlib.PurePosixPath(target).is_absolute():
        raise ValueError('unsafe LLVM archive link')
    parts = [] if hard else list(pathlib.PurePosixPath(name).parent.parts)
    for part in pathlib.PurePosixPath(target).parts:
        if part == '..':
            if len(parts) <= 1:
                raise ValueError('LLVM archive link escapes package')
            parts.pop()
        elif part != '.':
            parts.append(part)
    if not parts or parts[0] != LLVM_PACKAGE:
        raise ValueError('LLVM archive link escapes package')


def _unique_json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('LLVM selected manifest has duplicate keys')
        result[key] = value
    return result


def _read_llvm_selected_manifest():
    path = pathlib.Path(LLVM_SELECTED_MANIFEST_PATH)
    try:
        info = path.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_size > LLVM_SELECTED_MANIFEST_MAX_BYTES:
            raise ValueError('LLVM selected manifest is missing or not a regular file')
        raw = _read_bounded_regular_file(path, info.st_size,
                                        LLVM_SELECTED_MANIFEST_MAX_BYTES)
    except OSError as error:
        raise ValueError('LLVM selected manifest is unavailable') from error
    manifest_sha = digest_bytes(raw)
    if manifest_sha != LLVM_SELECTED_MANIFEST_SHA256:
        raise ValueError('LLVM selected manifest identity pin mismatch')
    try:
        document = json.loads(raw, object_pairs_hook=_unique_json_object)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError('LLVM selected manifest is invalid JSON') from error
    expected_keys = {'schema_version', 'package', 'archive_sha256',
                     'source_descriptor_sha256', 'member_count', 'members'}
    if not isinstance(document, dict) or set(document) != expected_keys:
        raise ValueError('LLVM selected manifest schema mismatch')
    if (document['schema_version'] != 1 or document['package'] != LLVM_PACKAGE or
            document['archive_sha256'] != LLVM_SHA or
            document['source_descriptor_sha256'] != LLVM_SELECTED_DESCRIPTOR_SHA256):
        raise ValueError('LLVM selected manifest package identity mismatch')
    members = document['members']
    if (not isinstance(members, dict) or document['member_count'] != LLVM_SELECTED_MAX_FILES or
            len(members) != LLVM_SELECTED_MAX_FILES):
        raise ValueError('LLVM selected manifest member count mismatch')
    for name, expected in members.items():
        if not isinstance(name, str):
            raise ValueError('LLVM selected manifest contains a non-text member path')
        path = pathlib.PurePosixPath(name)
        if (not name or '\x00' in name or '\\' in name or
                path.is_absolute() or path.as_posix() != name or
                any(part in ('', '.', '..') for part in path.parts) or
                not (name == 'bin/llvm-ar' or name.startswith('lib/clang/21/include/') or
                     name in ('LICENSE.TXT', 'LICENSE', 'NOTICE', 'include/llvm/Support/LICENSE.TXT'))):
            raise ValueError('LLVM selected manifest contains an unsafe or unselected path')
        if (not isinstance(expected, str) or len(expected) != 64 or
                any(char not in '0123456789abcdef' for char in expected)):
            raise ValueError('LLVM selected manifest member digest is invalid')
    if 'bin/llvm-ar' not in members or 'lib/clang/21/include/stddef.h' not in members:
        raise ValueError('LLVM selected manifest lacks required tool/resource members')
    return members, manifest_sha


def _reject_symlink_path_components(path):
    path = pathlib.Path(path)
    if not path.is_absolute():
        path = pathlib.Path.cwd() / path
    if '..' in path.parts:
        raise ValueError('LLVM selected root path has a parent traversal component')
    current = pathlib.Path(path.anchor)
    for part in path.parts[1:]:
        current = current / part
        try:
            mode = current.lstat().st_mode
        except OSError as error:
            raise ValueError('LLVM selected root path is unavailable') from error
        if stat.S_ISLNK(mode):
            raise ValueError('LLVM selected root has a symlink component')
    if not path.is_dir():
        raise ValueError('LLVM selected root is not a directory')
    return path


def _read_bounded_regular_file(path, expected_size, maximum):
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        raise ValueError('LLVM selected member cannot be opened safely') from error
    try:
        before = os.fstat(descriptor)
        if (not stat.S_ISREG(before.st_mode) or before.st_nlink != 1 or
                before.st_size != expected_size or before.st_size > maximum):
            raise ValueError('LLVM selected member type or size changed')
        chunks, total = [], 0
        with os.fdopen(descriptor, 'rb', closefd=False) as stream:
            while True:
                block = stream.read(min(1024 * 1024, maximum + 1 - total))
                if not block:
                    break
                chunks.append(block)
                total += len(block)
                if total > maximum:
                    raise ValueError('LLVM selected member exceeds finite bounds')
        after = os.fstat(descriptor)
        if (total != expected_size or after.st_size != expected_size or
                before.st_dev != after.st_dev or before.st_ino != after.st_ino):
            raise ValueError('LLVM selected member changed while being captured')
        return b''.join(chunks)
    finally:
        os.close(descriptor)


def _read_llvm_selected_root(root):
    """Capture the exact pinned selected members once, before any staging write."""
    members, manifest_sha = _read_llvm_selected_manifest()
    root = _reject_symlink_path_components(root)
    expected_dirs = set()
    for name in members:
        parts = pathlib.PurePosixPath(name).parts
        expected_dirs.update('/'.join(parts[:index]) for index in range(1, len(parts)))
    actual_files, actual_dirs, file_stats = set(), set(), {}
    stack = [(root, '')]
    while stack:
        directory, prefix = stack.pop()
        try:
            entries = os.scandir(directory)
        except OSError as error:
            raise ValueError('LLVM selected root cannot be enumerated') from error
        with entries:
            for entry in entries:
                relative = (prefix + '/' if prefix else '') + entry.name
                try:
                    info = entry.stat(follow_symlinks=False)
                except OSError as error:
                    raise ValueError('LLVM selected member metadata is unavailable') from error
                if stat.S_ISDIR(info.st_mode):
                    actual_dirs.add(relative)
                    if len(actual_dirs) > len(expected_dirs):
                        raise ValueError('LLVM selected root contains extra directories')
                    stack.append((pathlib.Path(entry.path), relative))
                elif stat.S_ISREG(info.st_mode):
                    actual_files.add(relative)
                    if relative not in members or len(actual_files) > LLVM_SELECTED_MAX_FILES:
                        raise ValueError('LLVM selected root contains an extra member')
                    if (info.st_nlink != 1 or info.st_size < 0 or
                            info.st_size > LLVM_SELECTED_MAX_MEMBER_BYTES):
                        raise ValueError('LLVM selected member type or size exceeds bounds')
                    file_stats[relative] = info
                else:
                    raise ValueError('LLVM selected root contains a link or special member')
    if actual_files != set(members) or actual_dirs != expected_dirs:
        raise ValueError('LLVM selected root member closure mismatch')
    total_size = sum(info.st_size for info in file_stats.values())
    if total_size > LLVM_SELECTED_MAX_TOTAL_BYTES:
        raise ValueError('LLVM selected root exceeds aggregate byte bounds')
    archiver_info = file_stats['bin/llvm-ar']
    if not archiver_info.st_mode & 0o111:
        raise ValueError('LLVM selected archiver is not executable')
    captured = {}
    for name in sorted(members):
        info = file_stats[name]
        data = _read_bounded_regular_file(root / name, info.st_size,
                                          LLVM_SELECTED_MAX_MEMBER_BYTES)
        if digest_bytes(data) != members[name]:
            raise ValueError('LLVM selected member digest mismatch: ' + name)
        captured[name] = data
    if sum(map(len, captured.values())) != total_size:
        raise ValueError('LLVM selected capture size changed')
    return captured, manifest_sha


def _read_llvm_selection(archive):
    # The in-process memo is used only after hashing the immutable input again.
    _verified_archive(archive, LLVM_SHA, 1503047352)
    if LLVM_SHA in _LLVM_SELECTION:
        return _LLVM_SELECTION[LLVM_SHA]
    files, nodes, total = {}, {}, 0
    with tarfile.open(archive, 'r|*') as incoming:
        for member in incoming:
            name = _safe_name(member.name, LLVM_PACKAGE)
            _check_nodes(nodes, name, member.isdir())
            total += member.size
            if len(nodes) > 300000 or member.size < 0 or member.size > 2 * 1024**3 or total > 64 * 1024**3:
                raise ValueError('LLVM archive exceeds finite bounds')
            relative = name[len(LLVM_PACKAGE) + 1:]
            selected = (relative == 'bin/llvm-ar' or relative.startswith('lib/clang/21/include/') or
                        relative in ('LICENSE.TXT', 'LICENSE', 'NOTICE', 'include/llvm/Support/LICENSE.TXT'))
            if member.issym() or member.islnk():
                _safe_llvm_link(name, member.linkname, member.islnk())
                if selected:
                    raise ValueError('selected LLVM member is a link')
            elif not (member.isfile() or member.isdir()):
                raise ValueError('LLVM special archive member is forbidden')
            if selected and member.isfile():
                with incoming.extractfile(member) as stream:
                    data = stream.read(member.size + 1)
                if len(data) != member.size:
                    raise ValueError('LLVM member length mismatch')
                files[relative] = data
    if 'bin/llvm-ar' not in files or 'lib/clang/21/include/stddef.h' not in files:
        raise ValueError('verified LLVM archive lacks required archive tool/resource headers')
    _LLVM_SELECTION[LLVM_SHA] = files
    return files


def _verify_sdk(sdk, archive):
    _verified_archive(archive, SDK_SHA, 8814217)
    files, seen, total = {}, set(), 0
    with zipfile.ZipFile(archive) as incoming:
        for member in incoming.infolist():
            name = _safe_name(member.filename, 'ps5-payload-sdk')
            mode = member.external_attr >> 16
            if name in seen or stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR):
                raise ValueError('unsafe SDK archive member')
            seen.add(name)
            total += member.file_size
            if len(seen) > 10000 or member.file_size > 16 * 1024**2 or total > 64 * 1024**2:
                raise ValueError('SDK archive exceeds finite bounds')
            if not member.is_dir():
                files[name[len('ps5-payload-sdk/'):]] = digest_bytes(incoming.read(member))
    _check_tree(sdk, files)
    return files


def _environment():
    env = os.environ.copy()
    for name in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'OBJC_INCLUDE_PATH',
                 'SDKROOT', 'MACOSX_DEPLOYMENT_TARGET', 'CCC_OVERRIDE_OPTIONS',
                 'GCC_EXEC_PREFIX', 'COMPILER_PATH', 'LIBRARY_PATH'):
        env.pop(name, None)
    env['ZERO_AR_DATE'] = '1'
    env['SOURCE_DATE_EPOCH'] = '0'
    return env


def _run(command, commands, env, *, stdin=None):
    result = subprocess.run([str(item) for item in command], input=stdin, capture_output=True,
                            text=True, env=env)
    commands.append({'argv': [str(item) for item in command], 'returncode': result.returncode,
                     'stdout': result.stdout, 'stderr': result.stderr})
    if result.returncode:
        raise ValueError('crypto command failed: ' + shlex.join([str(item) for item in command]) + '\n' + result.stderr)
    return result.stdout


def _native_tools(output_root, sdk, llvm_bin, *, llvm_selected_root=None):
    if sdk is None or llvm_bin is None:
        raise ValueError('native mode requires explicit verified SDK and LLVM paths')
    sdk, llvm_bin = pathlib.Path(sdk).resolve(), pathlib.Path(llvm_bin).resolve()
    if not sdk.is_dir() or not llvm_bin.is_dir() or llvm_bin.parent.name != LLVM_PACKAGE:
        raise ValueError('native SDK/LLVM cache path is not the pinned package')
    deps = llvm_bin.parents[2]
    sdk_files = _verify_sdk(sdk, deps / 'sdk.zip')
    for name, expected in TOOL_HASHES.items():
        try:
            matches = digest(llvm_bin / name) == expected
        except OSError as error:
            raise ValueError('native LLVM tool missing: ' + name) from error
        if not matches:
            raise ValueError('native LLVM tool identity mismatch: ' + name)
    if llvm_selected_root is None:
        pinned_members, manifest_sha = _read_llvm_selected_manifest()
        selected = _read_llvm_selection(deps / 'llvm.tar.xz')
        if {name: digest_bytes(data) for name, data in selected.items()} != pinned_members:
            raise ValueError('LLVM archive selection differs from pinned selected manifest')
        toolchain_input = {'mode': 'archive', 'archive_sha256': LLVM_SHA,
                           'manifest_sha256': manifest_sha}
    else:
        selected, manifest_sha = _read_llvm_selected_root(llvm_selected_root)
        toolchain_input = {'mode': 'selected_root', 'path': str(pathlib.Path(llvm_selected_root).absolute()),
                           'archive_sha256': LLVM_SHA, 'manifest_sha256': manifest_sha}
    extra = output_root / ('llvm-' + LLVM_SHA[:16])
    expected = {name: digest_bytes(data) for name, data in selected.items()}
    if extra.exists() or extra.is_symlink():
        _check_tree(extra, expected)
    else:
        output_root.mkdir(parents=True, exist_ok=True)
        stage = pathlib.Path(tempfile.mkdtemp(prefix='crypto-llvm-', dir=output_root))
        try:
            _write_files(stage, selected, ('bin/llvm-ar',))
            stage.rename(extra)
        finally:
            if stage.exists():
                shutil.rmtree(stage)
    identity = {'sdk_commit': SDK_COMMIT, 'sdk_archive_sha256': SDK_SHA,
                'llvm_archive_sha256': LLVM_SHA, 'tools': TOOL_HASHES,
                'llvm_selected_manifest_sha256': manifest_sha,
                'llvm_selected_members': expected,
                'sdk_members': sdk_files}
    return (llvm_bin / 'clang', extra / 'bin/llvm-ar', extra / 'lib/clang/21', sdk,
            identity, toolchain_input)


def _archive_members(archive, objects, mode):
    data = pathlib.Path(archive).read_bytes()
    if not data.startswith(b'!<arch>\n'):
        raise ValueError('crypto archive has an unsupported format')
    offset, long_names, members = 8, b'', []
    while offset < len(data):
        header = data[offset:offset + 60]
        if len(header) != 60 or header[58:] != b'`\n':
            raise ValueError('crypto archive member header is invalid')
        try:
            size = int(header[48:58])
        except ValueError as error:
            raise ValueError('crypto archive member size is invalid') from error
        payload = data[offset + 60:offset + 60 + size]
        if size < 0 or len(payload) != size:
            raise ValueError('crypto archive member is truncated')
        name = header[:16].decode('ascii').strip()
        offset += 60 + size + size % 2
        if name == '//':
            long_names = payload
            continue
        if name in ('/', '/SYM64/'):
            continue
        if name.startswith('#1/'):
            length = int(name[3:])
            name = payload[:length].rstrip(b'\x00').decode('ascii')
            payload = payload[length:]
        elif name.startswith('/'):
            start = int(name[1:])
            name = long_names[start:long_names.index(b'\n', start)].decode('ascii').rstrip('/')
        else:
            name = name.rstrip('/')
        if name.startswith('__.SYMDEF'):
            continue
        if name not in objects or any(item['name'] == name for item in members):
            raise ValueError('unexpected or duplicate crypto archive member')
        expected = pathlib.Path(objects[name]).read_bytes()
        if payload != expected:
            raise ValueError('crypto archive member differs from its compiled object')
        entry = {'name': name, 'sha256': digest_bytes(payload), 'size': len(payload)}
        if payload[:6] == b'\x7fELF\x02\x01':
            machine = struct.unpack_from('<H', payload, 18)[0]
            entry.update(format='ELF64', machine=machine, elf_type=struct.unpack_from('<H', payload, 16)[0])
            if mode == 'native' and (machine != 62 or entry['elf_type'] != 1):
                raise ValueError('native archive member is not target x86_64 relocatable ELF')
        elif mode == 'host' and payload[:4] in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf'):
            entry.update(format='MachO64', machine=struct.unpack_from('<I', payload, 4)[0])
        else:
            raise ValueError('crypto archive member target format is invalid')
        members.append(entry)
    if [member['name'] for member in members] != list(objects):
        raise ValueError('crypto archive member closure differs from the pinned source list')
    return members


def _include_closure(depfile, source_root, config_root, sdk, resource, mode, *,
                     original_build_directory=None, cache=None):
    dependency = pathlib.Path(depfile).read_text().replace('\\\n', ' ')
    paths = shlex.split(dependency.split(':', 1)[1])
    result = []
    for name in paths:
        path = pathlib.Path(name).absolute()
        if original_build_directory is not None and path.is_relative_to(original_build_directory):
            path = cache / path.relative_to(original_build_directory)
        path = path.resolve()
        origin, relative = None, None
        for label, root in (('source', source_root), ('overlay', config_root),
                            ('sdk', sdk), ('llvm_resource', resource)):
            if root is not None and path.is_relative_to(root):
                origin, relative = label, path.relative_to(root).as_posix()
                break
        if origin is None:
            if mode == 'native':
                raise ValueError('native include closure contains an unverified host header: ' + str(path))
            origin, relative = 'host', path.as_posix().lstrip('/')
        result.append({'origin': origin, 'path': relative, 'sha256': digest(path)})
    return result


def _verify_recorded_includes(closures, source_hashes, config_hashes, tools, mode):
    if set(closures) != set(CRYPTO_SOURCES):
        raise ValueError('cache include closure does not cover the pinned translation units')
    origins = {'source': {name[len(SOURCE_ROOT) + 1:]: value for name, value in source_hashes.items()},
               'overlay': config_hashes}
    if mode == 'native':
        origins['sdk'] = {name[len('target/include/'):]: value for name, value in tools['sdk_members'].items()
                          if name.startswith('target/include/')}
        origins['llvm_resource'] = {name[len('lib/clang/21/include/'):]: value
                                    for name, value in tools['llvm_selected_members'].items()
                                    if name.startswith('lib/clang/21/include/')}
    for source, headers in closures.items():
        if not any(item['origin'] == 'source' and item['path'] == 'library/' + source for item in headers):
            raise ValueError('cache include closure omits its compiled source')
        for item in headers:
            if item['origin'] == 'host' and mode == 'host':
                path = pathlib.Path('/' + item['path'])
                expected = digest(path) if path.is_file() else None
            else:
                expected = origins.get(item['origin'], {}).get(item['path'])
            if expected is None or expected != item['sha256']:
                raise ValueError('cache include identity does not match the verified header source')


def _compile_flags(target_flags, mapped_root, source_root, config):
    return [*target_flags, '-std=c11', '-Os', '-fno-common',
            '-ffile-prefix-map=' + str(mapped_root) + '=crypto',
            '-I', str(source_root / 'include'), '-I', str(source_root / 'library'),
            '-I', str(config), *['-D' + item for item in DEFINITIONS],
            '-include', str(source_root / 'library/common.h'),
            '-include', str(config / 'crypto_effective.h')]


def _verify_generated_rebuild(cache, manifest, compiler, ar, target_flags, env, sdk, resource, mode):
    """Do not let an unsigned cache re-authorize its own generated outputs."""
    stage = pathlib.Path(tempfile.mkdtemp(prefix='crypto-reuse-check-', dir=cache.parent))
    commands, status = [], 'rejected'
    try:
        objects_dir, evidence = stage / 'objects', stage / 'evidence'
        objects_dir.mkdir()
        evidence.mkdir()
        source_root, config = cache / manifest['source_root'], cache / 'config'
        common = _compile_flags(target_flags, cache, source_root, config)
        objects, includes = {}, {}
        for source in CRYPTO_SOURCES:
            name = pathlib.Path(source).stem + '.o'
            obj, dep = objects_dir / name, evidence / (name + '.d')
            _run([compiler, *common, '-MD', '-MF', dep, '-c', source_root / 'library' / source,
                  '-o', obj], commands, env)
            objects[name] = obj
            includes[source] = _include_closure(dep, source_root, config,
                                                sdk / 'target/include' if sdk else None,
                                                resource / 'include' if resource else None, mode)
        rebuilt = stage / 'libmbedcrypto.a'
        _run([ar, 'rcsD' if mode == 'native' else 'rcs', rebuilt, *objects.values()], commands, env)
        _archive_members(rebuilt, objects, mode)
        if includes != manifest['include_closure']:
            raise ValueError('cache generated include closure differs from anchored rebuild')
        for name, obj in objects.items():
            if obj.read_bytes() != (cache / 'objects' / name).read_bytes():
                raise ValueError('cache generated object differs from deterministic rebuild: ' + name)
        if rebuilt.read_bytes() != (cache / 'libmbedcrypto.a').read_bytes():
            raise ValueError('cache generated archive differs from deterministic rebuild')
        status = 'verified'
    finally:
        # Every receipt is new and external to the cached closure. Comparison
        # never overwrites the cached manifest, sources, objects or evidence.
        receipts = cache.parent / 'reuse-evidence'
        receipts.mkdir(exist_ok=True)
        with tempfile.NamedTemporaryFile(mode='w', prefix=cache.name + '-', suffix='.json',
                                         dir=receipts, delete=False) as stream:
            json.dump({'cache': cache.name, 'status': status,
                       'comparison': 'byte-for-byte every object and complete archive; no metadata normalization',
                       'commands': commands}, stream, sort_keys=True, indent=2)
            stream.write('\n')
        shutil.rmtree(stage)


def _descriptor(cache, manifest, toolchain_input=None):
    resource = cache.parent / ('llvm-' + LLVM_SHA[:16]) / 'lib/clang/21'
    native_options = (['-nostdinc', '-resource-dir', str(resource.resolve()),
                       '-isystem', str((resource / 'include').resolve())]
                      if manifest['inputs']['mode'] == 'native' else [])
    return {'archive': str((cache / 'libmbedcrypto.a').resolve()),
            'include_dirs': [str((cache / 'config').resolve()),
                             str((cache / manifest['source_root'] / 'include').resolve())],
            'compile_definitions': list(DEFINITIONS), 'config_id': manifest['config_id'],
            'native_compile_options': native_options,
            'identity': manifest['identity'], 'evidence_manifest': str((cache / 'manifest.json').resolve()),
            'toolchain_input': toolchain_input}


def prepare_crypto(mode, output_root, source_archive, framework_archive, *, sdk=None, llvm_bin=None,
                   llvm_selected_root=None):
    """Return the identified host/native libmbedcrypto.a and consumer settings."""
    if mode not in ('host', 'native'):
        raise ValueError('crypto mode must be host or native')
    if mode == 'native' and (sdk is None or llvm_bin is None):
        raise ValueError('native mode requires explicit verified SDK and LLVM paths')
    if mode == 'host' and llvm_selected_root is not None:
        raise ValueError('host mode cannot use native LLVM selected input')
    # Both complete archive validations precede every cache/output mutation.
    source_files = _read_source_archive(source_archive, SOURCE_SHA, SOURCE_ROOT)
    framework_files = _read_source_archive(framework_archive, FRAMEWORK_SHA, FRAMEWORK_ROOT)
    cmake = source_files[SOURCE_ROOT + '/library/CMakeLists.txt'].decode()
    upstream = tuple(re.search(r'set\(src_crypto\s+(.*?)\)', cmake, re.S).group(1).split())
    if upstream != CRYPTO_SOURCES:
        raise ValueError('pinned upstream crypto source recipe mismatch')
    overlay = CONFIG_HEADER.read_bytes()
    config_id = configuration_identity(overlay)
    output_root = pathlib.Path(output_root).absolute()
    if output_root.is_symlink():
        raise ValueError('crypto output root is a symlink')
    output_root = output_root.resolve()
    env, tool_commands = _environment(), []
    toolchain_input = None
    if mode == 'native':
        compiler, ar, resource, sdk, tool_identity, toolchain_input = _native_tools(
            output_root, sdk, llvm_bin, llvm_selected_root=llvm_selected_root)
        env['SCE_PROSPERO_SDK_DIR'] = str(sdk)
        env['LLVM_CONFIG'] = str(pathlib.Path(llvm_bin).resolve() / 'llvm-config')
        target_flags = ['-target', 'x86_64-sie-ps5', '-fvisibility-nodllstorageclass=default',
                        '-isysroot', str(sdk), '--sysroot=' + str(sdk), '-fno-stack-protector',
                        '-fno-plt', '-femulated-tls', '-nostdinc', '-resource-dir', str(resource),
                        '-isystem', str(sdk / 'target/include'), '-isystem', str(resource / 'include')]
        # Compare the ABI feature macros of direct clang with the pinned SDK wrapper.
        macro_flags = ['-nostdinc', '-resource-dir', str(resource), '-dM', '-E', '-x', 'c', '-']
        direct = _run([compiler, *target_flags, '-dM', '-E', '-x', 'c', '-'], tool_commands, env, stdin='')
        wrapper = _run([sdk / 'bin/prospero-clang', *macro_flags], tool_commands, env, stdin='')
        abi_names = ('__x86_64__', '__ELF__', '__FreeBSD__', '__LP64__', '__SIZEOF_POINTER__',
                     '__SIZEOF_LONG__', '__SIZEOF_SIZE_T__', '__BYTE_ORDER__', '__SSE2__')
        def abi(text):
            macros = {line.split()[1]: ' '.join(line.split()[2:]) for line in text.splitlines()
                      if line.startswith('#define ')}
            return {name: macros.get(name) for name in abi_names}
        if abi(direct) != abi(wrapper) or abi(direct)['__SIZEOF_POINTER__'] != '8':
            raise ValueError('direct LLVM target ABI differs from the verified SDK wrapper')
        tool_identity['target_abi_macros'] = abi(direct)
    else:
        if sdk is not None or llvm_bin is not None:
            raise ValueError('host fixtures must not receive native tools')
        compiler, ar = shutil.which('cc'), shutil.which('ar')
        if compiler is None or ar is None:
            raise ValueError('host compiler/archive tool is unavailable')
        compiler, ar = pathlib.Path(compiler).resolve(), pathlib.Path(ar).resolve()
        sdk, resource, target_flags = None, None, []
        tool_identity = {'compiler': {'name': compiler.name, 'sha256': digest(compiler)},
                         'archiver': {'name': ar.name, 'sha256': digest(ar)}}
    version = _run([compiler, '--version'], tool_commands, env)
    # InstalledDir is deliberately confined to actual-command evidence.
    tool_identity['compiler_version'] = version.splitlines()[0]
    inputs = {'mode': mode, 'config_id': config_id, 'source_archive_sha256': SOURCE_SHA,
              'framework_archive_sha256': FRAMEWORK_SHA, 'overlay_sha256': digest_bytes(overlay),
              'effective_header_sha256': digest_bytes(EFFECTIVE_HEADER), 'recipe_sha256': digest(__file__),
              'tools': tool_identity, 'sources': list(CRYPTO_SOURCES), 'compile_definitions': list(DEFINITIONS),
              'consumer_compile_options': ([] if mode == 'host' else [
                  '-nostdinc', '-resource-dir', '<verified-llvm-resource>',
                  '-isystem', '<verified-llvm-resource>/include'])}
    cache_key = digest_bytes(_json(inputs))
    cache = output_root / (mode + '-' + cache_key[:24])
    source_members = {'mbedtls': {name: digest_bytes(data) for name, data in source_files.items()},
                      'framework': {name: digest_bytes(data) for name, data in framework_files.items()}}
    config_hashes = {'crypto_config.h': digest_bytes(overlay),
                     'crypto_effective.h': digest_bytes(EFFECTIVE_HEADER),
                     'effective.c': digest_bytes(PROBE_SOURCE)}
    if cache.exists() or cache.is_symlink():
        if cache.is_symlink():
            raise ValueError('crypto cache is a symlink')
        try:
            manifest = json.loads((cache / 'manifest.json').read_text())
        except (OSError, ValueError) as error:
            raise ValueError('incomplete crypto cache manifest') from error
        if (manifest['inputs'] != inputs or manifest['config_id'] != config_id or
                manifest['source_root'] != 'sources/mbedtls/' + SOURCE_ROOT or
                manifest['framework_root'] != 'sources/framework/' + FRAMEWORK_ROOT):
            raise ValueError('crypto cache identity mismatch')
        if manifest['source_members'] != source_members:
            raise ValueError('crypto cache source identity does not match verified archives')
        _check_tree(cache, {**manifest['file_closure'], 'manifest.json': digest(cache / 'manifest.json')})
        _check_tree(cache / 'sources', {package + '/' + name: value
                    for package, members in source_members.items() for name, value in members.items()})
        _check_tree(cache / 'config', config_hashes)
        objects = {pathlib.Path(name).stem + '.o': cache / 'objects' / (pathlib.Path(name).stem + '.o')
                   for name in CRYPTO_SOURCES}
        _check_tree(cache / 'objects', {item['name']: item['sha256'] for item in manifest['members']})
        if _archive_members(cache / 'libmbedcrypto.a', objects, mode) != manifest['members']:
            raise ValueError('crypto cache archive member identity mismatch')
        _verify_recorded_includes(manifest['include_closure'], source_members['mbedtls'],
                                  config_hashes, tool_identity, mode)
        actual_closures = {source: _include_closure(
            cache / 'evidence' / (pathlib.Path(source).stem + '.o.d'),
            cache / manifest['source_root'], cache / 'config',
            sdk / 'target/include' if sdk else None, resource / 'include' if resource else None,
            mode, original_build_directory=pathlib.Path(manifest['original_build_directory']), cache=cache)
            for source in CRYPTO_SOURCES}
        if actual_closures != manifest['include_closure']:
            raise ValueError('crypto cache include closure differs from the compiled dependencies')
        if manifest['identity'] != {**inputs, 'source_members': manifest['source_members'],
                                    'include_closure': manifest['include_closure'],
                                    'archive_sha256': digest(cache / 'libmbedcrypto.a'),
                                    'members': manifest['members']}:
            raise ValueError('crypto cache descriptor identity mismatch')
        _verify_generated_rebuild(cache, manifest, compiler, ar, target_flags, env, sdk, resource, mode)
        return _descriptor(cache, manifest, toolchain_input)
    output_root.mkdir(parents=True, exist_ok=True)
    stage = pathlib.Path(tempfile.mkdtemp(prefix='crypto-build-', dir=output_root))
    commands = list(tool_commands)
    try:
        _write_files(stage / 'sources/mbedtls', source_files)
        _write_files(stage / 'sources/framework', framework_files)
        config = stage / 'config'
        _write_files(config, {'crypto_config.h': overlay, 'crypto_effective.h': EFFECTIVE_HEADER})
        source_root = stage / 'sources/mbedtls' / SOURCE_ROOT
        evidence = stage / 'evidence'
        objects_dir = stage / 'objects'
        evidence.mkdir()
        objects_dir.mkdir()
        common = _compile_flags(target_flags, stage, source_root, config)
        probe = config / 'effective.c'
        probe.write_bytes(PROBE_SOURCE)
        macros = _run([compiler, *common, '-dM', '-E', probe], commands, env)
        (evidence / 'effective-macros.txt').write_text(macros)
        closures, objects = {}, {}
        for source in CRYPTO_SOURCES:
            name = pathlib.Path(source).stem + '.o'
            obj, dep = objects_dir / name, evidence / (name + '.d')
            _run([compiler, *common, '-MD', '-MF', dep, '-c', source_root / 'library' / source,
                  '-o', obj], commands, env)
            objects[name] = obj
            closures[source] = _include_closure(dep, source_root, config,
                                               sdk / 'target/include' if sdk else None,
                                               resource / 'include' if resource else None, mode)
        for source in ('entropy.c', 'entropy_poll.c', 'ctr_drbg.c'):
            preprocessed = _run([compiler, *common, '-E', source_root / 'library' / source], commands, env)
            (evidence / (pathlib.Path(source).stem + '.i')).write_text(preprocessed)
        _run([ar, 'rcsD' if mode == 'native' else 'rcs', stage / 'libmbedcrypto.a', *objects.values()], commands, env)
        _run([ar, 't', stage / 'libmbedcrypto.a'], commands, env)
        members = _archive_members(stage / 'libmbedcrypto.a', objects, mode)
        if mode == 'native':
            for tool, arguments, output in (
                    ('llvm-readelf', ['-h'], 'archive-elf-headers.txt'),
                    ('llvm-nm', ['--defined-only'], 'archive-defined-symbols.txt'),
                    ('llvm-nm', ['--undefined-only'], 'archive-undefined-symbols.txt')):
                text = _run([pathlib.Path(llvm_bin) / tool, *arguments, stage / 'libmbedcrypto.a'], commands, env)
                (evidence / output).write_text(text)
            poll = _run([pathlib.Path(llvm_bin) / 'llvm-nm', '--defined-only', objects['entropy_poll.o']], commands, env)
            if 'mbedtls_platform_entropy_poll' in poll:
                raise ValueError('native crypto archive still defines the platform entropy poll')
        _verify_recorded_includes(closures, source_members['mbedtls'], config_hashes, tool_identity, mode)
        identity = {**inputs, 'source_members': source_members, 'include_closure': closures,
                    'archive_sha256': digest(stage / 'libmbedcrypto.a'), 'members': members}
        provenance = {'mbedtls_version': '3.6.7', 'mbedtls_commit': SOURCE_COMMIT,
                      'mbedtls_archive_sha256': SOURCE_SHA, 'framework_gitlink': FRAMEWORK_COMMIT,
                      'framework_archive_sha256': FRAMEWORK_SHA,
                      'license_choice': 'Apache-2.0 (upstream dual Apache-2.0 OR GPL-2.0-or-later)',
                      'source_mutations': [], 'configuration_delta': DEFINITIONS + [
                          'MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES', 'MBEDTLS_NO_PLATFORM_ENTROPY'],
                      'consumer_contract': 'Use include_dirs and compile_definitions; include crypto_effective.h in every Mbed consumer.',
                      'scope': 'offline crypto dependency only; no console execution or entropy-quality claim'}
        (evidence / 'provenance.json').write_text(json.dumps(provenance, indent=2, sort_keys=True) + '\n')
        (evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        manifest = {'schema_version': 1, 'config_id': config_id, 'inputs': inputs,
                    'original_build_directory': str(stage),
                    'source_root': 'sources/mbedtls/' + SOURCE_ROOT,
                    'framework_root': 'sources/framework/' + FRAMEWORK_ROOT,
                    'source_members': source_members, 'include_closure': closures,
                    'members': members, 'identity': identity, 'provenance': provenance,
                    'file_closure': _tree_hashes(stage)}
        (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        stage.rename(cache)
        return _descriptor(cache, manifest, toolchain_input)
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('host', 'native'), required=True)
    parser.add_argument('--output-root', type=pathlib.Path, required=True)
    parser.add_argument('--mbedtls-archive', type=pathlib.Path, required=True)
    parser.add_argument('--framework-archive', type=pathlib.Path, required=True)
    parser.add_argument('--sdk', type=pathlib.Path)
    parser.add_argument('--llvm-bin', type=pathlib.Path)
    parser.add_argument('--llvm-selected-root', type=pathlib.Path)
    args = parser.parse_args()
    result = prepare_crypto(args.mode, args.output_root, args.mbedtls_archive,
                            args.framework_archive, sdk=args.sdk, llvm_bin=args.llvm_bin,
                            llvm_selected_root=args.llvm_selected_root)
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
