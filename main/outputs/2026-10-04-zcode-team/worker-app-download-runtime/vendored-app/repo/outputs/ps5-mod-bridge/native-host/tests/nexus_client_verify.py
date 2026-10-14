#!/usr/bin/env python3
"""Run one real HTTPS body through the private provider-transfer fixture.

This starts only an owned loopback TLS server. The tested code keeps its fixed
Nexus URL, Host/SNI and certificate-name verification. A private link-time
socket shim accepts only the production-resolved 127.0.0.1:443 destination and
connects that socket to the fixture's unprivileged loopback port.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading
import time


ROOT = Path(__file__).resolve().parents[4]
NATIVE = ROOT / "outputs/ps5-mod-bridge/native-host"
SPIKE = ROOT / ".superpowers/native-dependency-spike"
RED = SPIKE / "nexus-provider-transfer-1/consumer-expiry-red-setup-corrected-1"
PROFILE = SPIKE / "native-curl-profile-experiment-1/host"
HOST3 = SPIKE / "native-download-host-integration-1/host-3/desktop_fixture-download"
APP_PROFILE = SPIKE / "native-nexus-provider-requests-1/app-host-2/desktop_fixture-download"
LIBCURL = SPIKE / "native-download-host-integration-1/host-3/dependencies-download/libcurl.a"
MBED_INCLUDE = SPIKE / "mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/include"
MBED_LIBRARY = SPIKE / "mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5/library"
CURL_INCLUDE = SPIKE / "curl-release/include"


def run_quiet(argv, timeout=120):
    result = subprocess.run(argv, capture_output=True, text=True, timeout=timeout)
    if result.returncode:
        raise RuntimeError(f"{Path(argv[0]).name} failed ({result.returncode})")
    return result


def generate_certificates(directory):
    ca_key = directory / "fixture-ca.key"
    ca_pem = directory / "fixture-ca.pem"
    leaf_key = directory / "fixture-leaf.key"
    leaf_csr = directory / "fixture-leaf.csr"
    leaf_pem = directory / "fixture-leaf.pem"
    extensions = directory / "leaf-extensions.cnf"
    extensions.write_text(
        "[server]\n"
        "basicConstraints=critical,CA:FALSE\n"
        "keyUsage=critical,digitalSignature\n"
        "extendedKeyUsage=serverAuth\n"
        "subjectAltName=DNS:api.nexusmods.com,DNS:users.nexusmods.com\n",
        encoding="ascii",
    )
    run_quiet(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(ca_key)])
    run_quiet([
        "openssl", "req", "-x509", "-new", "-key", str(ca_key),
        "-out", str(ca_pem), "-days", "2", "-subj", "/CN=Owned local fixture CA",
        "-addext", "basicConstraints=critical,CA:TRUE",
        "-addext", "keyUsage=critical,keyCertSign,cRLSign",
    ])
    run_quiet(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(leaf_key)])
    run_quiet([
        "openssl", "req", "-new", "-key", str(leaf_key),
        "-out", str(leaf_csr), "-subj", "/CN=api.nexusmods.com",
    ])
    run_quiet([
        "openssl", "x509", "-req", "-in", str(leaf_csr), "-CA", str(ca_pem),
        "-CAkey", str(ca_key), "-CAcreateserial", "-out", str(leaf_pem),
        "-days", "2", "-sha256", "-extfile", str(extensions), "-extensions", "server",
    ])
    os.chmod(ca_key, 0o600)
    os.chmod(leaf_key, 0o600)
    return ca_pem, leaf_pem, leaf_key


def port443_preflight():
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        probe.bind(("127.0.0.1", 443))
        return {"result": "available"}
    except OSError as error:
        return {"result": "denied", "error_type": type(error).__name__}
    finally:
        probe.close()


def suite_plan():
    mod = {
        "mod_id": 17, "game_id": 7, "domain_name": "newgame2028",
        "name": "Fixture Mod", "author": "Fixture Author", "available": True,
        "status": "published", "version": "1.0",
    }
    other_mod = dict(mod, mod_id=9223372036854775807, domain_name="othergame_2049")
    bad_final = [mod, {"mod_id": 18, "game_id": 7, "domain_name": "newgame2028",
                       "author": "Fixture Author", "available": True, "status": "published"}]
    games = [{"id": 1, "domain_name": "newgame2028", "name": "Fixture Game", "mods": 1}]
    files = {"files": [{"file_id": 3, "name": "Fixture File", "file_name": "fixture.zip",
                        "version": "1.0", "size_kb": 1.0}]}
    oauth = {"sub": "12345", "name": "Fixture User", "membership_roles": [],
             "preferences": {"adult": False}}
    api_user = {"user_id": 12345, "name": "Fixture User", "is_premium": False,
                "is_supporter": False}
    json_bytes = lambda value: json.dumps(value, separators=(",", ":")).encode("utf-8")

    def route(case, path, host="api.nexusmods.com", auth="apikey", body=None, status=200):
        return {"case": case, "path": path, "host": host, "auth": auth,
                "body": json_bytes(body) if body is not None else b"", "status": status}

    archive = b"\x50\x4b\x03\x04opaque\x00\xff\x01B"
    return [
        route("games", "/v1/games", body=games),
        route("mods", "/v1/games/newgame2028/mods/latest_added", auth="bearer", body=[mod]),
        route("mod", "/v1/games/newgame2028/mods/17", body=mod),
        route("files", "/v1/games/newgame2028/mods/17/files", auth="bearer", body=files),
        route("oauth-user", "/oauth/userinfo", host="users.nexusmods.com", auth="bearer", body=oauth),
        route("api-key-user", "/v1/users/validate", body=api_user),
        route("other-domain-max-id", "/v1/games/othergame_2049/mods/9223372036854775807",
              body=other_mod),
        route("short-report-after-decode", "/v1/games/newgame2028/mods/17", body=mod),
        route("malformed-final-record", "/v1/games/newgame2028/mods/latest_added",
              auth="bearer", body=bad_final),
        route("http-401", "/v1/games/newgame2028/mods/17", status=401,
              body={"fixture": "unauthorized"}),
        {"case": "body-length-over-limit", "path": "/v1/games", "host": "api.nexusmods.com",
         "auth": "bearer", "body": b"", "status": 200, "declared_length": 8388609},
        route("metadata-before-archive", "/v1/games", body=games),
        {"case": "opaque-archive", "path": "/owned-archive", "host": "api.nexusmods.com",
         "auth": "none", "body": archive, "status": 200},
        route("final-metadata-after-archive", "/v1/games", body=games),
    ]


def start_fixture(cert, key, routes):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(16)
    listener.settimeout(.25)
    port = listener.getsockname()[1]
    stop = threading.Event()
    state = {"requests": 0, "request_checks": [], "server_errors": []}
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.set_ciphers("ECDHE-ECDSA-AES128-GCM-SHA256")
    context.load_cert_chain(str(cert), str(key))

    def serve():
        try:
            for expected in routes:
                peer = None
                while not stop.is_set() and peer is None:
                    try:
                        peer, _ = listener.accept()
                    except socket.timeout:
                        continue
                if peer is None:
                    return
                state["requests"] += 1
                peer.settimeout(5)
                record = {"case": expected["case"], "tls": False,
                          "path_ok": False, "host_ok": False,
                          "auth_ok": False, "app_headers_ok": False,
                          "no_cookie_or_proxy_auth": False,
                          "declared_length": expected.get("declared_length", len(expected["body"])),
                          "body_bytes_sent": 0}
                try:
                    with context.wrap_socket(peer, server_side=True) as tls:
                        record["tls"] = True
                        request = bytearray()
                        while b"\r\n\r\n" not in request and len(request) < 16384:
                            block = tls.recv(4096)
                            if not block:
                                break
                            request.extend(block)
                        head = bytes(request).split(b"\r\n\r\n", 1)[0]
                        lines = head.decode("latin-1").split("\r\n")
                        headers = {}
                        for line in lines[1:]:
                            if ":" in line:
                                name, value = line.split(":", 1)
                                headers[name.strip().lower()] = value.strip()
                        record["path_ok"] = lines[0] == f"GET {expected['path']} HTTP/1.1"
                        record["host_ok"] = headers.get("host", "").lower() == expected["host"]
                        if expected["auth"] == "apikey":
                            record["auth_ok"] = (headers.get("apikey") == "fixture-only-api-key"
                                                 and "authorization" not in headers)
                        elif expected["auth"] == "bearer":
                            record["auth_ok"] = (headers.get("authorization") == "Bearer fixture-only-bearer"
                                                 and "apikey" not in headers)
                        else:
                            record["auth_ok"] = not any(
                                name in headers for name in ("authorization", "apikey", "cookie", "proxy-authorization"))
                        record["no_cookie_or_proxy_auth"] = not any(
                            name in headers for name in ("cookie", "proxy-authorization"))
                        if expected["auth"] == "none":
                            record["app_headers_ok"] = (
                                headers.get("accept-encoding") == "identity"
                                and headers.get("accept") in (None, "*/*")
                                and "application-name" not in headers
                                and "application-version" not in headers
                            )
                        else:
                            record["app_headers_ok"] = (
                                headers.get("accept-encoding") == "identity"
                                and headers.get("accept") == "application/json"
                                and headers.get("application-name") == "PS5ModBridge"
                                and headers.get("application-version") == "0.1.0"
                            )
                        status = expected["status"]
                        length = expected.get("declared_length", len(expected["body"]))
                        reason = "OK" if status == 200 else "Unauthorized"
                        payload = (
                            f"HTTP/1.1 {status} {reason}\r\n"
                            "Content-Type: application/json\r\n"
                            f"Content-Length: {length}\r\n"
                            "Connection: close\r\n\r\n"
                        ).encode("ascii")
                        tls.sendall(payload)
                        if expected.get("body"):
                            tls.sendall(expected["body"])
                            record["body_bytes_sent"] = len(expected["body"])
                except (OSError, ssl.SSLError) as error:
                    record["error_type"] = type(error).__name__
                    record["error_reason"] = getattr(error, "reason", "")
                finally:
                    state["request_checks"].append(record)
        except OSError as error:
            if not stop.is_set():
                state["server_errors"].append(type(error).__name__)
        finally:
            listener.close()

    thread = threading.Thread(target=serve, name="owned-nexus-tls-fixture", daemon=True)
    thread.start()
    def close():
        stop.set()
        listener.close()

    return port, thread, state, close


def sha256_file(path):
    import hashlib
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def reusable_cpp_objects():
    manifest = json.loads((APP_PROFILE / "manifest.json").read_text(encoding="utf-8"))
    sources = {
        "nexus_client.o": "outputs/ps5-mod-bridge/native-host/nexus_client.cpp",
        "nexus_bridge.o": "outputs/ps5-mod-bridge/native-host/nexus_bridge.cpp",
        "nexus.o": "outputs/ps5-mod-bridge/src/nexus.cpp",
    }
    result = {}
    for name, relative_source in sources.items():
        expected_source = manifest["source_sha256"].get(relative_source)
        expected_object = manifest["object_sha256"].get(name)
        source = ROOT / relative_source
        obj = APP_PROFILE / name
        if (not expected_source or not expected_object or not source.is_file() or not obj.is_file()
                or sha256_file(source) != expected_source or sha256_file(obj) != expected_object):
            raise RuntimeError(f"retained application object failed source/hash verification: {name}")
        closure = manifest.get("include_closure", {}).get(name + ".d")
        if not closure:
            raise RuntimeError(f"retained application object lacks its include closure: {name}")
        for entry in closure:
            included = Path(entry["path"])
            if not included.is_file() or sha256_file(included) != entry["sha256"]:
                raise RuntimeError(f"retained application object include bytes changed: {name}")
        result[name] = {"path": str(obj), "sha256": expected_object,
                        "source": relative_source, "source_sha256": expected_source,
                        "verified_include_entries": len(closure)}
    return [APP_PROFILE / name for name in sources], result


def compile_driver(directory, source_mode, timeout_priority, mode):
    objdir = directory / "objects"
    objdir.mkdir(mode=0o700)
    if source_mode == "pre-priority":
        curl_source = RED / "native-host/download/curl_download.c"
        curl_headers = RED / "native-host/download"
        sink_headers = RED / "native-host/archive/private"
    elif source_mode == "current":
        curl_source = NATIVE / "download/curl_download.c"
        curl_headers = NATIVE / "download"
        sink_headers = NATIVE / "archive/private"
    else:
        raise ValueError("unknown source mode")

    common = [
        "/usr/bin/clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Os",
        "-D_POSIX_C_SOURCE=200809L",
        "-include", str(HOST3 / "config.h"),
        "-I", str(NATIVE), "-I", str(NATIVE.parent / "include"),
        "-isystem", str(NATIVE.parent / "third_party"),
        "-isystem", str(NATIVE.parent / "third_party/picosha2"),
        "-I", str(curl_headers), "-I", str(sink_headers),
        "-I" + str(SPIKE / "native-curl-profile-experiment-1"),
        "-I" + str(MBED_INCLUDE), "-I" + str(MBED_LIBRARY),
        "-I" + str(CURL_INCLUDE), '-DMBEDTLS_CONFIG_FILE="tls_config.h"',
        "-include", str(MBED_LIBRARY / "common.h"),
        "-include", str(SPIKE / "native-curl-profile-experiment-1/tls_effective.h"),
    ]
    driver_defines = ("-DMB_NEXUS_LATE_ONLY=1", f"-DMB_EXPECT_TIMEOUT_PRIORITY={timeout_priority}") \
        if mode == "late-consumer" else ()
    sources = [
        ("owner", NATIVE / "download/owner.c", ()),
        ("curl-download", curl_source, ("-Dcurl_multi_add_handle=fixture_curl_multi_add_handle",)),
        ("file-sink", NATIVE / "archive/private/file_sink.c", ()),
        ("driver", NATIVE / "tests/nexus_client_driver.c", driver_defines),
    ]
    objects = []
    commands = []
    for name, source, extra in sources:
        target = objdir / f"{name}.o"
        command = [*common, *extra, "-c", str(source), "-o", str(target)]
        run_quiet(command)
        commands.append([Path(command[0]).name, *command[1:]])
        objects.append(target)
    reused, reused_records = ([], {})
    if mode != "late-consumer":
        reused, reused_records = reusable_cpp_objects()
    binary = directory / "nexus-client-driver"
    command = [
        "/usr/bin/clang++", *(str(item) for item in objects),
        *(str(item) for item in reused), str(LIBCURL),
        str(PROFILE / "libmbedtls.a"), str(PROFILE / "libmbedx509.a"),
        str(PROFILE / "libmbedcrypto.a"), "-framework", "SystemConfiguration",
        "-framework", "CoreFoundation", "-o", str(binary),
    ]
    run_quiet(command)
    commands.append({"tool": Path(command[0]).name, "source_objects": len(objects),
                     "reused_cpp_objects": len(reused), "output": str(binary)})
    pinned_libraries = {str(path): sha256_file(path) for path in (
        LIBCURL, PROFILE / "libmbedtls.a", PROFILE / "libmbedx509.a", PROFILE / "libmbedcrypto.a")}
    return binary, commands, reused_records, pinned_libraries


def main():
    global APP_PROFILE
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("late-consumer", "typed-six", "suite"), default="late-consumer")
    parser.add_argument("--source-mode", choices=("pre-priority", "current"), required=True)
    parser.add_argument("--expect", choices=("red", "green"), required=True)
    parser.add_argument("--app-profile", type=Path, default=APP_PROFILE,
                        help="prepared actual application host profile for checked C++ object reuse")
    args = parser.parse_args()
    APP_PROFILE = args.app_profile.resolve()
    if args.mode == "late-consumer":
        if (args.source_mode, args.expect) not in (("pre-priority", "red"), ("current", "green")):
            parser.error("late-consumer accepts pre-priority/red or current/green only")
    elif args.source_mode != "current" or args.expect != "green":
        parser.error("typed/suite modes require current source and green expectation")

    evidence_root = SPIKE / "nexus-client-https-fixture-1"
    evidence_root.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(evidence_root, 0o700)
    directory = Path(tempfile.mkdtemp(prefix=f"{args.expect}-", dir=evidence_root))
    os.chmod(directory, 0o700)
    ca_pem, leaf_pem, leaf_key = generate_certificates(directory)
    port443 = port443_preflight()
    priority = 0 if args.expect == "red" else 1
    binary, commands, reused_objects, pinned_libraries = compile_driver(
        directory, args.source_mode, priority, args.mode)
    shim_control = None
    if args.mode != "late-consumer":
        control = subprocess.run([str(binary), "--shim-check"], capture_output=True,
                                 text=True, timeout=5)
        try:
            shim_control = json.loads(control.stdout)
        except ValueError as error:
            raise RuntimeError("socket-shim control emitted an invalid receipt") from error
        if control.returncode != 0:
            raise RuntimeError("socket-shim destination refusal controls failed")
    if args.mode == "late-consumer":
        routes = [{"case": "late-consumer", "path": "/v1/games", "host": "api.nexusmods.com",
                  "auth": "bearer", "body": b"[]", "status": 200}]
    else:
        planned = suite_plan()
        routes = planned[:6] if args.mode == "typed-six" else planned
    port, thread, fixture, close_fixture = start_fixture(leaf_pem, leaf_key, routes)
    try:
        run = subprocess.run(
            [str(binary), args.mode, str(port), str(ca_pem)],
            capture_output=True, text=True, timeout=30,
        )
    finally:
        close_fixture()
    thread.join(timeout=3)
    if thread.is_alive():
        raise RuntimeError("owned TLS fixture thread did not stop within its bound")
    try:
        records = [json.loads(line) for line in run.stdout.splitlines() if line.strip()]
    except ValueError as error:
        raise RuntimeError("driver emitted a non-JSON receipt") from error
    checks = fixture.get("request_checks", [])
    route_checks_ok = len(checks) == len(routes) and all(
        all(check.get(field) for field in ("tls", "path_ok", "host_ok", "auth_ok",
                                           "app_headers_ok", "no_cookie_or_proxy_auth"))
        for check in checks)
    shim = next((entry for entry in records if entry.get("case") == "socket-shim"), {})
    runtime_close = next((entry for entry in records if entry.get("case") == "runtime-close"), {})
    expected_calls = len(routes)
    expected_status = 13 if args.expect == "red" else 12
    if args.mode == "late-consumer":
        observed = next((entry for entry in records if entry.get("case") == "late-consumer-expiry"), {})
        operation_checks = (
            observed.get("http") == 200 and observed.get("consumer_calls") == 1
            and observed.get("body_matches") is True and observed.get("body_wiped") is True
            and observed.get("status") == expected_status and observed.get("complete") == 0
        )
        minimum_windows = 3
    elif args.mode == "typed-six":
        expected_names = ["games", "mods", "mod", "files", "oauth-user", "api-key-user"]
        typed = [item for item in records if item.get("case") in expected_names]
        observed = {"typed_routes": [item.get("case") for item in typed], "socket_shim": shim,
                    "runtime_close": runtime_close}
        operation_checks = (
            [item.get("case") for item in typed] == expected_names
            and all(item.get("return") == 0 and item.get("status") == 0
                    and item.get("complete") == 1 and item.get("decoded") == 1
                    and item.get("http") == 200 and item.get("expected_marker") is True
                    and item.get("raw_wiped") is True for item in typed)
        )
        minimum_windows = 8
    else:
        by_case = {item.get("case"): item for item in records}
        successful = ["games", "mods", "mod", "files", "oauth-user", "api-key-user",
                      "other-domain-max-id", "metadata-before-archive", "final-metadata-after-archive"]
        success_checks = all(
            by_case.get(name, {}).get("return") == 0
            and by_case[name].get("status") == 0
            and by_case[name].get("complete") == 1
            and by_case[name].get("decoded") == 1
            and by_case[name].get("http") == 200
            and by_case[name].get("expected_marker") is True
            and by_case[name].get("raw_wiped") is True
            for name in successful if name in by_case
        ) and all(name in by_case for name in successful)
        short = by_case.get("short-report-after-decode", {})
        malformed = by_case.get("malformed-final-record", {})
        unauthorized = by_case.get("http-401", {})
        oversize = by_case.get("body-length-over-limit", {})
        archive_result = by_case.get("opaque-archive", {})
        checks_by_case = {check.get("case"): check for check in checks}
        oversize_fixture = checks_by_case.get("body-length-over-limit", {})
        archive_fixture = checks_by_case.get("opaque-archive", {})
        order_ok = [check.get("case") for check in checks] == [route["case"] for route in routes]
        focused = (
            short.get("return") == 3 and short.get("status") == 3 and not short.get("complete")
            and short.get("http") == 200 and short.get("required", 0) > 1
            and short.get("raw_wiped") and short.get("report_wiped")
            and malformed.get("return") == 0 and malformed.get("status") == 0
            and malformed.get("complete") and not malformed.get("metadata_decoded")
            and malformed.get("http") == 200 and malformed.get("raw_wiped")
            and malformed.get("no_prefix_published")
            and unauthorized.get("return") == 2 and unauthorized.get("status") == 2
            and not unauthorized.get("complete") and unauthorized.get("http") == 401
            and unauthorized.get("raw_wiped") and unauthorized.get("report_wiped")
            and oversize.get("return") == 2 and oversize.get("status") == 2
            and not oversize.get("complete") and oversize.get("http") == 200
            and oversize.get("curl") == 23 and oversize.get("received") == 0 and oversize.get("raw_wiped")
            and oversize.get("report_wiped")
            and oversize_fixture.get("declared_length") == 8388609
            and oversize_fixture.get("body_bytes_sent") == 0
            and archive_result.get("return") == 0 and archive_result.get("status") == 0
            and archive_result.get("complete") and archive_result.get("http") == 200
            and archive_result.get("received") == 14 and archive_result.get("sink_complete")
            and archive_result.get("readback_verified") and archive_result.get("bytes_match")
            and archive_fixture.get("auth_ok") and archive_fixture.get("no_cookie_or_proxy_auth")
            and archive_fixture.get("app_headers_ok") and order_ok
        )
        observed = {"successful_metadata_calls": successful,
                    "short_report": short, "malformed_final": malformed,
                    "http_401": unauthorized, "oversize_length": oversize,
                    "opaque_archive": archive_result,
                    "socket_shim": shim, "runtime_close": runtime_close}
        operation_checks = success_checks and focused
        minimum_windows = 16
    ok = (
        run.returncode == 0 and route_checks_ok and not fixture.get("server_errors")
        and fixture.get("requests") == expected_calls
        and shim.get("installs") == expected_calls and shim.get("opens") == expected_calls
        and shim.get("connected") == expected_calls and shim.get("refused") == 0
        and runtime_close.get("status") == 0 and runtime_close.get("initialized") == 0
        and runtime_close.get("windows", 0) >= minimum_windows and operation_checks
    )
    if args.mode != "late-consumer":
        ok = ok and all(shim_control.get(name) is True for name in (
            "accepts_loopback443", "rejects_port444", "rejects_non_loopback", "rejects_wrong_purpose"))
    receipt = {
        "test": args.mode,
        "source_mode": args.source_mode,
        "expectation": args.expect,
        "expected_provider_status": expected_status if args.mode == "late-consumer" else None,
        "observed": observed,
        "driver_records": records,
        "socket_shim_negative_controls": shim_control,
        "driver_exit": run.returncode,
        "fixture": fixture,
        "fixture_port": port,
        "port443_preflight": port443,
        "socket_routing": "private curl_multi_add_handle shim; destination predicate requires 127.0.0.1:443 and connects the already-approved socket to this owned ephemeral loopback listener",
        "compiler_commands": commands,
        "reused_cpp_objects": reused_objects,
        "pinned_library_sha256": pinned_libraries,
        "retained_dependency_reuse": "three C++ objects were reused only after their object, current source and complete include-closure bytes matched the selected application profile manifest; curl and Mbed TLS archives are identified by SHA-256",
        "route_checks_passed": bool(route_checks_ok),
        "assertions_passed": bool(ok),
    }
    (directory / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "expectation": args.expect,
        "source_mode": args.source_mode,
        "mode": args.mode,
        "provider_status": observed.get("status"),
        "complete": observed.get("complete"),
        "http": observed.get("http"),
        "consumer_calls": observed.get("consumer_calls"),
        "body_matches": observed.get("body_matches"),
        "body_wiped": observed.get("body_wiped"),
        "fixture_requests": fixture.get("requests", 0),
        "route_checks_passed": route_checks_ok,
        "port443_preflight": port443,
        "assertions_passed": bool(ok),
        "receipt": str(directory / "receipt.json"),
    }, sort_keys=True))
    if not ok:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
