from pathlib import Path
import json,os,subprocess
w=Path(__file__).resolve().parent;root=w.parent.parent
recipes=json.loads((root/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json').read_text())['recipes']
package=json.loads((root/'outputs/2026-10-07-l19-r3-full-build/prx-candidate-l19-2/build-receipt.json').read_text())
bridge=next(x for x in reversed(package['commands']) if '-c' in x['argv'] and any(v.endswith('lifecycle/runtime_start_bridge.cpp') for v in x['argv']))
out=w/'target-wiring-v2';out.mkdir(exist_ok=False);rows=[]
for name,recipe in [('runtime_start_bridge_r14_diag_spanpub',bridge),('TargetAbiCensus',recipes['unreal-unrealinitializer'])]:
 argv=recipe.get('original_argv',recipe.get('argv'))[:]
 prefix=[x for x in argv[:argv.index('-c')] if not x.endswith('.cpp')]
 prefix[1:1]=['-I',str(w/'candidate')]+recipes['unreal-unrealinitializer']['include_prefix']+['-I',str(root/'outputs/2026-10-07-l19-local-profile-r4/units')]
 command=prefix+['-c',str(w/'candidate'/(name+'.cpp')),'-o',str(out/(name+'.o')),'-MD','-MF',str(out/(name+'.d'))]
 env=dict(os.environ,LLVM_CONFIG=recipes['unreal-unrealinitializer']['LLVM_CONFIG'])
 p=subprocess.run(command,capture_output=True,text=True,env=env,timeout=180)
 rows.append(dict(name=name,argv=command,LLVM_CONFIG=env['LLVM_CONFIG'],exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stderr[-2500:] if p.returncode else '',flush=True)
 if p.returncode:raise SystemExit(p.returncode)
