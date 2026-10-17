"""Nexus provider protocol adapter; download is not console compatibility."""
from .credentials import Credentials, CredentialStore, load_private_json, save_private_json
from .errors import NexusError
from .transport import HTTPSTransport, Response
from .auth import OAuthConfig, OAuthSession, exchange_code, refresh_credentials, pkce_challenge
from .records import DownloadLink, DownloadSource, UserProfile
from .client import NexusClient
from .downloads import NXMAuthorization, DownloadResult, parse_nxm, download_to_quarantine

__all__ = ["Credentials", "CredentialStore", "NexusError", "HTTPSTransport", "Response", "load_private_json", "save_private_json"]
__all__ += ["OAuthConfig", "OAuthSession", "exchange_code", "refresh_credentials", "pkce_challenge"]
__all__ += ["DownloadLink", "DownloadSource", "UserProfile", "NexusClient"]
__all__ += ["NXMAuthorization", "DownloadResult", "parse_nxm", "download_to_quarantine"]

from .account import AccountManager
__all__ += ["AccountManager"]
