"""Development-only gates and loopback diagnostics; never execution evidence."""
# SPDX-License-Identifier: GPL-3.0-or-later
from dataclasses import dataclass
from typing import Optional
import hashlib
import ipaddress
import json
import os
import re
import socket
import stat
import struct
import time

from verify_elf import verify_bytes

MAX_ARTIFACT_BYTES = 1048576
MAX_REPORT_BYTES = 4096
CAPABILITIES = {
    "host.payload": "standalone", "filesystem.scratch": "standalone",
    "network.tls": "standalone", "ui.controller_display": "standalone",
    "host.native_module": "game_process", "runtime.lua": "game_process",
    "runtime.ue4ss": "game_process", "runtime.windows": "game_process",
    "translation.unreal_pak": "preparation", "translation.unreal_asset": "preparation"}


def _require(condition, reason="invalid diagnostic report"):
    if not condition:
        raise ValueError(reason)


def _integer(value, low, high):
    return type(value) is int and low <= value <= high


@dataclass(frozen=True)
class DiagnosticArtifact:
    data: bytes
    size: int
    sha256: str
    extent: int
    prefix_sha256: str


def inspect_artifact(path, *, expected_sha256, expected_size):
    """Check and snapshot exact bytes before any network operation."""
    _require(type(expected_sha256) is str and
             re.fullmatch(r"[0-9a-f]{64}", expected_sha256) is not None,
             "invalid expected artifact hash")
    _require(_integer(expected_size, 1, MAX_ARTIFACT_BYTES), "invalid expected artifact size")
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        _require(stat.S_ISREG(info.st_mode), "artifact must be a regular file")
        _require(info.st_size == expected_size, "artifact size mismatch")
        chunks, remaining = [], expected_size + 1
        while remaining:
            chunk = os.read(fd, min(65536, remaining))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        data = b"".join(chunks)
    finally:
        os.close(fd)
    _require(len(data) == expected_size, "artifact size mismatch")
    digest = hashlib.sha256(data).hexdigest()
    _require(digest == expected_sha256, "artifact hash mismatch")
    verify_bytes(data)
    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
    phoff, shoff = header[5:7]
    phsize, phnum, shsize, shnum = header[9:13]
    extent = shoff + shnum * shsize
    for i in range(shnum):
        section = struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shsize)
        if section[1] != 8:  # SHT_NOBITS has no transmitted bytes.
            extent = max(extent, section[4] + section[5])
    _require(64 <= extent <= len(data), "invalid loader extent")
    _require(phoff + phnum * phsize <= extent, "program table outside loader extent")
    for i in range(phnum):
        segment = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phsize)
        if segment[0] == 1:
            _require(segment[2] + segment[5] <= extent, "load bytes outside loader extent")
    return DiagnosticArtifact(data, len(data), digest, extent,
                              hashlib.sha256(data[:extent]).hexdigest())


def _keys(value, keys):
    _require(type(value) is dict and set(value) == set(keys))


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        _require(key not in result)
        result[key] = value
    return result


def _reject_constant(_value):
    raise ValueError("invalid diagnostic report")


