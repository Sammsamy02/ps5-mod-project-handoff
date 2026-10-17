from pathlib import Path
import subprocess,json,os,sys
w=Path(__file__).resolve().parent;root=w.parent.parent
p=root/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json'
r=json.loads(p.read_text())['recipes']['unreal-unrealinitializer']
args=r['original_argv'][:]
args=args[:args.index('-c')]
args[1:1]=r['include_prefix']+['-I',str(w/'candidate'),'-I',str(root/'outputs/2026-10-07-l19-local-profile-r4/units'),'-I',str(root/'.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/include')]
env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG'])
out=w/(sys.argv[1] if len(sys.argv)>1 else 'target');out.mkdir(exist_ok=False)
rows=[]
for name in ['NativeThreadAuthority','SelectedThreadAuthorityAdapter','UnrealInitializer']:
 argv=args+['-c',str(w/'candidate'/(name+'.cpp')),'-o',str(out/(name+'.o')),'-MD','-MF',str(out/(name+'.d'))]
 p=subprocess.run(argv,env=env,capture_output=True,text=True,timeout=180)
 rows.append(dict(name=name,argv=argv,LLVM_CONFIG=r['LLVM_CONFIG'],exit=p.returncode,stdout=p.stdout,stderr=p.stderr))
 (out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stderr[-2500:] if p.returncode else '',flush=True)
 if p.returncode:raise SystemExit(p.returncode)
