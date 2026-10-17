"""Private loopback development host. No console or translation claim."""
from dataclasses import asdict, is_dataclass
from copy import deepcopy
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import queue
import re
import secrets
import threading
import time
from urllib.parse import urlsplit

import modbridge_nexus as nexus
from modbridge_nexus.records import game_domain, positive_id
from modbridge_inventory import TitleInventory, TitleInventoryError, validate_reference
from modbridge_reports import validate_report


class HostError(Exception):
    def __init__(self, code, status=400):
        self.code, self.status = code, status


MESSAGES = {
    "invalid_input": "The request is invalid.", "session_required": "Open ModBridge again to start a local session.",
    "origin_refused": "The request did not come from this app.", "configuration": "Nexus sign-in has not been configured.",
    "not_found": "The requested item is unavailable.", "translation_unavailable": "Game preparation and installation are not available yet.",
    "job_limit": "The local job queue is full.", "intent_conflict": "That download request already refers to another file.",
    "busy": "An account operation is still running. Try again shortly.", "operation_failed": "The operation could not be completed.",
    "interrupted": "The app stopped before this job completed.", "download_authorization": "Continue on Nexus to authorize this download.",
    "storage_unavailable": "Private job storage is unavailable. Restart after checking its location.",
    "stale_target_ref": "The console target changed. Refresh the targets and select it again.",
    "inventory_page_stale": "The target inventory changed. Refresh the targets.",
    "inventory_refresh_busy": "A target refresh is already in progress.",
    "inventory_capture_in_progress": "The target inventory is being captured.",
}

INVENTORY_ERROR_STATUS = {"inventory_page_stale": 409, "inventory_refresh_busy": 409,
                          "inventory_capture_in_progress": 503, "invalid_input": 400}


def safe_error(error):
    if isinstance(error, HostError):
        return {"code": error.code, "message": MESSAGES.get(error.code, MESSAGES["operation_failed"])}
    if isinstance(error, nexus.NexusError):
        return {"code": error.code, "message": str(error)}
    return {"code": "operation_failed", "message": MESSAGES["operation_failed"]}


