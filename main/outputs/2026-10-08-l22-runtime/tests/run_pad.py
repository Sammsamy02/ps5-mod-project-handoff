from pathlib import Path
import json,subprocess,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
base=ROOT/'outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port'
source=Path(sys.argv[1]).resolve();name=sys.argv[2];out=HERE/name;out.mkdir(exist_ok=False)
recipe=json.loads((ROOT/'outputs/2026-10-04-zcode-team/worker-pad-coexist/host-1-coexist/compile/command.json').read_text())['argv']
flags=[];i=1
while i<len(recipe):
 a=recipe[i]
 if a=='-o':i+=2;continue
 if a.endswith(('.cpp','.cc','.c','.o','.a')):i+=1;continue
 if a.startswith('-fsanitize=') or a.startswith('-fno-sanitize-recover='):i+=1;continue
 flags.append(a);i+=1
records=[]
for mode,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])]:
 exe=out/mode;cmd=['/usr/bin/clang++','-I',str(base),*flags,*extra,str(HERE/'pad_markers.cpp'),str(source),'-o',str(exe)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(mode+'.build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError('compile failed: '+str(out/(mode+'.build.log')))
 run=subprocess.run([str(exe)],capture_output=True,text=True);(out/(mode+'.run.log')).write_text(run.stdout+run.stderr)
 records.append({'mode':mode,'argv':cmd,'exit':run.returncode,'output':run.stdout.strip()})
print(json.dumps([{k:r[k] for k in ('mode','exit','output')} for r in records],indent=2));(out/'RESULTS.json').write_text(json.dumps(records,indent=2)+'\n')
