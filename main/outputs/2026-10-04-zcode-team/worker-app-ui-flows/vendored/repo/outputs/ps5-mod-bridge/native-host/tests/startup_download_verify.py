#!/usr/bin/env python3
"""Exercise the selected download-runtime host through its real loopback HTTP boundary.

The binary must come from build.py's explicit --download-profile composition.
All requests are to the owned local process; this does not contact a provider
or expose download, account, job, or installation routes.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import time


def unused_port():
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        return reserve.getsockname()[1]


def request(port, path, method="GET", extra=""):
    authority = f"127.0.0.1:{port}"
    if method == "POST" and path == "/shutdown":
        extra = ("Content-Length: 0\r\nX-Modbridge-Shutdown: 1\r\n"
                 f"Origin: http://{authority}\r\n")
    raw = f"{method} {path} HTTP/1.1\r\nHost: {authority}\r\n{extra}\r\n".encode("ascii")
    with socket.create_connection(("127.0.0.1", port), timeout=3) as peer:
        peer.settimeout(3)
        peer.sendall(raw)
        data = bytearray()
        while True:
            part = peer.recv(16384)
            if not part:
                break
            data.extend(part)
    header, body = bytes(data).split(b"\r\n\r\n", 1)
    lines = header.decode("ascii").split("\r\n")
    fields = dict(line.split(": ", 1) for line in lines[1:])
    if int(fields["Content-Length"]) != len(body):
        raise AssertionError("host response length did not match its body")
    return int(lines[0].split()[1]), body


def start_and_report(binary, port, lifetime=20):
    process = subprocess.Popen([str(binary), "--port", str(port), "--lifetime", str(lifetime)],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    handed_off = False
    try:
        deadline = time.monotonic() + 8
        last_error = None
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise AssertionError(f"download host exited before report with status {process.returncode}")
            try:
                status, body = request(port, "/report")
                if status != 200:
                    raise AssertionError(f"download host report returned HTTP {status}")
                report = json.loads(body)
                handed_off = True
                return process, report
            except (OSError, ValueError) as error:
                last_error = error
                time.sleep(.02)
        raise AssertionError(f"download host did not become report-ready: {type(last_error).__name__}")
    finally:
        if not handed_off and process.poll() is None:
            process.terminate()
            process.wait(timeout=3)


def compile_observed_host(binary, output):
    profile = binary.parent
    try:
        manifest = json.loads((profile / "manifest.json").read_text())
        commands = [entry["argv"] for entry in manifest["commands"] if "argv" in entry]
    except (OSError, KeyError, TypeError, ValueError) as error:
        raise AssertionError("download profile lacks its recorded compiler/link commands") from error
    host_compile = next((command for command in commands
                         if "-c" in command and Path(command[command.index("-c") + 1]).name == "host.c"), None)
    link = next((command for command in commands
                 if "-o" in command and Path(command[command.index("-o") + 1]).resolve() == binary), None)
    if host_compile is None or link is None:
        raise AssertionError("download profile does not record host compilation and final link")
    instrumented = output / "instrumented"
    instrumented.mkdir(mode=0o700)

    def compile_command(original, source, destination, macros=()):
        stop = original.index("-c")
        prefix = list(original[:stop])
        clean = []
        index = 0
        while index < len(prefix):
            if prefix[index] == "-MD":
                index += 1
            elif prefix[index] == "-MF":
                index += 2
            else:
                clean.append(prefix[index])
                index += 1
        return [*clean, *macros, "-c", str(source), "-o", str(destination)]

    original_host_object = Path(host_compile[host_compile.index("-o") + 1]).resolve()
    host_object = instrumented / "host-observed.o"
    observer_object = instrumented / "startup-runtime-observer.o"
    observer_source = Path(__file__).with_name("startup_runtime_observer.c")
    for command in (
        compile_command(host_compile, host_compile[host_compile.index("-c") + 1], host_object,
                        ("-Dmb_curl_runtime_start_v1=mb_test_runtime_start_v1",
                         "-Dmb_curl_runtime_close_v1=mb_test_runtime_close_v1")),
        compile_command(host_compile, observer_source, observer_object),
    ):
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise AssertionError("private lifecycle observer compilation failed")

    observed_binary = instrumented / "modbridge-host-observed"
    command = list(link)
    output_index = command.index("-o") + 1
    command[output_index] = str(observed_binary)
    replaced = False
    for index, argument in enumerate(command):
        try:
            same_host = Path(argument).resolve() == original_host_object
        except (OSError, ValueError):
            same_host = False
        if same_host:
            command[index] = str(host_object)
            replaced = True
        if argument.startswith(("-Wl,-Map,", "-Wl,-map,")):
            option = "-Wl,-Map," if argument.startswith("-Wl,-Map,") else "-Wl,-map,"
            command[index] = option + str(instrumented / "link.map")
    if not replaced:
        raise AssertionError("final link does not consume the recorded host object")
    first_archive = next((index for index, argument in enumerate(command)
                          if argument.endswith(".a")), output_index)
    command.insert(first_archive, str(observer_object))
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    if result.returncode or not observed_binary.is_file():
        raise AssertionError("private lifecycle observer link failed")
    return observed_binary


def run_observed(binary, port, lifetime=20, refuse_entropy=False, shutdown=False):
    read_fd, write_fd = os.pipe()
    env = os.environ.copy()
    env.pop("MB_TEST_REFUSE_ENTROPY", None)
    env["MB_TEST_EVENT_FD"] = str(write_fd)
    if refuse_entropy:
        env["MB_TEST_REFUSE_ENTROPY"] = "1"
    try:
        process = subprocess.Popen([str(binary), "--port", str(port), "--lifetime", str(lifetime)],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   env=env, pass_fds=(write_fd,))
    except BaseException:
        os.close(write_fd)
        os.close(read_fd)
        raise
    os.close(write_fd)
    try:
        if shutdown:
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise AssertionError("observed host exited before its shutdown request")
                try:
                    status, _ = request(port, "/report")
                    if status != 200:
                        raise AssertionError("observed host report returned an error")
                    break
                except OSError:
                    time.sleep(.02)
            else:
                raise AssertionError("observed host did not become ready")
            status, body = request(port, "/shutdown", method="POST")
            if status != 200 or json.loads(body).get("status") != "shutdown_requested":
                raise AssertionError("observed host shutdown route failed")
        try:
            status = process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            raise AssertionError("observed host did not exit within its bounded control")
        events = bytearray()
        while True:
            part = os.read(read_fd, 256)
            if not part:
                break
            events.extend(part)
    except BaseException:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
        raise
    finally:
        os.close(read_fd)
    text = events.decode("ascii")
    phases = [line.split(":", 1)[0] for line in text.splitlines()]
    if phases != ["start.enter", "start.exit", "close.enter", "close.exit"]:
        raise AssertionError("host did not call runtime start and close exactly once in order")
    return status, text


def run_lifecycle_controls(binary):
    receipt = {}
    status, events = run_observed(binary, unused_port(), shutdown=True)
    if status != 0:
        raise AssertionError("observed host normal shutdown failed")
    receipt["normal_shutdown"] = {"status": status, "events": events.splitlines()}

    status, events = run_observed(binary, unused_port(), lifetime=1)
    if status != 0:
        raise AssertionError("observed host lifetime exit failed")
    receipt["lifetime_exit"] = {"status": status, "events": events.splitlines()}

    with socket.socket() as occupied:
        occupied.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        occupied.bind(("127.0.0.1", 0))
        occupied.listen(1)
        port = occupied.getsockname()[1]
        status, events = run_observed(binary, port)
    if status != 3:
        raise AssertionError("occupied listener did not exercise the post-start bind failure")
    receipt["bind_failure"] = {"status": status, "events": events.splitlines()}

    port = unused_port()
    status, events = run_observed(binary, port, refuse_entropy=True)
    if status != 5:
        raise AssertionError("entropy refusal did not stop startup")
    with socket.socket() as probe:
        if probe.connect_ex(("127.0.0.1", port)) == 0:
            raise AssertionError("listener opened after runtime entropy refusal")
    receipt["entropy_refusal"] = {"status": status, "events": events.splitlines(),
                                  "listener_opened": False}
    return receipt


def run_actual_bind_failure(binary):
    with socket.socket() as occupied:
        occupied.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        occupied.bind(("127.0.0.1", 0))
        occupied.listen(1)
        process = subprocess.Popen([str(binary), "--port", str(occupied.getsockname()[1]),
                                    "--lifetime", "20"],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            status = process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.terminate()
            process.wait(timeout=5)
            raise AssertionError("host did not exit from the occupied-port control")
    if status != 3:
        raise AssertionError("actual host did not report its listener bind failure")
    return status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path,
                        help="new directory for the bounded verifier receipt")
    args = parser.parse_args()
    binary, output = args.binary.resolve(), args.output.resolve()
    if not binary.is_file() or not binary.stat().st_mode & 0o111:
        raise ValueError("--binary must be an executable build artifact")
    if output.exists() or output == binary.parent or binary.parent in output.parents:
        raise ValueError("--output must be new and outside the build profile")
    output.mkdir(mode=0o700, parents=True)

    port = unused_port()
    process, report = start_and_report(binary, port)
    try:
        runtime = report.get("download_runtime")
        if not isinstance(runtime, dict):
            raise AssertionError("/report omitted download_runtime metadata")
        if (runtime.get("selected") is not True or runtime.get("status") != "initialized" or
                runtime.get("scope") != "process_runtime" or runtime.get("initialized") is not True):
            raise AssertionError("/report did not describe an initialized selected process runtime")
        if (runtime.get("terminal") is not False or runtime.get("window_active") is not False or
                runtime.get("window_expired") is not False):
            raise AssertionError("startup runtime snapshot is not in its healthy idle state")
        for key in ("entropy_windows", "entropy_callbacks", "entropy_bytes", "clock_samples"):
            if type(runtime.get(key)) is not int or runtime[key] < 0:
                raise AssertionError(f"download_runtime.{key} is not a nonnegative scalar")
        if report.get("observations", {}).get("drbg", {}).get("reason") != "not_selected":
            raise AssertionError("legacy DRBG checkpoint was not truthfully marked not_selected")
        if report.get("installation_supported") is not False:
            raise AssertionError("runtime selection changed the installation capability claim")

        status, body = request(port, "/api/capabilities")
        if status != 200:
            raise AssertionError("capabilities endpoint stopped responding")
        capabilities = json.loads(body).get("capabilities", {})
        if not capabilities or any(value is not False for value in capabilities.values()):
            raise AssertionError("linking the runtime enabled an unimplemented capability")
        if capabilities.get("jobs") is not False:
            raise AssertionError("download startup enabled the job capability")
        for path in ("/api/jobs", "/api/download"):
            status, _ = request(port, path)
            if status != 404:
                raise AssertionError(f"download startup unexpectedly exposed {path}")

        status, body = request(port, "/shutdown", method="POST")
        if status != 200 or json.loads(body).get("status") != "shutdown_requested":
            raise AssertionError("the owned shutdown route did not complete")
        if process.wait(timeout=5) != 0:
            raise AssertionError("normal shutdown returned a failure status")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)

    with socket.socket() as rebound:
        rebound.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        rebound.bind(("127.0.0.1", port))

    bind_failure_status = run_actual_bind_failure(binary)
    lifecycle = run_lifecycle_controls(compile_observed_host(binary, output))

    receipt = {
        "schema_version": 1,
        "binary": binary.name,
        "checks": ["selected_runtime_report", "legacy_drbg_not_selected",
                   "capabilities_remain_false", "shutdown_exit", "port_rebound",
                   "actual_bind_failure_exit",
                   "lifetime_exit_closes_once", "bind_failure_closes_once",
                   "entropy_refusal_closes_once_without_listener"],
        "runtime": runtime,
        "actual_bind_failure_status": bind_failure_status,
        "lifecycle_controls": lifecycle,
        "console_executed": False,
        "provider_contacted": False,
    }
    (output / "summary.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"checks": len(receipt["checks"]), "runtime_status": runtime["status"],
                      "console_executed": False, "provider_contacted": False}, sort_keys=True))


if __name__ == "__main__":
    main()
