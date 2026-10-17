#!/usr/bin/env python3
"""Focused fixed-query admission and real owned HTTPS transport controls.

The retained curl8.18/MbedTLS archives are reused without rebuilding. Only the
producer's curl_multi_add_handle call is renamed for the existing private
loopback socket shim. It preserves real Host/SNI/CA/name verification and sends
no request to a live Nexus service. Receipts contain scalar checks, never the
borrowed request/credential or received body.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[4]
PROJECT = ROOT / "outputs/ps5-mod-bridge"
HOST = PROJECT / "native-host"
PROFILE = ROOT / ".superpowers/native-dependency-spike/native-curl-profile-experiment-1/host"
DEFAULT_MANIFEST = ROOT / ".superpowers/native-dependency-spike/native-iostore-generations-1/configured-host-1/desktop_fixture-download/manifest.json"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_hash(manifest, suffix):
    matches = [value for key, value in manifest["source_sha256"].items() if key.endswith(suffix)]
    if len(matches) != 1:
        raise RuntimeError("retained source identity is ambiguous")
    return matches[0]


def retained_inputs(manifest_path):
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("console_executed") or manifest.get("profile", {}).get("profile") != "desktop_fixture":
        raise RuntimeError("ordinary-host retained manifest required")
    desc = manifest["download_descriptor"]
    identity = desc["identity"]
    archives = [Path(path) for path in desc["link_archives"]]
    if [path.name for path in archives] != ["libcurl.a", "libmbedtls.a", "libmbedx509.a", "libmbedcrypto.a"]:
        raise RuntimeError("unexpected TLS archive composition")
    for archive in archives:
        expected = identity["archive_sha256"] if archive.name == "libcurl.a" else identity["mbedtls_archives_sha256"][archive.name]
        if digest(archive) != expected:
            raise RuntimeError("retained archive changed")
    objects = [manifest_path.parent / name for name in ("owner.o", "file_sink.o")]
    for obj in objects:
        source = "native-host/download/owner.c" if obj.stem == "owner" else "native-host/archive/private/file_sink.c"
        if digest(obj) != manifest["object_sha256"][obj.name] or digest(PROJECT / source) != source_hash(manifest, source):
            raise RuntimeError("retained owner/file sink identity changed")
    return objects, archives, desc["link_options"]


def certificates(directory):
    helper = HOST / "tests/nexus_client_verify.py"
    spec = importlib.util.spec_from_file_location("owned_requirements_certificates", helper)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.generate_certificates(directory)


def expected_request():
    query = (
        "query ModBridgeRequirements($modId: Int!, $gameDomainName: String!) { "
        "modRequirements(modId: $modId, gameDomainName: $gameDomainName) { "
        "nexusRequirements { nodes { id externalRequirement gameId modId modName notes url } nodesCount totalCount } "
        "dlcRequirements { gameExpansion { gameId id name } notes } } }"
    )
    return json.dumps({"query": query, "variables": {"gameDomainName": "silenthilltownfall", "modId": 62}}, separators=(",", ":")).encode("ascii")


def start_fixture(cert, key):
    routes = ["canonical_bearer", "canonical_apikey", "old_GET", "consumer_refusal",
              "cancel_after_consumer", "late_timeout_over_consumer", "after_timeout",
              "redirect_not_followed", "late_terminal_owner_fault"]
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.set_ciphers("ECDHE-ECDSA-AES128-GCM-SHA256")
    context.load_cert_chain(cert, key)
    observed_sni = []
    context.set_servername_callback(lambda _socket, name, _context: observed_sni.append(name == "api.nexusmods.com"))
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0)); listener.listen(16); listener.settimeout(0.2)
    port = listener.getsockname()[1]
    stopped = threading.Event()
    records = []
    errors = []

    def serve():
        while not stopped.is_set():
            try:
                peer, _ = listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                peer.settimeout(5)
                with context.wrap_socket(peer, server_side=True) as conn:
                    incoming = b""
                    while b"\r\n\r\n" not in incoming:
                        part = conn.recv(4096)
                        if not part or len(incoming) + len(part) > 65536:
                            raise ValueError("request framing")
                        incoming += part
                    head, body = incoming.split(b"\r\n\r\n", 1)
                    lines = head.decode("ascii").split("\r\n")
                    headers = {}
                    for line in lines[1:]:
                        name, value = line.split(":", 1)
                        name = name.lower()
                        if name in headers:
                            raise ValueError("duplicate header")
                        headers[name] = value.strip()
                    length = int(headers.get("content-length", "0"))
                    if not 0 <= length <= 4096:
                        raise ValueError("request length")
                    while len(body) < length:
                        part = conn.recv(length - len(body))
                        if not part:
                            raise ValueError("short request")
                        body += part
                    index = len(records)
                    case = routes[index] if index < len(routes) else "unexpected"
                    old_get = case == "old_GET"
                    key_mode = case == "canonical_apikey"
                    okay = (
                        index < len(routes) and
                        lines[0] == ("GET /v1/games HTTP/1.1" if old_get else "POST /v2/graphql HTTP/1.1") and
                        headers.get("host") == "api.nexusmods.com" and
                        headers.get("accept") == "application/json" and
                        headers.get("accept-encoding") == "identity" and
                        headers.get("application-name") == "PS5ModBridge" and
                        headers.get("application-version") == "0.1.0" and
                        not any(name in headers for name in ("cookie", "proxy-authorization", "transfer-encoding")) and
                        (headers.get("apikey") == "owned-requirements-apikey" and "authorization" not in headers if key_mode else
                         headers.get("authorization") == "Bearer owned-requirements-bearer" and "apikey" not in headers) and
                        (not body and "content-type" not in headers if old_get else
                         body == expected_request() and len(body) == length and headers.get("content-type") == "application/json")
                    )
                    records.append({"case": case, "canonical_request_and_headers": okay,
                                    "tls12": conn.version() == "TLSv1.2"})
                    payload = b'{"owned_get_body":true}' if old_get else b'{"owned_requirements_body":true}'
                    status = "302 Found" if case == "redirect_not_followed" else "200 OK"
                    location = "Location: https://api.nexusmods.com/v1/games\r\n" if case == "redirect_not_followed" else ""
                    response = (f"HTTP/1.1 {status}\r\nContent-Type: application/json\r\nContent-Length: {len(payload)}\r\n"
                                f"{location}Connection: close\r\n\r\n").encode("ascii") + payload
                    conn.sendall(response)
            except Exception as error:
                errors.append(type(error).__name__)
                peer.close()

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    return listener, stopped, worker, port, records, errors, observed_sni, routes


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-profile", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False, mode=0o700)
    records = []
    receipt = {"scope": "Focused actual retained host curl8.18 admission and owned loopback TLS; explicit test socket/fault/cancellation boundaries.",
               "console_executed": False, "production_provider_contacted": False, "native_build": False, "passed": False}

    def run(name, argv, env=None):
        result = subprocess.run([str(value) for value in argv], capture_output=True, text=True, timeout=30, env=env)
        records.append({"name": name, "argv": [str(value) for value in argv], "returncode": result.returncode,
                        "stdout": result.stdout, "stderr": result.stderr})
        print(name, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError(name + " failed")
        return result

    sources = [HOST / "download/curl_download.c", HOST / "download/provider_transfer.h",
               PROJECT / "include/modbridge/nexus_requirements_query.h", HOST / "download/tests/requirements_post_control.c",
               HOST / "download/tests/requirements_post_tls.c", HOST / "tests/nexus_auth_socket_shim.c", Path(__file__).resolve()]
    source_before = {str(path.relative_to(ROOT)): digest(path) for path in sources}
    try:
        objects, archives, options = retained_inputs(args.app_profile.resolve())
        cc = [PROFILE / "cc.py", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(HOST / "download"), "-I" + str(PROJECT / "include")]
        run("producer_compile", [*cc, "-c", HOST / "download/curl_download.c", "-o", output / "producer.o"])
        run("admission_link", [*cc, HOST / "download/tests/requirements_post_control.c", output / "producer.o", *objects, *archives, *options, "-o", output / "admission"])
        admission = run("admission_run", [output / "admission"])
        admission_summary = json.loads(admission.stdout.split("REQUIREMENTS_POST_ADMISSION ")[1].splitlines()[0])
        if admission_summary != {"checks": 29, "failures": 0, "consumed": 0, "runtime_started": False}:
            raise RuntimeError("unexpected admission receipt")
        run("producer_fixture_compile", [*cc, "-Dcurl_multi_add_handle=mb_nexus_auth_fixture_curl_multi_add_handle", "-c",
                                          HOST / "download/curl_download.c", "-o", output / "producer-fixture.o"])
        run("socket_shim_compile", [*cc, "-c", HOST / "tests/nexus_auth_socket_shim.c", "-o", output / "socket-shim.o"])
        run("tls_driver_link", [*cc, HOST / "download/tests/requirements_post_tls.c", output / "producer-fixture.o", output / "socket-shim.o",
                                *objects, *archives, *options, "-o", output / "tls-driver"])
        certdir = output / "certificates"; certdir.mkdir(mode=0o700)
        ca, cert, key = certificates(certdir)
        listener, stopped, worker, port, requests, errors, sni, routes = start_fixture(cert, key)
        try:
            env = dict(os.environ, MB_NEXUS_AUTH_FIXTURE_PORT=str(port))
            tls = run("actual_tls_run", [output / "tls-driver", ca], env=env)
        finally:
            stopped.set(); listener.close(); worker.join(timeout=6)
        summary = json.loads(tls.stdout.split("REQUIREMENTS_POST_TLS ")[1].splitlines()[0])
        shim = json.loads(tls.stdout.split("NEXUS_AUTH_SOCKET_SHIM ")[1].splitlines()[0])
        if worker.is_alive() or errors or len(requests) != len(routes) or not all(record["canonical_request_and_headers"] and record["tls12"] for record in requests) or sni != [True] * len(routes):
            raise RuntimeError("owned TLS request contract failed")
        if summary["checks"] != 13 or summary["failures"] or not summary["terminal"] or not summary["entropy_calls"] or summary["entropy_bytes"] != summary["entropy_calls"] * 32:
            raise RuntimeError("TLS owner/control contract failed")
        if not shim["configured"] or shim["installs"] != len(routes) or shim["connected"] != len(routes) or shim["refused"]:
            raise RuntimeError("owned loopback socket contract failed")
        receipt.update(passed=True, admission=admission_summary, tls=summary, requests=requests,
                       request_count=len(requests), exact_sni_verified=True, socket_shim=shim,
                       retained_object_sha256={str(path): digest(path) for path in objects},
                       archive_sha256={str(path): digest(path) for path in archives},
                       host_manifest_sha256=digest(args.app_profile), ca_sha256=digest(ca), leaf_sha256=digest(cert))
    except Exception as error:
        receipt["error"] = type(error).__name__ + ": " + str(error)
    receipt["commands"] = records
    receipt["source_sha256"] = source_before
    receipt["sources_unchanged"] = source_before == {str(path.relative_to(ROOT)): digest(path) for path in sources}
    receipt["passed"] = receipt["passed"] and receipt["sources_unchanged"]
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"passed": receipt["passed"], "request_count": receipt.get("request_count", 0), "error": receipt.get("error")}), flush=True)
    return 0 if receipt["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
