#!/usr/bin/env python3
"""Exercise the native Nexus resolver and credential-free sink on owned TLS only."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time
from urllib.parse import quote, urlsplit, parse_qsl


ROOT = Path(__file__).resolve().parents[4]
NATIVE = ROOT / "outputs/ps5-mod-bridge/native-host"
SPIKE = ROOT / ".superpowers/native-dependency-spike"
ARCHIVE_HOST = "owned-archive.fixture.test"
ARCHIVE_URI = f"https://{ARCHIVE_HOST}/signed/archive.zip?sig=fixture-only-signature"
ARCHIVE_BODY = b"\x50\x4b\x03\x04owned-archive\x00\xff"
GRANT_KEY = "fixture+key&/v1"

FAKE_CREDENTIALS = {
    "fixture-only-premium-key": "premium",
    "fixture-only-grant-key": "website-grant",
    "fixture-only-free-key": "nonpremium-no-grant",
    "fixture-only-account-key": "wrong-account-grant",
    "fixture-only-source-key": "wrong-source-grant",
    "fixture-only-expired-key": "expired-grant",
    "fixture-only-hidden-key": "hidden-mod",
    "fixture-only-adult-key": "unknown-adult",
    "fixture-only-mirror-key": "malformed-final-mirror",
    "fixture-only-http-key": "bad-http",
    "fixture-only-timeout-key": "total-deadline",
    "fixture-only-healthy-key": "healthy-after-failures",
}

DOMAINS = {
    "premium": "fixture-premium", "website-grant": "fixture-grant",
    "nonpremium-no-grant": "fixture-no-grant", "wrong-account-grant": "fixture-wrong-account",
    "wrong-source-grant": "fixture-wrong-source", "expired-grant": "fixture-expired",
    "hidden-mod": "fixture-hidden", "unknown-adult": "fixture-adult",
    "malformed-final-mirror": "fixture-mirror", "bad-http": "fixture-http",
    "total-deadline": "fixture-timeout", "healthy-after-failures": "fixture-healthy",
}


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
        f"subjectAltName=DNS:api.nexusmods.com,DNS:users.nexusmods.com,DNS:{ARCHIVE_HOST}\n",
        encoding="ascii",
    )
    run_quiet(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(ca_key)])
    run_quiet([
        "openssl", "req", "-x509", "-new", "-key", str(ca_key), "-out", str(ca_pem),
        "-days", "2", "-subj", "/CN=Owned local Nexus download fixture CA",
        "-addext", "basicConstraints=critical,CA:TRUE",
        "-addext", "keyUsage=critical,keyCertSign,cRLSign",
    ])
    run_quiet(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(leaf_key)])
    run_quiet(["openssl", "req", "-new", "-key", str(leaf_key), "-out", str(leaf_csr),
               "-subj", "/CN=api.nexusmods.com"])
    run_quiet([
        "openssl", "x509", "-req", "-in", str(leaf_csr), "-CA", str(ca_pem),
        "-CAkey", str(ca_key), "-CAcreateserial", "-out", str(leaf_pem),
        "-days", "2", "-sha256", "-extfile", str(extensions), "-extensions", "server",
    ])
    os.chmod(ca_key, 0o600)
    os.chmod(leaf_key, 0o600)
    return ca_pem, leaf_pem, leaf_key


def mod_record(case):
    domain = DOMAINS[case]
    adult = case == "unknown-adult"
    available = case != "hidden-mod"
    status = "archived" if case == "hidden-mod" else "published"
    return {
        "mod_id": 17, "game_id": 7, "domain_name": domain,
        "name": "Fixture Mod", "author": "Fixture Author", "summary": "",
        "description": "", "version": "1.0", "contains_adult_content": adult,
        "available": available, "status": status,
    }


def profile_record(case):
    return {
        "user_id": 4404 if case == "wrong-account-grant" else {
            "premium": 1101, "website-grant": 2202, "nonpremium-no-grant": 3303,
            "wrong-source-grant": 5505, "expired-grant": 6606,
            "hidden-mod": 7707, "unknown-adult": 8808, "malformed-final-mirror": 9909,
            "bad-http": 1010, "total-deadline": 1111, "healthy-after-failures": 1212,
        }[case],
        "name": "Owned fixture profile",
        "is_premium": case not in ("website-grant", "nonpremium-no-grant", "wrong-account-grant"),
        "is_supporter": False,
    }


def link_body(case):
    if case == "malformed-final-mirror":
        return [
            {"URI": ARCHIVE_URI, "name": "First mirror", "short_name": "first"},
            {"URI": "http://invalid.fixture/archive", "name": "Bad mirror", "short_name": "bad"},
        ]
    return [
        {"URI": ARCHIVE_URI, "name": "First mirror", "short_name": "first"},
        {"URI": f"https://secondary.{ARCHIVE_HOST}/mirror.zip?sig=other-fixture", "name": "Second mirror", "short_name": "second"},
    ]


def start_fixture(cert, key):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(64)
    listener.settimeout(.2)
    port = listener.getsockname()[1]
    stopped = threading.Event()
    state = {"requests": 0, "checks": [], "server_errors": []}
    sni_by_socket = {}
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.set_ciphers("ECDHE-ECDSA-AES128-GCM-SHA256")
    context.load_cert_chain(str(cert), str(key))

    def record_sni(tls_socket, server_name, _initial_context):
        sni_by_socket[id(tls_socket)] = server_name

    context.set_servername_callback(record_sni)

    def serve():
        try:
            while not stopped.is_set():
                try:
                    peer, _ = listener.accept()
                except socket.timeout:
                    continue
                except OSError:
                    if stopped.is_set():
                        return
                    raise
                state["requests"] += 1
                peer.settimeout(5)
                record = {"case": "unrecognized", "route": "unrecognized", "tls": False,
                          "sni_ok": False,
                          "path_ok": False, "host_ok": False, "auth_ok": False,
                          "app_headers_ok": False, "no_cookie_or_proxy_auth": False,
                          "query_ok": False, "response_status": 0, "response_sent": False}
                try:
                    with context.wrap_socket(peer, server_side=True) as tls:
                        record["tls"] = True
                        sni_host = sni_by_socket.pop(id(tls), None)
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
                        target = lines[0].split(" ")[1] if len(lines[0].split(" ")) == 3 else ""
                        parsed = urlsplit(target)
                        host_header = headers.get("host", "").lower()
                        record["no_cookie_or_proxy_auth"] = not any(
                            name in headers for name in ("cookie", "proxy-authorization"))
                        if host_header == ARCHIVE_HOST:
                            record["case"] = "archive-transfer"
                            record["route"] = "archive"
                            record["host_ok"] = True
                            record["sni_ok"] = sni_host == ARCHIVE_HOST
                            record["path_ok"] = lines[0] == "GET /signed/archive.zip?sig=fixture-only-signature HTTP/1.1"
                            record["auth_ok"] = not any(name in headers for name in ("apikey", "authorization"))
                            record["app_headers_ok"] = (
                                headers.get("accept-encoding") == "identity"
                                and headers.get("accept") in (None, "*/*")
                                and "application-name" not in headers and "application-version" not in headers
                            )
                            record["query_ok"] = True
                            status, body = 200, ARCHIVE_BODY
                        else:
                            credential = headers.get("apikey")
                            case = FAKE_CREDENTIALS.get(credential)
                            record["case"] = case or "unrecognized"
                            record["host_ok"] = host_header == "api.nexusmods.com"
                            record["sni_ok"] = sni_host == "api.nexusmods.com"
                            record["auth_ok"] = bool(case and credential and
                                "authorization" not in headers and "apikey" in headers)
                            record["app_headers_ok"] = (
                                headers.get("accept-encoding") == "identity"
                                and headers.get("accept") == "application/json"
                                and headers.get("application-name") == "PS5ModBridge"
                                and headers.get("application-version") == "0.1.0"
                            )
                            route = ""
                            status = 200
                            if parsed.path == "/v1/users/validate":
                                route = "profile"
                                record["path_ok"] = lines[0] == "GET /v1/users/validate HTTP/1.1"
                                body = profile_record(case) if case else {}
                                record["query_ok"] = parsed.query == ""
                            else:
                                match = re.fullmatch(
                                    r"/v1/games/([a-z0-9_-]{1,80})/mods/([1-9][0-9]*)"
                                    r"(?:/files/([1-9][0-9]*)/download_link)?", parsed.path)
                                if match and case and match.group(1) == DOMAINS[case] and match.group(2) == "17":
                                    if match.group(3) is None:
                                        route = "mod"
                                        record["path_ok"] = lines[0] == f"GET {parsed.path} HTTP/1.1"
                                        body = mod_record(case)
                                        record["query_ok"] = parsed.query == ""
                                    elif match.group(3) == "3":
                                        route = "link"
                                        record["path_ok"] = lines[0] == f"GET {target} HTTP/1.1"
                                        if case == "website-grant":
                                            pairs = parse_qsl(parsed.query, keep_blank_values=True)
                                            expected_key = f"key={quote(GRANT_KEY, safe='')}"
                                            expiry_match = re.fullmatch(r"expires=([1-9][0-9]{0,18})", pairs[1][0] + "=" + pairs[1][1]) if len(pairs) == 2 else None
                                            record["query_ok"] = (
                                                len(pairs) == 2 and pairs[0] == ("key", GRANT_KEY)
                                                and bool(expiry_match) and int(expiry_match[1]) > int(time.time())
                                                and parsed.query == expected_key + "&expires=" + pairs[1][1]
                                            )
                                        else:
                                            record["query_ok"] = parsed.query == ""
                                        if case == "bad-http":
                                            status, body = 401, {"fixture": "unauthorized"}
                                        elif case == "total-deadline":
                                            body = link_body(case)
                                        else:
                                            body = link_body(case)
                                else:
                                    status, body = 404, {"fixture": "unrecognized"}
                            record["route"] = route or "unrecognized"
                            if case is None:
                                record["route"] = "unrecognized"
                        record["response_status"] = status
                        if record["case"] == "total-deadline" and record["route"] == "link":
                            time.sleep(3.5)
                        if record["route"] == "archive":
                            encoded = body
                            content_type = "application/octet-stream"
                        else:
                            encoded = json.dumps(body, separators=(",", ":")).encode("utf-8")
                            content_type = "application/json"
                        reason = "OK" if status == 200 else "Unauthorized" if status == 401 else "Not Found"
                        response = (f"HTTP/1.1 {status} {reason}\r\n"
                                    f"Content-Type: {content_type}\r\n"
                                    f"Content-Length: {len(encoded)}\r\n"
                                    "Connection: close\r\n\r\n").encode("ascii")
                        tls.sendall(response + encoded)
                        record["response_sent"] = True
                except (OSError, ssl.SSLError) as error:
                    record["server_error_type"] = type(error).__name__
                    record["server_error_reason"] = getattr(error, "reason", "")
                finally:
                    state["checks"].append(record)
        except OSError as error:
            if not stopped.is_set():
                state["server_errors"].append(type(error).__name__)
        except Exception as error:
            state["server_errors"].append(type(error).__name__)
        finally:
            listener.close()

    thread = threading.Thread(target=serve, name="owned-nexus-download-tls-fixture", daemon=True)
    thread.start()

    def close():
        stopped.set()
        listener.close()

    return port, thread, state, close


def client_verifier_module():
    path = NATIVE / "tests/nexus_client_verify.py"
    spec = importlib.util.spec_from_file_location("modbridge_nexus_client_verify", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load the shared verifier helpers")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def checked_download_object(module, app_profile):
    manifest_path = app_profile / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    name = "nexus_download.o"
    source_name = "outputs/ps5-mod-bridge/native-host/nexus_download.cpp"
    expected_source = manifest.get("source_sha256", {}).get(source_name)
    expected_object = manifest.get("object_sha256", {}).get(name)
    source = ROOT / source_name
    obj = app_profile / name
    closure = manifest.get("include_closure", {}).get(name + ".d")
    if (not expected_source or not expected_object or not source.is_file() or not obj.is_file()
            or module.sha256_file(source) != expected_source
            or module.sha256_file(obj) != expected_object or not closure):
        raise RuntimeError("retained resolver object failed manifest/source verification")
    header_path = (NATIVE / "nexus_download.h").resolve()
    includes_header = False
    for entry in closure:
        included = Path(entry["path"])
        if not included.is_file() or module.sha256_file(included) != entry["sha256"]:
            raise RuntimeError("retained resolver include closure changed")
        if included.resolve() == header_path:
            includes_header = True
    if not includes_header:
        raise RuntimeError("retained resolver object manifest omitted its public header")
    return obj, {"path": str(obj), "sha256": expected_object,
                 "source": source_name, "source_sha256": expected_source,
                 "verified_include_entries": len(closure), "includes_public_header": True}


def compile_driver(directory, app_profile):
    sys.dont_write_bytecode = True
    module = client_verifier_module()
    module.APP_PROFILE = app_profile
    shared_objects, shared_records = module.reusable_cpp_objects()
    resolver_object, resolver_record = checked_download_object(module, app_profile)
    common = [
        "/usr/bin/clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Os",
        "-D_POSIX_C_SOURCE=200809L", "-include", str(module.HOST3 / "config.h"),
        "-I", str(module.NATIVE), "-I", str(module.NATIVE.parent / "include"),
        "-isystem", str(module.NATIVE.parent / "third_party"),
        "-isystem", str(module.NATIVE.parent / "third_party/picosha2"),
        "-I", str(module.NATIVE / "download"), "-I", str(module.NATIVE / "archive/private"),
        "-I" + str(module.SPIKE / "native-curl-profile-experiment-1"),
        "-I" + str(module.MBED_INCLUDE), "-I" + str(module.MBED_LIBRARY),
        "-I" + str(module.CURL_INCLUDE), '-DMBEDTLS_CONFIG_FILE="tls_config.h"',
        "-include", str(module.MBED_LIBRARY / "common.h"),
        "-include", str(module.SPIKE / "native-curl-profile-experiment-1/tls_effective.h"),
    ]
    objdir = directory / "objects"
    objdir.mkdir(mode=0o700)
    sources = [
        ("owner", module.NATIVE / "download/owner.c", ()),
        ("curl-download", module.NATIVE / "download/curl_download.c",
         ("-Dcurl_multi_add_handle=fixture_curl_multi_add_handle",)),
        ("file-sink", module.NATIVE / "archive/private/file_sink.c", ()),
        ("resolver-driver", NATIVE / "tests/nexus_download_driver.c", ()),
    ]
    objects, commands = [], []
    for name, source, extra in sources:
        target = objdir / f"{name}.o"
        command = [*common, *extra, "-c", str(source), "-o", str(target)]
        run_quiet(command)
        commands.append([Path(command[0]).name, *command[1:]])
        objects.append(target)
    binary = directory / "nexus-download-driver"
    command = [
        "/usr/bin/clang++", *(str(item) for item in objects), str(resolver_object),
        *(str(item) for item in shared_objects), str(module.LIBCURL),
        str(module.PROFILE / "libmbedtls.a"), str(module.PROFILE / "libmbedx509.a"),
        str(module.PROFILE / "libmbedcrypto.a"), "-framework", "SystemConfiguration",
        "-framework", "CoreFoundation", "-o", str(binary),
    ]
    run_quiet(command)
    commands.append({"tool": Path(command[0]).name, "source_objects": len(objects),
                     "reused_app_objects": len(shared_objects) + 1, "output": str(binary)})
    libraries = (module.LIBCURL, module.PROFILE / "libmbedtls.a",
                 module.PROFILE / "libmbedx509.a", module.PROFILE / "libmbedcrypto.a")
    return binary, commands, {**shared_records, "nexus_download.o": resolver_record}, {
        str(path): module.sha256_file(path) for path in libraries
    }


FAILURE_STATUS = {
    "nonpremium-no-grant": 2,
    "wrong-account-grant": 2,
    "wrong-source-grant": 2,
    "expired-grant": 2,
    "hidden-mod": 3,
    "unknown-adult": 3,
    "malformed-final-mirror": 5,
    "bad-http": 4,
    "total-deadline": 6,
}


def failure_record_closed(record, expected_status):
    return (
        isinstance(record, dict)
        and record.get("return") == expected_status
        and record.get("status") == expected_status
        and record.get("complete") == 0
        and record.get("uri_bytes") == 0
        and record.get("scratch_wiped") is True
        and record.get("uri_wiped") is True
    )


def synthetic_failure_status_control():
    valid_receipt = {"return": 2, "status": 2, "complete": 0, "uri_bytes": 0,
                     "scratch_wiped": True, "uri_wiped": True}
    mismatched_receipt = dict(valid_receipt, **{"return": 0})
    return {
        "valid_return_2_status_2_passes": failure_record_closed(valid_receipt, 2),
        "return_0_status_2_fails": not failure_record_closed(mismatched_receipt, 2),
    }


def record_checks(records, fixture):
    by_case = {record.get("case"): record for record in records}
    successes = ("premium", "website-grant", "healthy-after-failures")
    success_ok = all(
        by_case.get(name, {}).get("status") == 0
        and by_case[name].get("return") == 0
        and by_case[name].get("complete") == 1
        and by_case[name].get("uri_matches") is True
        and by_case[name].get("scratch_wiped") is True
        and by_case[name].get("uri_wiped") is False
        for name in successes if name in by_case
    ) and all(name in by_case for name in successes)
    premium = by_case.get("premium", {})
    grant = by_case.get("website-grant", {})
    policy = (
        premium.get("is_premium") == 1 and premium.get("grant_used") == 0
        and grant.get("is_premium") == 0 and grant.get("grant_used") == 1
        and premium.get("user_id") == 1101 and grant.get("user_id") == 2202
        and premium.get("mod_id") == grant.get("mod_id") == 17
        and premium.get("file_id") == grant.get("file_id") == 3
        and by_case.get("nonpremium-no-grant", {}).get("status") == 2
        and by_case.get("wrong-account-grant", {}).get("status") == 2
        and by_case.get("wrong-source-grant", {}).get("status") == 2
        and by_case.get("expired-grant", {}).get("status") == 2
        and by_case.get("hidden-mod", {}).get("status") == 3
        and by_case.get("unknown-adult", {}).get("status") == 3
        and by_case.get("unknown-adult", {}).get("adult_content") == -1
        and by_case.get("malformed-final-mirror", {}).get("status") == 5
        and by_case.get("bad-http", {}).get("status") == 4
        and by_case.get("total-deadline", {}).get("status") == 6
    )
    fail_closed = all(
        name in by_case and failure_record_closed(by_case[name], expected_status)
        for name, expected_status in FAILURE_STATUS.items()
    )
    route_counts = {}
    for check in fixture.get("checks", []):
        case, route = check.get("case"), check.get("route")
        route_counts.setdefault(case, {})[route] = route_counts.setdefault(case, {}).get(route, 0) + 1
    all_route_checks = all(
        check.get("tls") and check.get("host_ok") and check.get("auth_ok")
        and check.get("sni_ok") and check.get("app_headers_ok") and check.get("no_cookie_or_proxy_auth")
        and check.get("path_ok") and check.get("query_ok")
        for check in fixture.get("checks", [])
    ) and bool(fixture.get("checks"))
    # Source mismatch and expired grants are rejected before any API request.
    refusal_routes = all(
        route_counts.get(name, {}).get("link", 0) == 0
        for name in ("nonpremium-no-grant", "wrong-account-grant", "wrong-source-grant",
                     "expired-grant", "hidden-mod", "unknown-adult")
    )
    success_routes = all(
        route_counts.get(name, {}).get("profile", 0) == 1
        and route_counts.get(name, {}).get("mod", 0) == 1
        and route_counts.get(name, {}).get("link", 0) == 1
        for name in ("premium", "website-grant", "malformed-final-mirror",
                     "bad-http", "total-deadline", "healthy-after-failures")
    )
    success_routes = success_routes and all(
        route_counts.get(name, {}).get(route, 0) <= 1
        for name in DOMAINS for route in ("profile", "mod", "link")
    )
    gated_routes = (
        route_counts.get("nonpremium-no-grant", {}).get("profile", 0) == 1
        and route_counts.get("nonpremium-no-grant", {}).get("mod", 0) == 0
        and route_counts.get("wrong-account-grant", {}).get("profile", 0) == 1
        and route_counts.get("wrong-account-grant", {}).get("mod", 0) == 0
        and route_counts.get("hidden-mod", {}).get("profile", 0) == 1
        and route_counts.get("hidden-mod", {}).get("mod", 0) == 1
        and route_counts.get("unknown-adult", {}).get("profile", 0) == 1
        and route_counts.get("unknown-adult", {}).get("mod", 0) == 1
        and route_counts.get("wrong-source-grant", {}).get("profile", 0) == 0
        and route_counts.get("wrong-source-grant", {}).get("mod", 0) == 0
        and route_counts.get("expired-grant", {}).get("profile", 0) == 0
        and route_counts.get("expired-grant", {}).get("mod", 0) == 0
    )
    archive = by_case.get("archive-transfer", {})
    archive_route = route_counts.get("archive-transfer", {}).get("archive", 0) == 1
    archive_ok = (
        archive.get("return") == 0 and archive.get("status") == 0
        and archive.get("complete") == 1 and archive.get("http") == 200
        and archive.get("file_complete") == 1 and archive.get("readback_verified") == 1
        and archive.get("bytes_match") is True and archive.get("received") == len(ARCHIVE_BODY)
        and archive.get("file_size") == len(ARCHIVE_BODY) and archive_route
    )
    runtime = by_case.get("runtime-close", {})
    shim_controls = by_case.get("socket-shim-controls", {})
    runtime_ok = (
        runtime.get("close_return") == 0 and runtime.get("initialized") == 0
        and runtime.get("terminal") == 0 and runtime.get("window_active") == 0
        and runtime.get("socket_installs") == fixture.get("requests")
        and runtime.get("socket_opens") == fixture.get("requests")
        and runtime.get("socket_connected") == fixture.get("requests")
        and runtime.get("socket_refused") == 0
        and runtime.get("windows", 0) > 0
    )
    return {
        "route_tls_headers_and_query": bool(all_route_checks),
        "premium_and_website_grant": bool(success_ok and policy),
        "published_identity_matches_request": bool(
            premium.get("user_id") == 1101 and grant.get("user_id") == 2202
            and premium.get("mod_id") == grant.get("mod_id") == 17
            and premium.get("file_id") == grant.get("file_id") == 3
        ),
        "failure_policy_and_uri_wipe": bool(policy and fail_closed and refusal_routes),
        "whole_response_before_uri_publish": by_case.get("malformed-final-mirror", {}).get("uri_wiped") is True,
        "total_deadline": by_case.get("total-deadline", {}).get("status") == 6,
        "healthy_owner_reuse": bool(success_routes and runtime_ok),
        "refusal_before_link_fetch": bool(refusal_routes and gated_routes),
        "socket_shim_destination_guard": all(shim_controls.get(name) is True for name in (
            "accepts_loopback_443", "rejects_other_port", "rejects_non_loopback", "rejects_wrong_purpose")),
        "credential_free_archive_sink": bool(archive_ok),
        "fixture_request_sequence": bool(success_routes and len(fixture.get("checks", [])) == fixture.get("requests")),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app-profile", required=True, type=Path,
                        help="fresh actual desktop_fixture-download build profile")
    args = parser.parse_args()
    app_profile = args.app_profile.resolve()
    if not app_profile.is_dir():
        parser.error("--app-profile must name an existing build profile")

    evidence_root = SPIKE / "nexus-download-https-fixture-1"
    evidence_root.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(evidence_root, 0o700)
    directory = Path(tempfile.mkdtemp(prefix="owned-", dir=evidence_root))
    os.chmod(directory, 0o700)
    ca_pem, leaf_pem, leaf_key = generate_certificates(directory)
    binary, commands, reused_objects, pinned_libraries = compile_driver(directory, app_profile)
    port, thread, fixture, close_fixture = start_fixture(leaf_pem, leaf_key)
    try:
        run = subprocess.run([str(binary), str(port), str(ca_pem), str(directory)],
                             capture_output=True, text=True, timeout=60)
    finally:
        close_fixture()
    thread.join(timeout=5)
    if thread.is_alive():
        raise RuntimeError("owned HTTPS fixture did not stop within its bound")
    try:
        records = [json.loads(line) for line in run.stdout.splitlines() if line.strip()]
    except ValueError as error:
        raise RuntimeError("native fixture driver emitted a non-JSON scalar receipt") from error
    checks = record_checks(records, fixture)
    synthetic_control = synthetic_failure_status_control()
    ok = (run.returncode == 0 and all(checks.values()) and not fixture.get("server_errors")
          and all(synthetic_control.values()))
    receipt = {
        "test": "owned_native_nexus_download_resolution",
        "network_scope": "owned_loopback_tls_only",
        "live_account_or_nexus_used": False,
        "fixture_credentials": "fixed fake values only; not recorded",
        "fixture_query_values": "fixed fake NXM/link secrets; only boolean checks recorded",
        "fixture": fixture,
        "driver_records": records,
        "driver_exit": run.returncode,
        "checks": checks,
        "synthetic_failure_return_control": synthetic_control,
        "compiler_commands": commands,
        "reused_app_objects": reused_objects,
        "pinned_library_sha256": pinned_libraries,
        "resolver_app_profile": str(app_profile),
        "socket_routing": "test-only curl_multi_add_handle shim accepts only resolved 127.0.0.1:443 and connects to this owned ephemeral TLS listener; original fixed Nexus/archive hostnames and TLS peer/name checks remain in force",
        "archive_origin": ARCHIVE_HOST,
        "assertions_passed": bool(ok),
    }
    receipt_path = directory / "receipt.json"
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    os.chmod(receipt_path, 0o600)
    print(json.dumps({"passed": bool(ok), "requests": fixture.get("requests", 0),
                      "checks": checks, "synthetic_failure_return_control": synthetic_control,
                      "receipt": str(receipt_path)}, sort_keys=True))
    if not ok:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
