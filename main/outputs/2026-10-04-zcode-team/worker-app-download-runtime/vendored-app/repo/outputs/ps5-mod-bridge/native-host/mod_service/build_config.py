"""Read-only selection of an explicitly supplied static ModForge service.

Descriptor schema (no source build, toolchain, shell or compiler options):
  {"schema_version": 1, "target": "host" | "native",
   "service_archive": {"path": "/absolute/service.a", "sha256": "..."},
   "dependency_archives": [{"path": "/absolute/dependency.a", "sha256": "..."}]}

Dependencies retain descriptor order. The primary archive is force-loaded so
static method registrars are retained. Existing application crypto/SDK inputs
remain owned by the application's build.
"""
import hashlib
import json
import pathlib
import re
import stat
import sys
import unicodedata

CONFIG_BYTES_LIMIT = 16 * 1024
PROFILE_BYTES_LIMIT = 64 * 1024
ARCHIVE_BYTES_LIMIT = 1024 * 1024 * 1024
LIMIT_KEYS = {'max_archive_entries', 'max_archive_bytes', 'max_entry_bytes', 'max_jobs'}


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate mod service JSON key: ' + key)
        result[key] = value
    return result


def _file_identity(path, limit):
    path = pathlib.Path(path)
    if path.is_symlink() or not stat.S_ISREG(path.stat().st_mode):
        raise ValueError('mod service input must be a regular file: ' + str(path))
    size = path.stat().st_size
    if size <= 0 or size > limit:
        raise ValueError('mod service input size exceeds bounds: ' + str(path))
    path = path.resolve(strict=True)
    sha = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            sha.update(block)
    return {'path': str(path), 'bytes': size, 'sha256': sha.hexdigest()}


def _read_object(path, limit):
    identity = _file_identity(path, limit)
    # Read once for parsing and bind those exact bytes to the identity.
    with pathlib.Path(identity['path']).open('rb') as stream:
        raw = stream.read(limit + 1)
    if len(raw) != identity['bytes'] or hashlib.sha256(raw).hexdigest() != identity['sha256']:
        raise ValueError('mod service input changed while reading')
    value = json.loads(raw.decode('utf-8'), object_pairs_hook=_unique_object)
    if not isinstance(value, dict):
        raise ValueError('mod service input must be a JSON object')
    return value, identity


def _device_path(value):
    if (not isinstance(value, str) or not value.startswith('/') or value == '/' or
            len(value.encode('utf-8')) > 1024 or
            any(unicodedata.category(character) == 'Cc' for character in value) or
            any(part in ('', '.', '..') for part in value.split('/')[1:])):
        raise ValueError('mod service directory must be a bounded canonical absolute path')
    # PS5 paths need not exist on the build host. Never resolve them locally.
    return value


def _configuration(value):
    if (not {'state_dir', 'mod_root_base'} <= set(value) or
            not set(value) <= {'state_dir', 'mod_root_base', 'data_dir', 'limits'}):
        raise ValueError('mod service config requires state_dir/mod_root_base and only known fields')
    for key in ('state_dir', 'mod_root_base', 'data_dir'):
        if key in value:
            _device_path(value[key])
    if value['state_dir'] == value['mod_root_base']:
        raise ValueError('mod service state_dir and mod_root_base must be distinct')
    if 'limits' in value:
        limits = value['limits']
        if (not isinstance(limits, dict) or not set(limits) <= LIMIT_KEYS or
                any(type(limit) is not int or not 0 < limit <= (1 << 63) - 1
                    for limit in limits.values())):
            raise ValueError('mod service limits must be known positive bounded integers')
    result = json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False)
    if len(result.encode('utf-8')) > CONFIG_BYTES_LIMIT:
        raise ValueError('mod service canonical config size exceeds bounds')
    return result


