"""Explicit public OAuth registration/trust embedded into an offline app build."""
import hashlib
import ipaddress
import json
from pathlib import Path
import re
from types import SimpleNamespace
from urllib.parse import urlsplit

FIELDS = {'client_id', 'scope', 'redirect_uri', 'ca_pem_path', 'users_resolve',
          'timeout_ms', 'connect_timeout_ms'}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate public OAuth setting')
        result[key] = value
    return result


def printable(value, limit, empty=False):
    return (isinstance(value, str) and (empty or bool(value)) and len(value) <= limit
            and all(32 <= ord(c) <= 126 for c in value))


def address_mapping(mapping, host):
    if not printable(mapping, 8191, empty=True):
        raise ValueError('invalid provider address mapping')
    if not mapping: return
    prefix = host + ':443:'
    if not mapping.startswith(prefix):
        raise ValueError('address mapping must use fixed provider host on 443')
    address = mapping[len(prefix):]
    if address.startswith('[') and address.endswith(']'):
        address = address[1:-1]
    try:
        ipaddress.ip_address(address)
    except ValueError as error:
        raise ValueError('address mapping must contain one literal address') from error
    if '%' in address:
        raise ValueError('scoped address mapping is unsupported')


def prepare_oauth(config_path, profile):
    if config_path is None:
        return SimpleNamespace(client_id='', scope='', redirect_uri='', users_resolve='', api_resolve='',
                               ca_pem=b'', timeout_ms=10000, connect_timeout_ms=5000,
                               identity={'configured': False, 'credential_storage': 'process_lifetime'})
    path = Path(config_path)
    if path.stat().st_size > 16384:
        raise ValueError('public OAuth configuration exceeds limit')
    try:
        with path.open('rb') as stream:
            encoded = stream.read(16385)
        if len(encoded) > 16384:
            raise ValueError('public OAuth configuration exceeds limit')
        fields = json.loads(encoded.decode('utf-8'), object_pairs_hook=unique_object)
    except (UnicodeError, json.JSONDecodeError) as error:
        raise ValueError('invalid public OAuth configuration') from error
    if not isinstance(fields, dict) or set(fields) not in (FIELDS, FIELDS | {'api_resolve'}):
        raise ValueError('public OAuth configuration has invalid fields')
    client = fields['client_id']
    scope = fields['scope']
    redirect = fields['redirect_uri']
    mapping = fields['users_resolve']
    api_mapping = fields.get('api_resolve', '')
    if (not isinstance(client, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,128}', client)
            or client == 'public_test' or not printable(scope, 1024, empty=True)):
        raise ValueError('invalid public OAuth client or scope')
    try:
        parsed = urlsplit(redirect) if printable(redirect, 4096) else None
        if (parsed is None or parsed.scheme != 'http' or parsed.hostname != '127.0.0.1'
                or parsed.username is not None or parsed.password is not None
                or parsed.path != '/auth/callback' or parsed.query or parsed.fragment
                or not parsed.port or not 1 <= parsed.port <= 65535
                or redirect != f'http://127.0.0.1:{parsed.port}/auth/callback'):
            raise ValueError('invalid callback')
        if profile['address'] != '127.0.0.1':
            raise ValueError('configured OAuth requires loopback profile')
        if profile['native'] and parsed.port != profile['port']:
            raise ValueError('native callback port does not match app')
    except (AttributeError, TypeError, ValueError) as error:
        raise ValueError('OAuth callback must match the loopback app') from error
    address_mapping(mapping, 'users.nexusmods.com')
    address_mapping(api_mapping, 'api.nexusmods.com')
    timeout, connect = fields['timeout_ms'], fields['connect_timeout_ms']
    if (type(timeout) is not int or type(connect) is not int
            or not 1 <= connect <= timeout <= 30000):
        raise ValueError('invalid OAuth network budgets')
    ca_path = fields['ca_pem_path']
    if not isinstance(ca_path, str) or not ca_path or len(ca_path) > 4096 or '\0' in ca_path:
        raise ValueError('invalid public CA path')
    ca_path = Path(ca_path)
    if not ca_path.is_absolute():
        ca_path = path.parent / ca_path
    if not 1 <= ca_path.stat().st_size <= 1048576:
        raise ValueError('public CA bundle exceeds limit')
    with ca_path.open('rb') as stream:
        ca = stream.read(1048577)
    if (not 1 <= len(ca) <= 1048576 or b'-----BEGIN CERTIFICATE-----' not in ca
            or b'-----END CERTIFICATE-----' not in ca or b'PRIVATE KEY' in ca
            or any(c > 127 or c == 0 for c in ca)):
        raise ValueError('public CA must contain certificate-only PEM bytes')
    identity = {'configured': True, 'client_id': client, 'scope': scope,
                'redirect_uri': redirect, 'users_resolve': mapping, 'api_resolve': api_mapping,
                'timeout_ms': timeout, 'connect_timeout_ms': connect,
                'ca_sha256': hashlib.sha256(ca).hexdigest(), 'ca_bytes': len(ca),
                'credential_storage': 'process_lifetime'}
    return SimpleNamespace(client_id=client, scope=scope, redirect_uri=redirect,
                           users_resolve=mapping, api_resolve=api_mapping, ca_pem=ca, timeout_ms=timeout,
                           connect_timeout_ms=connect, identity=identity)


def write_oauth_header(settings, destination):
    text = ['#ifndef MODBRIDGE_GENERATED_OAUTH_CONFIG_H',
            '#define MODBRIDGE_GENERATED_OAUTH_CONFIG_H',
            '#define MB_HOST_OAUTH_CONFIGURED ' + str(int(settings.identity['configured']))]
    for name, value in (('CLIENT_ID', settings.client_id), ('SCOPE', settings.scope),
                        ('REDIRECT_URI', settings.redirect_uri), ('USERS_RESOLVE', settings.users_resolve),
                        ('API_RESOLVE', settings.api_resolve)):
        text.append('#define MB_HOST_OAUTH_' + name + ' ' + json.dumps(value, ensure_ascii=True))
    text += ['#define MB_HOST_OAUTH_TIMEOUT_MS ' + str(settings.timeout_ms) + 'u',
             '#define MB_HOST_OAUTH_CONNECT_TIMEOUT_MS ' + str(settings.connect_timeout_ms) + 'u',
             '#define MB_HOST_OAUTH_CA_BYTES ' + str(len(settings.ca_pem)) + 'u',
             'static const unsigned char mb_host_oauth_ca[] = {']
    data = settings.ca_pem or b'\0'
    for offset in range(0, len(data), 16):
        text.append('  ' + ','.join(f'0x{value:02x}' for value in data[offset:offset+16]) + ',')
    text += ['};', '#endif', '']
    Path(destination).write_text('\n'.join(text), encoding='ascii')
