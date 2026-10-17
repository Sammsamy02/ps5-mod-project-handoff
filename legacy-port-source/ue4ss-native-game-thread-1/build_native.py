#!/usr/bin/env python3
import concurrent.futures, hashlib, json, os, pathlib, shlex, subprocess, sys, time
W=pathlib.Path(__file__).resolve().parent
BASE=W.parent/'ue4ss-native-hook-registration-errors-1/native-2'
OUT=W/(sys.argv[1] if len(sys.argv)>1 else 'native-2')
OUT.mkdir(exist_ok=False)
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def build(pair):
    name,filename=pair
    recipe=BASE/(name+'-command.json')
    old=json.loads(recipe.read_text())
    argv=old['argv'] if isinstance(old,dict) else old
    argv=list(argv)
    source=W/'src'/filename
    source_indices=[i for i,value in enumerate(argv) if value.endswith('/src/'+filename)]
    assert len(source_indices)==1,source_indices
    argv[source_indices[0]]=str(source)
    argv[argv.index('-o')+1]=str(OUT/(name+'.o'))
    # Replace previous dependency output, retain actual target profile/includes.
    for flag in ['-MF','-MT','-MQ']:
        while flag in argv:
            i=argv.index(flag); del argv[i:i+2]
    argv=[arg for arg in argv if arg not in ['-MMD','-MD']]
    argv[1:1]=['-I',str(W/'include')]
    argv+=['-MD','-MF',str(OUT/(name+'.d'))]
    command=OUT/(name+'-command.json')
    command.write_text(json.dumps({'argv':argv},indent=2)+'\n')
    start=time.monotonic()
    env=dict(os.environ)
    env['LLVM_CONFIG']=old['LLVM_CONFIG']
    run=subprocess.run(argv,capture_output=True,env=env)
    (OUT/(name+'.stdout')).write_bytes(run.stdout)
    (OUT/(name+'.stderr')).write_bytes(run.stderr)
    result={'name':name,'source':str(source),'source_sha256':digest(source),
        'exit':run.returncode,'elapsed_seconds':time.monotonic()-start,
        'command':str(command),'LLVM_CONFIG':env['LLVM_CONFIG'],
        'base_command':str(recipe),'base_command_sha256':digest(recipe)}
    obj=OUT/(name+'.o')
    if obj.exists(): result.update(object=str(obj),object_bytes=obj.stat().st_size,object_sha256=digest(obj))
    dep=OUT/(name+'.d')
    if dep.exists():
        text=dep.read_text().replace('\\\n',' ')
        paths=shlex.split(text.split(':',1)[1])
        hashes={p:digest(pathlib.Path(p))for p in paths if pathlib.Path(p).is_file()}
        dp=OUT/(name+'-dependency-hashes.json');dp.write_text(json.dumps(hashes,indent=2)+'\n')
        result.update(dependency_file=str(dep),dependency_hashes=str(dp))
    print(name,run.returncode,flush=True)
    return result
with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:
    units=list(pool.map(build,[('luamod','LuaMod.cpp'),('unreal-unrealinitializer','UnrealInitializer.cpp')]))
(OUT/'receipt.json').write_text(json.dumps({'units':units,'all_compiled':all(x['exit']==0 for x in units),
    'native_execution':False,'console_contact':False,'public_headers_layout_changed':False,
    'helper_header':str(W/'include/Unreal/Platform/NativeGameThreadIdentity.hpp'),
    'helper_header_sha256':digest(W/'include/Unreal/Platform/NativeGameThreadIdentity.hpp'),
    'build_script_sha256':digest(pathlib.Path(__file__))},indent=2)+'\n')
raise SystemExit(0 if all(x['exit']==0 for x in units)else 1)
