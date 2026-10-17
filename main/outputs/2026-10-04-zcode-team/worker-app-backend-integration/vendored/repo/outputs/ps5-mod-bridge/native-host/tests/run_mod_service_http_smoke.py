"""Loopback-only smoke for a desktop host linked to the actual mod service.

Build with the retained download runtime (for local-session admission), an
explicit service profile/config, and an offline OAuth fixture pinned to the same loopback port. Optionally
seed a downloaded record through the registration_service_smoke fixture first.
This runner never contacts a provider or a console and never installs a mod.
"""
import argparse
import hashlib
import http.client
import json
import pathlib
import socket
import subprocess
import time


def exchange(port, method, path, headers=None):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
    try:
        connection.request(method, path, headers=headers or {})
        response = connection.getresponse()
        raw = response.read()
        return response.status, dict(response.getheaders()), json.loads(raw)
    finally:
        connection.close()


def run_once(binary, expected_id, port):
    # The child exclusively binds this loopback port; never send shutdown to
    # another server. Its PID is checked before any subsequent request.
    with socket.socket() as reserve:
        reserve.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        reserve.bind(('127.0.0.1', port))
        port = reserve.getsockname()[1]
    child = subprocess.Popen([str(binary), '--port', str(port), '--lifetime', '45'],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        deadline = time.monotonic() + 15
        while True:
            if child.poll() is not None:
                stdout, stderr = child.communicate()
                raise AssertionError(f'host exited before listening: {child.returncode}\n{stdout}\n{stderr}')
            try:
                status, _, version = exchange(port, 'GET', '/version')
                break
            except (OSError, http.client.HTTPException):
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.05)
        assert status == 200, version
        status, _, observations = exchange(port, 'GET', '/report')
        assert status == 200 and observations['observations']['getpid']['value'] == child.pid, observations
        assert exchange(port, 'GET', '/api/mod-service/records')[0] == 401
        status, headers, capabilities = exchange(port, 'GET', '/api/capabilities')
        assert status == 200, capabilities
        cookie = headers['Set-Cookie'].split(';', 1)[0]
        session = {'Cookie': cookie}
        flags = capabilities['capabilities']
        assert flags['mod_service_records'] is True
        assert flags['deploy'] is False and flags['prepared_translation'] is False
        status, _, description = exchange(port, 'GET', '/api/mod-service', session)
        assert status == 200 and description['ok'] is True, description
        description = description['result']
        assert description['contract'] == 'modforge-service/1', description
        assert {'records.list', 'service.describe', 'mod.register_download'} <= set(description['methods'])
        status, _, records = exchange(port, 'GET', '/api/mod-service/records', session)
        assert status == 200 and records['ok'] is True, records
        records = records['result']
        assert isinstance(records['mods'], list), records
        assert 'archive_path' not in json.dumps(records)
        if expected_id:
            found = [row for row in records['mods'] if row['download_id'] == expected_id]
            assert len(found) == 1 and found[0]['state'] == 'downloaded', records
        assert exchange(port, 'POST', '/api/mod-service/records', session)[0] == 405
        status, _, _ = exchange(port, 'POST', '/shutdown', {
            'Origin': f'http://127.0.0.1:{port}', 'X-Modbridge-Shutdown': '1'})
        assert status == 200
        stdout, stderr = child.communicate(timeout=10)
        assert child.returncode == 0, (child.returncode, stdout, stderr)
        return {'build_id': version['build_id'], 'record_count': len(records['mods']),
                'methods': description['methods'], 'clean_exit': True,
                'record_states': [row['state'] for row in records['mods']]}
    finally:
        if child.poll() is None:
            child.terminate()
            try:
                child.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.communicate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=pathlib.Path, required=True)
    parser.add_argument('--expect-download-id')
    parser.add_argument('--port', type=int, required=True, help='the offline OAuth fixture callback port')
    parser.add_argument('--receipt', type=pathlib.Path)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    first = run_once(binary, args.expect_download_id, args.port)
    second = run_once(binary, args.expect_download_id, args.port)
    assert first == second, (first, second)
    receipt = {'scope': 'actual desktop service startup/session/read/shutdown/reopen',
               'binary': str(binary), 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
               'first_run': first, 'reopened': second,
               'console_executed': False, 'provider_contact': False,
               'installation_tested': False}
    if args.receipt:
        args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
