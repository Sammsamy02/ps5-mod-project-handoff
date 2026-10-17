"""Two fresh actual Program target TUs; outputs remain in this task's directory."""
from pathlib import Path
import hashlib,json,os,subprocess,sys
O=Path(__file__).resolve().parents[1];W=O.parents[1]
R3=W/'outputs/2026-10-07-l19-local-profile-r3'
commands=json.loads((W/'outputs/2026-10-07-l19-r3-full-build/runs/run-l19-1/commands.json').read_text())
template=next(c['argv'] for c in commands if c['name']=='retained-program')
source=O/'units/UE4SSProgram.cpp';header=O/'units/NativeLayoutDeclarations.hpp'
selected=O/'tests/fixtures/selected-selected.hpp'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
pins={str(p.relative_to(W)):sha(p) for p in [source,header,selected,R3/'units/RuntimeBuildProfile.hpp']}
rows=[]
env=dict(os.environ);env['LLVM_CONFIG']=str(W/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
for mode in ['absent','selected']:
    cmd=list(template);cmd[1:1]=['-I',str(O/'units'),'-I',str(R3/'units')]
    for i,v in enumerate(cmd):
        if v.endswith('/UE4SSProgram.cpp'):cmd[i]=str(source)
    obj=O/'tests/build'/('program-target-'+mode+'.o')
    cmd[cmd.index('-o')+1]=str(obj)
    if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(obj.with_suffix('.d'))
    if mode=='selected':cmd.insert(1,'-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(selected)+'"')
    r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120)
    rows.append(dict(mode=mode,argv=cmd,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
    print(mode,'target_compile',r.returncode,flush=True)
    if r.returncode:print(r.stderr,flush=True);break
    rows[-1]['object_sha256']=sha(obj)
    dep=obj.with_suffix('.d')
    if dep.exists():rows[-1]['dependency_sha256']=sha(dep)
assert pins=={str(p.relative_to(W)):sha(p) for p in [source,header,selected,R3/'units/RuntimeBuildProfile.hpp']}
p=O/'reports/TARGET-COMPILE.json';p.write_text(json.dumps({'pass':all(r['exit']==0 for r in rows) and len(rows)==2,'commands':rows,'source_pins':pins,'scope':'Two fresh whole actual Program target TUs, with provider absent/present; incomplete fixture is compile-only and makes no runtime acceptance claim'},indent=2)+'\n')
sys.exit(0 if len(rows)==2 and all(r['exit']==0 for r in rows) else 1)
