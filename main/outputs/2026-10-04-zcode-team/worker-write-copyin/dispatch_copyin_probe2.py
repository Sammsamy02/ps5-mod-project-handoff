#!/usr/bin/env python3
"""dispatch_copyin_probe2.py — worker-write-copyin, 2026-10-06. DISPATCH 2
(fixed-build retry 1 of the campaign's 2-retry budget; dispatch 1 raw
evidence preserved untouched).

Same bounded route as dispatch 1, adapted paths:
  upload ONCE  -> /data/pldmgr/payloads/PAYLOAD_COPYIN_PROBE2/*.elf
  record       -> /data/modbridge-copyin2.jsonl (pre-image isolated)
  case files   -> /data/modbridge-copyin2-{c1,c2,c3,m1,bh2,bn2,bd2,bhi2}.bin
Host-side verification law (pattern A from offset 0 of each source buffer):
  c1/c2/c3/m1 expected 256 B of 'A'+(j%26); bh2/bn2/bd2/bhi2 expected EMPTY
  on kernel refusal (non-empty recorded as-is). Host byte comparison is
  independent of the payload's on-target memcmp.

Never game-process evidence; no game files or ploader.ini touched.
One dispatch per invocation; no internal retries.
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
ALIAS = "PAYLOAD_COPYIN_PROBE2"
REMOTE_ELF = f"{FTP}/data/pldmgr/payloads/{ALIAS}/{ALIAS}.elf"
REMOTE_JSONL = f"{FTP}/data/modbridge-copyin2.jsonl"
CASES = ["c1", "c2", "c3", "m1", "bh2", "bn2", "bd2", "bhi2"]
CURL = ["curl", "-sS", "--connect-timeout", "10"]
COLLECT_BUDGET_S = 90
POLL_INTERVAL_S = 5

steps = []


def sh(name, argv, timeout=90, check=True):
    proc = subprocess.run([str(a) for a in argv], capture_output=True, text=True,
                          timeout=timeout, cwd=str(WORKER))
    (WORKER / "logs" / f"dispatch2-{name}.log").write_text(
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
    if case in ("c1", "c2", "c3", "m1"):
        return bytes(65 + (j % 26) for j in range(256)), True
    return b"", False


def finish(status, note):
    (WORKER / "results/dispatch-receipt2.json").write_text(
        json.dumps({"status": status, "note": note, "steps": steps}, indent=1) + "\n")
    print(f"== {status}: {note}")
    sys.exit(0 if status == "PASS" else 1)


def main():
    receipt = json.loads((WORKER / "build/build-receipt2.json").read_text())
    local = Path(receipt["builds"]["prospero"]["path"])
    pinned = receipt["builds"]["prospero"]["sha256"]
    build_id = receipt["builds"]["prospero"]["build_id"]
    elf_bytes = local.read_bytes()
    if sha256_bytes(elf_bytes) != pinned:
        finish("REFUSED", "local ELF drifted from its pinned sha before upload")

    body = sh("g0-list-payloads", CURL + [f"{HTTP}/list_payloads"], check=False)
    if '"payloads"' not in body:
        finish("FAIL", "G0: pldmgr 8084 did not answer with a payload catalog; "
                       "console presumed DOWN; no dispatch")
    print("G0 OK: payload catalog present")

    pre = subprocess.run(CURL + [REMOTE_JSONL], capture_output=True, text=True, timeout=60)
    pre_size = len(pre.stdout.encode()) if pre.returncode == 0 else -1
    steps.append({"step": "pre-image-jsonl", "exit_code": pre.returncode,
                  "pre_size_bytes": pre_size})
    print(f"pre-image: rc={pre.returncode} size={pre_size}")

    sh("upload-elf-once", CURL + ["--ftp-create-dirs", "-T", local, REMOTE_ELF],
       timeout=120)

    back = subprocess.run(CURL + [REMOTE_ELF], capture_output=True, timeout=120).stdout
    got = sha256_bytes(back)
    steps.append({"step": "readback-identity", "sha256": got,
                  "bytes": len(back), "matches_local": got == pinned})
    if got != pinned or len(back) != len(elf_bytes):
        finish("FAIL", f"readback identity MISMATCH got {got} ({len(back)} B), "
                       f"pinned {pinned} ({len(elf_bytes)} B); no dispatch")
    print(f"readback identity OK {got[:12]} ({len(back)} B)")

    disp = sh("dispatch-loadpayload",
              CURL + ["--max-time", "60",
                      f"{HTTP}/loadpayload:/data/pldmgr/payloads/{ALIAS}/{ALIAS}.elf"],
              timeout=90, check=False)
    steps.append({"step": "dispatch-response-head", "body_head": disp[:200]})

    deadline = time.time() + COLLECT_BUDGET_S
    record_text = None
    while time.time() < deadline:
        time.sleep(POLL_INTERVAL_S)
        gotj = subprocess.run(CURL + [REMOTE_JSONL], capture_output=True,
                              text=True, timeout=60)
        if gotj.returncode == 0 and len(gotj.stdout.encode()) > max(pre_size, 0):
            record_text = gotj.stdout[pre_size if pre_size > 0 else 0:]
            if '"v_verdict"' in record_text:
                break
    if record_text is None:
        finish("FAIL", f"no new JSONL bytes within {COLLECT_BUDGET_S}s collection "
                       f"budget; payload-run/write failure reported as-is (NOT retried)")
    time.sleep(1)
    (WORKER / "results/copyin2-console-record.jsonl").write_text(record_text)

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

    case_dir = WORKER / "results/case-files2"
    case_dir.mkdir(parents=True, exist_ok=True)
    file_results = {}
    for case in CASES:
        url = f"{FTP}/data/modbridge-copyin2-{case}.bin"
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
        "dispatch": 2,
        "evidence_class": "separate-process standalone payload (pldmgr /loadpayload); "
                          "NEVER game-process",
        "build_id": build_id,
        "elf_sha256": pinned,
        "elf_bytes": len(elf_bytes),
        "uploaded_once": True,
        "dispatched_once": True,
        "console_record": "results/copyin2-console-record.jsonl",
        "record_lines": len(checks),
        "parse_errors": parse_errors,
        "all_checks_decisive": all(c["decisive"] for c in checks.values()),
        "on_target_verdict": verdict,
        "case_files_host_verification": file_results,
        "case_files_dir": "results/case-files2",
    }
    (WORKER / "results/console-run2.json").write_text(json.dumps(result, indent=1) + "\n")
    finish("PASS", f"dispatch 2 complete; on_target_verdict={verdict} "
                   f"record_lines={len(checks)} parse_errors={len(parse_errors)}")


if __name__ == "__main__":
    main()
