"""Safe provider-facing records; no target compatibility state."""
from dataclasses import dataclass, field
import re

from .errors import NexusError
from .transport import https_url


def game_domain(value):
    if not isinstance(value, str) or not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,79}", value):
        raise NexusError("invalid_input")
    return value


def positive_id(value):
    if type(value) is not int or not 0 < value <= 9223372036854775807:
        raise NexusError("invalid_input")
    return value


@dataclass(frozen=True)
class UserProfile:
    user_id: int
    display_name: str
    is_premium: bool
    is_supporter: bool
    adult_content: bool = None


@dataclass(frozen=True)
class DownloadSource:
    game_domain: str
    mod_id: int
    file_id: int

    def __post_init__(self):
        game_domain(self.game_domain)
        positive_id(self.mod_id)
        positive_id(self.file_id)

    @property
    def source_url(self):
        return f"https://www.nexusmods.com/{self.game_domain}/mods/{self.mod_id}?tab=files&file_id={self.file_id}"


@dataclass(frozen=True)
class DownloadLink:
    uri: str = field(repr=False)
    name: str
    short_name: str
    source: DownloadSource

    def __post_init__(self):
        https_url(self.uri)
        if not isinstance(self.source, DownloadSource):
            raise NexusError("invalid_input")