def _archive(value):
    if (not isinstance(value, dict) or set(value) != {'path', 'sha256'} or
            not isinstance(value['path'], str) or not pathlib.Path(value['path']).is_absolute() or
            not isinstance(value['sha256'], str) or not re.fullmatch('[0-9a-f]{64}', value['sha256'])):
        raise ValueError('mod service archive requires an absolute path and lowercase SHA256')
    actual = _file_identity(value['path'], ARCHIVE_BYTES_LIMIT)
    if actual['sha256'] != value['sha256']:
        raise ValueError('mod service archive SHA256 mismatch: ' + actual['path'])
    with pathlib.Path(actual['path']).open('rb') as stream:
        if stream.read(8) != b'!<arch>\n':
            raise ValueError('mod service input must be a self-contained static archive (thin archives refused)')
    return actual


def prepare_mod_service(target, profile, configuration):
    """Validate all selected input bytes without building or executing anything."""
    if target not in ('host', 'native'):
        raise ValueError('mod service target must be host or native')
    if (profile is None) != (configuration is None):
        raise ValueError('mod service requires both --mod-service-profile and --mod-service-config')
    if profile is None:
        return {'identity': {'enabled': False}, 'config_json': '{}'}
    descriptor, profile_identity = _read_object(profile, PROFILE_BYTES_LIMIT)
    if (not {'schema_version', 'target', 'service_archive'} <= set(descriptor) or
            not set(descriptor) <= {'schema_version', 'target', 'service_archive', 'dependency_archives'} or
            type(descriptor['schema_version']) is not int or descriptor['schema_version'] != 1):
        raise ValueError('mod service profile must use schema_version 1 and only known fields')
    if descriptor['target'] != target:
        raise ValueError('mod service profile target does not match build target')
    primary = _archive(descriptor['service_archive'])
    dependencies = descriptor.get('dependency_archives', [])
    if not isinstance(dependencies, list) or len(dependencies) > 32:
        raise ValueError('mod service dependency archives must be a bounded list')
    dependencies = [_archive(row) for row in dependencies]
    paths = [row['path'] for row in (primary, *dependencies)]
    if len(set(paths)) != len(paths):
        raise ValueError('duplicate mod service archive path')
    value, configuration_identity = _read_object(configuration, CONFIG_BYTES_LIMIT)
    canonical = _configuration(value)
    identity = {'enabled': True, 'schema_version': 1, 'target': target,
                'profile': profile_identity, 'configuration': configuration_identity,
                'config_sha256': hashlib.sha256(canonical.encode('utf-8')).hexdigest(),
                'config': value, 'service_archive': primary, 'dependency_archives': dependencies,
                'primary_archive_force_loaded': True}
    return {'identity': identity, 'config_json': canonical}


def verify_mod_service_inputs(selected):
    """Refuse input mutation between initial validation and the link invocation."""
    identity = selected['identity']
    if not identity['enabled']:
        return
    for key, limit in (('profile', PROFILE_BYTES_LIMIT), ('configuration', CONFIG_BYTES_LIMIT)):
        expected = identity[key]
        if _file_identity(expected['path'], limit) != expected:
            raise ValueError('mod service ' + key + ' changed before link')
    for expected in [identity['service_archive'], *identity['dependency_archives']]:
        if _archive({'path': expected['path'], 'sha256': expected['sha256']}) != expected:
            raise ValueError('mod service archive changed before link')


def mod_service_link_arguments(selected):
    identity = selected['identity']
    if not identity['enabled']:
        return []
    primary = identity['service_archive']['path']
    if identity['target'] == 'host' and sys.platform == 'darwin':
        # Separate argv values also handle commas/spaces in the archive path.
        arguments = ['-Xlinker', '-force_load', '-Xlinker', primary]
    else:
        arguments = ['-Wl,--whole-archive', primary, '-Wl,--no-whole-archive']
    return [*arguments, *(row['path'] for row in identity['dependency_archives'])]


def mod_service_header(selected):
    return (f'#define MB_HOST_MOD_SERVICE {int(selected["identity"]["enabled"])}\n'
            '#define MB_HOST_MOD_SERVICE_CONFIG_JSON ' + json.dumps(selected['config_json'], ensure_ascii=False) + '\n')
