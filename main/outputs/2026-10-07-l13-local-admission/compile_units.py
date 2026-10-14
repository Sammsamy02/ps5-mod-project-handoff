#!/usr/bin/env python3
"""Compile the two changed TUs locally using recorded l12 SDK arguments.

Does not invoke composition, packaging, deployment, or any console tool.
All compiler outputs and dependency files stay in this candidate directory.
"""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LAUNCH = ROOT / "outputs/2026-10-04-zcode-controller/launch11-combined"
RUN = ROOT / ".superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-l12-2"
BUILD = HERE / "build"
BUILD.mkdir(exist_ok=True)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def command_for(name, source, receipt_path, records):
    record = next(c for c in records if "-c" in c.get("argv", [])
                  and any(str(a).endswith(source) for a in c["argv"]))
    argv = list(record["argv"])
    argv[argv.index("-c") + 1] = str(HERE / "units" / name)
    obj = BUILD / (name + ".o")
    argv[argv.index("-o") + 1] = str(obj)
    if "-MF" in argv:
        argv[argv.index("-MF") + 1] = str(BUILD / (name + ".d"))
    else:
        argv.extend(["-MD", "-MF", str(BUILD / (name + ".d"))])
    argv[1:1] = ["-I", str(HERE / "units")]
    return {"source": name, "baseline_receipt": str(receipt_path),
            "baseline_receipt_sha256": digest(receipt_path),
            "argv": argv, "object": str(obj)}


def compile_one(item):
    item["source_sha256"] = digest(HERE / "units" / item["source"])
    llvm_config = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config"
    env = dict(os.environ, LLVM_CONFIG=str(llvm_config))
    item["environment_override"] = {"LLVM_CONFIG": str(llvm_config)}
    p = subprocess.run(item["argv"], cwd=ROOT, env=env, capture_output=True, text=True)
    item.update(exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)
    if p.returncode == 0:
        item["object_sha256"] = digest(item["object"])
    (HERE / "receipts" / (item["source"] + ".compile.json")).write_text(
        json.dumps(item, indent=2) + "\n")
    return item


if __name__ == "__main__":
    init_receipt = RUN / "commands.json"
    bridge_receipt = LAUNCH / "prx-candidate-l12-2/build-receipt.json"
    tasks = [
        command_for("UnrealInitializer.cpp", "UnrealInitializer.cpp", init_receipt,
                    json.loads(init_receipt.read_text())),
        command_for("runtime_start_bridge_r14_diag_spanpub.cpp", "runtime_start_bridge.cpp",
                    bridge_receipt, json.loads(bridge_receipt.read_text())["commands"]),
    ]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(compile_one, tasks))
    for r in results:
        print(r["source"], "PASS" if r["exit_code"] == 0 else "FAIL", flush=True)
        if r["stderr"]:
            print(r["stderr"][-12000:])
    raise SystemExit(int(any(r["exit_code"] != 0 for r in results)))
