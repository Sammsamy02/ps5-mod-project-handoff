#!/usr/bin/env python3
import hashlib
import json
import os
import subprocess
from pathlib import Path

BASE=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=Path(__file__).resolve().parent
receipt=BASE/'outputs/2026-10-08-r5-thread-final-integration-v2/pass1/COMPOSITION-RECEIPT.json'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(receipt)=='82bad5f6016cf8a1407a1385d231b852683f553bfba9de0c37fd0127da6b720c'
unit=next(x for x in json.loads(receipt.read_text())['selected_units'] if x['name']=='lua-luauobject')
assert sha(Path(unit['source']))==unit['source_sha256']
candidate=OUT/'candidate/LuaUObject.cpp'
target=OUT/'target/configured'
target.mkdir(parents=True,exist_ok=True)
llvm_config=BASE/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config'
argv=list(unit['argv'])
changes=[]
for flag,new in [('-c',candidate),('-o',target/'lua-luauobject.o'),('-MF',target/'lua-luauobject.d')]:
    assert argv.count(flag)==1
    i=argv.index(flag)+1
    changes.append(dict(flag=flag,old=argv[i],new=str(new)))
    argv[i]=str(new)
(target/'COMMAND.json').write_text(json.dumps(dict(argv=argv,recipe_source=str(receipt),recipe_source_sha256=sha(receipt),delta=changes,LLVM_CONFIG=str(llvm_config)),indent=2)+'\n')
r=subprocess.run(argv,cwd=BASE,capture_output=True,text=True,env=dict(os.environ,LLVM_CONFIG=str(llvm_config)))
(target/'compile.stdout').write_text(r.stdout)
(target/'compile.stderr').write_text(r.stderr)
result=dict(returncode=r.returncode,whole_selected_TU=True,source=str(candidate),source_sha256=sha(candidate),
            compiler_invocation=str(Path(argv[0])),compiler_target=str(Path(argv[0]).resolve()),
            compiler_target_sha256=sha(Path(argv[0]).resolve()),
            LLVM_CONFIG=dict(path=str(llvm_config),sha256=sha(llvm_config)),
            actual_clang=dict(path=str((llvm_config.parent/'clang++').resolve()),sha256=sha((llvm_config.parent/'clang++').resolve())),
            command_sha256=sha(target/'COMMAND.json'),headers_or_flags_changed=False,
            runtime_execution=False)
if r.returncode==0:
    result['object']=dict(path=str(target/'lua-luauobject.o'),sha256=sha(target/'lua-luauobject.o'),size=(target/'lua-luauobject.o').stat().st_size)
    result['dependencies']=dict(path=str(target/'lua-luauobject.d'),sha256=sha(target/'lua-luauobject.d'))
(target/'RECEIPT.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(returncode=r.returncode,whole_TU=True,source_sha256=result['source_sha256'])))
assert r.returncode==0, r.stderr
