"""Owned HTTP/UI fixture with actual core analysis; no provider or device traffic."""
import hashlib
import json
import os
from pathlib import Path
import signal
import sys
import tempfile
import time
from urllib.parse import urlsplit
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "companion"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import modbridge_nexus as nexus
from modbridge_host import BridgeApplication, create_server
from host_tests import FakeAccount
from fake_profilegen import tiny_elf


class OwnedAccount(FakeAccount):
    def __init__(self):
        super().__init__()
        self.requirement_calls = 0
        self.search_calls = 0
        self.browse_calls = 0

    def status(self):
        time.sleep(float(os.environ.get("FIXTURE_ACCOUNT_DELAY", "0")))
        record = super().status()
        # FIXTURE_MODE=free: the account that needs a website link for every download also reports itself as free.
        if os.environ.get("FIXTURE_MODE") == "free":
            record["account"] = {**record["account"], "is_premium": False}
        return record

    def games(self):
        return [{"domain_name": "ownedgame", "name": "Owned Game Fixture", "id": 1}]

    def latest_mods(self, game):
        return [{"mod_id": 1, "name": "Owned Archive Fixture", "summary": "Local test data", "author": "Fixture"}]

    def files_and_updates(self, game, mod):
        # What the real client returns from one files.json fetch: file 2 (the one the UI downloads) links to the newer
        # file 3, which has no link out of it. FIXTURE_MODE=update-fails makes the provider fail instead, so the page
        # must show "Could not check for updates" with the catalog text rather than any verdict.
        if os.environ.get("FIXTURE_MODE") == "update-fails":
            raise nexus.NexusError("rate_limit")
        return {"files": [{"file_id": 2, "name": "Owned file", "version": "1", "category_name": "MAIN", "uploaded_timestamp": 1700000000},
                          {"file_id": 3, "name": "Owned file <b>update</b>", "version": "2.0", "category_name": "UPDATE", "uploaded_timestamp": 1710000000}],
                "file_updates": [{"old_file_id": 2, "new_file_id": 3, "old_file_name": "Owned file", "new_file_name": "Owned file update",
                                  "uploaded_timestamp": 1710000000, "uploaded_time": "2024-03-09T16:00:00.000+00:00"}]}

    def requirements(self, game, mod):
        # What the real client returns (NexusClient.requirements): the decoded record of what the author listed, every row
        # "unresolved". One Nexus mod, one outside-Nexus row and one DLC, as the details sheet must tell apart.
        # FIXTURE_MODE=requirements-fails: Nexus is rate limiting, every time. FIXTURE_MODE=requirements-flaky: only the first time,
        # so Retry gets a list. The host never caches a failure.
        self.requirement_calls += 1
        mode = os.environ.get("FIXTURE_MODE")
        if mode == "requirements-fails" or mode == "requirements-flaky" and self.requirement_calls == 1:
            raise nexus.NexusError("rate_limit")
        return {"game": game, "mod_id": mod, "discovery_complete": True,
                "nexus_requirements": [
                    {"id": "relation-1", "external_requirement": False, "game": game, "mod_id": "5", "name": "Fixture Framework",
                     "notes": "Install it before this mod.", "url": f"https://www.nexusmods.com/{game}/mods/5", "satisfaction": "unresolved"},
                    {"id": "relation-2", "external_requirement": True, "game": "External", "mod_id": "v1", "name": "Fixture Script Runtime",
                     "notes": "", "url": "https://example.com/fixture-runtime", "satisfaction": "unresolved"}],
                "dlc_requirements": [{"game": game, "id": "dlc-1", "name": "Fixture Expansion", "notes": "Story pack", "satisfaction": "unresolved"}],
                "file_version_constraints_provided": False, "installation_supported": False}

    def search_mods(self, game, text, page):
        # What NexusClient.search_mods returns: {"items", "total", "page", "pages"}, each item a latest-mods record (the full ten-field
        # record the real client emits; a search hit carries the summary only, so `description` is ""). The host has already judged
        # `text` (2 to 100 characters, trimmed, spaces collapsed) and `page` (1 to 50) before it asks.
        # FIXTURE_MODE=search-fails: Nexus is rate limiting, every time. FIXTURE_MODE=search-flaky: only the first time, so Retry gets results.
        # By text: "empty" matches nothing, "hidden" matches mods this app does not list, "paged" matches 45 mods (three pages of
        # two listed ones each), anything else matches the fixture's own mod and a hostile-looking one.
        self.search_calls += 1
        mode = os.environ.get("FIXTURE_MODE")
        if mode == "search-fails" or mode == "search-flaky" and self.search_calls == 1:
            raise nexus.NexusError("rate_limit")

        def record(mod_id, name):
            return {"mod_id": mod_id, "game_id": 1, "game_domain": game, "name": name, "author": "Fixture", "summary": f"Summary of {name}",
                    "description": "", "version": "1.0", "contains_adult_content": False,
                    "source_url": f"https://www.nexusmods.com/{game}/mods/{mod_id}"}

        if "empty" in text:
            return {"items": [], "total": 0, "page": page, "pages": 0}
        if "hidden" in text:
            return {"items": [], "total": 5, "page": page, "pages": 1}
        if "paged" in text:
            first = 100 * page
            items = [record(first + n, f"Paged result {first + n}") for n in (1, 2)] if page <= 3 else []
            return {"items": items, "total": 45, "page": page, "pages": 3}
        return {"items": [record(1, "Owned Archive Fixture"), record(2, "Hostile <b>Owned</b> result")], "total": 2, "page": page, "pages": 1}

    BROWSE_LABELS = {"downloads": "Most downloaded", "endorsements": "Most endorsed", "updated": "Recently updated", "name": "Name A-Z"}

    def browse_mods(self, game, sort, page):
        # What NexusClient.browse_mods returns: {"items", "total", "page", "pages", "sort"}, each item a latest-mods record. The host has
        # already judged `sort` (one of four keys) and `page` (1 to 50). 45 mods are counted (three pages) and two are listed per page, so
        # paging and sorting show without twenty cards; the first of the most-downloaded page is the fixture's own mod 1 (so the flows
        # that open "Owned Archive Fixture" work from the default view), every other title names its sort and page, and the second of the
        # name sort is hostile-looking markup. FIXTURE_MODE=browse-fails: Nexus is rate limiting, every time. FIXTURE_MODE=browse-flaky: only
        # the first time, so Retry gets a page. FIXTURE_MODE=browse-empty: Nexus counts no mods.
        self.browse_calls += 1
        mode = os.environ.get("FIXTURE_MODE")
        if mode == "browse-fails" or mode == "browse-flaky" and self.browse_calls == 1:
            raise nexus.NexusError("rate_limit")
        if mode == "browse-empty":
            return {"items": [], "total": 0, "page": page, "pages": 0, "sort": sort}
        order = list(self.BROWSE_LABELS).index(sort)

        def record(mod_id, name):
            return {"mod_id": mod_id, "game_id": 1, "game_domain": game, "name": name, "author": "Fixture", "summary": f"Summary of {name}",
                    "description": "", "version": "1.0", "contains_adult_content": False,
                    "source_url": f"https://www.nexusmods.com/{game}/mods/{mod_id}"}
        items = []
        for number in (1, 2):
            mod_id = 1000 * (order + 1) + 10 * page + number
            name = f"{self.BROWSE_LABELS[sort]} page {page} mod {number}"
            if (sort, page, number) == ("downloads", 1, 1):
                mod_id, name = 1, "Owned Archive Fixture"
            elif (sort, page, number) == ("name", 1, 2):
                name = "Hostile <b>Owned</b> A-Z"
            items.append(record(mod_id, name))
        return {"items": items if page <= 3 else [], "total": 45, "page": page, "pages": 3, "sort": sort}

    def sign_in_with_key(self, key):
        # The key flow of FIXTURE_MODE=key has its own, real account manager (see KeyProvider); the other modes have no such account.
        raise nexus.NexusError("provider", status=401)

    def download_links(self, game, mod, file, nxm=None):
        # FIXTURE_MODE=free: a free account needs a website link first (the job waits for one).
        if os.environ.get("FIXTURE_MODE") == "free" and nxm is None:
            raise nexus.NexusError("download_authorization")
        return super().download_links(game, mod, file)