def parse_report(data):
    """Validate schema only. The response has no authenticity or freshness."""
    _require(type(data) is bytes and 0 < len(data) <= MAX_REPORT_BYTES and data.endswith(b"\n"))
    try:
        record = json.loads(data.decode("utf-8"), object_pairs_hook=_unique_object,
                            parse_constant=_reject_constant)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise ValueError("invalid diagnostic report") from error
    _keys(record, ("schema_version", "record_kind", "evidence_origin", "build",
                   "candidate_entry_abi", "environment", "observations",
                   "output_resolve_code", "capabilities"))
    _require(_integer(record["schema_version"], 1, 1) and
             record["record_kind"] == "native_probe_report" and
             record["evidence_origin"] == "native_entry_unverified" and
             record["candidate_entry_abi"] == "sdk-payload-args-v0.43")
    build = record["build"]
    _keys(build, ("compiler", "target", "sizeof_pointer", "sizeof_long", "sizeof_int", "byte_order"))
    _require(type(build["compiler"]) is str and 1 <= len(build["compiler"]) <= 256 and
             all(32 <= ord(c) <= 126 for c in build["compiler"]))
    _require(build["target"] == "x86_64-sie-ps5" and build["byte_order"] == "little" and
             all(_integer(build[key], size, size) for key, size in
                 (("sizeof_pointer", 8), ("sizeof_long", 8), ("sizeof_int", 4))))
    environment = record["environment"]
    _keys(environment, ("firmware_verified", "loader", "loader_version", "game", "game_update"))
    _require(all(value is None for value in environment.values()))
    observations = record["observations"]
    _keys(observations, ("getpid", "getuid", "geteuid", "getpagesize"))
    for name, observation in observations.items():
        _keys(observation, ("status", "value", "resolve_code"))
        state, value, code = (observation[key] for key in ("status", "value", "resolve_code"))
        _require(type(state) is str and state in ("observed", "unavailable", "unmeasured"))
        _require(_integer(code, -(1 << 31), (1 << 31) - 1))
        uid = name in ("getuid", "geteuid")
        _require(value is None or _integer(value, 0 if uid else -(1 << 63),
                                           (1 << 32) - 1 if uid else (1 << 63) - 1))
        if state == "observed":
            _require(value is not None and code == 0 and (uid or value > 0))
        elif state == "unmeasured":
            _require(value is None and code == 0)
        else:
            _require(value is None or (not uid and value <= 0 and code == 0))
    _require(_integer(record["output_resolve_code"], -(1 << 31), (1 << 31) - 1))
    capabilities = record["capabilities"]
    _require(type(capabilities) is list and len(capabilities) == len(CAPABILITIES))
    seen = set()
    for capability in capabilities:
        _keys(capability, ("id", "context", "status", "evidence"))
        name = capability["id"]
        _require(type(name) is str and name in CAPABILITIES and name not in seen)
        _require(capability["context"] == CAPABILITIES[name] and
                 capability["status"] == "unknown" and capability["evidence"] is None)
        seen.add(name)
    return record


@dataclass(frozen=True)
class DiagnosticResult:
    status: str
    send_complete: bool = False
    bytes_sent: int = 0
    bytes_received: int = 0
    artifact_size: Optional[int] = None
    artifact_sha256: Optional[str] = None
    prefix_size: Optional[int] = None
    prefix_sha256: Optional[str] = None
    report: Optional[dict] = None


def send_loopback_diagnostic(host, port, artifact_path, *, expected_sha256,
                             expected_size, timeout_seconds=5.0):
    """Send data only to a numeric loopback fixture; never retry or attest execution."""
    try:
        _require(type(host) is str and "%" not in host)
        address = ipaddress.ip_address(host)
        _require(address.is_loopback)
        _require(_integer(port, 1, 65535))
        _require(type(timeout_seconds) in (int, float) and 0.01 <= timeout_seconds <= 60)
        artifact = inspect_artifact(artifact_path, expected_sha256=expected_sha256,
                                    expected_size=expected_size)
    except (OSError, ValueError, TypeError):
        return DiagnosticResult("preflight_rejected")

    sent = 0
    received = bytearray()
    send_complete = False

    def result(status, report=None):
        return DiagnosticResult(status, send_complete, sent, len(received),
                                artifact.size, artifact.sha256, artifact.extent,
                                artifact.prefix_sha256, report)

    deadline = time.monotonic() + timeout_seconds
    stream = None
    phase = "connect"
    try:
        family = socket.AF_INET if address.version == 4 else socket.AF_INET6
        stream = socket.socket(family, socket.SOCK_STREAM)

        def remaining():
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError()
            stream.settimeout(left)

        remaining()
        endpoint = (str(address), port) if address.version == 4 else (str(address), port, 0, 0)
        stream.connect(endpoint)
        phase = "send"
        prefix = memoryview(artifact.data)[:artifact.extent]
        while sent < len(prefix):
            remaining()
            count = stream.send(prefix[sent:])
            if count <= 0 or count > len(prefix) - sent:
                return result("send_failed")
            sent += count
        send_complete = True
        remaining()
        stream.shutdown(socket.SHUT_WR)
        phase = "response"
        while len(received) <= MAX_REPORT_BYTES:
            remaining()
            chunk = stream.recv(MAX_REPORT_BYTES + 1 - len(received))
            received.extend(chunk)
            if time.monotonic() >= deadline:
                raise TimeoutError()
            if not chunk:
                if not received:
                    return result("peer_closed_empty")
                try:
                    report = parse_report(bytes(received))
                except ValueError:
                    return result("response_invalid")
                return result("report_received_unverified", report)
        return result("response_overflow")
    except (OSError, TimeoutError) as error:
        if phase == "response":
            return result("response_timeout" if isinstance(error, (TimeoutError, socket.timeout))
                          else "response_failed")
        return result("connect_failed" if phase == "connect" else "send_failed")
    finally:
        if stream is not None:
            stream.close()
