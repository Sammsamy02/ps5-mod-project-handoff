#!/usr/bin/env python3
"""dispatch_copyin_probe.py — worker-write-copyin, 2026-10-06.

ONE bounded console dispatch of the already-built standalone copyin-probe
ELF (reuses the proven worker-readable-source dispatch route):

  0. G0 reachability: pldmgr 8084 /list_payloads (never probe alternates).
  1. Pre-image of /data/modbridge-copyin.jsonl (usually absent; recorded so
     collected bytes isolate to THIS dispatch).
  2. Upload EXACTLY ONCE via FTP STOR (--ftp-create-dirs) to
     /data/pldmgr/payloads/PAYLOAD_COPYIN_PROBE/PAYLOAD_COPYIN_PROBE.elf
  3. Readback identity gate: FTP fetch + sha256 must equal the local ELF's
     pinned sha (build id from build-receipt.json). Mismatch -> STOP.
  4. Dispatch EXACTLY ONCE via GET /loadpayload:<path>. Never re-sent.
  5. Bounded collection: poll the JSONL every 5 s for at most 90 s, then
     fetch the eight owned case files and HOST-VERIFY their bytes against
     the pattern law:
        page A byte i = 'A' + (i%26); page B byte i = 'a' + (i%26)
        a1 = A[0x100:0x200]  a2 = B[0x100:0x200]  as = A[0x40:0x48]
        x  = A[0xFF8:0x1000] + B[0:8]
        bh/bn/bd/bhi expected EMPTY on refusal (non-empty recorded as-is).
     Host-side byte comparison is INDEPENDENT of the payload's own on-target
     memcmp — two verifications of the same file bytes.

Evidence class: SEPARATE-PROCESS standalone payload run by pldmgr.
NEVER game-process evidence; no game files or ploader.ini touched; the
payload touches only its own /data/modbridge-copyin* files.

One dispatch per invocation. No retries inside this script: any failure is
recorded and stops (the campaign's retry budget is spent only on a fixed
build/route, tracked in results/campaign-log.json by the worker).
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
ALIAS = "PAYLOAD_COPYIN_PROBE"
REMOTE_ELF = f"{FTP}/data/pldmgr/payloads/{ALIAS}/{ALIAS}.elf"
REMOTE_JSONL = f"{FTP}/data/modbridge-copyin.jsonl"
CASES = ["a1", "a2", "as", "x", "bh", "bn", "bd", "bhi"]
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


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def expected_bytes(case):
    if case == "a1":
        return bytes(65 + ((0x100 + j) % 26) for j in range(256)), True
    if case == "a2":
        return bytes(97 + ((0x100 + j) % 26) for j in range(256)), True
    if case == "as":
        return bytes(65 + ((0x40 + j) % 26) for j in range(8)), True
    if case == "x":
        return (bytes(65 + ((0xFF8 + j) % 26) for j in range(8)) +
                bytes(97 + (j % 26) for j in range(8))), True
    return b"", False  # bh/bn/bd/bhi: empty expected on refusal (non-empty = recorded honestly)


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
    if sha256_bytes(elf_bytes) != pinned:
        finish("REFUSED", "local ELF drifted from its pinned sha before upload")

    # 0. reachability
    body = sh("g0-list-payloads", CURL + [f"{HTTP}/list_payloads"], check=False)
    if '"payloads"' not in body:
        finish("FAIL", "G0: pldmgr 8084 did not answer with a payload catalog; "
                       "console presumed DOWN; no dispatch")
    print("G0 OK: payload catalog present")

    # 1. pre-image
    pre = subprocess.run(CURL + [REMOTE_JSONL], capture_output=True, text=True, timeout=60)
    pre_size = len(pre.stdout.encode()) if pre.returncode == 0 else -1
    steps.append({"step": "pre-image-jsonl", "exit_code": pre.returncode,
                  "pre_size_bytes": pre_size})
    print(f"pre-image: rc={pre.returncode} size={pre_size}")

    # 2. upload EXACTLY ONCE
    sh("upload-elf-once", CURL + ["--ftp-create-dirs", "-T", local, REMOTE_ELF],
       timeout=120)

    # 3. readback identity gate
    back = subprocess.run(CURL + [REMOTE_ELF], capture_output=True, timeout=120).stdout
    got = sha256_bytes(back)
    steps.append({"step": "readback-identity", "sha256": got,
                  "bytes": len(back), "matches_local": got == pinned})
    if got != pinned or len(back) != len(elf_bytes):
        finish("FAIL", f"readback identity MISMATCH got {got} ({len(back)} B), "
                       f"pinned {pinned} ({len(elf_bytes)} B); no dispatch")
    print(f"readback identity OK {got[:12]} ({len(back)} B)")

    # 4. dispatch EXACTLY ONCE
    disp = sh("dispatch-loadpayload",
              CURL + ["--max-time", "60",
                      f"{HTTP}/loadpayload:/data/pldmgr/payloads/{ALIAS}/{ALIAS}.elf"],
              timeout=90, check=False)
    steps.append({"step": "dispatch-response-head", "body_head": disp[:200]})

    # 5. bounded collection of the record JSONL.
    # The payload appends ONE COMPLETE LINE PER CHECK (crash-safe prefix
    # evidence), so growth alone does NOT mean completion: wait for the final
    # d_scope line (or budget expiry, then fetch whatever prefix exists and
    # record it honestly — mid-write case files are caught by the byte
    # verification below).
    deadline = time.time() + COLLECT_BUDGET_S
    record_text = None
    while time.time() < deadline:
        time.sleep(POLL_INTERVAL_S)
        gotj = subprocess.run(CURL + [REMOTE_JSONL], capture_output=True,
                              text=True, timeout=60)
        if gotj.returncode == 0 and len(gotj.stdout.encode()) > max(pre_size, 0):
            record_text = gotj.stdout[pre_size if pre_size > 0 else 0:]
            if '"d_scope"' in record_text:
                break
    if record_text is None:
        finish("FAIL", f"no new JSONL bytes within {COLLECT_BUDGET_S}s collection "
                       f"budget; payload-run/write failure reported as-is (NOT retried)")
    time.sleep(1)  # settle: last fclose precedes case-file fetches
    (WORKER / "results/copyin-console-record.jsonl").write_text(record_text)

    # Every line must parse (per-check lines: any crash prefix is still evidence).
    checks = {}
    parse_errors = []
    for i, line in enumerate(record_text.strip().splitlines()):
        try:
            obj = json.loads(line)
            checks[obj.get("check")] = {"decisive": obj.get("decisive"),
                                        "detail": obj.get("detail")}
        except json.JSONDecodeError as e:
            parse_errors.append({"line": i, "error": str(e)})
    verdict = checks.get("v_verdict", {}).get("detail", {}).get("verdict", "absent")

    # 6. fetch + HOST-VERIFY the owned case files (independent of on-target memcmp)
    case_dir = WORKER / "results/case-files"
    case_dir.mkdir(parents=True, exist_ok=True)
    file_results = {}
    for case in CASES:
        url = f"{FTP}/data/modbridge-copyin-{case}.bin"
        f = subprocess.run(CURL + [url], capture_output=True, timeout=60)
        data = f.stdout if f.returncode == 0 else b""
        (case_dir / f"{case}.bin").write_bytes(data)
        want, strict = expected_bytes(case)
        entry = {"bytes": len(data), "sha256": sha256_bytes(data)}
        if strict:
            entry["matches_expected"] = (data == want)
            entry["expect_bytes"] = len(want)
        else:
            entry["expect_empty_on_refusal"] = True
            entry["nonempty"] = (len(data) > 0)
            if data:
                entry["head_hex"] = data[:32].hex()
        file_results[case] = entry
        steps.append({"step": f"fetch-case-{case}", "exit_code": f.returncode,
                      "bytes": len(data)})

    result = {
        "worker": "worker-write-copyin",
        "evidence_class": "separate-process standalone payload (pldmgr /loadpayload); "
                          "NEVER game-process",
        "build_id": build_id,
        "elf_sha256": pinned,
        "elf_bytes": len(elf_bytes),
        "uploaded_once": True,
        "dispatched_once": True,
        "console_record": "results/copyin-console-record.jsonl",
        "record_lines": len(checks),
        "parse_errors": parse_errors,
        "all_checks_decisive": all(c["decisive"] for c in checks.values()),
        "on_target_verdict": verdict,
        "case_files_host_verification": file_results,
        "case_files_dir": "results/case-files",
    }
    (WORKER / "results/console-run.json").write_text(json.dumps(result, indent=1) + "\n")
    finish("PASS", f"dispatch complete; on_target_verdict={verdict} "
                   f"record_lines={len(checks)} parse_errors={len(parse_errors)}")


if __name__ == "__main__":
    main()
