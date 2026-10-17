#!/usr/bin/env python3
"""Desktop preparation companion. Run provider requests only on explicit commands."""
from __future__ import annotations

import argparse
from dataclasses import asdict, is_dataclass
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from pathlib import Path
import stat
import socket
import subprocess
import sys
import tempfile
import time
from urllib.parse import urlsplit, urlunsplit
import webbrowser
import re

from modbridge_reports import analysis_report


class CommandError(Exception):
    def __init__(self, code, message):
        self.code = code
        self.message = message
        super().__init__(message)


class Parser(argparse.ArgumentParser):
    def error(self, message):
        raise CommandError("invalid_argument", "Invalid or missing command arguments; use --help")


def json_value(value):
    if isinstance(value, Path):
        return str(value)
    if is_dataclass(value):
        return asdict(value)
    raise TypeError("Unsupported result type")


def emit(value, stream=sys.stdout):
    stream.write(json.dumps(value, default=json_value, ensure_ascii=True) + "\n")
    stream.flush()


def read_private_input(path, limit=8192):
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            raise CommandError("invalid_input", "Expected a small regular input file")
        data = stream.read(limit + 1)
        if len(data) > limit:
            raise CommandError("invalid_input", "Input exceeded its size limit")
    try:
        return data.decode("utf-8").strip()
    except UnicodeError:
        raise CommandError("invalid_input", "Input must be UTF-8 text") from None


def positive_id(value):
    if not value.isdecimal() or int(value) <= 0:
        raise argparse.ArgumentTypeError("Expected a positive file or mod ID")
    return int(value)


def audit_download(downloaded, game, target_context=None):
    source = downloaded.source
    record = {"stage": "downloaded", "path": str(downloaded.path), "sha256": downloaded.sha256,
              "bytes_downloaded": downloaded.bytes_downloaded,
              "source": {"provider": "nexusmods", "game_domain": source.game_domain,
                         "mod_id": source.mod_id, "file_id": source.file_id, "url": source.source_url},
              "console_translation": "unverified"}
    project = Path(__file__).resolve().parents[1]
    binary = Path(os.environ.get("MODBRIDGE_BIN", project / "build" / "modbridge"))
    if not binary.is_file():
        record["analysis"] = {"status": "unavailable", "reason": "desktop_core_not_built"}
        return record
    folder = Path(tempfile.mkdtemp(prefix="analysis-", dir=Path(downloaded.path).parent))
    inventory = folder / "inventory.json"
    target = folder / "target.json"
    backends = folder / "backends.json"
    plan = folder / "plan.json"
    fingerprints = {}
    if target_context is not None:
        candidate = target_context.get("fingerprints") if isinstance(target_context, dict) else None
        if (not isinstance(candidate, dict) or set(candidate) - {"build_sha256"}
                or any(not isinstance(value, str) or not re.fullmatch(r"[a-f0-9]{64}", value) for value in candidate.values())):
            raise CommandError("invalid_target", "The trusted target context is invalid")
        fingerprints = dict(candidate)
    target.write_text(json.dumps({"platform": "ps5", "nexus_game_domain": game,
                                  "fingerprints": fingerprints, "capabilities": []}))
    backends.write_text("[]\n")

    def run_core(*arguments):
        result = subprocess.run([str(binary), *map(str, arguments)], capture_output=True, timeout=60)
        if result.returncode != 0:
            raise CommandError("analysis_failed", "The downloaded archive remains in quarantine; analysis could not complete")

    run_core("analyze", downloaded.path, "--output", inventory)
    actual = json.loads(inventory.read_text())
    if actual.get("package_sha256") != downloaded.sha256:
        raise CommandError("source_changed", "The archive changed between download and analysis")
    run_core("plan", inventory, "--target", target, "--backends", backends, "--output", plan)
    report = analysis_report(actual, json.loads(plan.read_text()))
    record["analysis"] = {"status": "analyzed", "inventory_path": str(inventory),
                           "plan_path": str(plan), "compatibility": "unresolved", "report": report}
    return record


def parser():
    result = Parser(description="ModBridge desktop Nexus preparation companion")
    commands = result.add_subparsers(dest="command", required=True, parser_class=Parser)
    for name in ("status", "connect-key", "login", "games", "mods", "files", "download", "disconnect"):
        command = commands.add_parser(name)
        command.add_argument("--store", required=True, type=Path,
                             help="Caller-selected private local credential file")
        if name == "connect-key":
            command.add_argument("--key-file", required=True, type=Path)
        if name == "login":
            command.add_argument("--client-id", required=True)
            command.add_argument("--redirect-uri", required=True)
            command.add_argument("--scope", default="")
        if name in ("mods", "files", "download"):
            command.add_argument("game", help="Nexus game domain; the same code serves all games")
        if name in ("files", "download"):
            command.add_argument("mod", type=positive_id)
        if name == "download":
            command.add_argument("file", type=positive_id)
            command.add_argument("--destination", required=True, type=Path,
                                 help="Explicit local quarantine directory")
            command.add_argument("--nxm-file", type=Path,
                                 help="Private text file holding website-authorized NXM URL")
    return result


