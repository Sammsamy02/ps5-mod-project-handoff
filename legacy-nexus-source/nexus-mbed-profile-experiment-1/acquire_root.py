"""One authenticated, bounded acquisition of the official catalog's X1 DER."""
import datetime
import hashlib
import json
import pathlib
import signal
import ssl
import urllib.request

from cryptography import x509
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

HERE = pathlib.Path(__file__).resolve().parent
URL = "https://letsencrypt.org/certs/isrgrootx1.der"
CATALOG = "https://letsencrypt.org/certificates/"
EXPECTED_SHA256 = "96bcec06264976f37460779acf28c5a7cfe8a3c0aae11a8ffcee05c0bddf08c6"


class RejectRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise RuntimeError("root acquisition redirects are forbidden")


def deadline(signum, frame):
    raise TimeoutError("root acquisition total deadline")


def main():
    marker = HERE / "root-acquisition.json"
    with marker.open("x") as f:
        f.write('{"status":"reserved_before_request"}\n')
    if signal.getitimer(signal.ITIMER_REAL) != (0.0, 0.0):
        raise RuntimeError("existing timer")
    prior = signal.getsignal(signal.SIGALRM)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.load_verify_locations(cafile="/private/etc/ssl/cert.pem")
    if not context.check_hostname or context.verify_mode != ssl.CERT_REQUIRED or context.keylog_filename:
        raise RuntimeError("authenticated non-keylogging context required")
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), RejectRedirect(),
                                         urllib.request.HTTPSHandler(context=context))
    result = {"status": "started", "catalog_url": CATALOG, "catalog_last_updated": "2026-07-08",
              "catalog_link": "ISRG Root X1 self-signed DER, link 34; URL resolved through web tool",
              "url": URL, "ca_file": "/private/etc/ssl/cert.pem", "hostname_verification": True,
              "redirects_allowed": False, "socket_timeout_seconds": 5, "total_deadline_seconds": 15,
              "der_byte_limit": 16384, "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat()}
    signal.signal(signal.SIGALRM, deadline)
    signal.setitimer(signal.ITIMER_REAL, 15)
    try:
        request = urllib.request.Request(URL, headers={"Accept-Encoding": "identity"})
        with opener.open(request, timeout=5) as response:
            if response.status != 200 or response.geturl() != URL:
                raise RuntimeError("unexpected acquisition response")
            data = response.read(16385)
            if len(data) > 16384 or response.read(1):
                raise RuntimeError("DER byte limit")
            result["http_status"] = response.status
            result["content_type"] = response.headers.get("Content-Type")
        digest = hashlib.sha256(data).hexdigest()
        if digest != EXPECTED_SHA256:
            raise RuntimeError("official DER differs from observed host-local X1 identity")
        cert = x509.load_der_x509_certificate(data)
        key = cert.public_key()
        if cert.subject != cert.issuer or cert.subject.rfc4514_string() != "CN=ISRG Root X1,O=Internet Security Research Group,C=US":
            raise RuntimeError("X1 self-issued identity required")
        if not isinstance(key, rsa.RSAPublicKey) or key.key_size != 4096:
            raise RuntimeError("X1 RSA4096 key required")
        if not cert.extensions.get_extension_for_class(x509.BasicConstraints).value.ca:
            raise RuntimeError("CA constraint required")
        if not cert.extensions.get_extension_for_class(x509.KeyUsage).value.key_cert_sign:
            raise RuntimeError("keyCertSign required")
        key.verify(cert.signature, cert.tbs_certificate_bytes, padding.PKCS1v15(), cert.signature_hash_algorithm)
        now = datetime.datetime.now(datetime.timezone.utc)
        if not cert.not_valid_before_utc <= now <= cert.not_valid_after_utc:
            raise RuntimeError("root DER validity does not enclose host clock")
        (HERE / "isrg-root-x1.der").write_bytes(data)
        (HERE / "official_root.h").write_text("/* Public self-signed X1 DER acquired from the official CA catalog. */\nstatic const unsigned char official_root_der[]={" + ",".join(str(b) for b in data) + "};\n")
        result.update(status="authenticated_official_der_acquired", der_bytes=len(data), der_sha256=digest,
                      subject=cert.subject.rfc4514_string(), issuer=cert.issuer.rfc4514_string(),
                      key_algorithm="RSA", key_bits=key.key_size, signature="RSA PKCS1v1.5 / SHA256",
                      self_signature_verified=True, ca=True, key_cert_sign=True,
                      not_before_utc=cert.not_valid_before_utc.isoformat(), not_after_utc=cert.not_valid_after_utc.isoformat())
    except Exception as error:
        result.update(status="failed", error_type=type(error).__name__, error=str(error)[:256])
        raise
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, prior)
        result["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        marker.write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result))


if __name__ == "__main__":
    main()
