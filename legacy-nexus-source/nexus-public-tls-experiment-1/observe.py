"""One public TLS-only attempt per fixed hostname; public certificate metadata only."""
import datetime
import json
import pathlib
import signal
import socket
import ssl
import sys
import time
import _ssl

from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, rsa

HOSTS = ("users.nexusmods.com", "api.nexusmods.com")
CA_FILE = "/private/etc/ssl/cert.pem"
CIPHER = "ECDHE-ECDSA-AES128-GCM-SHA256"
SOCKET_TIMEOUT_SECONDS = 8.0
TOTAL_ATTEMPT_SECONDS = 15.0
RESULT = pathlib.Path(__file__).with_name("observation.json")


class AttemptDeadline(RuntimeError):
    pass


def deadline_signal(signum, frame):
    raise AttemptDeadline("15 second total attempt deadline")


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def certificate_metadata(certificate):
    der = certificate.public_bytes(_ssl.ENCODING_DER)
    cert = x509.load_der_x509_certificate(der)
    key = cert.public_key()
    if isinstance(key, ec.EllipticCurvePublicKey):
        key_info = {"algorithm": "EC", "curve": key.curve.name, "bits": key.key_size}
    elif isinstance(key, rsa.RSAPublicKey):
        key_info = {"algorithm": "RSA", "bits": key.key_size}
    else:
        key_info = {"algorithm": type(key).__name__}
    digest = cert.signature_hash_algorithm
    return {
        "subject": cert.subject.rfc4514_string(),
        "issuer": cert.issuer.rfc4514_string(),
        "public_key": key_info,
        "signature_algorithm_oid": cert.signature_algorithm_oid.dotted_string,
        "signature_algorithm": cert.signature_algorithm_oid._name,
        "signature_hash": None if digest is None else digest.name,
        "not_before_utc": cert.not_valid_before_utc.isoformat(),
        "not_after_utc": cert.not_valid_after_utc.isoformat(),
        "certificate_sha256": cert.fingerprint(hashes.SHA256()).hex(),
    }


def save(document):
    RESULT.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def main():
    # A retained result forbids an accidental second run of either handshake.
    with RESULT.open("x", encoding="utf-8") as marker:
        marker.write('{"status":"reserved_before_network"}\n')
    if signal.getitimer(signal.ITIMER_REAL) != (0.0, 0.0):
        raise RuntimeError("an existing real timer prevents bounded execution")
    previous_handler = signal.getsignal(signal.SIGALRM)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.verify_mode = ssl.CERT_REQUIRED
    context.check_hostname = True
    # Explicit host-default CA path avoids environment-supplied trust/keylog paths.
    context.load_verify_locations(cafile=CA_FILE)
    context.set_ciphers(CIPHER)
    if context.keylog_filename is not None:
        raise RuntimeError("key logging must be disabled")
    offered = [c["name"] for c in context.get_ciphers() if c["protocol"] != "TLSv1.3"]
    if offered != [CIPHER]:
        raise RuntimeError("unexpected TLS 1.2 cipher offer")
    document = {
        "policy": {
            "domains": list(HOSTS), "port": 443,
            "tls_minimum": "TLSv1.2", "tls_maximum": "TLSv1.2",
            "tls12_cipher_offer": offered,
            "hostname_verification": True, "verify_mode": "CERT_REQUIRED",
            "system_default_ca_file": CA_FILE,
            "ca_file_bytes": pathlib.Path(CA_FILE).stat().st_size,
            "verify_flags": int(context.verify_flags),
            "groups_and_signature_algorithms": "host OpenSSL defaults; not the native fixture offer",
            "socket_timeout_seconds": SOCKET_TIMEOUT_SECONDS,
            "total_attempt_deadline_seconds": TOTAL_ATTEMPT_SECONDS,
            "http_requests": 0, "application_data_sent": 0,
            "retries": 0, "key_logging": False,
            "supplied_chain_source": "CPython _sslobj.get_unverified_chain()",
            "verified_chain_source": "CPython _sslobj.get_verified_chain(); may include locally supplied anchor",
        },
        "runtime": {"python": sys.version.split()[0], "openssl": ssl.OPENSSL_VERSION},
        "attempts": [],
    }
    save(document)
    signal.signal(signal.SIGALRM, deadline_signal)
    try:
        for hostname in HOSTS:
            attempt = {
                "hostname": hostname, "sni": hostname, "started_utc": utc_now(),
                "tls_handshake_attempts": 0, "phase": "connect",
                "status": "started", "version": None, "cipher": None,
                "supplied_chain": [], "verified_chain": [],
            }
            document["attempts"].append(attempt)
            save(document)
            started = time.monotonic()
            signal.setitimer(signal.ITIMER_REAL, TOTAL_ATTEMPT_SECONDS)
            raw = None
            channel = None
            try:
                raw = socket.create_connection((hostname, 443), timeout=SOCKET_TIMEOUT_SECONDS)
                attempt["peer_address"] = list(raw.getpeername())
                attempt["phase"] = "handshake"
                attempt["tls_handshake_attempts"] = 1
                # No application read/write, shutdown exchange, retry, or fallback.
                channel = context.wrap_socket(raw, server_hostname=hostname)
                raw = None
                attempt["status"] = "authenticated_handshake_succeeded"
                attempt["version"] = channel.version()
                cipher = channel.cipher()
                attempt["cipher"] = {"name": cipher[0], "protocol": cipher[1], "bits": cipher[2]}
                attempt["phase"] = "certificate_metadata"
                supplied = channel._sslobj.get_unverified_chain()
                verified = channel._sslobj.get_verified_chain()
                attempt["supplied_chain"] = [certificate_metadata(c) for c in supplied]
                attempt["verified_chain"] = [certificate_metadata(c) for c in verified]
                attempt["phase"] = "complete"
            except (OSError, ssl.SSLError, AttemptDeadline) as error:
                attempt["status"] = "failed"
                attempt["error_type"] = type(error).__name__
                attempt["error"] = str(error)[:256]
            except Exception as error:
                attempt["status"] = "metadata_or_harness_failed"
                attempt["error_type"] = type(error).__name__
                attempt["error"] = str(error)[:256]
            finally:
                try:
                    if channel is not None:
                        channel.close()
                    if raw is not None:
                        raw.close()
                finally:
                    signal.setitimer(signal.ITIMER_REAL, 0.0)
                    attempt["elapsed_seconds"] = round(time.monotonic() - started, 6)
                    attempt["finished_utc"] = utc_now()
                    save(document)
            print(json.dumps({k: attempt[k] for k in ("hostname", "status", "version", "cipher", "elapsed_seconds")}))
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0.0)
        signal.signal(signal.SIGALRM, previous_handler)


if __name__ == "__main__":
    main()
