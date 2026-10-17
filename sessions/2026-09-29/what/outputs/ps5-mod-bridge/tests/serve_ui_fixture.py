"""Owned HTTP/UI fixture with actual core analysis; no provider or device traffic."""
import hashlib
import json
import os
from pathlib import Path
import signal
import sys
import tempfile
import time
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "companion"))
import modbridge_nexus as nexus
from modbridge_host import BridgeApplication, create_server
from host_tests import FakeAccount


class OwnedAccount(FakeAccount):
    def status(self):
        time.sleep(float(os.environ.get("FIXTURE_ACCOUNT_DELAY", "0")))
        return super().status()

    def games(self):
        return [{"domain_name": "ownedgame", "name": "Owned Game Fixture", "id": 1}]

    def latest_mods(self, game):
        return [{"mod_id": 1, "name": "Owned Archive Fixture", "summary": "Local test data", "author": "Fixture"}]


def download(link, destination):
    destination.mkdir(mode=0o700, parents=True)
    path = destination / "owned.zip"
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("data/readme.txt", "Owned nonexecuting archive fixture\n")
    return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)


def stop(*_):
    raise KeyboardInterrupt()


def main():
    signal.signal(signal.SIGTERM, stop)
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary).resolve()
        app = BridgeApplication(root / "credentials.json", root / "quarantine", account=OwnedAccount(), download=download)
        server = create_server(app)
        print(json.dumps({"origin": app.origin, "evidence": "owned_desktop_fixture"}), flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()
            app.close()


if __name__ == "__main__":
    main()
