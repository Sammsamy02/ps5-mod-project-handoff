#!/usr/bin/env python3
"""Verify and package isolated PS5 loader mmap portability increment."""
from pathlib import Path
import difflib
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile

workspace = Path("/WORKSPACE/Documents/Codex/2026-10-13/can-x20")
team = Path("/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work")
base = team / "cg-reentrant-segment-20261013-40"
stage = team / "cg-ps5-loader-build-20261013-41"
outputs = workspace / "outputs"
evidence = workspace / "work/stage41-evidence"
evidence.mkdir(parents=True, exist_ok=True)
changed = [
    "loader/src/pe_loader.cpp",
]
def sha(data):
    return hashlib.sha256(data).hexdigest()

tests = [
    "run_dynamic_module_lookup.py",
    "run_guest_pe_batch.py",
    "run_mapped_module_registry.py",
    "run_guarded_shim_policy.py",
    "run_guarded_pe_activation.py",
    "run_preflight.py",
    "run_static_tls.py",
    "run_teb_lifecycle.py",
    "run_context_bridge.py",
    "run_import_registry.py",
    "run_lazy_import_resolution.py",
    "run_typed_import_install.py",
    "run_win32_sync_import.py",
    "run_thread_notification_policy.py",
    "run_real_tls_prepare.py",
]
for name in tests:
    cmd = ["python3", f"loader/tests/{name}"]
    run = subprocess.run(cmd, cwd=stage, capture_output=True, text=True, timeout=90)
    (evidence / f"{name}.log").write_text(run.stdout + run.stderr)
    print(f"{name}: exit={run.returncode}", flush=True)
    if run.returncode:
        raise SystemExit(f"regression failed: {name}")

asan_build = [
    "clang++", "-arch", "x86_64", "-std=c++17", "-O1", "-g",
    "-fsanitize=address", "-fno-omit-frame-pointer", "-Wall", "-Wextra",
    "-Werror", "-pthread", "-Iloader/src",
    "loader/tests/teb_lifecycle_test.cpp", "loader/src/static_tls_bank.cpp",
    "loader/src/guest_teb_lifecycle.cpp", "loader/src/win64_bridge.S",
    "-o", "loader/tests/teb-lifecycle-asan-stage41",
]
run = subprocess.run(asan_build, cwd=stage, capture_output=True, text=True, timeout=45)
(evidence / "teb-asan-build.log").write_text(run.stdout + run.stderr + f"exit={run.returncode}\n")
if run.returncode:
    raise SystemExit("TEB ASan build failed")
run = subprocess.run(["arch", "-x86_64", "loader/tests/teb-lifecycle-asan-stage41"],
                     cwd=stage, capture_output=True, text=True, timeout=30)
(evidence / "teb-asan-run.log").write_text(run.stdout + run.stderr + f"exit={run.returncode}\n")
if run.returncode or "TOTAL: 0 failed" not in run.stdout:
    raise SystemExit("TEB ASan run failed")
print("TEB ASan: PASS", flush=True)

build = [
    "clang++", "-arch", "x86_64", "-std=c++17", "-O1", "-g", "-Wall",
    "-Wno-unused-parameter", "-Wno-unused-variable", "-Iloader/include",
    "-Iloader/src", "-I../cap13/BUILD/src", "loader/tests/e2e_host.cpp",
    "loader/src/pe_loader.cpp", "loader/src/dllmain_invoker.cpp",
    "loader/src/win_api_shim.cpp", "loader/src/win64_bridge.S",
    "-o", "loader/tests/e2e-host-stage41",
]
run = subprocess.run(build, cwd=stage, capture_output=True, text=True, timeout=60)
(evidence / "e2e-build.log").write_text(run.stdout + run.stderr + f"exit={run.returncode}\n")
if run.returncode:
    raise SystemExit("e2e build failed")
fixture = team / "cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll"
fixture_hash = "acc201987edcee968a052ef889cb0e8bdcc5c5cd63fcdb4c0dda699cace51bce"
if sha(fixture.read_bytes()) != fixture_hash:
    raise SystemExit("fixture hash mismatch before e2e")
run = subprocess.run(["arch", "-x86_64", "loader/tests/e2e-host-stage41", "fixture", str(fixture)],
                     cwd=stage, capture_output=True, text=True, timeout=20)
(evidence / "e2e-fixture.log").write_text(run.stdout + run.stderr)
if run.returncode or "RESULT: PASS (11 checks, 0 failed)" not in run.stdout:
    raise SystemExit("e2e fixture failed")
if sha(fixture.read_bytes()) != fixture_hash:
    raise SystemExit("fixture changed after e2e")
print("e2e fixture: 11/11", flush=True)

patch_parts = []
files = {}
for rel in changed:
    old_path, new_path = base / rel, stage / rel
    before = old_path.read_bytes() if old_path.exists() else None
    after = new_path.read_bytes()
    if before == after:
        raise SystemExit(f"unchanged file listed: {rel}")
    old_text = before.decode() if before is not None else ""
    new_text = after.decode()
    patch_parts.extend(difflib.unified_diff(
        old_text.splitlines(keepends=True), new_text.splitlines(keepends=True),
        fromfile=f"a/{rel}" if before is not None else "/dev/null",
        tofile=f"b/{rel}", lineterm="\n"))
    files[rel] = {"base_sha256": sha(before) if before is not None else None,
                  "staged_sha256": sha(after), "size": len(after)}
patch = outputs / "ps5-loader-build-increment-20261013.patch"
patch.write_text("".join(patch_parts))
manifest = outputs / "ps5-loader-build-manifest-20261013.json"
manifest.write_text(json.dumps({"base_stage": str(base), "staged_stage": str(stage),
                                "files": files, "patch_sha256": sha(patch.read_bytes())},
                               indent=2) + "\n")

with tempfile.TemporaryDirectory(prefix="stage41-replay-", dir=workspace / "work") as temp:
    scratch = Path(temp) / "base"
    shutil.copytree(base, scratch)
    check = subprocess.run(["git", "apply", "--check", str(patch)], cwd=scratch,
                           capture_output=True, text=True)
    if check.returncode:
        raise SystemExit(f"git apply --check failed: {check.stderr}")
    replay = subprocess.run(["git", "apply", str(patch)], cwd=scratch,
                            capture_output=True, text=True)
    if replay.returncode:
        raise SystemExit(f"git apply failed: {replay.stderr}")
    for rel in changed:
        if (scratch / rel).read_bytes() != (stage / rel).read_bytes():
            raise SystemExit(f"replay mismatch: {rel}")
(evidence / "replay.log").write_text(
    f"base={base}\nstage={stage}\nchanged_files={len(changed)}\n"
    "git apply --check: PASS\ngit apply: PASS\nexact byte replay: PASS\n")
print(f"patch replay: {len(changed)} files exact", flush=True)

evidence_zip = outputs / "ps5-loader-build-host-evidence-20261013.zip"
with zipfile.ZipFile(evidence_zip, "w", zipfile.ZIP_DEFLATED) as archive:
    for item in sorted(evidence.iterdir()):
        if item.is_file():
            archive.write(item, f"stage41-evidence/{item.name}")
    archive.write(manifest, manifest.name)
    archive.write(patch, patch.name)
with zipfile.ZipFile(evidence_zip) as archive:
    bad = archive.testzip()
    if bad:
        raise SystemExit(f"ZIP CRC failure: {bad}")
print(f"zip integrity: PASS; size={evidence_zip.stat().st_size}", flush=True)
