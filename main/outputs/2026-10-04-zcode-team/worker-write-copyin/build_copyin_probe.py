#!/usr/bin/env python3
"""build_copyin_probe.py — worker-write-copyin, 2026-10-06.

Receipted build of the write(2)-copyin probe payload (reuses the proven
worker-readable-source build route exactly):
  * host build (macOS /usr/bin/clang++, -DHOST_COPYIN): pure
    classify_write()/code_name() table + line framing; kernel probes
    compiled out. Its JSONL output is parsed line-by-line as a validity gate.
  * prospero build (ps5-payload-sdk prospero-clang++, -Wl,--build-id=none
    per the argv-probe determinism finding).

Symbol-evidence gate: every required wrapper must be present in the prospero
ELF symbol table as T (static libc.a syscalls.o stub) or U (libkernel.so
dynamic import — the class worker-readable-source PROVED working on-target:
open('/dev/null')=fd and ::write(rc=8) ran through exactly these dynamic
stubs). mmap/mprotect are T per prior evidence; open/write/read/close/munmap
are U.

Every command receipted (argv + exit code + log). Build ids = first 12 hex
of sha256. Bounded: at most TWO retries of a FAILED step, never a replay of
a succeeded step. No console contact here; dispatch is a separate step.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import time

WORKER = Path(__file__).resolve().parent
WS = WORKER.parents[2]

SDK = WS / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
LLVM_CONFIG = WS / ("outputs/ps5-mod-bridge/native-probe/build/deps/llvm/"
                    "LLVM-21.1.8-macOS-ARM64/bin/llvm-config")
LLVM_BIN = LLVM_CONFIG.parent
SOURCE = WORKER / "src/payload_copyin_probe.cpp"

MAX_BUILD_RETRIES = 2
REQUIRED_SYMBOLS = ["open", "write", "read", "close", "mmap", "munmap", "mprotect"]

records = []


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(name, argv, env=None, timeout=240):
    logs = WORKER / "logs"
    logs.mkdir(exist_ok=True)
    attempts = []
    for attempt in range(1 + MAX_BUILD_RETRIES):
        log = logs / f"{name}.attempt{attempt + 1}.log"
        started = time.time()
        proc = subprocess.run([str(a) for a in argv], cwd=str(WORKER), env=env,
                              capture_output=True, text=True, timeout=timeout)
        log.write_text(proc.stdout + proc.stderr)
        attempts.append({"attempt": attempt + 1, "log": str(log.relative_to(WORKER)),
                         "exit_code": proc.returncode,
                         "seconds": round(time.time() - started, 2)})
        records.append({"name": f"{name}.attempt{attempt + 1}",
                        "argv": [str(a) for a in argv],
                        "exit_code": proc.returncode, "log": str(log)})
        if proc.returncode == 0:
            return attempts
        print(f"{name} attempt {attempt + 1} FAILED (rc={proc.returncode}), log={log}",
              file=sys.stderr)
    raise SystemExit(f"BUILD FAILED after {1 + MAX_BUILD_RETRIES} bounded attempts: {name}")


COMMON = ["-std=c++20", "-O1", "-Wall", "-Wextra", "-I", WORKER / "src"]


def build_host():
    out = WORKER / "host/payload_copyin_probe-host"
    run("host-build", ["/usr/bin/clang++", *COMMON, "-DHOST_COPYIN",
                       SOURCE, "-o", out])
    host_record = WORKER / "host/host-copyin.jsonl"
    if host_record.exists():
        host_record.unlink()
    run("host-run", [out, host_record], timeout=60)
    # Gate: EVERY host line must parse as JSON (framing validity proof).
    lines = host_record.read_text().splitlines()
    parsed = []
    for i, line in enumerate(lines):
        try:
            parsed.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"HOST JSON INVALID at line {i}: {e}")
    if not parsed or any(p.get("target") != "host" for p in parsed):
        raise SystemExit("HOST JSONL missing or wrong target marker")
    records.append({"name": "host-json-validity-gate", "exit_code": 0,
                    "lines": len(lines), "all_parsed": True})
    return out


def build_prospero():
    out = WORKER / "build/payload_copyin_probe.elf"
    env = dict(os.environ, LLVM_CONFIG=str(LLVM_CONFIG))
    run("prospero-build", [SDK / "bin/prospero-clang++", *COMMON,
                           "-Wl,--build-id=none", SOURCE, "-o", out], env=env)
    nm = subprocess.run([str(LLVM_BIN / "llvm-nm"), str(out)],
                        capture_output=True, text=True, env=env)
    seen = {}
    for l in nm.stdout.splitlines():
        parts = l.split()
        # U lines: "U name" (2 fields); T lines: "addr T name" (3 fields).
        if len(parts) == 3 and parts[1] in ("T", "U"):
            sym, kind = parts[2], parts[1]
        elif len(parts) == 2 and parts[0] in ("T", "U"):
            sym, kind = parts[1], parts[0]
        else:
            continue
        if sym in REQUIRED_SYMBOLS:
            seen.setdefault(sym, set()).add(kind)
    interesting = [f"{'|'.join(sorted(kinds))} {s}" for s, kinds in sorted(seen.items())]
    (WORKER / "build/payload-symbols.txt").write_text("\n".join(interesting) + "\n")
    records.append({"name": "prospero-symbol-evidence", "exit_code": 0,
                    "required_symbols_seen": interesting,
                    "out": "build/payload-symbols.txt"})
    missing = [s for s in REQUIRED_SYMBOLS if s not in seen]
    if missing:
        raise SystemExit(f"REFUSED: required syscall wrappers absent (T or U): {missing}")
    return out


def main():
    t0 = time.time()
    src_sha = sha256(SOURCE)
    records.append({"name": "pin-source", "exit_code": 0,
                    "sha256": src_sha, "bytes": SOURCE.stat().st_size})
    host_bin = build_host()
    prospero_bin = build_prospero()
    builds = {
        "host": {"path": str(host_bin), "sha256": sha256(host_bin),
                 "bytes": host_bin.stat().st_size,
                 "build_id": "host-" + sha256(host_bin)[:12]},
        "prospero": {"path": str(prospero_bin), "sha256": sha256(prospero_bin),
                     "bytes": prospero_bin.stat().st_size,
                     "build_id": "prospero-" + sha256(prospero_bin)[:12]},
    }
    receipt = {
        "worker": "worker-write-copyin",
        "utc_started_epoch": t0,
        "utc_finished_epoch": time.time(),
        "source": {"path": str(SOURCE), "sha256": src_sha},
        "commands": records,
        "builds": builds,
        "retries_allowed": MAX_BUILD_RETRIES,
        "labels": {"evidence_class": "separate-process standalone payload",
                   "never": "game-process; no ploader.ini; no game files"},
    }
    (WORKER / "build/build-receipt.json").write_text(json.dumps(receipt, indent=1) + "\n")
    print(json.dumps(builds, indent=1))
    print("elapsed_s:", round(time.time() - t0, 1))


if __name__ == "__main__":
    main()
