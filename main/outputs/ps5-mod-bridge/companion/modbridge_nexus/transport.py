"""Bounded HTTPS transport. http.client never follows redirects."""
from dataclasses import dataclass
import http.client
import json
import math
import socket
import ssl
import time
from urllib.parse import urlsplit

from .errors import NexusError

API_ORIGIN = "https://api.nexusmods.com"
USER_ORIGIN = "https://users.nexusmods.com"
PROVIDER_HOSTS = {"api.nexusmods.com", "users.nexusmods.com"}


@dataclass(frozen=True, repr=False)
class Response:
    status: int
    headers: dict
    body: bytes

    def __repr__(self):
        return f"Response(status={self.status!r}, body=<redacted>)"


def checked_json(response, *, max_bytes=1048576):
    if not isinstance(response, Response) or type(response.status) is not int:
        raise NexusError("payload")
    headers = {str(k).lower(): v for k, v in response.headers.items()}
    if response.status == 429:
        retry = headers.get("retry-after")
        try:
            retry = int(retry) if retry is not None else None
        except (ValueError, TypeError):
            retry = None
        raise NexusError("rate_limit", status=429, retry_after=retry)
    if 300 <= response.status <= 399:
        raise NexusError("redirect", status=response.status)
    if not 200 <= response.status <= 299:
        raise NexusError("provider", status=response.status)
    if not isinstance(response.body, bytes):
        raise NexusError("payload")
    if len(response.body) > max_bytes:
        raise NexusError("size_limit")
    def object_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError()
            result[key] = value
        return result
    try:
        return json.loads(response.body, object_pairs_hook=object_pairs,
                          parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
    except (UnicodeError, ValueError, RecursionError):
        raise NexusError("payload") from None


def https_url(url, *, provider=False):
    try:
        if not isinstance(url, str) or len(url) > 16384 or any(ord(c) <= 32 or ord(c) == 127 for c in url):
            raise ValueError()
        parsed = urlsplit(url)
        if parsed.scheme != "https" or not parsed.hostname or parsed.username is not None or parsed.password is not None or parsed.port not in (None, 443) or parsed.fragment:
            raise ValueError()
        host = parsed.hostname
        if provider and host not in PROVIDER_HOSTS:
            raise ValueError()
        if "\\" in url or not host.isascii():
            raise ValueError()
        return parsed
    except (TypeError, ValueError):
        raise NexusError("invalid_input") from None


def _positive_time(value):
    if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
        raise NexusError("configuration")
    return float(value)


def _remaining(connection, deadline):
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise NexusError("timeout")
    if connection.sock is not None:
        connection.sock.settimeout(remaining)
    return remaining


def _length(headers):
    value = headers.get("content-length")
    if value is None:
        return None
    try:
        if not value.isdigit():
            raise ValueError()
        return int(value)
    except (AttributeError, ValueError):
        raise NexusError("payload") from None


class _DeadlineReader:
    """Bound every byte of HTTP line parsing, including status and chunk headers."""
    def __init__(self, stream, sock, deadline):
        self.stream, self.sock, self.deadline = stream, sock, deadline

    def __getattr__(self, name):
        return getattr(self.stream, name)

    def _check(self):
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise NexusError("timeout")
        self.sock.settimeout(remaining)

    def read1(self, amount=-1):
        self._check()
        data = self.stream.read1(amount)
        self._check()
        return data

    def read(self, amount=-1):
        # http.client uses read(2) for chunk terminators. Retain the same total
        # deadline even if the connection has detached its close-delimited socket.
        chunks = bytearray()
        while amount < 0 or len(chunks) < amount:
            data = self.read1(65536 if amount < 0 else amount - len(chunks))
            if not data:
                break
            chunks.extend(data)
        return bytes(chunks)

    def readline(self, limit=-1):
        chunks = bytearray()
        while limit < 0 or len(chunks) < limit:
            self._check()
            byte = self.stream.read1(1)
            self._check()  # A late final newline or EOF cannot finish successfully.
            if not byte:
                break
            chunks.extend(byte)
            if byte == b"\n":
                break
        return bytes(chunks)


def _deadline_response_class(deadline):
    class DeadlineResponse(http.client.HTTPResponse):
        def __init__(self, sock, *args, **kwargs):
            super().__init__(sock, *args, **kwargs)
            self.fp = _DeadlineReader(self.fp, sock, deadline)
    return DeadlineResponse


class _DownloadStream:
    def __init__(self, connection, response, deadline):
        self._connection = connection
        self._response = response
        self._deadline = deadline
        self.status = response.status
        self.headers = {key.lower(): value for key, value in response.getheaders()}

    def read(self, amount):
        remaining = self._deadline - time.monotonic()
        if remaining <= 0:
            raise NexusError("timeout")
        if self._connection.sock is not None:
            self._connection.sock.settimeout(remaining)
        try:
            data = self._response.read1(amount)
            if time.monotonic() >= self._deadline:
                raise NexusError("timeout")
            return data
        except (TimeoutError, socket.timeout):
            raise NexusError("timeout") from None
        except (OSError, http.client.HTTPException):
            raise NexusError("network") from None

    def close(self):
        self._response.close()
        self._connection.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


class HTTPSTransport:
    def __init__(self, *, timeout=30.0, max_bytes=1048576, context=None):
        self.timeout = _positive_time(timeout)
        if type(max_bytes) is not int or not 0 < max_bytes <= 16777216:
            raise NexusError("configuration")
        self.max_bytes = max_bytes
        self.context = context or ssl.create_default_context()
        if not isinstance(self.context, ssl.SSLContext) or not self.context.check_hostname or self.context.verify_mode != ssl.CERT_REQUIRED:
            raise NexusError("configuration")

    def request(self, method, url, *, headers=None, body=None, timeout=None, max_bytes=None):
        parsed = https_url(url, provider=True)
        if method not in ("GET", "POST") or (body is not None and not isinstance(body, bytes)):
            raise NexusError("invalid_input")
        headers = dict(headers or {})
        if any(not isinstance(k, str) or not isinstance(v, str) or "\r" in k + v or "\n" in k + v for k, v in headers.items()):
            raise NexusError("invalid_input")
        if any(k.lower() in {"host", "cookie", "proxy-authorization"} for k in headers):
            raise NexusError("invalid_input")
        limit = self.max_bytes if max_bytes is None else max_bytes
        if type(limit) is not int or not 0 < limit <= self.max_bytes:
            raise NexusError("configuration")
        duration = self.timeout if timeout is None else min(_positive_time(timeout), self.timeout)
        deadline = time.monotonic() + duration
        connection = http.client.HTTPSConnection(parsed.hostname, timeout=duration, context=self.context)
        connection.response_class = _deadline_response_class(deadline)
        response = None
        try:
            target = parsed.path or "/"
            if parsed.query:
                target += "?" + parsed.query
            connection.connect()
            _remaining(connection, deadline)
            connection.request(method, target, body=body, headers=headers)
            _remaining(connection, deadline)
            response = connection.getresponse()
            response_headers = {k.lower(): v for k, v in response.getheaders()}
            length = _length(response_headers)
            if length is not None and length > limit:
                raise NexusError("size_limit")
            chunks, total = [], 0
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise NexusError("timeout")
                if connection.sock is not None:
                    connection.sock.settimeout(remaining)
                data = response.read1(min(65536, limit - total + 1))
                if time.monotonic() >= deadline:
                    raise NexusError("timeout")
                total += len(data)
                if total > limit:
                    raise NexusError("size_limit")
                if not data:
                    break
                chunks.append(data)
            if length is not None and length != total:
                raise NexusError("truncated")
            return Response(response.status, response_headers, b"".join(chunks))
        except (TimeoutError, socket.timeout):
            raise NexusError("timeout") from None
        except (OSError, http.client.HTTPException):
            raise NexusError("network") from None
        finally:
            if response is not None:
                response.close()
            connection.close()

    def open_download(self, url, *, timeout=300.0):
        parsed = https_url(url)
        duration = _positive_time(timeout)
        deadline = time.monotonic() + duration
        connection = http.client.HTTPSConnection(parsed.hostname, timeout=duration, context=self.context)
        connection.response_class = _deadline_response_class(deadline)
        response = None
        delivered = False
        try:
            target = parsed.path or "/"
            if parsed.query:
                target += "?" + parsed.query
            # Explicitly no API credentials, cookies, or configurable headers here.
            connection.connect()
            _remaining(connection, deadline)
            connection.request("GET", target, headers={"Accept": "application/octet-stream", "Accept-Encoding": "identity"})
            _remaining(connection, deadline)
            response = connection.getresponse()
            if time.monotonic() >= deadline:
                raise NexusError("timeout")
            stream = _DownloadStream(connection, response, deadline)
            delivered = True
            return stream
        except (TimeoutError, socket.timeout):
            raise NexusError("timeout") from None
        except (OSError, http.client.HTTPException):
            raise NexusError("network") from None
        finally:
            if not delivered:
                if response is not None:
                    response.close()
                connection.close()
