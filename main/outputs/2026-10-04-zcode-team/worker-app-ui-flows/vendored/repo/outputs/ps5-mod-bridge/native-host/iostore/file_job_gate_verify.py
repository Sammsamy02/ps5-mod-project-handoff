#!/usr/bin/env python3
"""Focused real-container host gate/ownership controls, never console execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
CHILDREN = ["none_range_reader.c", "container_header_merge.c", "toc_reader.c",
            "none_packer.c", "directory_merge.c", "toc_overlay.c"]
CASES = ["admission-single", "admission-batch", "post-read-single",
         "post-allocation-batch", "create-stop-single", "partial-output-single",
         "late-cleanup-batch", "event-refusal-single", "close-failure-single", "validation"]


def identity(path):
    path = Path(path).resolve()
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for block in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(block)
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest.hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", type=Path, required=True,
                        help="retained iostore-batch-file-job-1 profile")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baseline", type=Path,
                        help="frozen prior C/H; runs intended ignoring-control RED")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--cases", nargs="+", choices=CASES)
    parser.add_argument("--legacy", action="store_true",
                        help="also run the existing single and batch controls")
    args = parser.parse_args()
    profile = json.loads(args.profile.read_text())
    inputs = profile["preparation_inputs"]
    before = {k: identity(v["path"]) for k, v in inputs.items() if isinstance(v, dict)}
    if any(before[k] != inputs[k] for k in before):
        raise SystemExit("retained source input identity changed")
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    product = (args.baseline.resolve() if args.baseline else HERE) / "original_file_job.c"
    vendor = HERE / "vendor/blake3"
    helper = HERE / "preparation_tests/file_job_gate_control.c"
    common = [HERE / name for name in CHILDREN] + [vendor / name for name in
              ["blake3.c", "blake3_dispatch.c", "blake3_portable.c"]]
    facade = HERE.parent / "archive/private/posix_compat.c"
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
             "-Wno-unused-function", "-D_DARWIN_C_SOURCE", "-DBLAKE3_NO_SSE2",
             "-DBLAKE3_NO_SSE41", "-DBLAKE3_NO_AVX2", "-DBLAKE3_NO_AVX512",
             "-DBLAKE3_USE_NEON=0", "-I" + str(HERE), "-I" + str(vendor)]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    tracked = [product, product.with_suffix(".h"), helper, Path(__file__), facade, *common]
    tracked += [HERE / name for name in ["original_file_job.h", "none_range_reader.h",
                "container_header_merge.h", "toc_reader.h", "none_packer.h",
                "directory_merge.h", "toc_overlay.h"]]
    if args.legacy:
        tracked += [HERE / "preparation_tests/file_job_control.c",
                    HERE / "preparation_tests/batch_file_job_control.c"]
    revisions = {str(p.resolve()): identity(p) for p in tracked}
    receipt = {"phase": "incomplete", "scope": "real retained-container host component proof",
               "baseline_ignoring_control": bool(args.baseline), "commands": [], "controls": [],
               "inputs_before": before, "source_before": revisions,
               "network_used": False, "console_connected": False, "native_execution": False,
               "game_files_written": False, "close_failure_injection": "real close then EINTR in product TU only",
               "sanitized": args.sanitize}
    path = out / "receipt.json"

    def save():
        path.write_text(json.dumps(receipt, indent=2) + "\n")

    environment = os.environ.copy()
    if args.sanitize:
        environment["ASAN_OPTIONS"] = "detect_leaks=0" if sys.platform == "darwin" else "detect_leaks=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1"
        receipt["leak_sanitizer"] = "unavailable on Darwin; tracked release/close checked" if sys.platform == "darwin" else "enabled"

    def run(label, command, expected=0):
        start = time.monotonic()
        process = subprocess.run(list(map(str, command)), capture_output=True, timeout=60, env=environment)
        (out / (label + ".stdout")).write_bytes(process.stdout)
        (out / (label + ".stderr")).write_bytes(process.stderr)
        receipt["commands"].append({"label": label, "argv": list(map(str, command)),
                                   "exit": process.returncode, "seconds": time.monotonic() - start})
        save()
        if process.returncode != expected:
            receipt.update(phase="failed", failure=label)
            save()
            raise SystemExit(process.stderr.decode(errors="replace") or "unexpected exit")
        return process.stdout

    save()
    obj = out / "original_file_job.o"
    run("compile-product", ["/usr/bin/clang", *flags, "-Dclose=mb_iostore_gate_test_close", "-c", product, "-o", obj])
    driver = out / "gate_control"
    red_flag = ["-DMB_IOSTORE_GATE_RED_BASELINE"] if args.baseline else []
    run("compile-controls", ["/usr/bin/clang", *flags, *red_flag, helper, obj, facade, *common, "-o", driver])
    cases = args.cases or (["admission-single", "admission-batch", "late-cleanup-batch"] if args.baseline else CASES)
    input_args = [inputs[k]["path"] for k in ["mod_a_toc", "mod_a_ucas", "mod_b_toc", "mod_b_ucas",
                                            "original_base_toc", "captured_base_container_header"]]
    for case in cases:
        destination = out / case
        destination.mkdir(mode=0o700)
        observed = json.loads(run(case, [driver, case, *input_args, inputs["observed_base_ucas_bytes"], destination],
                                  expected=1 if args.baseline else 0))
        if args.baseline and not (observed["failures"] and observed["status"] == 0 and observed["prepared"] == 1 and observed["gate_calls"] == 0):
            receipt.update(phase="failed", failure="RED did not isolate missing control")
            save()
            raise SystemExit("unexpected RED behavior")
        receipt["controls"].append(observed)
        save()
    if args.legacy:
        if args.baseline:
            raise SystemExit("legacy proof uses current product only")
        for kind in ["single", "batch"]:
            binary = out / (kind + "_legacy_control")
            source = HERE / "preparation_tests" / ("file_job_control.c" if kind == "single" else "batch_file_job_control.c")
            run("compile-legacy-" + kind, ["/usr/bin/clang", *flags, HERE / "original_file_job.c", facade, *common, source, "-o", binary])
            destination = out / ("legacy-" + kind)
            destination.mkdir(mode=0o700)
            if kind == "single":
                argv = [inputs[k]["path"] for k in ["mod_a_toc", "mod_a_ucas", "original_base_toc", "captured_base_container_header"]]
                argv += [inputs["observed_base_ucas_bytes"], destination, profile["single_post_job_oracle_directory"]]
            else:
                argv = [*input_args, inputs["observed_base_ucas_bytes"], destination]
            receipt[kind + "_legacy"] = json.loads(run("legacy-" + kind, [binary, *argv]))
            save()
    after = {k: identity(v["path"]) for k, v in inputs.items() if isinstance(v, dict)}
    source_after = {str(p.resolve()): identity(p) for p in tracked}
    if before != after or revisions != source_after:
        receipt.update(phase="failed", failure="input changed during proof")
        save()
        raise SystemExit("input changed")
    receipt.update(phase="red-observed" if args.baseline else "passed", inputs_after=after,
                   source_after=source_after, source_unchanged=True,
                   assertions=sum(item["checks"] for item in receipt["controls"]),
                   failures=sum(item["failures"] for item in receipt["controls"]))
    save()
    print(json.dumps({"phase": receipt["phase"], "receipt": str(path), "assertions": receipt["assertions"],
                      "failures": receipt["failures"], "controls": len(receipt["controls"])}))


if __name__ == "__main__":
    main()
