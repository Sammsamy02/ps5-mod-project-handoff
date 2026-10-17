#!/usr/bin/env python3
"""Package the observer combined object into modbridge_runtime_aligned_observer.prx.

Exact mechanism proven by today's successful PRX builds (frozen lineage
package-commands.json, worker-composition-5-rebuild and
worker-startup-checkpoint/diag receipts): the SharpProspero packager never
needs a system `dotnet` — it uses the self-contained dotnet SDK stored in the
workspace at .superpowers/native-dependency-spike/sharpprospero-host-tool-1/
dotnet/dotnet (v10.0.401, osx-arm64), invoked exactly as the frozen lineage:

  1. <dotnet> build <out>/harness/Packager.csproj -c Release --ignore-failed-sources
  2. <dotnet> <out>/harness/bin/Release/net10.0/Packager.dll <combined.o>
              <frozen 191-row import-contract.json> <out>/bare-original/
              modbridge_runtime_aligned_observer.prx
  3. python3 finalize_sdk.py <bare> <profile.json> <out>/bare-sdk/<name>   (SDK stamp 0x11000000)
  4. python3 make_fself.py   <bare-sdk> <out>/modbridge_runtime_aligned_observer.prx

Harness sources are the observer worker's Program.cs/Packager.csproj (frozen
harness + moduleName/exportLibraryName modbridge_runtime_aligned_observer,
diff-verified). Writes stay inside this worker's directory.

Usage: package_observer.py <build-dir> <combined-object.o>
"""
from pathlib import Path
import datetime as _dt
import hashlib
import json
import os
import shutil
import subprocess
import sys

base = Path(__file__).resolve().parent
root = base.parents[2]  # .../what
SPIKE = root / ".superpowers/native-dependency-spike"
DOTNET_DIR = SPIKE / "sharpprospero-host-tool-1"
DOTNET = DOTNET_DIR / "dotnet/dotnet"
CONTRACT = SPIKE / "native-full-runtime-aligned-entry-1/import-contract.json"
CONTRACT_SHA256 = "58d94bc65997d1a9fac839cf126dc323806214893df7a20d3e435341aceab86c"
FINALIZE_SDK = SPIKE / "native-runtime-import-diagnostics-1/finalize_sdk.py"
MAKE_FSELF = (root / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/"
                    "ps5-payload-sdk/samples/install_app/make_fself.py")
HARNESS_SRC = base / "observer-src/harness"
MODULE = "modbridge_runtime_aligned_observer"
MARKER = "modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556"
BUILD_ID = "e9a58a9a8740438ebcb59136252a0556"

records: list[dict] = []
log: list[str] = []

def note(msg):
    stamp = _dt.datetime.now().isoformat(timespec="seconds")
    log.append(f"[{stamp}] {msg}")
    print(msg, flush=True)

def run(argv, env_extra=None, allow_fail=False):
    env = dict(os.environ)
    env.update({
        "DOTNET_ROOT": str(DOTNET_DIR / "dotnet"),
        "DOTNET_CLI_HOME": str(base / "dotnet-cli-home"),
        "DOTNET_MULTILEVEL_LOOKUP": "0",
        "DOTNET_CLI_TELEMETRY_OPTOUT": "1",
        "DOTNET_SKIP_FIRST_TIME_EXPERIENCE": "1",
        "DOTNET_NOLOGO": "1",
        "DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE": "1",
        "NUGET_PACKAGES": str(base / "nuget-packages"),
    })
    if env_extra:
        env.update(env_extra)
    note("run: " + " ".join(str(a) for a in argv))
    res = subprocess.run([str(a) for a in argv], env=env, capture_output=True, text=True)
    records.append({"argv": [str(a) for a in argv], "rc": res.returncode,
                    "stdout": res.stdout[-20000:], "stderr": res.stderr[-20000:]})
    if res.returncode != 0 and not allow_fail:
        raise RuntimeError(f"command failed rc={res.returncode}: {res.stderr[-2000:]}")
    return res

