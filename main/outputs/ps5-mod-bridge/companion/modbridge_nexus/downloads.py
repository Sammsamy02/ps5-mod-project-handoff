"""Account-bound NXM authorization and bounded, unpublished quarantine streams."""
from dataclasses import dataclass, field
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import tempfile
import time
from urllib.parse import parse_qsl, urlsplit

from .credentials import _secret, checked_path
from .errors import NexusError
from .records import DownloadLink, DownloadSource, game_domain, positive_id
from .transport import HTTPSTransport, _length


@dataclass(frozen=True)
class NXMAuthorization:
    game_domain: str
    mod_id: int
    file_id: int
    key: str = field(repr=False)
    expires: int
    user_id: int

    def __post_init__(self):
        try:
            game_domain(self.game_domain)
            for value in (self.mod_id, self.file_id, self.user_id, self.expires):
                positive_id(value)
            if not _secret(self.key) or len(self.key) > 1024:
                raise NexusError("invalid_input")
        except NexusError:
            raise NexusError("download_authorization") from None

    def validate(self, account_id, game, mod_id, file_id, *, now=None):
        moment = time.time() if now is None else now
        if (account_id, game, mod_id, file_id) != (self.user_id, self.game_domain, self.mod_id, self.file_id) or type(moment) not in (int, float) or not math.isfinite(moment) or self.expires <= moment:
            raise NexusError("download_authorization")


def parse_nxm(url):
    try:
        if not isinstance(url, str) or not url.startswith("nxm://") or len(url) > 8192 or any(ord(c) <= 32 or ord(c) >= 127 for c in url):
            raise ValueError()
        parsed = urlsplit(url)
        domain = game_domain(parsed.netloc)
        match = re.fullmatch(r"/mods/([1-9][0-9]{0,18})/files/([1-9][0-9]{0,18})", parsed.path)
        if not match or parsed.fragment:
            raise ValueError()
        pairs = parse_qsl(parsed.query, keep_blank_values=True, strict_parsing=True)
        params = dict(pairs)
        if len(pairs) != len(params) or set(params) != {"key", "expires", "user_id"}:
            raise ValueError()
        if not re.fullmatch(r"[1-9][0-9]{0,18}", params["expires"]) or not re.fullmatch(r"[1-9][0-9]{0,18}", params["user_id"]):
            raise ValueError()
        return NXMAuthorization(domain, int(match[1]), int(match[2]), params["key"], int(params["expires"]), int(params["user_id"]))
    except (NexusError, TypeError, ValueError):
        raise NexusError("download_authorization") from None


@dataclass(frozen=True)
class DownloadResult:
    path: Path
    sha256: str
    bytes_downloaded: int
    source: DownloadSource
    stage: str = field(default="downloaded", init=False)

    @property
    def provenance_path(self):
        return self.path.with_name(self.path.name + ".provenance.json")


def _quarantine_folder(destination):
    path = checked_path(destination)
    try:
        # New folders are private; an existing shared directory is never chmodded.
        path.mkdir(mode=0o700, parents=True, exist_ok=True)
        checked_path(path)
        info = path.stat()
        if not stat.S_ISDIR(info.st_mode) or info.st_mode & 0o077 or info.st_uid != os.getuid():
            raise NexusError("destination")
        return path
    except OSError:
        raise NexusError("destination") from None


def download_to_quarantine(link, destination, source=None, *, transport=None, max_bytes=1073741824, timeout=300.0):
    if not isinstance(link, DownloadLink) or source is not None and source != link.source:
        raise NexusError("invalid_input")
    source = link.source
    if type(max_bytes) is not int or not 0 < max_bytes <= 1099511627776 or type(timeout) not in (int, float) or not math.isfinite(timeout) or timeout <= 0:
        raise NexusError("configuration")
    deadline = time.monotonic() + timeout
    folder = _quarantine_folder(destination)
    temporary, metadata_temporary, published = None, None, None
    try:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise NexusError("timeout")
        with (transport or HTTPSTransport()).open_download(link.uri, timeout=remaining) as stream:
            headers = {str(k).lower(): v for k, v in stream.headers.items()}
            if 300 <= stream.status <= 399:
                raise NexusError("redirect", status=stream.status)
            if stream.status == 429:
                value = headers.get("retry-after")
                hint = int(value) if isinstance(value, str) and value.isdigit() else None
                raise NexusError("rate_limit", status=429, retry_after=hint)
            if stream.status != 200:
                raise NexusError("provider", status=stream.status)
            if headers.get("content-encoding", "identity").lower() != "identity":
                raise NexusError("payload")
            expected = _length(headers)
            if expected is not None and expected > max_bytes:
                raise NexusError("size_limit")
            fd, temporary = tempfile.mkstemp(prefix=".partial-", dir=folder)
            total, digest = 0, hashlib.sha256()
            with os.fdopen(fd, "wb") as output:
                os.fchmod(output.fileno(), 0o600)
                while True:
                    if time.monotonic() >= deadline:
                        raise NexusError("timeout")
                    try:
                        data = stream.read(min(65536, max_bytes - total + 1))
                    except TimeoutError:
                        raise NexusError("timeout") from None
                    except OSError:
                        raise NexusError("network") from None
                    if time.monotonic() >= deadline:
                        raise NexusError("timeout")
                    if not isinstance(data, bytes):
                        raise NexusError("payload")
                    total += len(data)
                    if total > max_bytes:
                        raise NexusError("size_limit")
                    if not data:
                        break
                    output.write(data)
                    digest.update(data)
                if expected is not None and total != expected:
                    raise NexusError("truncated")
                output.flush()
                os.fsync(output.fileno())
        sha256 = digest.hexdigest()
        final = folder / f"{source.game_domain}-{source.mod_id}-{source.file_id}-{sha256}.archive"
        metadata = final.with_name(final.name + ".provenance.json")
        checked_path(final)
        checked_path(metadata)
        if final.exists() or metadata.exists():
            raise NexusError("destination")
        record = {"provider":"nexusmods", "game_domain":source.game_domain, "mod_id":source.mod_id,
                  "file_id":source.file_id, "source_url":source.source_url, "sha256":sha256,
                  "bytes_downloaded":total, "stage":"downloaded"}
        fd, metadata_temporary = tempfile.mkstemp(prefix=".provenance-", dir=folder)
        with os.fdopen(fd, "wb") as output:
            os.fchmod(output.fileno(), 0o600)
            output.write(json.dumps(record, sort_keys=True, separators=(",", ":")).encode("utf-8"))
            output.flush()
            os.fsync(output.fileno())
        if time.monotonic() >= deadline:
            raise NexusError("timeout")
        # Hard-link publication is atomic and never replaces an existing destination.
        os.link(temporary, final)
        published = final
        os.link(metadata_temporary, metadata)
        published = None
        return DownloadResult(final, sha256, total, source)
    except (TimeoutError,):
        raise NexusError("timeout") from None
    except OSError:
        raise NexusError("network" if temporary is None else "destination") from None
    finally:
        for path in (temporary, metadata_temporary, published):
            if path is not None:
                try:
                    os.unlink(path)
                except OSError:
                    pass
