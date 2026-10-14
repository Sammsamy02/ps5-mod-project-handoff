#!/usr/bin/env python3
"""Compile and run the focused POSIX syscall return/carry regression."""

import json
import pathlib
import subprocess
import tempfile


HERE = pathlib.Path(__file__).resolve().parent


def main() -> int:
    source = HERE / "tests" / "posix_compat_test.c"
    adapter = HERE / "private" / "posix_compat.c"
    with tempfile.TemporaryDirectory(prefix="mb-posix-compat-") as temporary:
        executable = pathlib.Path(temporary) / "posix_compat_test"
        compile_argv = [
            "cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-DMB_ARCHIVE_POSIX_TEST_BACKEND=1", str(source), str(adapter),
            "-o", str(executable),
        ]
        compiled = subprocess.run(compile_argv, text=True, capture_output=True)
        result = {
            "compile_argv": compile_argv,
            "compile_exit": compiled.returncode,
            "compile_stdout": compiled.stdout,
            "compile_stderr": compiled.stderr,
        }
        if compiled.returncode == 0:
            ran = subprocess.run([str(executable)], text=True, capture_output=True)
            result.update({
                "run_argv": [str(executable)],
                "run_exit": ran.returncode,
                "run_stdout": ran.stdout,
                "run_stderr": ran.stderr,
            })
        else:
            result["run_skipped"] = True
        print(json.dumps(result, sort_keys=True))
        return int(compiled.returncode != 0 or result.get("run_exit", 1) != 0)


if __name__ == "__main__":
    raise SystemExit(main())
