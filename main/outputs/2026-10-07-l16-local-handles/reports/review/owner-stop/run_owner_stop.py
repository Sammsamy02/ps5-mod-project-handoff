#!/usr/bin/env python3
"""Run exactly four real-owner cleanup cases, retaining source/command receipts."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
CANDIDATE = HERE.parents[2]
WORKSPACE = CANDIDATE.parents[1]
BRIDGE = CANDIDATE / "units/runtime_start_bridge_r14_diag_spanpub.cpp"
FIXTURE = CANDIDATE / "tests/test_retained.cpp"
OWNER = WORKSPACE / "outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp"
SUPPLIER = WORKSPACE / "outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp"
ACTIVATION_INCLUDE = WORKSPACE / ".superpowers/native-dependency-spike/native-coherent-runtime-1/source/activation"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def section(text, start, end):
    begin = text.index(start)
    return text[begin:text.index(end, begin)]


def execute(argv):
    result = subprocess.run(argv, capture_output=True, text=True, timeout=30)
    return {"argv": argv, "exit": result.returncode,
            "stdout": result.stdout, "stderr": result.stderr}


def main():
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
    run_dir = HERE / "runs" / run_id
    run_dir.mkdir(parents=True)
    bridge = BRIDGE.read_text()
    fixture = FIXTURE.read_text()
    sink = section(bridge, "// ---- HOST-TEST-EXTRACT-BEGIN", "// Legacy explicit diagnostics only.")
    sink = sink.replace("std::chrono::steady_clock::now()", "TestClock::now()")
    stop = section(bridge, 'extern "C" int modbridge_runtime_stop(void)',
                   'extern "C" int modbridge_runtime_start_with_activation')
    supplier = section(SUPPLIER.read_text(), "auto StopRuntimeWithAck(",
                       "\n#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST")
    supplier = supplier.replace("StopRuntimeWithAck", "RealStopRuntimeWithAck", 1)
    adapters = fixture[:fixture.index("namespace modbridge { namespace native {")]
    adapters = "#include <atomic>\nstatic std::atomic_bool* review_done=nullptr;static int close_before_join=0;\n" + adapters
    needle = "live_fds.erase(std::remove"
    if adapters.count(needle) != 1:
        raise ValueError("close adapter changed: expected one instrumentation site")
    adapters = adapters.replace(needle,
        "if(review_done&&!review_done->load())++close_before_join;\n    " + needle, 1)
    generated = {"sink_extract.inc": sink, "stop_extract.inc": stop,
                 "supplier_stop.inc": supplier, "kernel_adapters.inc": adapters}
    for name, text in generated.items():
        (run_dir / name).write_text(text)

    inputs = [BRIDGE, FIXTURE, OWNER, OWNER.parent / "runtime_owner.hpp", SUPPLIER,
              ACTIVATION_INCLUDE / "modforge_activation.h", HERE / "test_owner_stop.cpp",
              Path(__file__).resolve()]
    receipt = {"scope": "four actual selected RuntimeOwner cleanup cases; local host only",
               "run_id": run_id, "console_contact": False,
               "production_or_main_fixture_modified": False,
               "source_sha256": sha(BRIDGE),
               "inputs": [{"path": str(path), "sha256": sha(path)} for path in inputs],
               "generated": [{"path": str(run_dir / name), "sha256": sha(run_dir / name)}
                             for name in generated],
               "seams": ["Sony I/O adapts to real host descriptors",
                         "clock-only substitution in extracted sink region",
                         "close adapter records any close before actual worker completion",
                         "supplier symbol renamed with body unchanged; wrapper injects exception or negative result",
                         "null acknowledgement sink; unused mfa_status link shims"],
               "runs": []}
    for name, extra in [("plain", []), ("san", ["-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"])]:
        binary = run_dir / name
        compile_argv = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-function", "-O1", *extra,
                        "-I", str(run_dir), "-I", str(CANDIDATE / "units"),
                        "-I", str(OWNER.parent), "-I", str(ACTIVATION_INCLUDE),
                        str(HERE / "test_owner_stop.cpp"), str(OWNER), "-pthread",
                        "-o", str(binary)]
        compiled = execute(compile_argv)
        executed = execute([str(binary), str(run_dir / (name + "-sinks"))]) if compiled["exit"] == 0 else None
        passed = (compiled["exit"] == 0 and executed["exit"] == 0 and
                  executed["stdout"].count("PASS ") == 4 and
                  "failures=0" in executed["stdout"] and not executed["stderr"])
        row = {"name": name, "compile": compiled, "run": executed,
               "assertions": executed["stdout"].count("PASS ") if executed else 0,
               "pass": bool(passed), "binary_sha256": sha(binary) if binary.exists() else None}
        receipt["runs"].append(row)
        print(json.dumps({"name": name, "compile_exit": compiled["exit"],
                          "run_exit": executed["exit"] if executed else None,
                          "assertions": row["assertions"], "pass": bool(passed)}))
    receipt["pass"] = all(row["pass"] for row in receipt["runs"])
    output = json.dumps(receipt, indent=2) + "\n"
    (run_dir / "RECEIPT.json").write_text(output)
    (HERE / "LATEST-RECEIPT.json").write_text(output)
    print("receipt=" + str(run_dir / "RECEIPT.json"))
    return 0 if receipt["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