def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def main() -> int:
    out = (base / sys.argv[1]).resolve()
    combined = Path(sys.argv[2]).resolve()
    assert out.is_dir() and combined.is_file()
    assert sha256_file(CONTRACT) == CONTRACT_SHA256, "frozen 191-row contract moved"
    combined_sha = sha256_file(combined)
    note(f"packaging combined object {combined} sha256 {combined_sha}")

    # Fresh harness per build dir (never reuse bin/obj; frozen recipe).
    harness = out / "harness"
    harness.mkdir(parents=True, exist_ok=True)
    for stale in ("bin", "obj"):
        shutil.rmtree(harness / stale, ignore_errors=True)
    for f in ("Program.cs", "Packager.csproj"):
        shutil.copyfile(HARNESS_SRC / f, harness / f)
    note("harness staged: observer Program.cs (moduleName "
         f"{MODULE}) + Packager.csproj (absolute HintPaths to prebuilt "
         "SharpProspero.Link/Prx DLLs in native-runtime-import-probe-1)")

    # 1. build the Packager harness with the self-contained SDK
    run([DOTNET, "build", harness / "Packager.csproj",
         "-c", "Release", "--ignore-failed-sources"])
    harness_dll = harness / "bin/Release/net10.0/Packager.dll"
    assert harness_dll.is_file(), "Packager.dll missing after build"

    # 2. run the Packager -> bare-original PRX
    bare = out / "bare-original" / f"{MODULE}.prx"
    bare.parent.mkdir(parents=True, exist_ok=True)
    pack = run([DOTNET, harness_dll, combined, CONTRACT, bare])
    packager_json = json.loads(pack.stdout)
    assert packager_json["output_sha256"] == sha256_file(bare)

    # 3. SDK stamp 0x11000000 (console-proven finalize)
    profile = {
        "title_id": "PPSA33286",
        "module_path": f"/data/plugins/{MODULE}.prx",
        "log_path": "/data/townfall_runtime_aligned_observer.log",
        "marker": MARKER,
        "build_id": BUILD_ID,
        "sdk_version": "0x11000000",
        "sdk_stamp_provenance": "console-proven fix for ESDKVERSION 0x80020063 "
                                "(frozen aligned-entry lineage profile.json)",
        "combined_object_sha256": combined_sha,
        "import_contract": str(CONTRACT),
        "import_contract_sha256": CONTRACT_SHA256,
        "total_imports": 191,
        "promotion_allowed": False,
        "native_executed": False,
        "note": "observer profile: frozen lineage profile with module/log/marker "
                "changed per STAGE-PLAN; no input_path (observer loads no startup "
                "input); finalizing code reads only sdk_version",
    }
    profile_path = out / "profile.json"
    profile_path.write_text(json.dumps(profile, indent=2) + "\n")
    bare_sdk = out / "bare-sdk" / f"{MODULE}.prx"
    bare_sdk.parent.mkdir(parents=True, exist_ok=True)
    fin = run(["python3", FINALIZE_SDK, bare, profile_path, bare_sdk])
    finalize_json = json.loads(fin.stdout)

    # 4. fake-self wrapper
    wrapped = out / f"{MODULE}.prx"
    run(["python3", MAKE_FSELF, bare_sdk, wrapped])
    note(f"PRX written: {wrapped} bytes={wrapped.stat().st_size} "
         f"sha256={sha256_file(wrapped)}")

    receipt = {
        "worker": "worker-observer-packaging",
        "script": str(Path(__file__).resolve()),
        "utc": _dt.datetime.now().isoformat(),
        "build_dir": str(out),
        "combined_object": {"path": str(combined), "sha256": combined_sha,
                            "bytes": combined.stat().st_size},
        "contract": {"path": str(CONTRACT), "sha256": CONTRACT_SHA256},
        "dotnet": {"path": str(DOTNET), "mechanism":
                   "self-contained workspace SDK "
                   ".superpowers/native-dependency-spike/sharpprospero-host-tool-1 "
                   "(no system dotnet needed)"},
        "packager_output": packager_json,
        "finalize_sdk_output": finalize_json,
        "final_prx": {"path": str(wrapped), "bytes": wrapped.stat().st_size,
                      "sha256": sha256_file(wrapped)},
        "bare_sha256": sha256_file(bare),
        "bare_sdk_sha256": sha256_file(bare_sdk),
        "gaps": [],
        "log": log,
        "commands": records,
    }
    (out / "build-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    note("receipt written")
    return 0

if __name__ == "__main__":
    sys.exit(main())
