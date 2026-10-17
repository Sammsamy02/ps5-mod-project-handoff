"""Focused actual PS5 compiles, using pinned coherent recipes and candidate header."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
w=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
recipe=w/'.superpowers/native-dependency-spike/native-coherent-runtime-1/compile-recipes.json'
recipes=json.loads(recipe.read_text())['units']
selected=json.loads((w/'outputs/2026-10-07-l19-r3-full-build/receipts/prx-candidate-l19-2/whole-selected-prx-candidate-l19-2.json').read_text())['selected']
out=root/'target'
out.mkdir(exist_ok=True)
headerdir=out/'include/Unreal'
headerdir.mkdir(parents=True,exist_ok=True)
shutil.copyfile(root/'candidate/FField.hpp',headerdir/'FField.hpp')
rows=[]
for name in ('unreal-ffield','unreal-src-property-ffieldpathproperty'):
    r=next(r for r in recipes if r['name']==name)
    old=Path(r['source'].replace('@ROOT@',str(w)))
    assert hashlib.sha256(old.read_bytes()).hexdigest()==r['source_sha256']
    src=root/'candidate/FField.cpp' if name=='unreal-ffield' else old
    cmd=[a.replace('@ROOT@',str(w)) for a in r['argv']]
    cmd[1:1]=['-I',str(out/'include')]
    for i,a in enumerate(cmd):
        if a.endswith('.cpp'):cmd[i]=str(src)
    cmd[cmd.index('-o')+1]=str(out/(name+'.o'))
    if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(out/(name+'.d'))
    env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG'].replace('@ROOT@',str(w)))
    result=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120)
    (out/(name+'.stdout')).write_text(result.stdout)
    (out/(name+'.stderr')).write_text(result.stderr)
    row={'name':name,'argv':cmd,'exit':result.returncode,'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),
         'stderr_path':str(out/(name+'.stderr'))}
    rows.append(row)
    print(name,'exit',result.returncode)
    if result.returncode:
        print(result.stderr[-1800:])
        break
    obj=out/(name+'.o')
    row['object_sha256']=hashlib.sha256(obj.read_bytes()).hexdigest()
    row['dependency_sha256']=hashlib.sha256((out/(name+'.d')).read_bytes()).hexdigest()
    nm=w/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm'
    oldobj=Path(selected[name]['object'])
    assert hashlib.sha256(oldobj.read_bytes()).hexdigest()==selected[name]['object_sha256']
    get=lambda p:set(subprocess.check_output([str(nm),'--undefined-only','--just-symbol-name',str(p)],text=True).splitlines())
    before,after=get(oldobj),get(obj)
    row['undefined_added']=sorted(after-before)
    row['undefined_removed']=sorted(before-after)
    print('undefined delta:',len(after-before),'added;',len(before-after),'removed')
receipt={'scope':'two actual PS5 source compiles, no link/package/console evidence','commands':rows,
         'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),recipe,root/'candidate/FField.hpp',root/'candidate/FField.cpp')}}
(root/'TARGET-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(int(any(r['exit'] for r in rows)))
