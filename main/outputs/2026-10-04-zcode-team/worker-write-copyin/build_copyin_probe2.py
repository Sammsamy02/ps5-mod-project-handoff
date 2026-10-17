#!/usr/bin/env python3
"""build_copyin_probe2.py — worker-write-copyin, 2026-10-06. BUILD 2
(fixed-build retry for the dispatch-1 all-EFAULT source-region anomaly).

  * hostmac logic build (-DHOST_COPYIN2, target marker hostmac-logic-only):
    validates the ladder logic, classification and JSONL framing on macOS —
    clearly labeled logic-only, never console evidence.
  * prospero build (same proven link class as build 1).

Symbol gate identical to build 1 (T or U accepted; U = libkernel dynamic
import class proven on-target). Bounded: 2 retries per failed step.
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
SOURCE = WORKER / "src/payload_copyin_probe2.cpp"

MAX_BUILD_RETRIES = 2
REQUIRED_SYMBOLS = ["open", "write", "read", "close", "mmap", "munmap", "mprotect"]

records = []


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(name, argv, env=None, timeout=240, retries=None, ok_codes=(0,)):
    retries = MAX_BUILD_RETRIES if retries is None else retries
    logs = WORKER / "logs"
    logs.mkdir(exist_ok=True)
    for attempt in range(1 + retries):
        log = logs / f"{name}.attempt{attempt + 1}.log"
        started = time.time()
        proc = subprocess.run([str(a) for a in argv], cwd=str(WORKER), env=env,
                              capture_output=True, text=True, timeout=timeout)
        log.write_text(proc.stdout + proc.stderr)
        records.append({"name": f"{name}.attempt{attempt + 1}",
                        "argv": [str(a) for a in argv],
                        "exit_code": proc.returncode, "log": str(log),
                        "seconds": round(time.time() - started, 2)})
        if proc.returncode in ok_codes:
            return
        print(f"{name} attempt {attempt + 1} FAILED (rc={proc.returncode}), log={log}",
              file=sys.stderr)
    raise SystemExit(f"BUILD FAILED after {1 + MAX_BUILD_RETRIES} bounded attempts: {name}")


COMMON = ["-std=c++20", "-O1", "-Wall", "-Wextra"]


def build_host():
    out = WORKER / "host/payload_copyin_probe2-host"
    run("host2-build", ["/usr/bin/clang++", *COMMON, "-DHOST_COPYIN2",
                        SOURCE, "-o", out])
    # hostmac LOGIC gate (never console evidence): macOS has no /data, so the
    # payload's honest behavior is open ENOENT -> decisive=false on file cases
    # -> verdict INCONCLUSIVE. The gate requires exactly that end-to-end
    # behavior plus the /dev/null vacuous controls (rc==8 both sources), with
    # EVERY line valid JSON. Exit code 1 is the EXPECTED all_decisive=false.
    host_record = WORKER / "host/host-copyin2.jsonl"
    last_err = None
    for attempt in range(1 + MAX_BUILD_RETRIES):
        host_record.unlink(missing_ok=True)   # fresh record per attempt
        run(f"host2-run.attempt{attempt + 1}", [out, host_record], timeout=60, retries=0, ok_codes=(0, 1))
        lines = host_record.read_text().splitlines()
        try:
            parsed = [json.loads(l) for l in lines]
        except json.JSONDecodeError as e:
            last_err = f"JSON invalid: {e}"
            continue
        by_id = {p.get("check"): p for p in parsed}
        ok = (
            bool(parsed)
            and all(p.get("target") == "hostmac-logic-only" for p in parsed)
            and by_id.get("v_verdict", {}).get("detail", {}).get("verdict") == "INCONCLUSIVE"
            and by_id.get("v_verdict", {}).get("detail", {}).get("note", "").startswith("setup failed")
            and by_id.get("d_devnull_controls", {}).get("decisive") is True
            and by_id.get("d_devnull_controls", {}).get("detail", {}).get("devnull_mmap_src", {}).get("rc") == 8
            and by_id.get("d_devnull_controls", {}).get("detail", {}).get("devnull_bss_src", {}).get("rc") == 8
            and by_id.get("0_setup", {}).get("detail", {}).get("hole_ok") is False  # macOS munmap EINVAL honestly recorded
        )
        if ok:
            records.append({"name": "host2-logic-gate", "exit_code": 0,
                            "lines": len(lines),
                            "note": "end-to-end logic validated under macOS conditions; "
                                    "INCONCLUSIVE is the correct host verdict"})
            return out
        last_err = f"logic gate unmet: verdict={by_id.get('v_verdict')}"
    raise SystemExit(f"HOST LOGIC GATE FAILED after bounded attempts: {last_err}")


def build_prospero():
    out = WORKER / "build/payload_copyin_probe2.elf"
    env = dict(os.environ, LLVM_CONFIG=str(LLVM_CONFIG))
    run("prospero2-build", [SDK / "bin/prospero-clang++", *COMMON,
                            "-Wl,--build-id=none", SOURCE, "-o", out], env=env)
    nm = subprocess.run([str(LLVM_BIN / "llvm-nm"), str(out)],
                        capture_output=True, text=True, env=env)
    seen = {}
    for l in nm.stdout.splitlines():
        parts = l.split()
        if len(parts) == 3 and parts[1] in ("T", "U"):
            sym, kind = parts[2], parts[1]
        elif len(parts) == 2 and parts[0] in ("T", "U"):
            sym, kind = parts[1], parts[0]
        else:
            continue
        if sym in REQUIRED_SYMBOLS:
            seen.setdefault(sym, set()).add(kind)
    (WORKER / "build/payload-symbols2.txt").write_text(
        "\n".join(f"{'|'.join(sorted(k))} {s}" for s, k in sorted(seen.items())) + "\n")
    records.append({"name": "prospero2-symbol-evidence", "exit_code": 0,
                    "required_symbols_seen": sorted(seen)})
    missing = [s for s in REQUIRED_SYMBOLS if s not in seen]
    if missing:
        raise SystemExit(f"REFUSED: required wrappers absent (T or U): {missing}")
    return out


def main():
    t0 = time.time()
    src_sha = sha256(SOURCE)
    records.append({"name": "pin-source2", "exit_code": 0,
                    "sha256": src_sha, "bytes": SOURCE.stat().st_size})
    host_bin = build_host()
    prospero_bin = build_prospero()
    builds = {
        "host": {"path": str(host_bin), "sha256": sha256(host_bin),
                 "build_id": "host-" + sha256(host_bin)[:12]},
        "prospero": {"path": str(prospero_bin), "sha256": sha256(prospero_bin),
                     "bytes": prospero_bin.stat().st_size,
                     "build_id": "prospero-" + sha256(prospero_bin)[:12]},
    }
    receipt = {
        "worker": "worker-write-copyin",
        "build": 2,
        "reason": "fixed-build retry: dispatch-1 all-EFAULT address-independent anomaly "
                  "contradicted by same-process stdio copyin success; source-region-class ladder",
        "utc_started_epoch": t0,
        "utc_finished_epoch": time.time(),
        "source": {"path": str(SOURCE), "sha256": src_sha},
        "commands": records,
        "builds": builds,
        "retries_allowed": MAX_BUILD_RETRIES,
        "labels": {"evidence_class": "separate-process standalone payload",
                   "never": "game-process; no ploader.ini; no game files"},
    }
    (WORKER / "build/build-receipt2.json").write_text(json.dumps(receipt, indent=1) + "\n")
    print(json.dumps(builds, indent=1))
    print("elapsed_s:", round(time.time() - t0, 1))


if __name__ == "__main__":
    main()
