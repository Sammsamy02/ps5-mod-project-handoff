#!/usr/bin/env python3
"""Run one pinned native TU command and keep bounded compiler evidence."""
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--command-json", required=True, type=Path)
parser.add_argument("--source", required=True, type=Path)
parser.add_argument("--out-dir", required=True, type=Path)
parser.add_argument("--overlay-include", type=Path)
args = parser.parse_args()
command_data = json.loads(args.command_json.read_text())
argv = list(command_data["argv"])
source_abs = str(args.source.resolve())
source_index = argv.index("-c") + 1
output_index = argv.index("-o") + 1
argv[source_index] = source_abs
args.out_dir.mkdir(parents=True, exist_ok=True)
object_path = args.out_dir / (args.source.stem + ".o")
if object_path.exists():
    object_path.unlink()
argv[output_index] = str(object_path)
if args.overlay_include is not None:
    overlay_index = next(i for i, value in enumerate(argv[:-1]) if value == "-I" and "ue4ss-real-lua-link-route-1/overlay/include" in argv[i + 1]) + 1
    argv[overlay_index] = str(args.overlay_include.resolve())
env = os.environ.copy()
env["LLVM_CONFIG"] = command_data["LLVM_CONFIG"]
completed = subprocess.run(argv, cwd=Path.cwd(), env=env, text=True, capture_output=True)
(args.out_dir / "stdout.txt").write_text(completed.stdout)
(args.out_dir / "stderr.txt").write_text(completed.stderr)
receipt = {
    "source": source_abs,
    "argv": argv,
    "returncode": completed.returncode,
    "object_exists": object_path.is_file(),
    "object_bytes": object_path.stat().st_size if object_path.is_file() else 0,
    "stdout_bytes": len(completed.stdout.encode()),
    "stderr_bytes": len(completed.stderr.encode()),
}
(args.out_dir / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(json.dumps(receipt, indent=2))
raise SystemExit(0 if completed.returncode == 0 and object_path.is_file() else 1)
