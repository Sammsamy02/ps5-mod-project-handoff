"""Fresh actual PS5 header consumers; preserve all original sources and objects."""
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
PIN = HERE / "OWNER-HEADER-DEPENDENT-UNITS.json"
OWNER = ROOT / "outputs/2026-10-07-l19-r4-owner-variant"
AUDIT = ROOT / "outputs/2026-10-07-l19-r4-owner-variant-review/FINAL-AUDIT.json"
NM = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm"
OUT = HERE / "dependent-build-pass1"
PENDING = {"program", "unreal-unrealinitializer", "native-layoutbinding"}
HEADER_SHA = "6891a6f15fa5124f5f0f57aafe081d501a5baec52e9001d2e3403d5a3f53ad3c"
SOURCE_SHA = "f56f9d407975e4335bf0157919f62cfdf5c892d1d3ca4a028f40d9a5d51b2b47"
AUDIT_SHA = "a0df909b04777fadafd6f9a8e6427907e7fa2f13c342d786e60a85075b085a28"

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    assert sha(AUDIT) == AUDIT_SHA
    assert sha(OWNER / "candidate/FField.hpp") == HEADER_SHA
    assert sha(OWNER / "candidate/FField.cpp") == SOURCE_SHA
    census = json.loads(PIN.read_text())
    assert sha(census["selected_receipt"]) == census["selected_receipt_sha256"]
    OUT.mkdir(exist_ok=False)
    include = OUT / "include/Unreal"
    include.mkdir(parents=True)
    shutil.copyfile(OWNER / "candidate/FField.hpp", include / "FField.hpp")
    rows = [r for r in census["affected"] if r["name"] not in PENDING]
    assert len(rows) == 144 and len(census["affected"]) == 147
    def compile_one(row):
        name = row["name"]
        assert sha(row["source"]) == row["source_sha256"]
        assert sha(row["object"]) == row["object_sha256"]
        assert sha(row["dependency"]) == row["dependency_sha256"]
        src = OWNER / "candidate/FField.cpp" if name == "unreal-ffield" else Path(row["source"])
        obj, dep = OUT / (name + ".o"), OUT / (name + ".d")
        argv = list(row["argv"])
        argv[1:1] = ["-I", str(include.parent)]
        assert argv.count(row["source"]) == 1
        argv[argv.index(row["source"])] = str(src)
        argv[argv.index("-o") + 1] = str(obj)
        argv[argv.index("-MF") + 1] = str(dep)
        env = dict(os.environ, LLVM_CONFIG=row["LLVM_CONFIG"])
        result = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=180)
        (OUT / (name + ".stdout")).write_text(result.stdout)
        (OUT / (name + ".stderr")).write_text(result.stderr)
        record = {"name": name, "argv": argv, "exit": result.returncode,
                  "source": str(src), "source_sha256": sha(src),
                  "old_object_sha256": row["object_sha256"], "LLVM_CONFIG": row["LLVM_CONFIG"]}
        if not result.returncode:
            assert str(include / "FField.hpp") in dep.read_text()
            def und(path):
                return set(subprocess.check_output([str(NM), "--undefined-only", "--just-symbol-name", str(path)], text=True).splitlines())
            before, after = und(row["object"]), und(obj)
            record.update(object=str(obj), object_sha256=sha(obj), dependency_file=str(dep), dependency_sha256=sha(dep),
                          undefined_added=sorted(after - before), undefined_removed=sorted(before - after), accepted_header_selected=True)
        (OUT / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
        return record
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
        records = list(executor.map(compile_one, rows))
    failed = [r["name"] for r in records if r["exit"]]
    delta = {r["name"]: {k: r[k] for k in ("undefined_added", "undefined_removed")}
             for r in records if not r["exit"] and (r["undefined_added"] or r["undefined_removed"])}
    receipt = {"scope": "local actual target compilation; not linked/packaged/executed", "console_contact": False,
               "affected_total": 147, "fresh_compiles": len(records), "pending_profile_program_units": sorted(PENDING),
               "input_pins": {str(p): sha(p) for p in (PIN, AUDIT, Path(__file__), OWNER / "candidate/FField.hpp", OWNER / "candidate/FField.cpp")},
               "failed": failed, "individual_undefined_deltas": delta, "selected_units": records}
    (OUT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"fresh_compiles": len(records), "failed": failed, "individual_delta_units": len(delta), "receipt": str(OUT / "RECEIPT.json")}))
    return bool(failed)

if __name__ == "__main__":
    sys.exit(main())
