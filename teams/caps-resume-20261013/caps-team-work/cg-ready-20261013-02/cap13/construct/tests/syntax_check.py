#!/usr/bin/env python3
"""Syntax-only target check. Uses existing public toolchain; no objects/PRX/link.
Does not write to shared candidate or frozen build trees.
"""
from pathlib import Path
import json,subprocess,os,hashlib
root=Path(__file__).resolve().parents[1]
wt=root.parents[3]
what=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
r11=what/'outputs/2026-10-08-l26-build-variants/archive-r6/R11-watch-world-uenum/l26-A'
command=r11/'pass1/objects/cppmod.command.json'
scratch=root/'build/syntax';scratch.mkdir(parents=True,exist_ok=True)
source=what/'outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/Mod/CppMod.cpp'
inc=source.parent/'NativeModEngineServices.inc'
overlay=scratch/'overlay.json';overlay.write_text(json.dumps({'version':0,'roots':[{'type':'file','name':str(inc),'external-contents':str(root/'src/Mod/NativeModEngineServices.inc')}]},indent=2)+'\n')
driver=scratch/'driver.cpp';driver.write_text('#include '+json.dumps(str(source))+'\nstatic_assert(offsetof(mbm_host_v1,construct_object)==184);\n')
args=json.loads(command.read_text())['argv'];argv=[];i=0
while i<len(args):
 a=args[i]
 if a in ('-o','-MF','-MT','-MQ'):i+=2;continue
 if a in ('-MD','-MMD','-c'):i+=1;continue
 if a.endswith('.cpp'):a=str(driver)
 old=str(what/'outputs/2026-10-08-l26-build-variants/l26-A')
 if a.startswith(old) and not Path(a).exists():a=a.replace(old,str(r11))
 argv.append(a);i+=1
argv[1:1]=['-I'+str(root/'include'),'-I'+str(source.parent)]
argv+=['-ivfsoverlay',str(overlay),'-fsyntax-only']
llvm=what/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
p=subprocess.run(argv,capture_output=True,text=True,env=env,timeout=60)
(scratch/'stderr.log').write_text(p.stderr)
(scratch/'command.json').write_text(json.dumps({'argv':argv,'environment':{'LLVM_CONFIG':env['LLVM_CONFIG']},'exit':p.returncode,'original_command_sha256':hashlib.sha256(command.read_bytes()).hexdigest()},indent=2)+'\n')
print('target syntax exit=',p.returncode,'errors=',p.stderr.count(' error:'),'warnings=',p.stderr.count(' warning:'))
print(p.stderr[-5000:]);raise SystemExit(p.returncode)
