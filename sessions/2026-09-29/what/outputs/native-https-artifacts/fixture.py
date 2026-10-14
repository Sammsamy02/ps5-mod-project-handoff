"""Owned HTTPS fixture. No accounts, game access or console control.

Preparation is separate from the one-connection LAN run. The server is
development tooling; it is not a runtime dependency of the finished app.
"""
import argparse
import datetime
import hashlib
import ipaddress
import json
import os
import socket
import ssl
import time
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID

HERE = Path(__file__).resolve().parent
NAME = 'native-https.modbridge.test'
REQUEST = (b'GET /native-https-v1 HTTP/1.1\r\n'
           b'Host: native-https.modbridge.test\r\n'
           b'Connection: close\r\nAccept-Encoding: identity\r\n\r\n')
RESPONSE = (b'HTTP/1.1 200 OK\r\nContent-Length: 24\r\n'
            b'Content-Type: text/plain\r\nConnection: close\r\n\r\n'
            b'native-https-fixture-v1\n')


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def dump(path, obj):
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(obj, indent=2) + '\n')
    os.replace(temporary, path)


def prepare():
    public = HERE / 'fixture-public.json'
    if public.exists():
        meta = json.loads(public.read_text())
        for name, identity in meta['public_files'].items():
            if digest((HERE / name).read_bytes()) != identity:
                raise RuntimeError('existing fixture identity changed: ' + name)
        if not (HERE / 'server-private/leaf.key').is_file():
            raise RuntimeError('existing fixture private input missing')
        return meta
    private = HERE / 'server-private'
    private.mkdir(mode=0o700, exist_ok=False)
    now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
    root_key = rsa.generate_private_key(public_exponent=65537, key_size=4096)
    issuer_key = ec.generate_private_key(ec.SECP384R1())
    leaf_key = ec.generate_private_key(ec.SECP256R1())
    names = [x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, n)]) for n in
             ['Owned console fixture RSA root', 'Owned console fixture P384 issuer', NAME]]

    def cert(key, subject, issuer, signer, serial, ca, path_length, algorithm):
        builder = (x509.CertificateBuilder().subject_name(subject).issuer_name(issuer)
                   .public_key(key.public_key()).serial_number(serial)
                   .not_valid_before(now - datetime.timedelta(days=1))
                   .not_valid_after(now + datetime.timedelta(days=7))
                   .add_extension(x509.BasicConstraints(ca=ca, path_length=path_length), True)
                   .add_extension(x509.KeyUsage(digital_signature=True, content_commitment=False,
                       key_encipherment=False, data_encipherment=False, key_agreement=False,
                       key_cert_sign=ca, crl_sign=ca, encipher_only=False, decipher_only=False), True))
        if not ca:
            builder = (builder.add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), False)
                       .add_extension(x509.SubjectAlternativeName([x509.DNSName(NAME)]), False))
        return builder.sign(signer, algorithm)

    root = cert(root_key, names[0], names[0], root_key, 1001, True, 1, hashes.SHA256())
    issuer = cert(issuer_key, names[1], names[0], root_key, 1002, True, 0, hashes.SHA256())
    leaf = cert(leaf_key, names[2], names[1], issuer_key, 1003, False, None, hashes.SHA384())
    # Only the server leaf key persists. Root/issuer private keys never leave
    # this preparation operation; these are owned test credentials.
    key = private / 'leaf.key'
    fd = os.open(key, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'wb') as f:
        f.write(leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                      serialization.NoEncryption()))
    pem = lambda c: c.public_bytes(serialization.Encoding.PEM)
    der = lambda c: c.public_bytes(serialization.Encoding.DER)
    files = {'root.der': der(root), 'root.pem': pem(root), 'issuer.der': der(issuer),
             'leaf.der': der(leaf), 'chain.pem': pem(leaf) + pem(issuer)}
    for name, contents in files.items():
        (HERE / name).write_bytes(contents)
    meta = {'created_utc': utc(), 'hostname': NAME, 'root_der_bytes': len(files['root.der']),
            'request_bytes': len(REQUEST), 'response_bytes': len(RESPONSE), 'body_bytes': 24,
            'certificate_shape': 'RSA4096 root / P384 issuer / P256 SHA384-signed leaf',
            'not_before_utc': leaf.not_valid_before_utc.isoformat(),
            'not_after_utc': leaf.not_valid_after_utc.isoformat(),
            'public_files': {name: digest(contents) for name, contents in files.items()},
            'private_root_or_issuer_keys_persisted': False, 'keylog_enabled': False,
            'game_or_account_data': False}
    dump(public, meta)
    return meta


