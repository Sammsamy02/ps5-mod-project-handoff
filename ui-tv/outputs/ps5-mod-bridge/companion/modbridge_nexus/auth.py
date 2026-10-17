"""Configured public-client PKCE. No password capture or bundled secret."""
from dataclasses import asdict, dataclass
import base64
import hashlib
import math
import re
import secrets
import time
from urllib.parse import parse_qsl, urlencode, urlsplit

from .credentials import Credentials, _secret
from .errors import NexusError
from .transport import HTTPSTransport, USER_ORIGIN, checked_json


@dataclass(frozen=True)
class OAuthConfig:
    client_id: str
    redirect_uri: str
    scope: str = ""

    def __post_init__(self):
        if not isinstance(self.client_id, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,128}", self.client_id) or self.client_id == "public_test":
            raise NexusError("configuration")
        if not isinstance(self.scope, str) or len(self.scope) > 1024 or any(ord(c) < 32 or ord(c) >= 127 for c in self.scope):
            raise NexusError("configuration")
        try:
            parsed = urlsplit(self.redirect_uri)
            if not isinstance(self.redirect_uri, str) or len(self.redirect_uri) > 4096 or any(ord(c) <= 32 or ord(c) >= 127 for c in self.redirect_uri):
                raise ValueError()
            if not parsed.scheme or not parsed.path or parsed.username is not None or parsed.password is not None or parsed.query or parsed.fragment or "\\" in self.redirect_uri:
                raise ValueError()
            if parsed.scheme == "http":
                if parsed.hostname not in {"localhost", "127.0.0.1", "::1"}:
                    raise ValueError()
                parsed.port
            elif parsed.scheme == "https":
                if not parsed.hostname:
                    raise ValueError()
                parsed.port
            elif not re.fullmatch(r"[a-z][a-z0-9+.-]*", parsed.scheme) or parsed.scheme in {"javascript", "data", "file", "ftp", "about"}:
                raise ValueError()
        except (AttributeError, TypeError, ValueError):
            raise NexusError("configuration") from None


def pkce_challenge(verifier):
    if not isinstance(verifier, str) or not re.fullmatch(r"[A-Za-z0-9._~-]{43,128}", verifier):
        raise NexusError("authorization")
    return base64.urlsafe_b64encode(hashlib.sha256(verifier.encode("ascii")).digest()).rstrip(b"=").decode("ascii")


class OAuthSession:
    def __init__(self, config, *, clock=time.time, lifetime=300):
        if not isinstance(config, OAuthConfig) or type(lifetime) not in (int, float) or not 0 < lifetime <= 3600:
            raise NexusError("configuration")
        self.config = config
        self.clock = clock
        self.created_at = clock()
        self.expires_at = self.created_at + lifetime
        self._state = secrets.token_urlsafe(32)
        self._verifier = secrets.token_urlsafe(48)
        self._used = False

    def __repr__(self):
        return "OAuthSession(secrets=<redacted>)"

    @property
    def authorize_url(self):
        return USER_ORIGIN + "/oauth/authorize?" + urlencode({
            "client_id": self.config.client_id, "response_type": "code", "scope": self.config.scope,
            "redirect_uri": self.config.redirect_uri, "state": self._state,
            "code_challenge_method": "S256", "code_challenge": pkce_challenge(self._verifier),
        })

    def consume_callback(self, url):
        try:
            parsed, expected = urlsplit(url), urlsplit(self.config.redirect_uri)
            if self._used or not self.created_at <= self.clock() < self.expires_at:
                raise ValueError()
            if (parsed.scheme, parsed.netloc, parsed.path, parsed.fragment) != (expected.scheme, expected.netloc, expected.path, ""):
                raise ValueError()
            pairs = parse_qsl(parsed.query, keep_blank_values=True, strict_parsing=True)
            params = dict(pairs)
            if len(params) != len(pairs) or set(params) not in ({"code", "state"}, {"error", "state"}):
                raise ValueError()
            if not secrets.compare_digest(params["state"], self._state):
                raise ValueError()
            if "error" in params and params["error"] != "access_denied":
                raise ValueError()
            if "code" in params and not _secret(params["code"]):
                raise ValueError()
        except (AttributeError, TypeError, ValueError):
            raise NexusError("authorization") from None
        self._used = True
        if "error" in params:
            raise NexusError("authorization_cancelled")
        return params["code"]

    def to_record(self):
        """Contains secrets; callers must use private storage, never UI output."""
        return {"config": asdict(self.config), "state": self._state, "verifier": self._verifier,
                "created_at": self.created_at, "expires_at": self.expires_at, "used": self._used}

    @classmethod
    def from_record(cls, record, *, clock=time.time):
        try:
            if not isinstance(record, dict) or set(record) != {"config", "state", "verifier", "created_at", "expires_at", "used"}:
                raise ValueError()
            config = OAuthConfig(**record["config"])
            pkce_challenge(record["verifier"])
            if not isinstance(record["state"], str) or not re.fullmatch(r"[A-Za-z0-9_-]{43}", record["state"]):
                raise ValueError()
            start, end = record["created_at"], record["expires_at"]
            if type(start) not in (int, float) or type(end) not in (int, float) or not math.isfinite(start) or not 0 < end - start <= 3600 or type(record["used"]) is not bool:
                raise ValueError()
            session = cls(config, clock=clock)
            session.created_at, session.expires_at = start, end
            session._state, session._verifier, session._used = record["state"], record["verifier"], record["used"]
            return session
        except (NexusError, TypeError, ValueError):
            raise NexusError("authorization") from None


def _token_request(config, payload, transport, clock):
    response = (transport or HTTPSTransport()).request("POST", USER_ORIGIN + "/oauth/token",
        headers={"Content-Type": "application/x-www-form-urlencoded", "Accept": "application/json"},
        body=urlencode(payload).encode("ascii"))
    record = checked_json(response)
    if not isinstance(record, dict) or not _secret(record.get("refresh_token")) or not isinstance(record.get("token_type"), str) or record["token_type"].lower() != "bearer" or type(record.get("expires_in")) not in (int, float) or not 0 < record["expires_in"] <= 31536000:
        raise NexusError("payload")
    try:
        return Credentials(access_token=record.get("access_token"), refresh_token=record.get("refresh_token"), expires_at=clock() + record["expires_in"])
    except NexusError:
        raise NexusError("payload") from None


def exchange_code(session, callback_url, transport=None):
    if not isinstance(session, OAuthSession):
        raise NexusError("authorization")
    code = session.consume_callback(callback_url)
    return _token_request(session.config, {"grant_type":"authorization_code", "redirect_uri":session.config.redirect_uri,
        "client_id":session.config.client_id, "code":code, "code_verifier":session._verifier}, transport, session.clock)


def refresh_credentials(config, credentials, transport=None, *, clock=time.time):
    if not isinstance(config, OAuthConfig) or not isinstance(credentials, Credentials) or not credentials.access_token or not credentials.refresh_token:
        raise NexusError("credentials")
    return _token_request(config, {"grant_type":"refresh_token", "client_id":config.client_id,
                                  "refresh_token":credentials.refresh_token}, transport, clock)
