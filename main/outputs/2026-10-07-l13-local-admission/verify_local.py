#!/usr/bin/env python3
"""Run this candidate's local checks; network is denied on macOS.

No production harvest, PRX packaging, upload or console run is performed.
"""
import concurrent.futures
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(name, commands):
    results = []
    for argv in commands:
        actual = [str(x) for x in argv]
        # The harvest harness applies its own network-denied sandbox to the
        # actual script. macOS rejects installing a second nested sandbox.
        if name != "harvest" and Path("/usr/bin/sandbox-exec").exists():
            actual = ["/usr/bin/sandbox-exec", "-p", "(version 1) (allow default) (deny network*)"] + actual
        p = subprocess.run(actual, cwd=ROOT, capture_output=True, text=True)
        results.append(dict(argv=actual, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr))
        if p.returncode:
            break
    receipt = dict(name=name, passed=all(r["exit_code"] == 0 for r in results), commands=results)
    (out / (name + ".json")).write_text(json.dumps(receipt, indent=2) + "\n")
    return receipt


if __name__ == "__main__":
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    out = HERE / "receipts" / ("verification-" + stamp)
    out.mkdir()
    src_before = {str(p.relative_to(HERE)): sha(p) for p in (HERE / "units").iterdir() if p.is_file()}
    tasks = [
        ("admission-engine", [["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1",
                                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                                "-I" + str(HERE / "units"), HERE / "tests/admission_engine_test.cpp",
                                "-o", HERE / "tests/admission_engine_test_asan"],
                               [HERE / "tests/admission_engine_test_asan"]]),
        ("initializer-integration", [[sys.executable, HERE / "tests/initializer_admission_host.py"]]),
        ("sink-production", [[sys.executable, HERE / "sink-tests/run_sink_tests.py"]]),
        ("harvest", [[sys.executable, HERE / "tests/harvest/test_harvest.py", "--script", "/tmp/l12-harvest/harvest.sh",
                       "--receipts", out / "harvest-fixtures"], ["/bin/bash", "-n", HERE / "tools/harvest.sh"]]),
        ("ps5-unit-compilation", [[sys.executable, HERE / "compile_units.py"]]),
    ]
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        reports = list(pool.map(lambda x: run(*x), tasks))
    reports.append(run("import-boundary", [[sys.executable, HERE / "verify_import_boundary.py"]]))
    baseline = json.loads((HERE / "baseline.json").read_text())
    preserve = baseline["baseline"] + baseline["untouched_pad"] + [baseline["frozen_prx"]]
    preserved = [{"path": r["path"], "expected": r["sha256"],
                  "actual": sha(ROOT / r["path"])} for r in preserve]
    src_after = {str(p.relative_to(HERE)): sha(p) for p in (HERE / "units").iterdir() if p.is_file()}
    checks = {
        "local_test_and_compile_commands_pass": all(r["passed"] for r in reports),
        "candidate_sources_stable_during_verification": src_before == src_after,
        "l12_frozen_prx_and_pad_sources_preserved": all(p["expected"] == p["actual"] for p in preserved),
        "harvest_durable_copy_matches": sha(HERE / "tools/harvest.sh") == sha(Path("/tmp/l12-harvest/harvest.sh")),
    }
    gate = dict(status="PASS_MAC_ONLY" if all(checks.values()) else "FAIL", checks=checks,
                source_hashes=src_after, preserved=preserved,
                receipts=[str(out / (r["name"] + ".json")) for r in reports],
                console_contact=False, full_prx_packaged=False, console_proven=False,
                upload_gate="HOLD: no user authorization; full PRX packaging/import resolution not validated")
    (out / "GATES.json").write_text(json.dumps(gate, indent=2) + "\n")
    (HERE / "receipts/LATEST-GATES.json").write_text(json.dumps(gate, indent=2) + "\n")
    for r in reports:
        print(r["name"], "PASS" if r["passed"] else "FAIL")
        if not r["passed"]:
            for c in r["commands"]:
                print(c["stdout"][-3000:], c["stderr"][-3000:])
    print(gate["status"], out / "GATES.json")
    raise SystemExit(0 if all(checks.values()) else 1)
