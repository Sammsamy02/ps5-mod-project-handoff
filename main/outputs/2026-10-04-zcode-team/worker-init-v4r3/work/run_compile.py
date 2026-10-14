import json, subprocess, sys, os, hashlib, time

BASE = "/WORKSPACE/.codex/worktrees/translation-core/what"
W = f"{BASE}/outputs/2026-10-04-zcode-team/worker-init-v4r3"
argv = json.load(open(f"{BASE}/outputs/2026-10-04-zcode-team/worker-fname-validator-v4/receipts/compile-gate.argv.json"))
assert len(argv) == 190, len(argv)

def run(tag, src, obj, mf):
    a = list(argv)
    a[184] = src; a[186] = obj; a[189] = mf
    env = dict(os.environ)
    env["LLVM_CONFIG"] = f"{BASE}/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config"
    t0 = time.time()
    p = subprocess.run(a, capture_output=True, text=True, env=env, cwd=W)
    dt = time.time() - t0
    json.dump(a, open(f"{W}/receipts/{tag}.argv.json", "w"), indent=1)
    open(f"{W}/receipts/{tag}.stdout", "w").write(p.stdout)
    open(f"{W}/receipts/{tag}.stderr", "w").write(p.stderr)
    h = hashlib.sha256(open(obj, "rb").read()).hexdigest() if os.path.exists(obj) else None
    rec = {"tag": tag, "exit": p.returncode, "seconds": round(dt, 1),
           "source": src, "object": obj, "object_sha256": h,
           "object_bytes": os.path.getsize(obj) if os.path.exists(obj) else None,
           "stderr_lines": p.stderr.count("\n"), "warnings": p.stderr.count("warning:")}
    json.dump(rec, open(f"{W}/receipts/{tag}.record.json", "w"), indent=1)
    print(json.dumps(rec))
    return p.returncode, h

rc1, h1 = run("control-v4-final-units",
              f"{BASE}/outputs/2026-10-04-zcode-controller/launch10-build/final-units/UnrealInitializer.cpp",
              f"{W}/receipts/unreal-unrealinitializer-v4-control.o",
              f"{W}/receipts/unreal-unrealinitializer-v4-control.d")
rc2, h2 = run("v4r3-merged-pass1",
              f"{W}/staging/UnrealInitializer-v4r3.cpp",
              f"{W}/receipts/unreal-unrealinitializer-v4r3.o",
              f"{W}/receipts/unreal-unrealinitializer-v4r3.d")
rc3, h3 = run("v4r3-merged-pass2-determinism",
              f"{W}/staging/UnrealInitializer-v4r3.cpp",
              f"{W}/receipts/unreal-unrealinitializer-v4r3-pass2.o",
              f"{W}/receipts/unreal-unrealinitializer-v4r3-pass2.d")
print("CONTROL-EXPECTED a437861a17614705b46e9cd6ba7401b9db09cbc37c58952eca179661a04be27a")
print("CONTROL-MATCH:", h1 == "a437861a17614705b46e9cd6ba7401b9db09cbc37c58952eca179661a04be27a")
print("DETERMINISM:", h2 == h3)
sys.exit(0 if (rc1 == 0 and rc2 == 0 and rc3 == 0 and h1 == "a437861a17614705b46e9cd6ba7401b9db09cbc37c58952eca179661a04be27a" and h2 == h3) else 1)
