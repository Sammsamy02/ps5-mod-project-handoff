"""Focused owned controls, then at most one real-Mbed handshake per fixed public name."""
import argparse
import datetime
import json
import pathlib
import socket
import ssl
import subprocess
import sys
import threading
import time

HERE = pathlib.Path(__file__).resolve().parent
BINARY = HERE / "host/tls_client"


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def save(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n")


def run_client(hostname, address, port, mode):
    args = [str(BINARY), hostname, address, str(port), mode]
    result = subprocess.run(args, capture_output=True, text=True, timeout=25)
    return {"command": args, "exit_code": result.returncode,
            "receipt": json.loads(result.stdout), "stderr": result.stderr[:512], "outer_timeout_seconds": 25}


class Fixture(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.listener.settimeout(5)
        self.port = self.listener.getsockname()[1]
        self.result = {"connections": 0, "handshake_completed": False,
                       "application_bytes_received": 0, "clean_tls_close": False}

    def run(self):
        raw = None
        channel = None
        try:
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
            context.set_ciphers("ECDHE-ECDSA-AES128-GCM-SHA256")
            context.set_ecdh_curve("prime256v1")
            context.options |= ssl.OP_NO_TICKET
            context.load_cert_chain(HERE / "fixture-chain.pem", HERE / "fixture-private/leaf.key")
            context.set_servername_callback(lambda sock, name, ctx: self.result.update(sni=name))
            raw, peer = self.listener.accept()
            self.result["connections"] += 1
            raw.settimeout(5)
            channel = context.wrap_socket(raw, server_side=True)
            raw = None
            self.result["handshake_completed"] = True
            data = channel.recv(1)
            self.result["application_bytes_received"] = len(data)
            if data:
                raise RuntimeError("client sent unexpected application data")
            raw = channel.unwrap()
            channel = None
            self.result["clean_tls_close"] = True
        except Exception as error:
            self.result.update(error_type=type(error).__name__, error=str(error)[:256])
        finally:
            if channel is not None:
                channel.close()
            if raw is not None:
                raw.close()
            self.listener.close()


def controls():
    path = HERE / "controls.json"
    with path.open("x") as f:
        f.write('{"status":"reserved_before_owned_controls"}\n')
    output = {"scope": "two owned-loopback focused cases; real host OS entropy", "started_utc": utc_now(), "cases": []}
    for hostname in ("nexus-mbed.fixture.test", "wrong.nexus-mbed.fixture.test"):
        server = Fixture()
        server.start()
        case = {"hostname": hostname}
        try:
            case["client"] = run_client(hostname, "127.0.0.1", server.port, "fixture")
        except Exception as error:
            case.update(error_type=type(error).__name__, error=str(error)[:256])
        server.join(timeout=6)
        case["server"] = server.result
        case["server_finished"] = not server.is_alive()
        output["cases"].append(case)
        save(path, output)
    valid, wrong = output["cases"]
    v = valid.get("client", {}).get("receipt", {})
    w = wrong.get("client", {}).get("receipt", {})
    output["passed"] = bool(valid.get("client", {}).get("exit_code") == 0 and v.get("status") == "ok"
                            and v.get("verify_flags") == 0 and v.get("handshake_accepted")
                            and v.get("clean_peer_close") and v.get("source_calls") == 2 and v.get("source_closed")
                            and v.get("owned_workspace_wiped") and v.get("application_sent_bytes") == 0
                            and valid["server"].get("clean_tls_close") and valid["server"].get("application_bytes_received") == 0
                            and wrong.get("client", {}).get("exit_code") == 1 and w.get("verify_flags") == 4
                            and not w.get("handshake_accepted") and w.get("owned_workspace_wiped")
                            and w.get("application_sent_bytes") == 0 and all(c["server_finished"] for c in output["cases"]))
    output["finished_utc"] = utc_now()
    save(path, output)
    print(json.dumps(output))
    if not output["passed"]:
        raise SystemExit(1)


def public():
    prerequisite = json.loads((HERE / "controls.json").read_text())
    if not prerequisite.get("passed"):
        raise RuntimeError("owned controls did not pass")
    build = json.loads((HERE / "host/build-summary.json").read_text())
    path = HERE / "public-results.json"
    with path.open("x") as f:
        f.write('{"status":"reserved_before_any_public_dns_or_dispatch"}\n')
    output = {"scope": "host real-Mbed TLS-only public snapshot, no native execution",
              "config_id": build["config_id"], "started_utc": utc_now(),
              "dns_process_timeout_seconds": 5, "client_process_timeout_seconds": 25,
              "application_requests": 0, "retries": 0, "attempts": []}
    for hostname in ("users.nexusmods.com", "api.nexusmods.com"):
        case = {"hostname": hostname, "phase": "dns", "started_utc": utc_now(), "client_dispatched": False}
        output["attempts"].append(case)
        save(path, output)
        started = time.monotonic()
        try:
            dns = subprocess.run([sys.executable, str(HERE / "dns.py"), hostname],
                                 capture_output=True, text=True, timeout=5)
            case["dns_exit_code"] = dns.returncode
            if dns.returncode:
                raise RuntimeError("DNS failed: " + dns.stderr[:256])
            case["dns"] = json.loads(dns.stdout)
            case["phase"] = "client_dispatch"
            case["client_dispatched"] = True
            save(path, output)  # Persist the single dispatch before entering the subprocess.
            case["client"] = run_client(hostname, case["dns"]["selected_ipv4"], 443, "public")
            case["phase"] = "complete"
            receipt = case["client"]["receipt"]
            case["passed"] = bool(case["client"]["exit_code"] == 0 and receipt.get("status") == "ok"
                                  and receipt.get("config_id") == build["config_id"] and receipt.get("verify_flags") == 0
                                  and receipt.get("handshake_accepted") and receipt.get("clean_peer_close")
                                  and receipt.get("application_sent_bytes") == 0 and receipt.get("application_received_bytes") == 0
                                  and receipt.get("owned_workspace_wiped") and receipt.get("descriptor_close_rc") == 0)
        except Exception as error:
            case.update(passed=False, error_type=type(error).__name__, error=str(error)[:256])
        finally:
            case["elapsed_seconds"] = round(time.monotonic() - started, 6)
            case["finished_utc"] = utc_now()
            save(path, output)
        print(json.dumps(case))
    output["passed"] = all(c.get("passed") for c in output["attempts"])
    output["finished_utc"] = utc_now()
    save(path, output)
    if not output["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("controls", "public"))
    args = parser.parse_args()
    (controls if args.mode == "controls" else public)()
