#!/usr/bin/env python3
"""Actual app-socket auth regression; fake Nexus TLS is owned by this test."""
import argparse
import base64
import hashlib
import http.client
import importlib.util
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import ssl
import tempfile
import threading
import time
from urllib.parse import parse_qsl, urlencode, urlsplit


ROOT = Path(__file__).resolve().parents[4]
SPIKE = ROOT / ".superpowers/native-dependency-spike"
RED_PROFILE = SPIKE / "native-nexus-provider-requests-1/app-host-2/desktop_fixture-download"
CONFIGURED_PROFILE = SPIKE / "native-nexus-sign-in-1/app-host-configured-1/desktop_fixture-download"
EVIDENCE = SPIKE / "native-nexus-sign-in-1"
FIXTURE_CONFIG_DIR = EVIDENCE / "fixture-config-1"
MATRIX_DIR = EVIDENCE / "actual-http-tls-1"
NATIVE = ROOT / "outputs/ps5-mod-bridge/native-host"
SHIM_SOURCE = NATIVE / "tests/nexus_auth_socket_shim.c"


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
        return digest.hexdigest()


def clean_build_env():
    return {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin"}


def run_build(argv):
    result = subprocess.run(argv, env=clean_build_env(), capture_output=True, text=True, timeout=180)
    if result.returncode:
        raise RuntimeError(f"fixture relink failed with status {result.returncode}")
    return result


def verify_profile(profile):
    profile = Path(profile).resolve()
    manifest_path = profile / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    artifact = profile / manifest["artifact"]
    if not artifact.is_file() or sha256(artifact) != manifest.get("artifact_sha256"):
        raise RuntimeError("configured app artifact does not match its manifest")
    for relative, expected in manifest.get("source_sha256", {}).items():
        path = ROOT / relative
        if not path.is_file() or sha256(path) != expected:
            raise RuntimeError(f"configured app source identity changed: {Path(relative).name}")
    object_hashes = manifest.get("object_sha256", {})
    include_closure = manifest.get("include_closure", {})
    cpp_records = {}
    for object_name, expected in object_hashes.items():
        object_path = profile / object_name
        if not object_path.is_file() or sha256(object_path) != expected:
            raise RuntimeError(f"configured app object identity changed: {object_name}")
        dependency_name = object_name + ".d"
        entries = include_closure.get(dependency_name)
        if not isinstance(entries, list) or not entries:
            raise RuntimeError(f"configured app object has no recorded include closure: {object_name}")
        for entry in entries:
            included = Path(entry["path"])
            if not included.is_file() or sha256(included) != entry["sha256"]:
                raise RuntimeError(f"configured app include bytes changed: {object_name}")
        if object_name in {"oauth_code.o", "auth.o", "nexus_client.o", "nexus_download.o",
                           "nexus_bridge.o", "nexus.o", "core_bridge.o"}:
            source_path = next((name for name in manifest["source_sha256"]
                                if Path(name).suffix == ".cpp"
                                and Path(name).stem + ".o" == object_name), None)
            if source_path is None:
                raise RuntimeError(f"configured app object has no matching C++ source: {object_name}")
            cpp_records[object_name] = {
                "source_path": source_path,
                "source_sha256": manifest["source_sha256"][source_path],
                "object_sha256": expected,
                "include_entries": len(entries),
            }
    commands = manifest.get("commands", [])
    compile_curl = next((entry["argv"] for entry in commands
                         if isinstance(entry, dict) and isinstance(entry.get("argv"), list)
                         and "-c" in entry["argv"]
                         and Path(entry["argv"][entry["argv"].index("-c") + 1]).resolve()
                         == (NATIVE / "download/curl_download.c").resolve()), None)
    link_argv = next((entry["argv"] for entry in reversed(commands)
                      if isinstance(entry, dict) and isinstance(entry.get("argv"), list)
                      and "-o" in entry["argv"]
                      and Path(entry["argv"][entry["argv"].index("-o") + 1]).resolve()
                      == artifact.resolve()), None)
    if not compile_curl or not link_argv:
        raise RuntimeError("configured app manifest lacks its C transport or final link recipe")
    if manifest.get("source_sha256", {}).get("outputs/ps5-mod-bridge/native-host/tests/nexus_auth_socket_shim.c"):
        raise RuntimeError("test shim unexpectedly entered the production app source manifest")
    return profile, manifest, compile_curl, link_argv, cpp_records


def relink_with_socket_shim(profile):
    profile, manifest, original_compile, original_link, cpp_records = verify_profile(profile)
    MATRIX_DIR.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(MATRIX_DIR, 0o700)
    private = Path(tempfile.mkdtemp(prefix="app-with-test-shim-", dir=MATRIX_DIR))
    os.chmod(private, 0o700)
    curl_object = private / "curl_download_fixture.o"
    curl_dep = private / "curl_download_fixture.o.d"
    shim_object = private / "nexus_auth_socket_shim.o"
    shim_dep = private / "nexus_auth_socket_shim.o.d"
    binary = private / "modbridge-native-auth-fixture"

    compile_curl = list(original_compile)
    output_index = compile_curl.index("-o") + 1
    dep_index = compile_curl.index("-MF") + 1
    compile_curl[output_index] = str(curl_object)
    compile_curl[dep_index] = str(curl_dep)
    compile_curl.insert(compile_curl.index("-c"),
                        "-Dcurl_multi_add_handle=mb_nexus_auth_fixture_curl_multi_add_handle")
    run_build(compile_curl)

    compile_prefix = list(original_compile[:original_compile.index("-MD")])
    shim_compile = [*compile_prefix, "-MD", "-MF", str(shim_dep), "-c",
                    str(SHIM_SOURCE), "-o", str(shim_object)]
    run_build(shim_compile)

    link = list(original_link)
    original_curl = str(profile / "curl_download.o")
    if original_curl not in link:
        raise RuntimeError("configured app link does not contain the verified curl object")
    link[link.index(original_curl)] = str(curl_object)
    first_archive = next((index for index, value in enumerate(link)
                          if value.endswith(".a") and Path(value).is_file()), None)
    if first_archive is None:
        raise RuntimeError("configured app link has no retained dependency archive")
    link.insert(first_archive, str(shim_object))
    output_index = link.index("-o") + 1
    link[output_index] = str(binary)
    for index, value in enumerate(link):
        if value.startswith("-Wl,-map,"):
            link[index] = "-Wl,-map," + str(private / "fixture-link.map")
    run_build(link)
    if not binary.is_file():
        raise RuntimeError("fixture app link did not produce an executable")
    return binary, {
        "profile": str(profile),
        "profile_artifact_sha256": manifest["artifact_sha256"],
        "cpp_objects": cpp_records,
        "curl_source_sha256": manifest["source_sha256"].get(
            "outputs/ps5-mod-bridge/native-host/download/curl_download.c"),
        "shim_source_sha256": sha256(SHIM_SOURCE),
        "fixture_curl_object_sha256": sha256(curl_object),
        "fixture_shim_object_sha256": sha256(shim_object),
        "fixture_binary_sha256": sha256(binary),
        "private_directory": str(private),
        "test_only_curl_callsite_redirect": True,
    }


def start_owned_tls_fixture(cert, key, routes, shared):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen(8)
    listener.settimeout(.2)
    port = listener.getsockname()[1]
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(str(cert), str(key))
    stop = threading.Event()
    state = {"records": [], "errors": []}
    state_changed = threading.Condition()
    state["condition"] = state_changed

    def receive_exact(stream, target):
        result = bytearray()
        while len(result) < target:
            block = stream.recv(target - len(result))
            if not block:
                return None
            result.extend(block)
        return bytes(result)

    def serve_one(peer, index):
        record = {"index": index, "unexpected": index >= len(routes), "tls": False,
                  "method_path_ok": False, "host_ok": False, "credential_ok": False,
                  "headers_ok": False, "form_ok": False, "pkce_ok": False,
                  "response_sent": False, "status": 0}
        peer.settimeout(5)
        try:
            with context.wrap_socket(peer, server_side=True) as stream:
                record["tls"] = True
                received = bytearray()
                while b"\r\n\r\n" not in received and len(received) <= 16384:
                    block = stream.recv(4096)
                    if not block:
                        break
                    received.extend(block)
                if b"\r\n\r\n" not in received or len(received) > 16384:
                    return record
                header_bytes, body = bytes(received).split(b"\r\n\r\n", 1)
                lines = header_bytes.decode("latin-1").split("\r\n")
                parts = lines[0].split(" ")
                if len(parts) == 3:
                    method, path, version = parts
                else:
                    method, path, version = "", "", ""
                headers = {}
                for line in lines[1:]:
                    if ":" not in line:
                        continue
                    name, value = line.split(":", 1)
                    headers.setdefault(name.strip().lower(), []).append(value.strip())
                content_length = 0
                lengths = headers.get("content-length", [])
                if len(lengths) == 1:
                    try:
                        content_length = int(lengths[0])
                    except ValueError:
                        content_length = -1
                if content_length < 0 or content_length > 131072:
                    return record
                if len(body) < content_length:
                    extra = receive_exact(stream, content_length - len(body))
                    if extra is None:
                        return record
                    body += extra
                body = body[:content_length]

                route = routes[index] if index < len(routes) else None
                if route is None:
                    record["method_path_ok"] = False
                    record["status"] = 404
                    response_body = b"{}"
                else:
                    record["case"] = route["case"]
                    record["method_path_ok"] = (
                        method == route["method"] and path == route["path"] and version == "HTTP/1.1")
                    record["host_ok"] = headers.get("host") == ["users.nexusmods.com"]
                    absent_auth = all(name not in headers for name in (
                        "apikey", "cookie", "proxy-authorization"))
                    if route["kind"] == "token":
                        record["credential_ok"] = absent_auth and "authorization" not in headers
                        record["headers_ok"] = (
                            headers.get("content-type", [""])[0].split(";", 1)[0].lower()
                            == "application/x-www-form-urlencoded"
                            and headers.get("accept", [""])[0].lower() == "application/json")
                        try:
                            pairs = parse_qsl(body.decode("ascii"), keep_blank_values=True,
                                              strict_parsing=True)
                            form = dict(pairs)
                            unique = len(form) == len(pairs)
                            record["form_ok"] = (
                                unique and set(form) == {"grant_type", "redirect_uri", "client_id",
                                                        "code", "code_verifier"}
                                and form["grant_type"] == "authorization_code"
                                and form["redirect_uri"] == shared["redirect_uri"]
                                and form["client_id"] == shared["client_id"]
                                and form["code"] == shared.get("expected_code"))
                            verifier = form.get("code_verifier", "")
                            challenge = base64.urlsafe_b64encode(
                                hashlib.sha256(verifier.encode("ascii")).digest()).rstrip(b"=").decode("ascii")
                            record["pkce_ok"] = (
                                len(verifier) >= 43 and challenge == shared.get("expected_challenge"))
                        except (UnicodeError, ValueError, KeyError):
                            record["form_ok"] = False
                            record["pkce_ok"] = False
                    elif route["kind"] == "userinfo":
                        record["credential_ok"] = (
                            absent_auth and headers.get("authorization") == ["Bearer " + route["bearer"]])
                        record["headers_ok"] = (
                            headers.get("accept", [""])[0].lower() == "application/json")
                        record["form_ok"] = body == b""
                        record["pkce_ok"] = True
                    record["status"] = route["status"]
                    if route.get("delay_seconds"):
                        time.sleep(route["delay_seconds"])
                    response_body = route["body"]

                reason = {200: "OK", 401: "Unauthorized", 404: "Not Found"}.get(
                    record["status"], "Fixture Response")
                response = (
                    f"HTTP/1.1 {record['status']} {reason}\r\n"
                    "Content-Type: application/json\r\n"
                    f"Content-Length: {len(response_body)}\r\n"
                    "Connection: close\r\n\r\n"
                ).encode("ascii") + response_body
                stream.sendall(response)
                record["response_sent"] = True
        except (OSError, ssl.SSLError, ValueError, UnicodeError) as error:
            record["transport_error"] = type(error).__name__
        return record

    def serve():
        index = 0
        while not stop.is_set():
            try:
                peer, _ = listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            record = serve_one(peer, index)
            with state_changed:
                state["records"].append(record)
                state_changed.notify_all()
            index += 1
        try:
            listener.close()
        except OSError:
            pass

    thread = threading.Thread(target=serve, name="owned-nexus-auth-tls", daemon=True)
    thread.start()

    def close():
        stop.set()
        try:
            listener.close()
        except OSError:
            pass
        thread.join(timeout=5)

    return port, state, close


def wait_for_record_count(state, expected, timeout=1.0):
    condition = state.get("condition")
    if condition is None:
        raise RuntimeError("owned TLS fixture has no record synchronization")
    deadline = time.monotonic() + timeout
    with condition:
        while len(state["records"]) < expected:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            condition.wait(remaining)
        return len(state["records"])


def local_request(port, method, path, headers=None, body=b"", *, split_body=False, timeout=8):
    if not isinstance(body, bytes):
        raise TypeError("HTTP fixture body must be bytes")
    values = [("Host", f"127.0.0.1:{port}"), ("Connection", "close")]
    if isinstance(headers, dict):
        names = {name.lower() for name in headers}
        values = [(name, value) for name, value in values if name.lower() not in names]
        values.extend(headers.items())
    elif headers:
        values.extend(headers)
    if method in ("POST", "PUT") and not any(name.lower() == "content-length" for name, _ in values):
        values.append(("Content-Length", str(len(body))))
    lines = [f"{method} {path} HTTP/1.1"]
    lines.extend(f"{name}: {value}" for name, value in values)
    wire_head = ("\r\n".join(lines) + "\r\n\r\n").encode("ascii")
    sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    sock.settimeout(timeout)
    try:
        sock.sendall(wire_head)
        if body:
            if split_body:
                time.sleep(.02)
            sock.sendall(body)
        received = bytearray()
        while len(received) <= 16384:
            try:
                block = sock.recv(4096)
            except socket.timeout:
                raise RuntimeError("local app response exceeded its bounded test deadline") from None
            if not block:
                break
            received.extend(block)
        if len(received) > 16384 or b"\r\n\r\n" not in received:
            raise RuntimeError("local app response framing was invalid")
        head, response_body = bytes(received).split(b"\r\n\r\n", 1)
        lines = head.decode("latin-1").split("\r\n")
        status_parts = lines[0].split(" ", 2)
        if len(status_parts) < 2 or not status_parts[1].isdigit():
            raise RuntimeError("local app response status was invalid")
        response_headers = {}
        for line in lines[1:]:
            if ":" not in line:
                continue
            name, value = line.split(":", 1)
            response_headers.setdefault(name.strip().lower(), []).append(value.strip())
        return int(status_parts[1]), response_headers, response_body
    finally:
        sock.close()


def fixture_json(value):
    return json.dumps(value, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def token_route(label, access, refresh, *, malformed=False):
    body = (b'{"access_token":' if malformed else fixture_json({
        "access_token": access, "refresh_token": refresh,
        "token_type": "Bearer", "expires_in": 3600,
    }))
    return {"case": label, "kind": "token", "method": "POST", "path": "/oauth/token",
            "status": 200, "body": body}


def user_route(label, bearer, *, status=200, delay=0):
    body = fixture_json({
        "sub": "12345", "name": "Fixture User", "membership_roles": ["premium"],
        "preferences": {"adult": False},
    }) if status == 200 else fixture_json({"error": "fixture_denied"})
    return {"case": label, "kind": "userinfo", "method": "GET",
            "path": "/oauth/userinfo", "status": status,
            "body": body, "bearer": bearer, "delay_seconds": delay}


def decode_json(payload):
    try:
        value = json.loads(payload)
    except (UnicodeError, ValueError):
        return None
    return value if isinstance(value, dict) else None


def get_cookie_and_csrf(headers, payload):
    values = headers.get("set-cookie", [])
    csrf = payload.get("csrf_token") if isinstance(payload, dict) else None
    if len(values) != 1 or not isinstance(csrf, str):
        return None, csrf, False
    pieces = [piece.strip() for piece in values[0].split(";")]
    token = pieces[0].partition("=")
    attrs = {piece.lower() for piece in pieces[1:]}
    good = (token[0] == "mb_session" and bool(re.fullmatch(r"[A-Za-z0-9_-]{43}", token[2]))
            and attrs == {"path=/", "httponly", "samesite=lax"}
            and isinstance(csrf, str)
            and bool(re.fullmatch(r"[A-Za-z0-9_-]{43}", csrf)))
    return (token[2] if token[0] == "mb_session" else None), csrf, good


def parse_authorization_url(payload, config):
    value = decode_json(payload)
    if not value or not isinstance(value.get("authorization_url"), str):
        return None
    try:
        parsed = urlsplit(value["authorization_url"])
        pairs = parse_qsl(parsed.query, keep_blank_values=True, strict_parsing=True)
        query = dict(pairs)
        if len(query) != len(pairs):
            return None
        if (parsed.scheme != "https" or parsed.hostname != "users.nexusmods.com"
                or parsed.path != "/oauth/authorize" or parsed.username is not None
                or parsed.password is not None or parsed.fragment):
            return None
        if set(query) != {"client_id", "response_type", "scope", "redirect_uri", "state",
                          "code_challenge_method", "code_challenge"}:
            return None
        if (query["client_id"] != config["client_id"] or query["response_type"] != "code"
                or query["scope"] != config["scope"]
                or query["redirect_uri"] != config["redirect_uri"]
                or query["code_challenge_method"] != "S256"
                or not re.fullmatch(r"[A-Za-z0-9_-]{43}", query["state"])
                or not re.fullmatch(r"[A-Za-z0-9_-]{43}", query["code_challenge"])):
            return None
        return {"state": query["state"], "challenge": query["code_challenge"]}
    except (ValueError, TypeError):
        return None


def callback_target(state, *, code="fixture-code-initial", duplicate_code=False):
    query = urlencode({"code": code, "state": state})
    if duplicate_code:
        query = "code=" + code + "&" + query
    return "/auth/callback?" + query


def add_check(checks, name, passed, status=None):
    checks[name] = {"passed": bool(passed)}
    if status is not None:
        checks[name]["http_status"] = int(status)
    return bool(passed)


def callback_request(port, cookie, state, *, wrong_cookie=False, duplicate_code=False,
                     code="fixture-code-initial"):
    value = "C" * 43 if wrong_cookie else cookie
    return local_request(port, "GET", callback_target(state, code=code, duplicate_code=duplicate_code),
                         {"Cookie": "mb_session=" + value})


def start_signin(port, cookie, csrf, config, shared, checks, label):
    status, headers, body = local_request(
        port, "POST", "/api/auth/start",
        {"Origin": config["redirect_uri"].rsplit("/auth/callback", 1)[0],
         "Cookie": "mb_session=" + cookie, "X-CSRF-Token": csrf,
         "Content-Type": "application/json"},
        b"{}", split_body=True,
    )
    parsed = parse_authorization_url(body, config)
    if parsed:
        shared["expected_challenge"] = parsed["challenge"]
    add_check(checks, label, status == 200 and parsed is not None, status)
    return status, headers, body, parsed


def load_fixture_config(path):
    path = Path(path).resolve()
    config = json.loads(path.read_text(encoding="utf-8"))
    expected = {"client_id", "scope", "redirect_uri", "ca_pem_path", "users_resolve",
                "timeout_ms", "connect_timeout_ms"}
    if set(config) != expected:
        raise RuntimeError("fake OAuth configuration has an unexpected field set")
    if config["client_id"] != "fixture_public_client" or config["scope"] != "fixture_scope":
        raise RuntimeError("fixture configuration is not the expected fake public client")
    if config["users_resolve"] != "users.nexusmods.com:443:127.0.0.1":
        raise RuntimeError("fixture configuration contains a non-loopback provider mapping")
    if config["timeout_ms"] != 2500 or config["connect_timeout_ms"] != 1000:
        raise RuntimeError("fixture configuration is outside the planned bounded provider budget")
    parsed = urlsplit(config["redirect_uri"])
    if (parsed.scheme != "http" or parsed.hostname != "127.0.0.1"
            or parsed.path != "/auth/callback" or parsed.query or parsed.fragment):
        raise RuntimeError("fixture callback is not the exact loopback callback")
    ca = Path(config["ca_pem_path"])
    if not ca.is_file() or not ca.resolve().is_relative_to(FIXTURE_CONFIG_DIR.resolve()):
        raise RuntimeError("fixture CA is outside the owned test directory")
    return config, parsed.port, ca


def verify_full(profile, config_path):
    config, app_port, ca_pem = load_fixture_config(config_path)
    app_binary, link_identity = relink_with_socket_shim(profile)
    leaf_pem = FIXTURE_CONFIG_DIR / "fixture-leaf.pem"
    leaf_key = FIXTURE_CONFIG_DIR / "fixture-leaf.key"
    if not leaf_pem.is_file() or not leaf_key.is_file():
        raise RuntimeError("owned TLS leaf certificate material is missing")

    old_access = "fixture-old-access-token"
    old_refresh = "fixture-old-refresh-token"
    candidate_access = "fixture-candidate-access-token"
    candidate_refresh = "fixture-candidate-refresh-token"
    routes = [
        token_route("initial-token", old_access, old_refresh),
        user_route("initial-userinfo", old_access),
        user_route("account-live-check", old_access),
        token_route("malformed-token", "unused", "unused", malformed=True),
        token_route("replacement-token", candidate_access, candidate_refresh),
        user_route("replacement-userinfo-refused", candidate_access, status=401),
        user_route("preserved-account-check", old_access),
        user_route("provider-timeout", old_access, delay=3.2),
        user_route("provider-after-timeout", old_access),
    ]
    shared = {"client_id": config["client_id"], "redirect_uri": config["redirect_uri"]}
    fixture_port, fixture_state, close_fixture = start_owned_tls_fixture(
        leaf_pem, leaf_key, routes, shared)
    process = None
    checks = {}
    app_observations = {}
    shim = None
    started_at = time.monotonic()
    child_env = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin",
                 "MB_NEXUS_AUTH_FIXTURE_PORT": str(fixture_port)}
    try:
        process = subprocess.Popen(
            [str(app_binary), "--port", str(app_port), "--lifetime", "120"],
            env=child_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        deadline = time.monotonic() + 5
        initial = None
        while time.monotonic() < deadline:
            try:
                initial = local_request(app_port, "GET", "/api/capabilities", timeout=.3)
                break
            except OSError:
                if process.poll() is not None:
                    raise RuntimeError("configured app exited before accepting HTTP")
                time.sleep(.04)
        if initial is None:
            raise RuntimeError("configured app did not accept HTTP within its startup bound")
        cap_status, cap_headers, cap_body = initial
        caps = decode_json(cap_body)
        if not caps:
            raise RuntimeError("capability endpoint did not return JSON")
        cookie, csrf, cookie_ok = get_cookie_and_csrf(cap_headers, caps)
        actual_caps = caps.get("capabilities", {})
        add_check(checks, "capability_auth_and_account_enabled",
                  cap_status == 200 and actual_caps.get("nexus_auth_start") is True
                  and actual_caps.get("account_status") is True, cap_status)
        add_check(checks, "lax_session_cookie_and_csrf_bootstrap",
                  cookie_ok and isinstance(csrf, str), cap_status)
        add_check(checks, "unrelated_install_capabilities_remain_disabled",
                  all(actual_caps.get(key) is False for key in
                      ("jobs", "download_analyze", "prepared_translation", "deploy")), cap_status)
        if not cookie_ok or not cookie or not csrf:
            raise RuntimeError("configured app did not bootstrap the expected private session")
        app_observations["capabilities_http"] = cap_status

        origin = f"http://127.0.0.1:{app_port}"
        wrong_cookie_status, _, _ = local_request(
            app_port, "POST", "/api/auth/start",
            {"Origin": origin, "Cookie": "mb_session=" + "C" * 43,
             "X-CSRF-Token": csrf, "Content-Type": "application/json"}, b"{}")
        add_check(checks, "wrong_session_cookie_rejected_before_provider",
                  wrong_cookie_status == 401 and len(fixture_state["records"]) == 0,
                  wrong_cookie_status)

        wrong_csrf_status, _, _ = local_request(
            app_port, "POST", "/api/auth/start",
            {"Origin": origin, "Cookie": "mb_session=" + cookie,
             "X-CSRF-Token": "D" * 43, "Content-Type": "application/json"}, b"{}")
        add_check(checks, "wrong_csrf_rejected_before_provider",
                  wrong_csrf_status == 403 and len(fixture_state["records"]) == 0,
                  wrong_csrf_status)

        wrong_origin_status, _, _ = local_request(
            app_port, "POST", "/api/auth/start",
            {"Origin": "http://example.invalid", "Cookie": "mb_session=" + cookie,
             "X-CSRF-Token": csrf, "Content-Type": "application/json"}, b"{}")
        add_check(checks, "wrong_origin_rejected_before_provider",
                  wrong_origin_status == 403 and len(fixture_state["records"]) == 0,
                  wrong_origin_status)

        duplicate_length_status, _, _ = local_request(
            app_port, "POST", "/api/auth/start",
            [("Origin", origin), ("Cookie", "mb_session=" + cookie),
             ("X-CSRF-Token", csrf), ("Content-Type", "application/json"),
             ("Content-Length", "2"), ("Content-Length", "2")], b"{}")
        add_check(checks, "duplicate_content_length_rejected",
                  duplicate_length_status == 400 and len(fixture_state["records"]) == 0,
                  duplicate_length_status)

        nonempty_status, _, _ = local_request(
            app_port, "POST", "/api/auth/start",
            {"Origin": origin, "Cookie": "mb_session=" + cookie,
             "X-CSRF-Token": csrf, "Content-Type": "application/json"}, b"[]")
        add_check(checks, "nonempty_json_object_rejected",
                  nonempty_status == 400 and len(fixture_state["records"]) == 0,
                  nonempty_status)

        oversized_status, _, _ = local_request(
            app_port, "POST", "/api/auth/start",
            {"Origin": origin, "Cookie": "mb_session=" + cookie,
             "X-CSRF-Token": csrf, "Content-Type": "application/json"},
            b'{"padding":12345}')
        add_check(checks, "body_over_16_bytes_rejected_before_provider",
                  oversized_status == 413 and len(fixture_state["records"]) == 0,
                  oversized_status)

        _, _, _, first = start_signin(app_port, cookie, csrf, config, shared,
                                      checks, "valid_pkce_start")
        if first is None:
            raise RuntimeError("valid auth start did not return a checked PKCE URL")
        shared["expected_challenge"] = first["challenge"]
        shared["expected_code"] = "fixture-code-initial"
        wrong_state = "A" * 43
        if wrong_state == first["state"]:
            wrong_state = "B" * 43
        wrong_state_status, _, _ = callback_request(app_port, cookie, wrong_state)
        add_check(checks, "wrong_state_callback_rejected_before_token_exchange",
                  wrong_state_status == 403 and len(fixture_state["records"]) == 0,
                  wrong_state_status)
        wrong_callback_cookie_status, _, _ = callback_request(
            app_port, cookie, first["state"], wrong_cookie=True)
        add_check(checks, "wrong_callback_cookie_rejected_before_token_exchange",
                  wrong_callback_cookie_status == 401 and len(fixture_state["records"]) == 0,
                  wrong_callback_cookie_status)
        duplicate_callback_status, _, _ = local_request(
            app_port, "GET", callback_target(first["state"], duplicate_code=True),
            {"Cookie": "mb_session=" + cookie})
        add_check(checks, "duplicate_callback_code_rejected_before_token_exchange",
                  duplicate_callback_status == 403 and len(fixture_state["records"]) == 0,
                  duplicate_callback_status)

        first_status, first_headers, first_body = local_request(
            app_port, "GET", callback_target(first["state"], code=shared["expected_code"]),
            {"Cookie": "mb_session=" + cookie}, timeout=8)
        location = first_headers.get("location", [])
        add_check(checks, "callback_exchange_redirects_to_app",
                  first_status == 303 and location == ["/app"] and len(first_body) <= 8192,
                  first_status)
        request_count_after_callback = wait_for_record_count(fixture_state, 2)
        add_check(checks, "first_callback_used_token_then_bearer_userinfo",
                  request_count_after_callback == 2
                  and all(item.get("method_path_ok") and item.get("host_ok")
                          and item.get("credential_ok") and item.get("headers_ok")
                          for item in fixture_state["records"][:2])
                  and fixture_state["records"][0].get("form_ok")
                  and fixture_state["records"][0].get("pkce_ok"),
                  first_status)

        replay_status, _, _ = callback_request(app_port, cookie, first["state"])
        add_check(checks, "callback_state_replay_refused_without_provider_request",
                  replay_status == 403 and len(fixture_state["records"]) == 2,
                  replay_status)

        account_status, _, account_body = local_request(
            app_port, "GET", "/api/account", {"Cookie": "mb_session=" + cookie}, timeout=8)
        account = decode_json(account_body) or {}
        account_person = account.get("account") if isinstance(account.get("account"), dict) else {}
        add_check(checks, "account_summary_is_live_verified_profile",
                  account_status == 200 and account.get("status") == "connected"
                  and account.get("live_service") == "verified"
                  and account_person.get("display_name") == "Fixture User"
                  and account_person.get("user_id") == 12345
                  and account_person.get("is_premium") is True,
                  account_status)

        _, _, _, malformed = start_signin(app_port, cookie, csrf, config, shared,
                                          checks, "replacement_malformed_start")
        if malformed is None:
            raise RuntimeError("malformed-token replacement start failed")
        shared["expected_code"] = "fixture-code-malformed"
        malformed_status, _, _ = callback_request(app_port, cookie, malformed["state"],
                                                   code=shared["expected_code"])
        malformed_record_count = wait_for_record_count(fixture_state, 4)
        add_check(checks, "malformed_token_response_fails_without_account_replacement",
                  malformed_status == 502 and malformed_record_count == 4
                  and fixture_state["records"][3].get("form_ok")
                  and fixture_state["records"][3].get("pkce_ok"),
                  malformed_status)

        _, _, _, candidate = start_signin(app_port, cookie, csrf, config, shared,
                                          checks, "replacement_candidate_start")
        if candidate is None:
            raise RuntimeError("candidate replacement start failed")
        shared["expected_code"] = "fixture-code-candidate"
        candidate_status, _, _ = callback_request(app_port, cookie, candidate["state"],
                                                   code=shared["expected_code"])
        candidate_record_count = wait_for_record_count(fixture_state, 6)
        add_check(checks, "failed_candidate_userinfo_preserves_previous_account",
                  candidate_status == 502 and candidate_record_count == 6
                  and fixture_state["records"][4].get("form_ok")
                  and fixture_state["records"][4].get("pkce_ok")
                  and fixture_state["records"][5].get("credential_ok") is True
                  and fixture_state["records"][5].get("status") == 401,
                  candidate_status)

        preserved_status, _, preserved_body = local_request(
            app_port, "GET", "/api/account", {"Cookie": "mb_session=" + cookie}, timeout=8)
        preserved_record_count = wait_for_record_count(fixture_state, 7)
        preserved = decode_json(preserved_body) or {}
        preserved_person = preserved.get("account") if isinstance(preserved.get("account"), dict) else {}
        add_check(checks, "old_account_remains_live_after_failed_replacements",
                  preserved_status == 200 and preserved.get("status") == "connected"
                  and preserved.get("live_service") == "verified"
                  and preserved_person.get("display_name") == "Fixture User"
                  and preserved_record_count == 7,
                  preserved_status)

        timeout_started = time.monotonic()
        timeout_status, _, timeout_body = local_request(
            app_port, "GET", "/api/account", {"Cookie": "mb_session=" + cookie}, timeout=8)
        timeout_elapsed_ms = int((time.monotonic() - timeout_started) * 1000)
        timeout_json = decode_json(timeout_body) or {}
        add_check(checks, "provider_timeout_response_delivered_with_fresh_send_budget",
                  timeout_status == 504 and 2300 <= timeout_elapsed_ms <= 7000
                  and isinstance(timeout_json.get("error"), dict)
                  and len(fixture_state["records"]) >= 7,
                  timeout_status)

        healthy_status, _, healthy_body = local_request(
            app_port, "GET", "/api/account", {"Cookie": "mb_session=" + cookie}, timeout=8)
        healthy = decode_json(healthy_body) or {}
        healthy_person = healthy.get("account") if isinstance(healthy.get("account"), dict) else {}
        healthy_record_count = wait_for_record_count(fixture_state, 9)
        add_check(checks, "shared_owner_accepts_healthy_userinfo_after_timeout",
                  healthy_status == 200 and healthy.get("status") == "connected"
                  and healthy.get("live_service") == "verified"
                  and healthy_person.get("display_name") == "Fixture User"
                  and healthy_record_count == 9,
                  healthy_status)

        _, _, _, stale = start_signin(app_port, cookie, csrf, config, shared,
                                      checks, "pending_before_disconnect_start")
        if stale is None:
            raise RuntimeError("disconnect pending-state setup failed")
        shared["expected_code"] = "fixture-code-after-disconnect"
        disconnect_status, _, disconnect_body = local_request(
            app_port, "POST", "/api/auth/disconnect",
            {"Origin": origin, "Cookie": "mb_session=" + cookie,
             "X-CSRF-Token": csrf, "Content-Type": "application/json"}, b"{}",
            split_body=True)
        disconnect = decode_json(disconnect_body) or {}
        add_check(checks, "disconnect_erases_credentials_and_pending_auth",
                  disconnect_status == 200 and disconnect.get("status") == "disconnected",
                  disconnect_status)
        stale_status, _, _ = callback_request(app_port, cookie, stale["state"],
                                              code=shared["expected_code"])
        stale_record_count = wait_for_record_count(fixture_state, 9, timeout=.05)
        add_check(checks, "disconnect_invalidates_pending_callback_without_exchange",
                  stale_status == 403 and stale_record_count == 9,
                  stale_status)
        disconnected_status, _, disconnected_body = local_request(
            app_port, "GET", "/api/account", {"Cookie": "mb_session=" + cookie})
        disconnected = decode_json(disconnected_body) or {}
        add_check(checks, "disconnected_account_is_not_verified_from_cached_state",
                  disconnected_status == 200 and disconnected.get("status") == "disconnected"
                  and disconnected.get("credentials_present") is False
                  and disconnected.get("live_service") == "not_checked",
                  disconnected_status)

        route_checks = len(fixture_state["records"]) == len(routes)
        for index, route in enumerate(routes):
            if index >= len(fixture_state["records"]):
                route_checks = False
                continue
            observed = fixture_state["records"][index]
            expected_ok = (observed.get("case") == route["case"]
                           and observed.get("unexpected") is False
                           and observed.get("tls") is True
                           and observed.get("method_path_ok") is True
                           and observed.get("host_ok") is True
                           and observed.get("credential_ok") is True
                           and observed.get("headers_ok") is True)
            if route["kind"] == "token":
                expected_ok = expected_ok and observed.get("form_ok") is True
                if route["case"] != "malformed-token":
                    expected_ok = expected_ok and observed.get("pkce_ok") is True
            else:
                expected_ok = expected_ok and observed.get("form_ok") is True
            if route["case"] != "provider-timeout":
                expected_ok = expected_ok and observed.get("response_sent") is True
            route_checks = route_checks and expected_ok
        add_check(checks, "owned_tls_requests_match_fixed_token_and_userinfo_contract",
                  route_checks, len(fixture_state["records"]))
        if process.poll() is not None:
            add_check(checks, "app_remains_running_after_provider_flow", False)
        else:
            add_check(checks, "app_remains_running_after_provider_flow", True)
    finally:
        if process is not None and process.poll() is None:
            try:
                local_request(app_port, "POST", "/shutdown",
                              {"Origin": f"http://127.0.0.1:{app_port}",
                               "X-Modbridge-Shutdown": "1", "Content-Length": "0"}, b"", timeout=3)
            except (OSError, RuntimeError):
                pass
        if process is not None:
            try:
                stdout, _ = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                stdout, _ = process.communicate(timeout=3)
            shim_lines = [line for line in stdout.decode("utf-8", "replace").splitlines()
                          if line.startswith("NEXUS_AUTH_SOCKET_SHIM ")]
            if len(shim_lines) == 1:
                try:
                    shim = json.loads(shim_lines[0].split(" ", 1)[1])
                except ValueError:
                    shim = None
        close_fixture()

    records = fixture_state["records"]
    expected_transfer_count = len(routes)
    add_check(checks, "private_socket_shim_routed_only_expected_tls_transfers",
              isinstance(shim, dict) and shim.get("configured") is True
              and shim.get("installs") == expected_transfer_count
              and shim.get("open_calls") == expected_transfer_count
              and shim.get("connected") == expected_transfer_count
              and shim.get("sockopt_calls") == expected_transfer_count
              and shim.get("refused") == 0,
              expected_transfer_count)
    add_check(checks, "owned_tls_server_recorded_exact_transfer_count",
              len(records) == expected_transfer_count, len(records))
    summary = {
        "profile": str(Path(profile).resolve()),
        "fixture_binary_sha256": link_identity["fixture_binary_sha256"],
        "profile_artifact_sha256": link_identity["profile_artifact_sha256"],
        "shim_source_sha256": link_identity["shim_source_sha256"],
        "reused_cpp_objects": link_identity["cpp_objects"],
        "shim": shim,
        "tls_cases": [{key: record.get(key) for key in
                       ("case", "tls", "method_path_ok", "host_ok", "credential_ok",
                        "headers_ok", "form_ok", "pkce_ok", "response_sent", "status")}
                      for record in records],
        "checks": checks,
        "elapsed_ms": int((time.monotonic() - started_at) * 1000),
        "no_live_nexus": True,
        "fake_credentials_only": True,
        "no_raw_provider_or_callback_values_recorded": True,
        "private_socket_route_limitation":
            "test-only curl call-site hook maps only the production-resolved 127.0.0.1:443 socket to the owned ephemeral TLS listener; real HTTPS SNI, Host, CA and certificate-name validation remain active",
    }
    MATRIX_DIR.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(MATRIX_DIR, 0o700)
    run_number = 1
    while (MATRIX_DIR / f"run-{run_number}").exists():
        run_number += 1
    receipt_dir = MATRIX_DIR / f"run-{run_number}"
    receipt_dir.mkdir(parents=True, exist_ok=False, mode=0o700)
    os.chmod(receipt_dir, 0o700)
    receipt = receipt_dir / "receipt.json"
    receipt.write_text(json.dumps(summary, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    os.chmod(receipt, 0o600)
    return summary, receipt


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def create_certificates(directory):
    helper_path = ROOT / "outputs/ps5-mod-bridge/native-host/tests/nexus_client_verify.py"
    spec = importlib.util.spec_from_file_location("nexus_client_verify_fixture_helper", helper_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load the existing owned TLS certificate helper")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.generate_certificates(directory)


def prepare_build_config():
    EVIDENCE.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(EVIDENCE, 0o700)
    FIXTURE_CONFIG_DIR.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(FIXTURE_CONFIG_DIR, 0o700)
    existing = FIXTURE_CONFIG_DIR / "config-receipt.json"
    if existing.is_file():
        return json.loads(existing.read_text(encoding="utf-8"))
    ca_pem, leaf_pem, leaf_key = create_certificates(FIXTURE_CONFIG_DIR)
    callback_port = free_port()
    config = {
        "client_id": "fixture_public_client",
        "scope": "fixture_scope",
        "redirect_uri": f"http://127.0.0.1:{callback_port}/auth/callback",
        "ca_pem_path": str(ca_pem.resolve()),
        "users_resolve": "users.nexusmods.com:443:127.0.0.1",
        "timeout_ms": 2500,
        "connect_timeout_ms": 1000,
    }
    path = FIXTURE_CONFIG_DIR / "oauth-config.json"
    path.write_text(json.dumps(config, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    os.chmod(path, 0o600)
    receipt = {
        "config_path": str(path.resolve()), "callback_port": callback_port,
        "client_id_is_fake": True, "ca_sha256": sha256(ca_pem),
        "leaf_sha256": sha256(leaf_pem), "leaf_key_mode": oct(leaf_key.stat().st_mode & 0o777),
        "total_timeout_ms": config["timeout_ms"],
        "connect_timeout_ms": config["connect_timeout_ms"],
    }
    summary_path = FIXTURE_CONFIG_DIR / "config-receipt.json"
    summary_path.write_text(json.dumps(receipt, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    os.chmod(summary_path, 0o600)
    return receipt


def request(port, method, path, headers=None, body=None, timeout=2):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        actual_headers = {"Host": f"127.0.0.1:{port}", "Connection": "close"}
        if headers:
            actual_headers.update(headers)
        connection.request(method, path, body=body, headers=actual_headers)
        response = connection.getresponse()
        payload = response.read()
        return response.status, dict(response.getheaders()), payload
    finally:
        connection.close()


def verify_red(profile=RED_PROFILE):
    profile = Path(profile).resolve()
    manifest = json.loads((profile / "manifest.json").read_text(encoding="utf-8"))
    binary = profile / manifest["artifact"]
    if not binary.is_file() or sha256(binary) != manifest["artifact_sha256"]:
        raise RuntimeError("RED host artifact does not match its build manifest")
    for relative in ("outputs/ps5-mod-bridge/native-host/host.c",
                     "outputs/ps5-mod-bridge/native-host/http.c",
                     "outputs/ps5-mod-bridge/native-host/http.h"):
        if sha256(ROOT / relative) != manifest["source_sha256"].get(relative):
            raise RuntimeError("RED host source no longer matches the recorded app profile")

    port = free_port()
    process = subprocess.Popen(
        [str(binary), "--port", str(port), "--lifetime", "12"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    observation = {"profile": str(profile), "artifact_sha256": manifest["artifact_sha256"],
                   "source_hashes_verified": True, "port": port}
    try:
        deadline = time.monotonic() + 4
        capabilities = None
        while time.monotonic() < deadline:
            try:
                capabilities = request(port, "GET", "/api/capabilities", timeout=.25)
                break
            except OSError:
                if process.poll() is not None:
                    raise RuntimeError("RED host exited before accepting a local request")
                time.sleep(.04)
        if capabilities is None:
            raise RuntimeError("RED host did not start within its bound")
        cap_status, cap_headers, cap_body = capabilities
        cap_json = json.loads(cap_body)
        caps = cap_json.get("capabilities", {})
        observation.update({
            "capabilities_http": cap_status,
            "session_cookie_set": any(name.lower() == "set-cookie" for name in cap_headers),
            "csrf_token_present": isinstance(cap_json.get("csrf_token"), str),
            "nexus_auth_start": caps.get("nexus_auth_start"),
            "account_status": caps.get("account_status"),
        })

        start_status, _, start_body = request(
            port, "POST", "/api/auth/start",
            headers={"Origin": f"http://127.0.0.1:{port}",
                     "Cookie": "mb_session=fixture-session",
                     "X-CSRF-Token": "fixture-csrf",
                     "Content-Type": "application/json"},
            body=b"{}",
        )
        observation["auth_start_http"] = start_status
        observation["auth_start_returns_authorization_url"] = False
        try:
            value = json.loads(start_body)
            observation["auth_start_returns_authorization_url"] = isinstance(
                value.get("authorization_url"), str)
        except (UnicodeError, ValueError, AttributeError):
            pass

        callback_status, _, _ = request(
            port, "GET", "/auth/callback?code=fixture-code&state=fixture-state",
            headers={"Cookie": "mb_session=fixture-session"},
        )
        observation["callback_http"] = callback_status
        observation["expected_red"] = (
            cap_status == 200
            and observation["session_cookie_set"] is False
            and observation["csrf_token_present"] is False
            and observation["nexus_auth_start"] is False
            and observation["account_status"] is False
            and start_status in (400, 405, 404)
            and observation["auth_start_returns_authorization_url"] is False
            and callback_status in (400, 404, 405)
        )
        if observation["expected_red"] is not True:
            raise RuntimeError("current app did not exhibit the expected auth-unavailable RED")
    finally:
        if process.poll() is None:
            try:
                request(port, "POST", "/shutdown", headers={
                    "Origin": f"http://127.0.0.1:{port}",
                    "X-Modbridge-Shutdown": "1",
                    "Content-Length": "0",
                }, body=b"")
            except OSError:
                pass
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=2)

    EVIDENCE.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(EVIDENCE, 0o700)
    receipt_dir = EVIDENCE / "actual-app-red-1"
    receipt_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(receipt_dir, 0o700)
    receipt = receipt_dir / "receipt.json"
    receipt.write_text(json.dumps(observation, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    os.chmod(receipt, 0o600)
    return observation, receipt


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("red", "prepare-config", "full"), default="red")
    parser.add_argument("--app-profile", type=Path, default=RED_PROFILE)
    parser.add_argument("--oauth-config", type=Path,
                        default=FIXTURE_CONFIG_DIR / "oauth-config.json")
    args = parser.parse_args()
    if args.mode == "prepare-config":
        print(json.dumps(prepare_build_config(), sort_keys=True))
        return
    if args.mode == "red":
        observation, receipt = verify_red(args.app_profile)
        print(json.dumps({"observation": observation, "receipt": str(receipt)}, sort_keys=True))
        return
    profile = args.app_profile if args.app_profile != RED_PROFILE else CONFIGURED_PROFILE
    summary, receipt = verify_full(profile, args.oauth_config)
    print(json.dumps({"receipt": str(receipt), "checks": summary["checks"],
                      "shim": summary["shim"], "tls_cases": summary["tls_cases"],
                      "fixture_binary_sha256": summary["fixture_binary_sha256"]},
                     sort_keys=True))
    if not all(check.get("passed") for check in summary["checks"].values()):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
