#!/usr/bin/env python3
"""dispatch_selftest.py — worker-payload-selftest, 2026-10-05.

ONE bounded console dispatch of the already-built standalone payload ELF:

  0. G0 reachability: pldmgr 8084 /list_payloads (never probe alternates).
  1. Pre-image of /data/modbridge-selftest.jsonl (usually absent; recorded
     so collected bytes can be isolated to THIS dispatch).
  2. Upload EXACTLY ONCE via FTP STOR (--ftp-create-dirs) to
     /data/pldmgr/payloads/PAYLOAD_SELFTEST/PAYLOAD_SELFTEST.elf
  3. Readback identity gate: FTP fetch + sha256 must equal the local ELF's
     pinned sha (build id prosperino-<sha12> from build-receipt.json).
     Mismatch -> STOP, no dispatch, report honestly.
  4. Dispatch EXACTLY ONCE via GET /loadpayload:<path>. Never re-sent.
  5. Bounded collection: poll the JSONL every 5 s for at most 90 s, fetch,
     isolate the bytes after the pre-image size, tag with the build id.

Evidence class: SEPARATE-PROCESS standalone payload run by pldmgr.
NEVER game-process evidence; no game/console behavior claims.

One dispatch total. No retries of dispatch/upload/readback: any failure is
reported and stops (bounded terminal condition; retries are a BUILD-failure
budget, already unused).
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import time

WORKER = Path(__file__).resolve().parent
HTTP = "http://ps5-host-not-configured.invalid:8084"
FTP = "ftp://ps5-host-not-configured.invalid:2121"
ALIAS = "PAYLOAD_SELFTEST"
REMOTE_ELF = f"{FTP}/data/pldmgr/payloads/{ALIAS}/{ALIAS}.elf"
REMOTE_JSONL = f"{FTP}/data/modbridge-selftest.jsonl"
CURL = ["curl", "-sS", "--connect-timeout", "10"]
COLLECT_BUDGET_S = 90
POLL_INTERVAL_S = 5

steps = []


def sh(name, argv, timeout=90, check=True):
    proc = subprocess.run([str(a) for a in argv], capture_output=True, text=True,
                          timeout=timeout, cwd=str(WORKER))
    (WORKER / "logs" / f"dispatch-{name}.log").write_text(
        f"argv: {argv}\n\nstdout:\n{proc.stdout}\n\nstderr:\n{proc.stderr}\n")
    steps.append({"step": name, "argv": [str(a) for a in argv],
                  "exit_code": proc.returncode,
                  "stdout_head": proc.stdout[:400]})
    print(f"[{name}] rc={proc.returncode}")
    if check and proc.returncode != 0:
        finish("FAIL", f"step {name} failed rc={proc.returncode}")
    return proc.stdout


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def finish(status, note):
    (WORKER / "results/dispatch-receipt.json").write_text(
        json.dumps({"status": status, "note": note, "steps": steps}, indent=1) + "\n")
    print(f"== {status}: {note}")
    sys.exit(0 if status == "PASS" else 1)


def main():
    receipt = json.loads((WORKER / "build/build-receipt.json").read_text())
    local = Path(receipt["builds"]["prospero"]["path"])
    pinned = receipt["builds"]["prospero"]["sha256"]
    build_id = receipt["builds"]["prospero"]["build_id"]
    elf_bytes = local.read_bytes()
    if sha256(elf_bytes) != pinned:
        finish("REFUSED", "local ELF drifted from its pinned sha before upload")

    # 0. reachability
    body = sh("g0-list-payloads", CURL + [f"{HTTP}/list_payloads"], check=False)
    if '"payloads"' not in body:
        finish("FAIL", "G0: pldmgr 8084 did not answer with a payload catalog; console presumed DOWN; no dispatch")
    print("G0 OK: payload catalog present")

    # 1. pre-image
    pre = subprocess.run(CURL + [REMOTE_JSONL], capture_output=True, text=True, timeout=60)
    pre_size = len(pre.stdout.encode()) if pre.returncode == 0 else -1
    steps.append({"step": "pre-image-jsonl", "exit_code": pre.returncode,
                  "pre_size_bytes": pre_size})
    print(f"pre-image: rc={pre.returncode} size={pre_size}")

    # 2. upload EXACTLY ONCE
    sh("upload-elf-once", CURL + ["--ftp-create-dirs",
                                  "-T", local, REMOTE_ELF], timeout=120)

    # 3. readback identity gate
    back = subprocess.run(CURL + [REMOTE_ELF], capture_output=True, timeout=120).stdout
    got = sha256(back)
    steps.append({"step": "readback-identity", "sha256": got,
                  "bytes": len(back), "matches_local": got == pinned})
    if got != pinned or len(back) != len(elf_bytes):
        finish("FAIL", f"readback identity MISMATCH got {got} ({len(back)} B), pinned {pinned} ({len(elf_bytes)} B); no dispatch")
    print(f"readback identity OK {got[:12]} ({len(back)} B)")

    # 4. dispatch EXACTLY ONCE
    disp = sh("dispatch-loadpayload", CURL + ["--max-time", "60",
                                              f"{HTTP}/loadpayload:/data/pldmgr/payloads/{ALIAS}/{ALIAS}.elf"],
              timeout=90, check=False)
    steps.append({"step": "dispatch-response-head", "body_head": disp[:200]})

    # 5. bounded collection
    deadline = time.time() + COLLECT_BUDGET_S
    record_text = None
    while time.time() < deadline:
        time.sleep(POLL_INTERVAL_S)
        got = subprocess.run(CURL + [REMOTE_JSONL], capture_output=True, text=True, timeout=60)
        if got.returncode == 0 and len(got.stdout.encode()) > pre_size:
            record_text = got.stdout[pre_size if pre_size > 0 else 0:]
            break
    if record_text is None:
        finish("FAIL", f"no new JSONL bytes within {COLLECT_BUDGET_S}s collection budget; "
                       f"payload-run/write failure reported as-is (NOT retried)")
    (WORKER / "results/selftest-console-record.jsonl").write_text(record_text)
    try:
        record = json.loads(record_text.strip().splitlines()[-1])
    except json.JSONDecodeError as e:
        finish("FAIL", f"collected record is not valid JSON: {e}")
    if record.get("target") != "prospero":
        finish("FAIL", f"collected record target={record.get('target')!r}, expected prospero")

    verdicts = {c["id"]: c["status"] for c in record["checks"]}
    result = {
        "worker": "worker-payload-selftest",
        "evidence_class": "separate-process standalone payload (pldmgr /loadpayload); NEVER game-process",
        "build_id": build_id,
        "elf_sha256": pinned,
        "elf_bytes": len(elf_bytes),
        "uploaded_once": True,
        "dispatched_once": True,
        "console_record": "results/selftest-console-record.jsonl",
        "record_all_pass": record.get("all_pass"),
        "checks": verdicts,
        "sizeof_wchar_t_on_target": record.get("sizeof_wchar_t"),
        "source_main_lua_sha256": record.get("source", {}).get("main_lua_sha256"),
    }
    (WORKER / "results/console-run.json").write_text(json.dumps(result, indent=1) + "\n")
    finish("PASS", f"dispatch complete; record all_pass={record.get('all_pass')} {verdicts}")


if __name__ == "__main__":
    main()