downloads = []


def report_slowly(progress, total):
    # FIXTURE_MODE=slow: the bytes arrive in four steps with a short pause between them. When FIXTURE_RELEASE names a path, the pause
    # after the second step lasts until that file exists (the test's go-ahead; bounded, so a broken test cannot hang the host),
    # which lets a browser look at a download that is half done without racing the page's own polling.
    release = os.environ.get("FIXTURE_RELEASE")
    for step in range(1, 5):
        progress(total * step // 4, total)
        if step == 2 and release:
            deadline = time.monotonic() + 20
            while not os.path.exists(release) and time.monotonic() < deadline:
                time.sleep(0.05)
        elif step < 4:
            time.sleep(0.2)


def download(link, destination, progress=None):
    # Takes the optional `progress` callback, which the host passes only to a function that accepts it; the default mode never calls it.
    downloads.append(link.source)
    # FIXTURE_MODE=flaky: the first transfer fails, so a job ends failed and must be requested again.
    if os.environ.get("FIXTURE_MODE") == "flaky" and len(downloads) == 1:
        raise nexus.NexusError("network")
    destination.mkdir(mode=0o700, parents=True)
    path = destination / "owned.zip"
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("data/readme.txt", "Owned nonexecuting archive fixture\n")
        # FIXTURE_MODE=lua: a UE4SS script, read (never run) against the bundled Lua runtime manifest.
        if os.environ.get("FIXTURE_MODE") == "lua":
            archive.writestr("Mods/Owned/Scripts/main.lua",
                             "ExecuteInGameThread(function() end)\nRegisterKeyBind(Key.F1, function() end)\n"
                             "RegisterLoadMapPreHook(function() end)\n")
    if progress is not None and os.environ.get("FIXTURE_MODE") == "slow":
        report_slowly(progress, path.stat().st_size)
    return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)


