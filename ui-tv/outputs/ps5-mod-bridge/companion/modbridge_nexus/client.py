"""One REST metadata/download adapter for all ordinary Nexus game domains."""
import re
import time
from urllib.parse import urlencode

from .credentials import Credentials
from .errors import NexusError
from .records import DownloadLink, DownloadSource, UserProfile, game_domain, positive_id
from .transport import API_ORIGIN, USER_ORIGIN, HTTPSTransport, checked_json


def _text(value, maximum=65536):
    if not isinstance(value, str) or len(value) > maximum or any(ord(c) < 32 and c not in "\n\r\t" for c in value):
        raise NexusError("payload")
    return value


def _list(value):
    if not isinstance(value, list) or len(value) > 10000:
        raise NexusError("payload")
    return value


def _identifier(value):
    try:
        return positive_id(value)
    except NexusError:
        raise NexusError("payload") from None


class NexusClient:
    def __init__(self, credentials=None, transport=None, *, account=None, clock=time.time, app_name="PS5ModBridge", app_version="0.1.0"):
        if account is None and not isinstance(credentials, Credentials):
            raise NexusError("credentials")
        if not isinstance(app_name, str) or not re.fullmatch(r"[A-Za-z0-9 _.()-]{1,64}", app_name) or not isinstance(app_version, str) or not re.fullmatch(r"[A-Za-z0-9_.+-]{1,32}", app_version):
            raise NexusError("configuration")
        self.account = account
        self.clock = clock
        self.credentials = credentials
        self.transport = transport or HTTPSTransport()
        self.app_name, self.app_version = app_name, app_version
        self.rate_limits = {}

    def __repr__(self):
        return "NexusClient(credentials=<redacted>)"

    def _current_credentials(self, force=False):
        if self.account is not None:
            self.credentials = self.account.credentials(force=force)
        elif self.credentials.access_token and self.credentials.expires_at is not None and self.credentials.expires_at <= self.clock():
            raise NexusError("reconnect_required")
        return self.credentials

    def _request(self, path, *, user_service=False):
        for attempt in range(2):
            credentials = self._current_credentials(force=bool(attempt))
            headers = {"Accept":"application/json", "Application-Name":self.app_name, "Application-Version":self.app_version}
            if credentials.api_key:
                headers["apikey"] = credentials.api_key
            else:
                headers["Authorization"] = "Bearer " + credentials.access_token
            origin = USER_ORIGIN if user_service else API_ORIGIN + "/v1"
            response = self.transport.request("GET", origin + path, headers=headers)
            if response.status != 401 or self.account is None or attempt:
                break
        if hasattr(response, "headers") and isinstance(response.headers, dict):
            normalized = {str(k).lower(): v for k, v in response.headers.items()}
            for prefix in ("daily", "hourly"):
                for suffix in ("limit", "remaining", "reset"):
                    value = normalized.get(f"x-rl-{prefix}-{suffix}")
                    if isinstance(value, str) and value.isdigit() and len(value) <= 12:
                        self.rate_limits[f"{prefix}_{suffix}"] = int(value)
        return checked_json(response)

    def user(self):
        if self._current_credentials().api_key:
            record = self._request("/users/validate")
            if not isinstance(record, dict) or type(record.get("is_premium")) is not bool or type(record.get("is_supporter")) is not bool:
                raise NexusError("payload")
            return UserProfile(_identifier(record.get("user_id")), _text(record.get("name"), 256), record["is_premium"], record["is_supporter"])
        record = self._request("/oauth/userinfo", user_service=True)
        if not isinstance(record, dict) or not isinstance(record.get("sub"), str) or not re.fullmatch(r"[0-9]{1,19}", record["sub"]):
            raise NexusError("payload")
        roles = _list(record.get("membership_roles"))
        if len(roles) > 100 or any(not isinstance(role, str) or len(role) > 128 for role in roles):
            raise NexusError("payload")
        preferences = record.get("preferences")
        adult = preferences.get("adult") if isinstance(preferences, dict) else None
        adult = adult if type(adult) is bool else None
        return UserProfile(_identifier(int(record["sub"])), _text(record.get("name"), 256), "premium" in roles, "supporter" in roles, adult)

    def games(self):
        games = []
        for record in _list(self._request("/games")):
            if not isinstance(record, dict):
                raise NexusError("payload")
            try:
                domain = game_domain(record.get("domain_name"))
            except NexusError:
                raise NexusError("payload") from None
            item = {"id":_identifier(record.get("id")), "domain_name":domain,
                    "name":_text(record.get("name"), 256), "source_url":f"https://www.nexusmods.com/{domain}"}
            if "mods" in record:
                if type(record["mods"]) is not int or record["mods"] < 0:
                    raise NexusError("payload")
                item["mods"] = record["mods"]
            games.append(item)
        return games

    def _mod_record(self, record, domain, expected_id=None, *, profile=None):
        if not isinstance(record, dict):
            raise NexusError("payload")
        mod_id, game_id = _identifier(record.get("mod_id")), _identifier(record.get("game_id"))
        if record.get("domain_name") != domain or expected_id is not None and mod_id != expected_id:
            raise NexusError("payload")
        adult = record.get("contains_adult_content")
        allow_adult = isinstance(profile, UserProfile) and profile.adult_content is True
        if (type(adult) is not bool or (adult and not allow_adult)
                or record.get("available") is not True or record.get("status") != "published"):
            return None
        return {"mod_id":mod_id, "game_id":game_id, "game_domain":domain,
                "name":_text(record.get("name"), 1024), "author":_text(record.get("author"), 1024),
                "summary":_text(record.get("summary", "")), "description":_text(record.get("description", "")),
                "version":_text(record.get("version", ""), 256), "contains_adult_content":adult,
                "source_url":f"https://www.nexusmods.com/{domain}/mods/{mod_id}"}

    def _content_profile(self, records):
        # Call only after validating record identities. Preferences are scoped
        # to this operation; API-key validation does not expose a preference.
        if any(record.get("contains_adult_content") is True and record.get("available") is True
               and record.get("status") == "published" for record in records):
            if not self._current_credentials().api_key:
                return self.user()
        return None

    def latest_mods(self, game):
        domain = game_domain(game)
        records = _list(self._request(f"/games/{domain}/mods/latest_added"))
        mods = [self._mod_record(record, domain) for record in records]
        profile = self._content_profile(records)
        if profile is not None and profile.adult_content is True:
            mods = [self._mod_record(record, domain, profile=profile) for record in records]
        return [record for record in mods if record is not None]

    def _fetch_mod(self, domain, mod_id, *, profile=None):
        raw = self._request(f"/games/{domain}/mods/{mod_id}")
        record = self._mod_record(raw, domain, mod_id, profile=profile)
        if record is None and profile is None:
            profile = self._content_profile((raw,))
            if profile is not None:
                record = self._mod_record(raw, domain, mod_id, profile=profile)
        if record is None:
            raise NexusError("content_hidden")
        return record

    def mod(self, game, mod_id):
        return self._fetch_mod(game_domain(game), positive_id(mod_id))

    def files(self, game, mod_id):
        domain, mod_id = game_domain(game), positive_id(mod_id)
        self.mod(domain, mod_id)
        record = self._request(f"/games/{domain}/mods/{mod_id}/files")
        if not isinstance(record, dict):
            raise NexusError("payload")
        files = []
        for file in _list(record.get("files")):
            if not isinstance(file, dict):
                raise NexusError("payload")
            file_id = _identifier(file.get("file_id"))
            size = file.get("size_kb")
            if type(size) not in (int, float) or not 0 <= size <= 1e12:
                raise NexusError("payload")
            files.append({"file_id":file_id, "name":_text(file.get("name"), 1024),
                "file_name":_text(file.get("file_name"), 1024), "version":_text(file.get("version"), 256),
                "description":_text(file.get("description", "")), "size_kb":size,
                "source_url":DownloadSource(domain, mod_id, file_id).source_url})
        return files

    def download_links(self, game, mod_id, file_id, nxm=None):
        domain, mod_id, file_id = game_domain(game), positive_id(mod_id), positive_id(file_id)
        user = self.user()  # Live provider identity/entitlement, never local JWT claims or a premium flag.
        query = ""
        if nxm is not None:
            from .downloads import NXMAuthorization, parse_nxm
            authorization = parse_nxm(nxm) if isinstance(nxm, str) else nxm
            if not isinstance(authorization, NXMAuthorization):
                raise NexusError("download_authorization")
            authorization.validate(user.user_id, domain, mod_id, file_id)
            query = "?" + urlencode({"key":authorization.key, "expires":authorization.expires})
        elif not user.is_premium:
            raise NexusError("download_authorization")
        self._fetch_mod(domain, mod_id, profile=user)
        source = DownloadSource(domain, mod_id, file_id)
        records = _list(self._request(f"/games/{domain}/mods/{mod_id}/files/{file_id}/download_link" + query))
        if not records or len(records) > 32:
            raise NexusError("payload")
        links = []
        for record in records:
            if not isinstance(record, dict):
                raise NexusError("payload")
            try:
                links.append(DownloadLink(record.get("URI"), _text(record.get("name"), 256), _text(record.get("short_name"), 64), source))
            except NexusError:
                raise NexusError("payload") from None
        return links
