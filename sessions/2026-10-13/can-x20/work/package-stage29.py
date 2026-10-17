#!/usr/bin/env python3
from pathlib import Path
import difflib, hashlib, json, shutil, subprocess, tempfile, zipfile
workspace = Path("/WORKSPACE/Documents/Codex/2026-10-13/can-x20")
team = Path("/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work")
base = team / "cap03"
stage = team / "cg-cap03-query-guard-20261013-29"
outputs = workspace / "outputs"
evidence = workspace / "work/stage29-evidence"
evidence.mkdir(parents=True, exist_ok=True)
changed = ["src/CodePatch.cpp", "tests/host_test_protocol.cpp"]
def sha(b): return hashlib.sha256(b).hexdigest()
run = subprocess.run(["bash", "tests/run_host_test.sh"], cwd=stage, capture_output=True, text=True, timeout=60)
(evidence / "cap03-host-protocol.log").write_text(run.stdout + run.stderr)
if run.returncode or "SUMMARY: checks=105 failures=0 -> PASS" not in run.stdout or "HOST TEST: PASS" not in run.stdout:
    raise SystemExit("cap03 host regression failed")
print("cap03 host: 105/105; static CHECK encoding check passed (copied original PS5 object)", flush=True)
parts=[]; files={}
for rel in changed:
    before=(base/rel).read_bytes(); after=(stage/rel).read_bytes()
    if before==after: raise SystemExit(f"unchanged file listed: {rel}")
    parts.extend(difflib.unified_diff(before.decode().splitlines(keepends=True), after.decode().splitlines(keepends=True), fromfile=f"a/{rel}", tofile=f"b/{rel}", lineterm="\n"))
    files[rel]={"base_sha256":sha(before), "staged_sha256":sha(after), "size":len(after)}
patch=outputs/"cap03-query-guard-increment-20261013.patch"
patch.write_text("".join(parts))
manifest=outputs/"cap03-query-guard-manifest-20261013.json"
manifest.write_text(json.dumps({"base_stage":str(base), "staged_stage":str(stage), "files":files, "patch_sha256":sha(patch.read_bytes())},indent=2)+"\n")
with tempfile.TemporaryDirectory(prefix="stage29-replay-",dir=workspace/"work") as temp:
    scratch=Path(temp)/"base"; shutil.copytree(base,scratch)
    for args in (["git","apply","--check",str(patch)], ["git","apply",str(patch)]):
        r=subprocess.run(args,cwd=scratch,capture_output=True,text=True)
        if r.returncode: raise SystemExit(f"{args}: {r.stderr}")
    for rel in changed:
        if (scratch/rel).read_bytes()!=(stage/rel).read_bytes(): raise SystemExit(f"replay mismatch {rel}")
(evidence/"replay.log").write_text(f"base={base}\nstage={stage}\ngit apply --check: PASS\ngit apply: PASS\nexact byte replay: PASS\n")
(evidence/"target-limit.txt").write_text("No PS5 compiler on PATH. The runner's PS5 object disassembly is copied from the base and does not compile the new source. No console execution.\n")
zip_path=outputs/"cap03-query-guard-evidence-20261013.zip"
with zipfile.ZipFile(zip_path,"w",zipfile.ZIP_DEFLATED) as z:
    for item in sorted(evidence.iterdir()):
        if item.is_file(): z.write(item,f"stage29-evidence/{item.name}")
    z.write(patch,patch.name); z.write(manifest,manifest.name)
with zipfile.ZipFile(zip_path) as z:
    if z.testzip(): raise SystemExit("ZIP CRC failure")
print("two-file patch exact replay and ZIP CRC passed",flush=True)
