#!/usr/bin/env python3
"""build_readable_source.py — worker-readable-source, 2026-10-05.

Receipted build of the kernel-mediated-read probe payload:
  * host build (macOS, /usr/bin/clang++, HOST_READABLESOURCE — the pure
    classify() table runs; kernel probes compiled out);
  * prospero build (ps5-payload-sdk prospero-clang++, x86_64-sie-ps5, proven
    /loadpayload standalone link class, -Wl,--build-id=none per the
    argv-probe determinism finding).

Every command is receipted (argv + exit code + log). Build ids are the first
12 hex chars of each binary's sha256.

Bounded retries: at most TWO retries of a FAILED build step (fresh log per
attempt, never a replay of a succeeded step), then stop and report.

Writes only inside this worker directory. No console contact here; dispatch
is a separate bounded step.
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
TEAM = WORKER.parent

SDK = WS / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
LLVM_CONFIG = WS / ("outputs/ps5-mod-bridge/native-probe/build/deps/llvm/"
                    "LLVM-21.1.8-macOS-ARM64/bin/llvm-config")
LLVM_BIN = LLVM_CONFIG.parent
SOURCE = WORKER / "src/payload_readable_source.cpp"
SOURCE_SHA = "PIN-SET-BELOW"  # pinned at first build, then verified

MAX_BUILD_RETRIES = 2

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
                         "exit_code": proc.returncode, "seconds": round(time.time() - started, 2)})
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
    out = WORKER / "host/payload_readable_source-host"
    run("host-build", ["/usr/bin/clang++", *COMMON, "-DHOST_READABLESOURCE",
                       SOURCE, "-o", out])
    # Run the host binary once; its record is a build artifact + sanity gate.
    host_record = WORKER / "host/host-readablesource.jsonl"
    if host_record.exists():
        host_record.unlink()
    run("host-run", [out, host_record], timeout=60)
    return out


def build_prospero():
    out = WORKER / "build/payload_readable_source.elf"
    env = dict(os.environ, LLVM_CONFIG=str(LLVM_CONFIG))
    run("prospero-build", [SDK / "bin/prospero-clang++", *COMMON,
                           "-Wl,--build-id=none", SOURCE, "-o", out], env=env)
    # Wrapper-convention evidence: `ptrace` and `syscall` must both be DEFINED
    # (T) in the payload itself (syscalls.o static link, same class as the
    # composed candidate's runtime.map `.text.syscall` entries).
    nm = subprocess.run([str(LLVM_BIN / "llvm-nm"), str(out)],
                        capture_output=True, text=True, env=env)
    interesting = [l for l in nm.stdout.splitlines()
                   if l.endswith(" ptrace") or l.endswith(" syscall")
                   or l.endswith(" __syscall")]
    (WORKER / "build/payload-symbols.txt").write_text("\n".join(interesting) + "\n")
    records.append({"name": "prospero-wrapper-evidence",
                    "exit_code": 0,
                    "defined_symbols": interesting,
                    "out": "build/payload-symbols.txt"})
    if not any(l.endswith(" ptrace") for l in interesting):
        raise SystemExit("REFUSED: ptrace is not DEFINED (T) in the prospero ELF")
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
        "worker": "worker-readable-source",
        "utc_started_epoch": t0,
        "utc_finished_epoch": time.time(),
        "source": {"path": str(SOURCE), "sha256": src_sha},
        "commands": records,
        "builds": builds,
        "retries_allowed": MAX_BUILD_RETRIES,
        "labels": {"evidence_class": "separate-process standalone payload",
                   "never": "game-process"},
    }
    (WORKER / "build/build-receipt.json").write_text(json.dumps(receipt, indent=1) + "\n")
    print(json.dumps(builds, indent=1))
    print("elapsed_s:", round(time.time() - t0, 1))


if __name__ == "__main__":
    main()
