#!/usr/bin/env python3
"""Exercise the real curl 8.18 -> file-sink API on owned loopback HTTPS.

Uses retained matching host libraries. No dependency rebuild, native binary,
console, live Nexus account or installation. Outputs must be fresh and outside
the retained dependency trees. Selected original archives are served unchanged.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import socket
import ssl
import subprocess
import threading
import zipfile

HERE = Path(__file__).resolve().parent
BLOCK = (b"owned-stream-download-v1\n" * 3000)[:65536]
LARGE_BYTES = 186 * 1024 * 1024
PATCHED_CURL_SHA = "f4679b3d3e24203d1914be8444a60a21db495bd98c64d5765218e1fbe899a86e"


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            sha.update(block)
    return sha.hexdigest()


def generated_hash(size):
    sha = hashlib.sha256()
    while size:
        n = min(size, len(BLOCK))
        sha.update(BLOCK[:n])
        size -= n
    return sha.hexdigest()


def owned_zip():
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", zipfile.ZIP_STORED) as archive:
        archive.writestr("Config/Engine.ini", "[SystemSettings]\nr.MotionBlurQuality=0\n")
    return output.getvalue()


class Fixture:
    def __init__(self, certs, case):
        self.case = case
        self.stop = threading.Event()
        self.receipt = {"connections": 0, "handshakes": 0, "requests": 0,
                        "request_contract": False, "redirect_connections": 0}
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = self.context.maximum_version = ssl.TLSVersion.TLSv1_2
        self.context.set_ciphers("ECDHE-ECDSA-AES128-GCM-SHA256")
        self.context.load_cert_chain(certs / "chain.pem", certs / "server-private/leaf.key")
        self.listener, self.trap = self.listen(), self.listen()
        self.port, self.trap_port = self.listener.getsockname()[1], self.trap.getsockname()[1]
        self.threads = [threading.Thread(target=fn, daemon=True)
                        for fn in (self.serve, self.redirect_trap)]
        for thread in self.threads:
            thread.start()

    @staticmethod
    def listen():
        result = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        result.bind(("127.0.0.1", 0))
        result.listen(2)
        result.settimeout(.1)
        return result

    def redirect_trap(self):
        while not self.stop.is_set():
            try:
                stream, _ = self.trap.accept()
                self.receipt["redirect_connections"] += 1
                stream.close()
            except socket.timeout:
                continue
            except OSError:
                return

    def blocks(self):
        case = self.case
        if "source" in case:
            with case["source"].open("rb") as stream:
                yield from iter(lambda: stream.read(65536), b"")
        elif case["name"] == "large":
            remaining = case["bytes"]
            while remaining:
                block = BLOCK[:min(remaining, len(BLOCK))]
                yield block
                remaining -= len(block)
        elif case.get("fragmented"):
            yield from (bytes([b]) for b in case["data"])
        else:
            yield case["data"]

    def serve(self):
        while not self.stop.is_set():
            try:
                raw, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            self.receipt["connections"] += 1
            raw.settimeout(10)
            try:
                with self.context.wrap_socket(raw, server_side=True) as stream:
                    self.receipt["handshakes"] += 1
                    request = b""
                    while b"\r\n\r\n" not in request and len(request) < 8192:
                        part = stream.recv(1024)
                        if not part:
                            break
                        request += part
                    if b"\r\n\r\n" not in request:
                        return
                    self.receipt["requests"] += 1
                    lines = request.split(b"\r\n")
                    self.receipt["request_contract"] = (
                        lines[0] == b"GET /owned-archive?grant=fixture-only HTTP/1.1"
                        and b"Accept-Encoding: identity" in lines
                        and not any(line.lower().startswith((b"authorization:", b"cookie:", b"proxy-authorization:")) for line in lines))
                    request = b""
                    case = self.case
                    if case["name"] == "timeout":
                        self.stop.wait(1)
                        return
                    if case["name"] == "redirect":
                        stream.sendall(b"HTTP/1.1 302 Found\r\nLocation: https://127.0.0.1:" + str(self.trap_port).encode() + b"/no-follow\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
                        return
                    status = b"HTTP/1.1 200 OK\r\nConnection: close\r\n"
                    framing = b"Transfer-Encoding: chunked\r\n" if case.get("chunked") else b"Content-Length: " + str(case["bytes"]).encode() + b"\r\n"
                    if case["name"] in ("close-delimited", "close-delimited-raw"):
                        framing = b""
                    encoding = b"Content-Encoding: gzip\r\n" if case["name"] == "gzip" else b""
                    stream.sendall(status + framing + encoding + b"\r\n")
                    for block in self.blocks():
                        if self.stop.is_set():
                            return
                        stream.sendall((f"{len(block):x}\r\n".encode() + block + b"\r\n") if case.get("chunked") else block)
                    if case.get("chunked") and case["name"] != "truncated-chunked":
                        stream.sendall(b"0\r\n\r\n")
                    if case["name"] == "close-delimited":
                        # Without HTTP length/chunk framing, an authenticated
                        # TLS end is necessary. Raw TCP EOF is a failure case.
                        self.receipt["tls_shutdown_attempted"] = True
                        plain = stream.unwrap()
                        plain.close()
                        self.receipt["tls_shutdown_complete"] = True
            except (OSError, ssl.SSLError) as error:
                self.receipt["error_type"] = type(error).__name__
            finally:
                raw.close()
            return

    def close(self):
        self.stop.set()
        self.listener.close()
        self.trap.close()
        for thread in self.threads:
            thread.join(2)
        self.receipt["listeners_closed"] = not any(t.is_alive() for t in self.threads)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--curl-source", required=True, type=Path)
    parser.add_argument("--patched-curl", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--archive", action="append", type=Path, default=[])
    parser.add_argument("--small", action="store_true", help="Omit the generated stream above 128 MiB")
    args = parser.parse_args()
    profile, curl, patched, out = (p.resolve() for p in
                                  (args.profile, args.curl_source, args.patched_curl, args.output))
    if any(out == p or p in out.parents for p in (profile, curl, patched.parent)):
        raise RuntimeError("outputs must be outside retained dependency trees")
    if digest(patched) != PATCHED_CURL_SHA:
        raise RuntimeError("unexpected retained patched curl archive")
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    cc = profile / "host/cc.py"
    objects = []
    sources = [HERE / "owner.c", HERE / "curl_download.c",
               HERE.parent / "archive/private/file_sink.c", HERE / "tests/stream_driver.c"]
    for index, source in enumerate(sources):
        obj = out / (source.stem + ".o")
        command = [str(cc), "-Wall", "-Wextra", "-Werror", "-O2",
                   "-I" + str(curl / "include"), "-c", str(source), "-o", str(obj)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (out / f"compile-{index}.log").write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"compile failed: {source.name}; see compile-{index}.log")
        objects.append(obj)
    binary = out / "stream-driver"
    libraries = [profile / "host" / ("lib" + name + ".a") for name in ("mbedtls", "mbedx509", "mbedcrypto")]
    result = subprocess.run([str(cc), *map(str, objects), str(patched), *map(str, libraries),
                             "-framework", "SystemConfiguration", "-framework", "CoreFoundation", "-o", str(binary)],
                            capture_output=True, text=True, timeout=60)
    (out / "link.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError("link failed; see link.log")
    # Only this private owner copy sees synthetic clock functions. The normal
    # TLS binary above uses real clocks/getentropy; the OS clock never changes.
    clock_commands = [
        [str(cc), "-Wall", "-Wextra", "-Werror", "-Dtime=owner_fixture_time",
         "-Dclock_gettime=owner_fixture_clock_gettime", "-c", str(HERE / "owner.c"), "-o", str(out / "clock-owner.o")],
        [str(cc), "-Wall", "-Wextra", "-Werror", "-c", str(HERE / "tests/owner_clock_control.c"), "-o", str(out / "clock-control.o")],
        [str(cc), str(out / "clock-owner.o"), str(out / "clock-control.o"), "-o", str(out / "clock-control")]]
    for index, command in enumerate(clock_commands):
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (out / f"clock-build-{index}.log").write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"clock control build failed: {index}")
    clock_controls = []
    for name in ("forward-between-windows", "timeout-then-next-window", "source-failure", "outside-window"):
        result = subprocess.run([str(out / "clock-control"), name], capture_output=True, text=True, timeout=10)
        row = {"case": name, "exit": result.returncode, "client": json.loads(result.stdout), "stderr_bytes": len(result.stderr)}
        clock_controls.append(row)
    (out / "clock-controls.json").write_text(json.dumps(clock_controls, indent=2) + "\n")
    data = owned_zip()
    cases = [] if args.small else [{"name": "large", "bytes": LARGE_BYTES,
                                    "sha256": generated_hash(LARGE_BYTES), "accepted": True}]
    cases += [{"name": "fixed", "data": data, "accepted": True},
              {"name": "wrong-root", "data": data, "accepted": False},
              {"name": "wrong-name", "data": data, "accepted": False}]
    for archive in args.archive:
        archive = archive.resolve()
        if out == archive or out in archive.parents:
            raise RuntimeError("input archive cannot be an output")
        cases.append({"name": "retained-archive", "source": archive,
                      "bytes": archive.stat().st_size, "sha256": digest(archive), "accepted": True})
    cases += [{"name": "chunked-fragmented", "data": data, "chunked": True, "fragmented": True, "accepted": True},
              {"name": "truncated-fixed", "data": data[:-7], "bytes": len(data), "accepted": False},
              {"name": "truncated-chunked", "data": data, "chunked": True, "accepted": False},
              {"name": "redirect", "data": b"", "accepted": False},
              {"name": "gzip", "data": data, "accepted": False},
              {"name": "cap", "data": data, "cap": 64, "accepted": False},
              {"name": "timeout", "data": data, "timeout": 500, "accepted": False},
              {"name": "close-delimited-raw", "data": data, "accepted": False},
              {"name": "close-delimited", "data": data, "accepted": True},
              {"name": "sink-refuse", "data": data, "accepted": False},
              {"name": "readback-tamper", "data": data, "accepted": False},
              {"name": "cancel", "data": data, "accepted": False},
              {"name": "fixed-after-failures", "data": data, "accepted": True}]
    fixtures = []
    argv = [str(binary), str(out)]
    for case in cases:
        case.setdefault("bytes", len(case.get("data", b"")))
        case.setdefault("sha256", hashlib.sha256(case.get("data", b"")).hexdigest())
        fixture = Fixture(profile.parent / "native-https-console-fixture-1", case)
        fixtures.append(fixture)
        mode = case["name"] if case["name"] in ("wrong-root", "wrong-name", "sink-refuse", "readback-tamper", "cancel") else "normal"
        argv += [str(fixture.port), mode, str(case.get("cap", 1073741824)), str(case.get("timeout", 300000))]
    try:
        result = subprocess.run(argv, capture_output=True, text=True, timeout=90)
        (out / "stream.stdout.jsonl").write_text(result.stdout)
        (out / "stream.stderr.log").write_text(result.stderr)
        rows = [json.loads(line) for line in result.stdout.splitlines()]
    finally:
        for fixture in fixtures:
            fixture.close()
    jobs = [row for row in rows if row["kind"] == "job"]
    checks = {"driver_exit": result.returncode == 0, "no_stderr": not result.stderr,
              "no_signed_url_in_output": "grant=fixture-only" not in result.stdout + result.stderr,
              "all_jobs_observed": len(jobs) == len(cases),
              "job_order": [job["index"] for job in jobs] == list(range(len(cases))),
              "started_once": rows[0]["initialized"] == 1,
              "closed": rows[-1]["kind"] == "close" and rows[-1]["initialized"] == 0 and rows[-1]["window_active"] == 0,
              "real_fresh_entropy": rows[-1].get("entropy_callbacks", 0) > rows[0].get("entropy_callbacks", 0)}
    checks["private_clock_and_source_controls"] = all(row["exit"] == 0 and row["client"]["passed"] and row["stderr_bytes"] == 0 for row in clock_controls)
    receipts = []
    for case, fixture, job in zip(cases, fixtures, jobs):
        success = job["return"] == 0 and job["complete"] == 1
        output = out / f"job-{job['index']}.archive"
        measured = digest(output)
        row = {"case": case["name"], "client": job, "server": fixture.receipt,
               "expected_accepted": case["accepted"], "output_sha256": measured}
        valid = (success == case["accepted"] and not fixture.receipt["redirect_connections"]
                 and fixture.receipt["listeners_closed"] and job["owner_terminal"] == 0)
        if case["accepted"]:
            valid = valid and (job["received"] == case["bytes"] == job["file_bytes"]
                    and job["readback"] == 1 and job["file_complete"] == 1
                    and job["sha256"] == measured == case["sha256"]
                    and job["feed_calls"] <= (case["bytes"] + 65535) // 65536
                    and fixture.receipt["request_contract"])
            with output.open("rb") as stream:
                if stream.read(4).startswith(b"PK"):
                    with zipfile.ZipFile(output) as archive:
                        row["zip_members"] = archive.namelist()
        else:
            valid = valid and job["complete"] == 0 and job["file_complete"] == 0 and job["readback"] == 0 and job["sha256"] == "0" * 64
        if case["name"] in ("wrong-root", "wrong-name"):
            valid = valid and fixture.receipt["requests"] == 0
        else:
            valid = valid and fixture.receipt["requests"] == 1 and fixture.receipt["request_contract"]
        if case["name"] == "redirect":
            valid = valid and job["http"] == 302
        if "source" in case:
            valid = valid and digest(case["source"]) == case["sha256"]
        row["passed"] = bool(valid)
        checks[f"job-{job['index']}-{case['name']}"] = row["passed"]
        receipts.append(row)
    refusal = subprocess.run([str(binary), "init-refuse"], capture_output=True, text=True, timeout=10)
    (out / "refusal.stdout.json").write_text(refusal.stdout)
    (out / "refusal.stderr.log").write_text(refusal.stderr)
    checks["terminal_init_refusal"] = refusal.returncode == 0 and not refusal.stderr
    summary = {"passed": all(checks.values()), "checks": checks, "jobs": receipts,
               "private_clock_controls": clock_controls,
               "runtime": rows[-1], "init_refusal": json.loads(refusal.stdout),
               "native_build": False, "console_contact": False, "live_nexus": False,
               "app_dispatch_connected": False, "game_files_changed": False,
               "peer_close_notify_observed": False, "uses_retained_host_dependencies": True}
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({"passed": summary["passed"], "jobs": len(jobs),
                      "failed": [key for key, value in checks.items() if not value],
                      "receipt": str(out / "summary.json")}))
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
