"""Sequential private-file credentials; no shared developer credentials."""
from dataclasses import asdict, dataclass
import json
import os
from pathlib import Path
import stat
import tempfile

from .errors import NexusError


def _secret(value):
    return isinstance(value, str) and 0 < len(value) <= 16384 and all(32 < ord(c) < 127 for c in value)


@dataclass(frozen=True, repr=False)
class Credentials:
    api_key: str = None
    access_token: str = None
    refresh_token: str = None
    expires_at: float = None

    def __post_init__(self):
        if bool(self.api_key) == bool(self.access_token):
            raise NexusError("credentials")
        if self.api_key is not None and (not _secret(self.api_key) or self.refresh_token is not None or self.expires_at is not None):
            raise NexusError("credentials")
        if self.access_token is not None and not _secret(self.access_token):
            raise NexusError("credentials")
        if self.refresh_token is not None and not _secret(self.refresh_token):
            raise NexusError("credentials")
        if self.expires_at is not None and (type(self.expires_at) not in (int, float) or not 0 < self.expires_at < 1e12):
            raise NexusError("credentials")

    def __repr__(self):
        return "Credentials(api_key=<redacted>)" if self.api_key else "Credentials(oauth=<redacted>)"


def checked_path(path):
    try:
        candidate = Path(path).absolute()
        for part in (candidate, *candidate.parents):
            if part.is_symlink():
                raise NexusError("storage")
        return candidate
    except (OSError, TypeError, ValueError):
        raise NexusError("storage") from None


def _private_parent(path, create=False):
    checked_path(path)
    if create:
        missing = []
        parent = path.parent
        while not parent.exists():
            missing.append(parent)
            parent = parent.parent
        for parent in reversed(missing):
            parent.mkdir(mode=0o700)
    info = path.parent.stat()
    if not stat.S_ISDIR(info.st_mode) or info.st_mode & 0o077 or info.st_uid != os.getuid():
        raise NexusError("storage")


def save_private_json(path, record):
    path = checked_path(path)
    temporary = None
    try:
        _private_parent(path, create=True)
        if path.exists() and not path.is_file():
            raise NexusError("storage")
        data = json.dumps(record, separators=(",", ":"), allow_nan=False).encode("utf-8")
        if len(data) > 65536:
            raise NexusError("storage")
        fd, temporary = tempfile.mkstemp(prefix=".nexus-", dir=path.parent)
        with os.fdopen(fd, "wb") as stream:
            os.fchmod(stream.fileno(), 0o600)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        checked_path(path)
        os.replace(temporary, path)
        temporary = None
    except (OSError, TypeError, ValueError):
        raise NexusError("storage") from None
    finally:
        if temporary:
            try:
                os.unlink(temporary)
            except OSError:
                pass


def load_private_json(path):
    path = checked_path(path)
    try:
        if not path.exists():
            return None
        _private_parent(path)
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
        with os.fdopen(fd, "rb") as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o077 or info.st_uid != os.getuid() or info.st_nlink != 1:
                raise NexusError("storage")
            data = stream.read(65537)
        if len(data) > 65536:
            raise NexusError("storage")
        record = json.loads(data)
        if not isinstance(record, dict):
            raise NexusError("storage")
        return record
    except (OSError, UnicodeError, ValueError):
        raise NexusError("storage") from None


class CredentialStore:
    def __init__(self, path):
        self.path = checked_path(path)

    def save(self, credentials):
        if not isinstance(credentials, Credentials):
            raise NexusError("credentials")
        save_private_json(self.path, asdict(credentials))

    def save_account(self, credentials, config=None):
        from .auth import OAuthConfig
        if not isinstance(credentials, Credentials) or (config is not None and not isinstance(config, OAuthConfig)):
            raise NexusError("credentials")
        if credentials.api_key and config is not None:
            raise NexusError("credentials")
        save_private_json(self.path, {"version": 1, "credentials": asdict(credentials),
                                      "oauth_config": asdict(config) if config else None})

    def load(self):
        return self.load_account()[0]

    def load_account(self):
        from .auth import OAuthConfig
        record = load_private_json(self.path)
        if record is None:
            return None, None
        try:
            config = None
            if "version" in record:
                if set(record) != {"version", "credentials", "oauth_config"} or type(record["version"]) is not int or record["version"] != 1:
                    raise ValueError()
                config = OAuthConfig(**record["oauth_config"]) if record["oauth_config"] is not None else None
                record = record["credentials"]
            if not isinstance(record, dict) or set(record) - {"api_key", "access_token", "refresh_token", "expires_at"}:
                raise ValueError()
            credentials = Credentials(**record)
            if credentials.api_key and config is not None:
                raise ValueError()
            return credentials, config
        except (NexusError, TypeError, ValueError):
            raise NexusError("storage") from None

    def disconnect(self):
        checked_path(self.path)
        try:
            self.path.unlink(missing_ok=True)
        except OSError:
            raise NexusError("storage") from None
