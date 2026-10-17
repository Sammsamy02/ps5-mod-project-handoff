from pathlib import Path
import json,hashlib,subprocess,os
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');HERE=Path(__file__).resolve().parent;OUT=HERE/'target-with-pinned-toolchain';OUT.mkdir(exist_ok=True)
receipt=ROOT/'outputs/2026-10-07-l19-r4-full-build/prx-candidate-l19-2/build-receipt.json';j=json.loads(receipt.read_text())
cmd=next(c['argv'] for c in j['commands'] if '-c' in c.get('argv',[]) and c['argv'][c['argv'].index('-c')+1].endswith('/runtime_module_entry.c'))
a=list(cmd)
for f,v in [('-c',str(HERE/'runtime_module_entry.c')),('-o',str(OUT/'runtime_module_entry.o')),('-MF',str(OUT/'runtime_module_entry.d'))]:a[a.index(f)+1]=v
toolchain=ROOT/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json'
llvm=json.loads(toolchain.read_text())['recipes']['unreal-uobject']['LLVM_CONFIG']
env=dict(os.environ,LLVM_CONFIG=llvm)
r=subprocess.run(a,cwd=ROOT,env=env,capture_output=True,text=True);(OUT/'STDOUT.log').write_text(r.stdout);(OUT/'STDERR.log').write_text(r.stderr)
pin=lambda p:{'path':str(p),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
record={'scope':'Actual whole C entry source with unchanged selected SDK recipe and headers; target compilation only','command':a,'LLVM_CONFIG':llvm,'exit':r.returncode,'base_recipe_receipt':pin(receipt),'toolchain_recipe':pin(toolchain),'candidate':pin(HERE/'runtime_module_entry.c'),'native_execution':False}
if not r.returncode:record['object']=pin(OUT/'runtime_module_entry.o');record['dependency']=pin(OUT/'runtime_module_entry.d')
(OUT/'TARGET-ENTRY-RECEIPT.json').write_text(json.dumps(record,indent=2)+'\n');print('whole entry actual SDK compile',r.returncode);print(r.stderr[-800:] if r.returncode else '')
raise SystemExit(r.returncode)
