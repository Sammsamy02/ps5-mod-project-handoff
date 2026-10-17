"""Owned generic inventory with real local host; no remote provider or console."""
import json
from pathlib import Path
import signal
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'companion'))
from modbridge_host import BridgeApplication, create_server
from modbridge_inventory import TitleInventory
from host_tests import FakeAccount
from host_title_paging_tests import captured


def stop(*_):
    raise KeyboardInterrupt()


def main():
    signal.signal(signal.SIGTERM, stop)
    capture_count = 0
    def provider():
        nonlocal capture_count
        capture_count += 1
        print(json.dumps({'event': 'owned_capture', 'count': capture_count}), flush=True)
        value = captured(128)
        value['items'][-1]['build_fingerprint'] = {
            'algorithm': 'sha256', 'value': 'a' * 64, 'confidence': 'exact_bytes',
            'source': 'owned_fixture', 'provenance': {'method': 'sha256_exact_bytes',
                'byte_length': 9223372036854775807, 'complete': True, 'artifact_kind': 'content'}}
        return value
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary).resolve()
        app = BridgeApplication(root / 'credentials.json', root / 'quarantine',
            account=FakeAccount(), inventory=TitleInventory(provider))
        server = create_server(app)
        print(json.dumps({'origin': app.origin, 'evidence': 'owned_generic_title_fixture'}), flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()
            app.close()


if __name__ == '__main__':
    main()