def listener_for(address, port):
    parsed = ipaddress.IPv4Address(address)
    if not (parsed.is_private and not parsed.is_unspecified and not parsed.is_multicast):
        raise ValueError('explicit private IPv4 bind required')
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        listener.bind((address, port))
        listener.listen(1)
        return listener
    except BaseException:
        listener.close()
        raise


def exchange(listener, expected_peer, lifetime):
    start = time.monotonic()
    record = {'started_utc': utc(), 'peer_matches': False, 'handshake_ok': False,
              'sni_matches': False, 'request_matches': False, 'http_request_bytes': 0,
              'response_sent_bytes': 0, 'clean_peer_close': False, 'listener_closed': False,
              'keylog_enabled': False, 'accounts_or_game_files_accessed': False}
    raw = conn = None
    try:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
        ctx.set_ciphers('ECDHE-ECDSA-AES128-GCM-SHA256')
        ctx.set_ecdh_curve('prime256v1')
        ctx.options |= ssl.OP_NO_TICKET
        ctx.keylog_filename = None
        ctx.load_cert_chain(HERE / 'chain.pem', HERE / 'server-private/leaf.key')
        def sni(sock, name, context):
            record['sni_matches'] = name == NAME
        ctx.sni_callback = sni
        listener.settimeout(lifetime)
        raw, peer = listener.accept()
        record['peer_matches'] = peer[0] == expected_peer
        if not record['peer_matches']:
            raise RuntimeError('unexpected peer; fixture ends without application data')
        remaining = lifetime - (time.monotonic() - start)
        if remaining <= 0:
            raise TimeoutError('fixture lifetime exhausted')
        raw.settimeout(min(5, remaining))
        conn = ctx.wrap_socket(raw, server_side=True)
        raw = None
        record.update(handshake_ok=True, version=conn.version(), cipher=conn.cipher()[0])
        buf = b''
        deadline = min(start + lifetime, time.monotonic() + 5)
        while b'\r\n\r\n' not in buf and len(buf) < 256:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError('request deadline exhausted')
            conn.settimeout(remaining)
            part = conn.recv(256 - len(buf))
            if not part:
                break
            buf += part
        record['http_request_bytes'] = len(buf)
        record['request_matches'] = buf == REQUEST and record['sni_matches']
        if not record['request_matches']:
            raise RuntimeError('fixture request mismatch')
        conn.settimeout(min(5, max(0.001, start + lifetime - time.monotonic())))
        conn.sendall(RESPONSE)
        record['response_sent_bytes'] = len(RESPONSE)
        raw = conn.unwrap()
        conn = None
        record['clean_peer_close'] = True
    except (OSError, RuntimeError) as e:
        record['failure_type'] = type(e).__name__
        record['failure_reason'] = getattr(e, 'reason', None) or str(e)
    finally:
        if conn is not None:
            conn.close()
        if raw is not None:
            raw.close()
        listener.close()
        record.update(listener_closed=True, finished_utc=utc(), elapsed_seconds=round(time.monotonic()-start, 6))
    return record


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare', action='store_true')
    parser.add_argument('--bind')
    parser.add_argument('--port', type=int, default=8443)
    parser.add_argument('--peer', default='ps5-host-not-configured.invalid')
    parser.add_argument('--lifetime', type=int, default=90)
    args = parser.parse_args()
    if args.prepare:
        print(json.dumps(prepare()), flush=True)
        return
    if not args.bind or not 1 <= args.port <= 65535 or not 1 <= args.lifetime <= 120:
        parser.error('explicit bind/valid port/bounded lifetime required')
    prepare()
    expected = ipaddress.IPv4Address(args.peer)
    if not expected.is_private or expected.is_multicast or expected.is_unspecified:
        parser.error('expected peer must be explicit private IPv4')
    listener = listener_for(args.bind, args.port)
    print(json.dumps({'ready': True, 'bind': args.bind, 'port': args.port, 'expected_peer':args.peer,
                      'pid': os.getpid(), 'lifetime_seconds': args.lifetime, 'utc': utc()}), flush=True)
    record = exchange(listener, args.peer, args.lifetime)
    dump(HERE / 'console-server-receipt.json', record)
    print(json.dumps(record), flush=True)
    raise SystemExit(0 if record['request_matches'] and record['clean_peer_close'] else 1)


if __name__ == '__main__':
    main()