class KeyProvider:
    """FIXTURE_MODE=key: the provider boundary under a REAL AccountManager and NexusClient (no Nexus traffic; nothing leaves this process).

    Only the personal key GOOD_KEY is accepted: GET /v1/users/validate answers it with an identity and anything else with 401. The catalog
    is one game with three published mods, and the v2 GraphQL listing is a fixed set of 45 mods ordered by the sort the query names
    (parsed out of the query text), so the browse route is exercised end to end, decoder included.
    """
    GOOD_KEY = "owned-fixture-key-0123456789"
    SORTS = (("downloads: {direction: DESC}", "downloads"), ("endorsements: {direction: DESC}", "endorsements"),
             ("updatedAt: {direction: DESC}", "updated"), ("name: {direction: ASC}", "name"))

    def __init__(self):
        self.requests = []

    @staticmethod
    def mods():
        # Names are chosen so that the four orders differ: id order, reverse, a rotation and the alphabet.
        return [{"id": 100 + n, "name": f"{chr(65 + n * 7 % 26)} fixture mod {n:02d}"} for n in range(1, 46)]

    def ordered(self, sort):
        mods = self.mods()
        if sort == "endorsements":
            return mods[::-1]
        if sort == "updated":
            return mods[10:] + mods[:10]
        if sort == "name":
            return sorted(mods, key=lambda mod: mod["name"])
        return mods

    def request(self, method, url, headers=None, body=None, **_):
        parsed = urlsplit(url)
        self.requests.append((method, parsed.path))
        headers = headers or {}

        def reply(status, value):
            return nexus.Response(status, {}, json.dumps(value).encode())
        if parsed.path == "/v1/users/validate":
            if headers.get("apikey") != self.GOOD_KEY:
                return reply(401, {"message": "Please provide a valid API Key"})
            return reply(200, {"user_id": 42, "name": "Key fixture", "is_premium": True, "is_supporter": False})
        if headers.get("apikey") != self.GOOD_KEY:
            return reply(401, {})
        if parsed.path == "/v1/games":
            return reply(200, [{"id": 1, "domain_name": "ownedgame", "name": "Owned Game Fixture", "mods": 45}])
        if parsed.path == "/v1/games/ownedgame/mods/latest_added":
            return reply(200, [{"mod_id": 100 + n, "game_id": 1, "domain_name": "ownedgame", "name": f"Newest fixture mod {n}",
                                "summary": "Newest", "description": "", "version": "1", "author": "Fixture", "contains_adult_content": False,
                                "status": "published", "available": True} for n in (1, 2)])
        if method == "POST" and parsed.path == "/v2/graphql":
            request = json.loads(body)
            sort = next(key for text, key in self.SORTS if text in request["query"])
            offset, count = request["variables"]["offset"], request["variables"]["count"]
            nodes = [{"modId": mod["id"], "name": mod["name"], "summary": f"Summary of {mod['name']}", "version": "1", "author": "Fixture",
                      "adultContent": False, "status": "published", "uploader": {"name": "Fixture"},
                      "game": {"id": 1, "domainName": request["variables"]["gameDomainName"]}} for mod in self.ordered(sort)[offset:offset + count]]
            return reply(200, {"data": {"mods": {"nodes": nodes, "nodesCount": len(nodes), "totalCount": 45}}})
        raise AssertionError("Unexpected fixture provider endpoint " + parsed.path)