def bounded_json(data):
    if len(data) > 65536:
        raise HostError("invalid_input", 413)
    depth, quoted, escaped = 0, False, False
    for char in data:
        if quoted:
            if escaped:
                escaped = False
            elif char == 92:
                escaped = True
            elif char == 34:
                quoted = False
        elif char == 34:
            quoted = True
        elif char in (91, 123):
            depth += 1
            if depth > 32:
                raise HostError("invalid_input")
        elif char in (93, 125):
            depth -= 1

    def pairs(values):
        record = {}
        for key, value in values:
            if key in record:
                raise ValueError()
            record[key] = value
        return record
    try:
        value = json.loads(data, object_pairs_hook=pairs,
            parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
    except (ValueError, UnicodeError, RecursionError):
        raise HostError("invalid_input") from None
    if not isinstance(value, dict):
        raise HostError("invalid_input")
    return value


class BridgeApplication:
    def __init__(self, store, quarantine, oauth_config=None, *, account=None, download=None, analyze=None, inventory=None):
        self.store = nexus.CredentialStore(store)
        self.quarantine = Path(quarantine).absolute()
        # The same storage checks used for credentials create private parents.
        self.receipts = self.quarantine / ".jobs"
        nexus.save_private_json(self.receipts / ".format", {"version": 1})
        self.account = account or nexus.AccountManager(self.store)
        self.oauth_config = oauth_config
        self.download = download or nexus.download_to_quarantine
        if analyze is None:
            from nexus import audit_download
            analyze = audit_download
        self.analyze = analyze
        self.inventory = inventory if inventory is not None else TitleInventory()
        self.origin = None
        self.operation_lock = threading.RLock()
        self.state_lock = threading.RLock()
        self.sessions = {}
        self.authorizations = {}
        self.download_authorizations = {}
        self.records = {}
        self.intents = {}
        self.storage_failed = False
        self.pending = queue.Queue(maxsize=16)
        self.stopping = threading.Event()
        self._load_receipts()
        self.worker = threading.Thread(target=self._work, name="modbridge-jobs", daemon=True)
        self.worker.start()

    def _load_receipts(self):
        paths = sorted(self.receipts.glob("*.json"))
        if len(paths) > 100:
            raise HostError("job_limit")
        for path in paths:
            if not re.fullmatch(r"[a-f0-9]{32}\.json", path.name):
                continue
            record = nexus.load_private_json(path)
            record = self._validated_receipt(record, path.stem)
            if record.get("status") in ("queued", "downloading", "analyzing"):
                record.update(status="interrupted", error=safe_error(HostError("interrupted")))
                nexus.save_private_json(path, record)
            self.records[record["id"]] = record
            key = record.get("idempotency_key")
            if key in self.intents:
                raise HostError("invalid_input")
            self.intents[key] = record["id"]

    def _validated_receipt(self, record, identifier):
        required = {"id", "kind", "idempotency_key", "game", "mod_id", "file_id",
                    "label", "status", "stage", "compatibility", "source_url"}
        stages = {"queued": {"requested"}, "downloading": {"requested"},
            "analyzing": {"downloaded"}, "analyzed": {"analyzed"},
            "needs_nexus_authorization": {"requested"},
            "failed": {"requested", "downloaded"}, "interrupted": {"requested", "downloaded"}}
        try:
            if (not isinstance(record, dict) or not required <= set(record)
                    or set(record) - required - {"sha256", "bytes_downloaded", "error", "target_ref", "analysis_report"}
                    or record["id"] != identifier or record["kind"] != "download_analyze"
                    or not isinstance(record["idempotency_key"], str)
                    or not re.fullmatch(r"[a-zA-Z0-9_-]{8,80}", record["idempotency_key"])
                    or record["compatibility"] != "unresolved"
                    or record["stage"] not in stages.get(record["status"], set())):
                raise ValueError()
            if "target_ref" in record:
                validate_reference(record["target_ref"])
            if "analysis_report" in record:
                if record["status"] != "analyzed":
                    raise ValueError()
                validate_report(record["analysis_report"])
            source = nexus.DownloadSource(record["game"], record["mod_id"], record["file_id"])
            if (record["source_url"] != source.source_url
                    or record["label"] != "Mod " + str(source.mod_id) + " · file " + str(source.file_id)):
                raise ValueError()
            downloaded = record["stage"] in ("downloaded", "analyzed")
            if downloaded:
                if (not isinstance(record.get("sha256"), str)
                        or not re.fullmatch(r"[a-f0-9]{64}", record["sha256"])
                        or type(record.get("bytes_downloaded")) is not int
                        or not 0 <= record["bytes_downloaded"] <= 9223372036854775807):
                    raise ValueError()
            elif "sha256" in record or "bytes_downloaded" in record:
                raise ValueError()
            failed = record["status"] in ("failed", "interrupted", "needs_nexus_authorization")
            if failed:
                error = record.get("error")
                if not isinstance(error, dict) or set(error) != {"code", "message"} or not isinstance(error["code"], str):
                    raise ValueError()
                code = error["code"]
                allowed = [safe_error(nexus.NexusError(code))]
                if code in MESSAGES:
                    allowed.append(safe_error(HostError(code)))
                if error not in allowed:
                    raise ValueError()
            elif "error" in record:
                raise ValueError()
            return record
        except (ValueError, TypeError, KeyError, nexus.NexusError, TitleInventoryError):
            raise HostError("invalid_input") from None

    def _save(self, record):
        try:
            nexus.save_private_json(self.receipts / (record["id"] + ".json"), record)
        except Exception:
            self.storage_failed = True
            raise HostError("storage_unavailable", 503) from None

    def _update(self, identifier, **values):
        with self.state_lock:
            record = dict(self.records[identifier], **values)
            self._save(record)
            self.records[identifier] = record

    def _operation(self, action):
        if not self.operation_lock.acquire(timeout=2):
            raise HostError("busy", 409)
        try:
            return action()
        finally:
            self.operation_lock.release()

    def capabilities(self):
        return {"mode": "desktop_preparation", "console_execution": "unverified",
            "capabilities": {"nexus_catalog": True, "account_status": True,
            "nexus_auth_start": self.oauth_config is not None, "detected_titles": False,
            "title_inventory_paging": True,
            "jobs": True, "installed_records": True, "download_analyze": not self.storage_failed,
            "prepared_translation": False, "deploy": False}}

    def account_status(self):
        record = self._operation(self.account.status)
        result = {key: record[key] for key in ("credentials_present", "live_service") if key in record}
        result["status"] = ("connected" if record.get("connected") is True and record.get("live_service") == "verified"
            else "error" if record.get("live_service") == "unavailable" else "disconnected")
        user = record.get("account")
        if record.get("live_service") == "verified" and isinstance(user, dict):
            result["account"] = {key: user[key] for key in ("user_id", "display_name", "is_premium", "is_supporter", "adult_content") if key in user}
        if isinstance(record.get("error"), dict):
            result["error"] = {"code": record["error"].get("code", "operation_failed"),
                               "message": MESSAGES["operation_failed"]}
        result["console_translation"] = "unverified"
        return result

    def catalog(self, domain=None, mod=None):
        def action():
            client = self.account.client()
            if domain is None:
                return client.games()
            validated_domain = game_domain(domain)
            if mod is None:
                return client.latest_mods(validated_domain)
            return client.files(validated_domain, positive_id(mod))
        return {"items": self._operation(action)}

    def mod_details(self, domain, mod):
        validated_domain, validated_mod = game_domain(domain), positive_id(mod)
        return {"item": self._operation(lambda: self.account.client().mod(validated_domain, validated_mod))}

    def detected_titles(self, refresh=False):
        return self.inventory.snapshot(refresh=True) if refresh else self.inventory.snapshot()

    def title_page(self, cursor=None, refresh=False):
        return self.inventory.page(cursor, refresh=refresh)

    def title_current(self, snapshot_id):
        return self.inventory.current(snapshot_id)

    def _target_context(self, reference):
        try:
            return self.inventory.resolve(reference)
        except TitleInventoryError as error:
            status = 409 if error.code == "stale_target_ref" else INVENTORY_ERROR_STATUS.get(error.code, 400)
            raise HostError(error.code, status) from None

    def authorize(self, session_id):
        if self.oauth_config is None:
            raise HostError("configuration", 409)
        if self.oauth_config.redirect_uri != self.origin + "/auth/callback":
            raise HostError("configuration", 409)
        with self.state_lock:
            now = time.time()
            self.authorizations = {key: value for key, value in self.authorizations.items()
                                   if value.expires_at > now and not value.to_record()["used"]}
            if len(self.authorizations) >= 16:
                raise HostError("busy", 409)
            pending = nexus.OAuthSession(self.oauth_config)
            self.authorizations[session_id] = pending
            return {"authorization_url": pending.authorize_url}

    def callback(self, query):
        # Match state without logging or echoing authorization codes.
        from urllib.parse import parse_qs
        state = parse_qs(query).get("state", [])
        if len(state) != 1:
            raise HostError("invalid_input")
        with self.state_lock:
            pending = next((value for value in self.authorizations.values()
                if secrets.compare_digest(value._state, state[0])), None)
        if pending is None:
            raise HostError("invalid_input")
        def complete():
            with self.state_lock:
                if not any(value is pending for value in self.authorizations.values()):
                    raise HostError("invalid_input")
            return self.account.complete_sign_in(pending, self.origin + "/auth/callback?" + query)
        user = self._operation(complete)
        return asdict(user) if is_dataclass(user) else None

    def disconnect(self):
        def action():
            # Serialized with callback validation and credential replacement.
            # A callback retrieved before logout must still be pending here.
            with self.state_lock:
                self.authorizations.clear()
                self.download_authorizations.clear()
            self.account.disconnect()
        return self._operation(action)

    def _public_job(self, record):
        fields = ("id", "kind", "game", "mod_id", "file_id", "label", "status", "stage",
                  "compatibility", "source_url", "sha256", "bytes_downloaded", "error", "target_ref", "analysis_report")
        return deepcopy({key: record[key] for key in fields if key in record})

    def jobs(self, identifier=None):
        with self.state_lock:
            if identifier is not None:
                if identifier not in self.records:
                    raise HostError("not_found", 404)
                return self._public_job(self.records[identifier])
            return [self._public_job(record) for record in self.records.values()]

    def submit(self, intent):
        required = {"kind", "game", "mod_id", "file_id", "idempotency_key"}
        if not isinstance(intent, dict) or not required <= set(intent) or set(intent) - required - {"target_ref", "nxm"}:
            raise HostError("invalid_input")
        if intent["kind"] != "download_analyze":
            raise HostError("translation_unavailable", 409)
        game, mod, file = game_domain(intent["game"]), positive_id(intent["mod_id"]), positive_id(intent["file_id"])
        key = intent["idempotency_key"]
        if not isinstance(key, str) or not re.fullmatch(r"[a-zA-Z0-9_-]{8,80}", key):
            raise HostError("invalid_input")
        authorization = None
        if "nxm" in intent:
            authorization = nexus.parse_nxm(intent["nxm"])
            # Admission binds the file and expiry. The provider still validates
            # the grant against its live account identity before issuing a link.
            authorization.validate(authorization.user_id, game, mod, file)
        reference = None
        if "target_ref" in intent:
            try:
                reference = validate_reference(intent["target_ref"])
            except TitleInventoryError:
                raise HostError("invalid_input") from None
        with self.state_lock:
            existing = self.intents.get(key)
            if existing:
                old = self.records[existing]
                if (old["game"], old["mod_id"], old["file_id"], old.get("target_ref")) != (game, mod, file, reference):
                    raise HostError("intent_conflict", 409)
                if old["status"] != "needs_nexus_authorization" or authorization is None:
                    return self._public_job(old)
            if reference is not None:
                self._target_context(reference)
            if self.storage_failed:
                raise HostError("storage_unavailable", 503)
            if (not existing and len(self.records) >= 100) or self.pending.full() or self.stopping.is_set():
                raise HostError("job_limit", 409)
            identifier = existing or secrets.token_hex(16)
            if existing:
                record = dict(old, status="queued")
                record.pop("error", None)
            else:
                record = {"id": identifier, "kind": "download_analyze", "idempotency_key": key,
                    "game": game, "mod_id": mod, "file_id": file,
                    "label": "Mod " + str(mod) + " · file " + str(file), "status": "queued",
                    "stage": "requested", "compatibility": "unresolved",
                    "source_url": nexus.DownloadSource(game, mod, file).source_url}
                if reference is not None:
                    record["target_ref"] = reference
            self._save(record)
            self.records[identifier] = record
            self.intents[key] = identifier
            if authorization is not None:
                self.download_authorizations[identifier] = authorization
            self.pending.put_nowait(identifier)
            return self._public_job(record)

    def _work(self):
        while not self.stopping.is_set():
            try:
                identifier = self.pending.get(timeout=.1)
            except queue.Empty:
                continue
            try:
                record = self.records[identifier]
                if "target_ref" in record:
                    self._target_context(record["target_ref"])
                self._update(identifier, status="downloading")
                def download_links():
                    with self.state_lock:
                        authorization = self.download_authorizations.pop(identifier, None)
                    client = self.account.client()
                    args = (record["game"], record["mod_id"], record["file_id"])
                    if authorization is not None:
                        return client.download_links(*args, nxm=authorization)
                    return client.download_links(*args)
                links = self._operation(download_links)
                if not links:
                    raise HostError("operation_failed")
                result = self.download(links[0], self.quarantine / identifier)
                self._update(identifier, status="analyzing", stage="downloaded",
                    sha256=result.sha256, bytes_downloaded=result.bytes_downloaded)
                if "target_ref" in record:
                    target = self._target_context(record["target_ref"])
                    analysis = self.analyze(result, record["game"], target)
                    self._target_context(record["target_ref"])
                else:
                    analysis = self.analyze(result, record["game"])
                if analysis.get("analysis", {}).get("status") != "analyzed":
                    raise HostError("operation_failed")
                extra = {}
                if "report" in analysis["analysis"]:
                    try:
                        extra["analysis_report"] = validate_report(analysis["analysis"]["report"])
                    except (ValueError, TypeError):
                        raise HostError("invalid_input") from None
                self._update(identifier, status="analyzed", stage="analyzed", compatibility="unresolved", **extra)
            except Exception as error:
                state = "needs_nexus_authorization" if isinstance(error, nexus.NexusError) and error.code == "download_authorization" else "failed"
                with self.state_lock:
                    self.download_authorizations.pop(identifier, None)
                    try:
                        self._update(identifier, status=state, error=safe_error(error))
                    except Exception:
                        # Disk failures must not kill the worker or keep a live job
                        # spinning. The last durable phase becomes interrupted on
                        # restart; the current session sees the storage failure.
                        self.storage_failed = True
                        self.records[identifier] = dict(self.records[identifier],
                            status="failed", error=safe_error(HostError("storage_unavailable")))
            finally:
                self.pending.task_done()

    def close(self):
        with self.state_lock:
            self.stopping.set()
            self.download_authorizations.clear()
        self.worker.join(timeout=2)


class RequestReader:
    def __init__(self, stream, connection, deadline):
        self.stream, self.connection, self.deadline = stream, connection, deadline

    def __getattr__(self, name):
        return getattr(self.stream, name)

    def readline(self, limit=-1):
        line = bytearray()
        while limit < 0 or len(line) < limit:
            remaining = self.deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError()
            self.connection.settimeout(remaining)
            data = self.stream.read(1)
            if time.monotonic() >= self.deadline:
                raise TimeoutError()
            if not data:
                break
            line.extend(data)
            if data == b"\n":
                break
        return bytes(line)


def create_server(application, host="127.0.0.1", port=0):
    if host != "127.0.0.1":
        raise HostError("origin_refused")
    web = Path(__file__).resolve().parents[1] / "web"

    class Handler(BaseHTTPRequestHandler):
        def setup(self):
            super().setup()
            self.deadline = time.monotonic() + 10
            self.rfile = RequestReader(self.rfile, self.connection, self.deadline)

        def log_message(self, *_):
            pass

        def send_error(self, code, message=None, explain=None):
            self.failure(HostError("invalid_input", code))

        def respond(self, code, value=None, *, body=None, content_type="application/json", cookie=None, location=None):
            if body is None:
                body = json.dumps(value, ensure_ascii=True, allow_nan=False).encode()
            self.connection.settimeout(5)
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'")
            if cookie:
                self.send_header("Set-Cookie", "mb_session=" + cookie + "; Path=/; HttpOnly; SameSite=Strict")
            if location:
                self.send_header("Location", location)
            self.end_headers()
            self.wfile.write(body)

        def local(self):
            hosts = self.headers.get_all("Host", [])
            if hosts != [application.origin.removeprefix("http://")]:
                raise HostError("origin_refused", 403)
            origins = self.headers.get_all("Origin", [])
            if origins and origins != [application.origin]:
                raise HostError("origin_refused", 403)
            if self.headers.get("Sec-Fetch-Site") == "cross-site":
                raise HostError("origin_refused", 403)

        def session(self, bootstrap=False):
            cookies = SimpleCookie()
            try:
                cookies.load(self.headers.get("Cookie", ""))
                token = cookies.get("mb_session")
                token = token.value if token else None
            except Exception:
                token = None
            with application.state_lock:
                now = time.monotonic()
                application.sessions = {key: val for key, val in application.sessions.items() if val[1] > now}
                if token in application.sessions:
                    return token, application.sessions[token][0]
                if not bootstrap:
                    raise HostError("session_required", 401)
                if len(application.sessions) >= 16:
                    raise HostError("busy", 409)
                token, csrf = secrets.token_urlsafe(32), secrets.token_urlsafe(32)
                application.sessions[token] = (csrf, now + 1800)
                return token, csrf

        def read_body(self, max_bytes=65536):
            if (self.headers.get("Transfer-Encoding") is not None or len(self.headers.get_all("Content-Length", [])) != 1
                    or self.headers.get("Content-Type", "").split(";", 1)[0].lower() != "application/json"):
                raise HostError("invalid_input")
            try:
                amount = int(self.headers["Content-Length"])
            except ValueError:
                raise HostError("invalid_input") from None
            if not 0 <= amount <= max_bytes:
                raise HostError("invalid_input", 413 if max_bytes == 65536 else 400)
            data = bytearray()
            while len(data) < amount:
                remaining = self.deadline - time.monotonic()
                if remaining <= 0:
                    raise HostError("invalid_input", 408)
                self.connection.settimeout(remaining)
                chunk = self.rfile.read1(min(65536, amount - len(data)))
                if not chunk or time.monotonic() >= self.deadline:
                    raise HostError("invalid_input", 408)
                data.extend(chunk)
            return bounded_json(bytes(data))

        def paging_request(self):
            return getattr(self, "path", "").startswith("/api/detected-titles/pages")

        def paging_path(self):
            # No decoding or query normalization: cursors are exact server tokens.
            if len(self.path) > 96 or not self.path.isascii() or any(c in self.path for c in "?#%"):
                raise HostError("invalid_input")
            return self.path

        def failure(self, error):
            if self.paging_request():
                if isinstance(error, TitleInventoryError):
                    code = error.code if error.code in INVENTORY_ERROR_STATUS else "operation_failed"
                    status = INVENTORY_ERROR_STATUS.get(code, 500)
                elif isinstance(error, HostError):
                    code = error.code if error.code in MESSAGES else "operation_failed"
                    status = error.status
                else:
                    code, status = "operation_failed", 500
                body = json.dumps({"code": code, "error": MESSAGES[code]}, ensure_ascii=True,
                                  allow_nan=False, sort_keys=True, separators=(",", ":")).encode("ascii")
                self.respond(status, body=body)
            else:
                status = (error.status if isinstance(error, HostError) else
                          INVENTORY_ERROR_STATUS.get(error.code, 500) if isinstance(error, TitleInventoryError) else
                          400 if isinstance(error, nexus.NexusError) else 500)
                value = safe_error(HostError(error.code, status)) if isinstance(error, TitleInventoryError) else safe_error(error)
                self.respond(status, {"error": value})

        def do_GET(self):
            try:
                # OAuth redirects are cross-site navigation; state/PKCE validate
                # this special route instead of an existing browser cookie.
                parsed = urlsplit(self.path)
                if parsed.path == "/auth/callback":
                    if self.headers.get_all("Host", []) != [application.origin.removeprefix("http://")]:
                        raise HostError("origin_refused", 403)
                    application.callback(parsed.query)
                    self.respond(303, {}, location="/")
                    return
                self.local()
                if self.paging_request():
                    path = self.paging_path()
                    self.session()
                    if path == "/api/detected-titles/pages":
                        body = application.title_page()
                    else:
                        cursor = re.fullmatch(r"/api/detected-titles/pages/([a-f0-9]{32})", path)
                        current = re.fullmatch(r"/api/detected-titles/pages/current/([a-f0-9]{32})", path)
                        if cursor:
                            body = application.title_page(cursor[1])
                        elif current:
                            body = application.title_current(current[1])
                        else:
                            raise HostError("invalid_input")
                    self.respond(200, body=body)
                    return
                if parsed.query:
                    raise HostError("not_found", 404)
                if parsed.path in ("/", "/index.html", "/app.js", "/style.css",
                        "/controller-keyboard.js", "/config.js", "/installer-service.js",
                        "/installer-mock.js", "/installer-ui.js"):
                    name = "index.html" if parsed.path == "/" else parsed.path[1:]
                    path = web / name
                    if not path.is_file():
                        raise HostError("not_found", 404)
                    content_type = "text/javascript; charset=utf-8" if name.endswith(".js") else {"index.html": "text/html; charset=utf-8", "style.css": "text/css; charset=utf-8"}[name]
                    self.respond(200, body=path.read_bytes(), content_type=content_type)
                    return
                if parsed.path.endswith(".js") and not parsed.path.startswith("/api/"):
                    raise HostError("not_found", 404)
                if parsed.path == "/api/capabilities":
                    token, csrf = self.session(True)
                    self.respond(200, dict(application.capabilities(), csrf_token=csrf), cookie=token)
                    return
                self.session()
                if parsed.path == "/api/account":
                    value = application.account_status()
                elif parsed.path == "/api/games":
                    value = application.catalog()
                elif parsed.path == "/api/jobs":
                    value = {"items": application.jobs()}
                elif parsed.path == "/api/installed":
                    value = {"items": []}
                elif parsed.path == "/api/detected-titles":
                    value = application.detected_titles()
                else:
                    match = re.fullmatch(r"/api/games/([a-z0-9_-]{1,80})/mods(?:/([0-9]{1,19})/files)?", parsed.path)
                    detail = re.fullmatch(r"/api/games/([a-z0-9_-]{1,80})/mods/([0-9]{1,19})", parsed.path)
                    job = re.fullmatch(r"/api/jobs/([a-f0-9]{32})", parsed.path)
                    if match:
                        value = application.catalog(match[1], int(match[2]) if match[2] else None)
                    elif detail:
                        value = application.mod_details(detail[1], int(detail[2]))
                    elif job:
                        value = application.jobs(job[1])
                    else:
                        raise HostError("not_found", 404)
                self.respond(200, value)
            except Exception as error:
                self.failure(error)

        def do_POST(self):
            try:
                self.local()
                session, csrf = self.session()
                if self.headers.get_all("Origin", []) != [application.origin] or not secrets.compare_digest(self.headers.get("X-CSRF-Token", ""), csrf):
                    raise HostError("origin_refused", 403)
                if self.paging_request():
                    if self.paging_path() != "/api/detected-titles/pages/refresh":
                        raise HostError("invalid_input")
                    if len(self.headers.get_all("X-CSRF-Token", [])) != 1:
                        raise HostError("origin_refused", 403)
                    if self.read_body(16) != {}:
                        raise HostError("invalid_input")
                    self.respond(200, body=application.title_page(refresh=True))
                    return
                record = self.read_body()
                if self.path == "/api/auth/start" and record == {}:
                    value, code = application.authorize(session), 200
                elif self.path == "/api/auth/disconnect" and record == {}:
                    application.disconnect()
                    value, code = {"status": "disconnected"}, 200
                elif self.path == "/api/jobs":
                    value, code = application.submit(record), 202
                elif self.path == "/api/detected-titles/refresh" and record == {}:
                    value, code = application.detected_titles(refresh=True), 200
                else:
                    raise HostError("not_found", 404)
                self.respond(code, value)
            except Exception as error:
                self.failure(error)

    class Server(ThreadingHTTPServer):
        daemon_threads = True
        def handle_error(self, *_):
            pass

    server = Server((host, port), Handler)
    application.origin = "http://127.0.0.1:" + str(server.server_address[1])
    return server