def login(args, store, provider, *, timeout=300):
    callback = urlsplit(args.redirect_uri)
    try:
        port = callback.port
    except ValueError:
        port = None
    if (callback.scheme != "http" or callback.hostname not in ("127.0.0.1", "localhost")
            or callback.username is not None or callback.password is not None
            or port is None or not 1 <= port <= 65535 or callback.query or callback.fragment
            or not callback.path.startswith("/")):
        raise CommandError("invalid_callback", "This companion requires an approved HTTP loopback callback with a port and path")
    config = provider.OAuthConfig(args.client_id, args.redirect_uri, args.scope)
    session = provider.OAuthSession(config)
    outcome = {}
    deadline = time.monotonic() + timeout

    class BudgetTransport:
        def __init__(self):
            self.transport = provider.HTTPSTransport()
        def request(self, method, url, **kwargs):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise provider.NexusError("timeout")
            kwargs["timeout"] = remaining
            result = self.transport.request(method, url, **kwargs)
            if time.monotonic() >= deadline:
                raise provider.NexusError("timeout")
            return result

    account = provider.AccountManager(store, transport=BudgetTransport())

    class Callback(BaseHTTPRequestHandler):
        def log_message(self, *_args):
            # Callback query strings contain authorization codes; never log them.
            pass

        def setup(self):
            super().setup()
            from modbridge_nexus.transport import _DeadlineReader
            self.rfile = _DeadlineReader(self.rfile, self.connection, deadline)

        def handle(self):
            try:
                super().handle()
            except (provider.NexusError, OSError):
                pass

        def do_GET(self):
            received = urlsplit(self.path)
            if received.scheme or received.netloc or received.path != callback.path:
                self.send_error(404, "Unknown callback path")
                return
            callback_url = urlunsplit((callback.scheme, callback.netloc,
                                       received.path, received.query, received.fragment))
            try:
                account.complete_sign_in(session, callback_url)
                outcome["status"] = "authorized"
                text = b"Authorization complete. Return to ModBridge."
                status = 200
            except Exception:
                if session.to_record()["used"]:
                    outcome["status"] = "failed"
                text = b"Authorization could not be completed. Restart sign-in in ModBridge."
                status = 400
            self.send_response(status)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(text)))
            self.end_headers()
            self.wfile.write(text)

    with HTTPServer((callback.hostname, port), Callback) as server:
        server.timeout = min(1, timeout)
        emit({"status": "authorization_pending", "authorization_url": session.authorize_url})
        webbrowser.open(session.authorize_url)
        while not outcome and time.monotonic() < deadline:
            server.timeout = min(1, max(0, deadline - time.monotonic()))
            server.handle_request()
    if outcome.get("status") != "authorized":
        raise CommandError("authorization_incomplete", "Sign-in did not complete")
    emit({"status": "authorized", "console_translation": "unverified"})


def execute(args, provider):
    store = provider.CredentialStore(args.store)
    if args.command == "status":
        emit(provider.AccountManager(store).status())
        return
    if args.command == "disconnect":
        store.disconnect()
        emit({"status": "disconnected"})
        return
    if args.command == "connect-key":
        try:
            key = read_private_input(args.key_file)
        except OSError:
            raise CommandError("invalid_input", "Could not read a regular key file") from None
        credentials = provider.Credentials(api_key=key)
        profile = provider.NexusClient(credentials).user()
        store.save_account(credentials)
        emit({"status":"connected", "connected":True, "live_service":"verified", "account":asdict(profile)})
        return
    if args.command == "login":
        login(args, store, provider)
        return
    credentials = store.load()
    if credentials is None:
        raise CommandError("credentials_required", "Connect your own Nexus account before requesting the API")
    client = provider.AccountManager(store).client()
    if args.command == "games":
        emit(client.games())
    elif args.command == "mods":
        emit(client.latest_mods(args.game))
    elif args.command == "files":
        emit(client.files(args.game, args.mod))
    elif args.command == "download":
        nxm = provider.parse_nxm(read_private_input(args.nxm_file)) if args.nxm_file else None
        links = client.download_links(args.game, args.mod, args.file, nxm=nxm)
        if not links:
            raise CommandError("download_unavailable", "Nexus did not return a usable download link")
        downloaded = provider.download_to_quarantine(links[0], args.destination)
        emit(audit_download(downloaded, args.game))


def main(argv=None):
    try:
        args = parser().parse_args(argv)
        import modbridge_nexus as provider
        execute(args, provider)
        return 0
    except CommandError as error:
        emit({"error": {"code": error.code, "message": error.message}}, sys.stderr)
    except Exception as error:
        # A raw transport exception can contain credentials or signed URLs.
        code = getattr(error, "code", "operation_failed")
        result = {"error": {"code": code, "message": "The requested operation could not be completed"}}
        for field in ("status", "retry_after"):
            value = getattr(error, field, None)
            if isinstance(value, (int, float)):
                result["error"][field] = value
        emit(result, sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