def profiles_mirror(root):
    """FIXTURE_MODE=profiles: a folder laid out like the console, for the runtime-profile flows (nothing here is a real game).

    A Townfall-like dump whose decrypted executable is a tiny fake ELF, a dump whose eboot.bin is a SELF (encrypted), a dump with no
    executable at all, and a title that only app data lists. Returns the folder.
    """
    def param(title_id, version, name):
        return json.dumps({"titleId": title_id, "contentVersion": version, "masterVersion": "01.00",
                           "localizedParameters": {"defaultLanguage": "en-US", "en-US": {"titleName": name}}})

    def put(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if isinstance(data, bytes) else data.encode())
    console = root / "console"
    town = console / "data" / "homebrew" / "Silent Hill Townfall PPSA33286"
    put(town / "sce_sys" / "param.json", param("PPSA33286", "02.003.000", "SILENT HILL: Townfall"))
    put(town / "decrypted" / "eboot.bin", tiny_elf(b"fixture-townfall-executable"))
    astro = console / "data" / "homebrew" / "LIZARD ASTRO BOT 01.018.000 PPSA21567"
    put(astro / "sce_sys" / "param.json", param("PPSA21567", "01.018.000", "ASTRO BOT"))
    put(astro / "eboot.bin", b"\x4f\x15\x3d\x1d" + bytes(60))
    put(console / "data" / "homebrew" / "Empty dump PPSA00031" / "sce_sys" / "param.json", param("PPSA00031", "1.0", "Dump without an executable"))
    put(console / "user" / "appmeta" / "PPSA00010" / "param.json", param("PPSA00010", "1.0", "Installed only"))
    return console


def stop(*_):
    raise KeyboardInterrupt()


def main():
    signal.signal(signal.SIGTERM, stop)
    # FIXTURE_ROOT names a directory that outlives this process (the private store and the quarantine live in it), so a test can stop the
    # host and start another on the same files; by default everything is a temporary directory removed on exit.
    keep = os.environ.get("FIXTURE_ROOT")
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(keep or temporary).resolve()
        store = root / "credentials.json"
        if os.environ.get("FIXTURE_MODE") == "key":
            account = nexus.AccountManager(nexus.CredentialStore(store), transport=KeyProvider())
        else:
            account = OwnedAccount()
        # FIXTURE_TITLES_ROOT: a folder laid out like the console's /data/homebrew and appmeta paths supplies the title list.
        titles = os.environ.get("FIXTURE_TITLES_ROOT")
        extra, announce = {}, {}
        if os.environ.get("FIXTURE_MODE") == "profiles":
            # The real host over a console-shaped mirror, profiles in <root>/profiles, and tests/fake_profilegen.py in place of the real
            # generator (FIXTURE_GENERATOR names its mode; with `hold`, FIXTURE_RELEASE is the file that lets it finish).
            titles = str(profiles_mirror(root))
            command = [sys.executable, str(Path(__file__).resolve().parent / "fake_profilegen.py"), "--mode", os.environ.get("FIXTURE_GENERATOR", "ok")]
            if os.environ.get("FIXTURE_RELEASE"):
                command += ["--release", os.environ["FIXTURE_RELEASE"]]
            extra = {"runtime_profiles": root / "profiles", "generator_command": command}
            announce = {"console": titles, "profiles": str(root / "profiles")}
        app = BridgeApplication(store, root / "quarantine", account=account, download=download,
                                titles_root=Path(titles) if titles else None, **extra)
        server = create_server(app)
        print(json.dumps({"origin": app.origin, "evidence": "owned_desktop_fixture", **announce}), flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()
            app.close()


if __name__ == "__main__":
    main()
