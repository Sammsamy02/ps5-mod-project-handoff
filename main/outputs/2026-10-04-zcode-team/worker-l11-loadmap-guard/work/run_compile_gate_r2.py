#!/usr/bin/env python3
# worker-l11-loadmap-guard r2 compile gate: frozen 172-token argv, only
# source/-o/-MF redirected (argv_drift_policy per dry-run VERDICT GO).
import json,subprocess,hashlib,os,sys
ROOT="/WORKSPACE/.codex/worktrees/translation-core/what"
TEAM=f"{ROOT}/outputs/2026-10-04-zcode-team"
W=f"{TEAM}/worker-l11-loadmap-guard"
argv=json.load(open(f"{TEAM}/worker-luamod-l11-dryrun/scratch/staging/luamod-argv.json"))["argv"]
assert len(argv)==172, len(argv)
src_i=argv.index(f"{ROOT}/.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/src/LuaMod.cpp")
o_i=argv.index("-o")+1
mf_i=argv.index("-MF")+1
env=dict(os.environ)
env["LLVM_CONFIG"]=f"{ROOT}/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config"
runs={
 "control-r2": f"{TEAM}/worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp",
 "defer-r2":  f"{W}/staging/luamod-l11-loadmap-defer-r2/LuaMod.cpp",
}
out={}
for name,src in runs.items():
    a=list(argv)
    a[src_i]=src
    a[o_i]=f"{W}/work/luamod-{name}.o"
    a[mf_i]=f"{W}/work/luamod-{name}.d"
    p=subprocess.run(a,capture_output=True,text=True,env=env,cwd=ROOT)
    open(f"{W}/work/compile-gate-{name}.stderr","w").write(p.stderr)
    open(f"{W}/work/compile-gate-{name}.stdout","w").write(p.stdout)
    obj=f"{W}/work/luamod-{name}.o"
    sha=hashlib.sha256(open(obj,'rb').read()).hexdigest() if os.path.exists(obj) else None
    # warnings/errors quick count
    errs=sum(1 for l in p.stderr.splitlines() if " error: " in l)
    warns=sum(1 for l in p.stderr.splitlines() if " warning: " in l)
    # undefined symbols
    nm=f"{ROOT}/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm"
    u=subprocess.run([nm,"-u",obj],capture_output=True,text=True)
    open(f"{W}/work/und-{name}.txt","w").write(u.stdout)
    out[name]={"exit":p.returncode,"obj_sha256":sha,"errors":errs,"warnings":warns,"und_bytes":len(u.stdout)}
print(json.dumps(out,indent=1))
