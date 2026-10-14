#!/usr/bin/env python3
"""Real pthread/Win64 KERNEL32 mutex+wait controls; direct PS5 owner plus unchanged win64cc IR.
VENDORED ADAPTED COPY (origin: pattern of outputs/2026-10-04-zcode-team/worker-4-win-services/verify_init_once.py, itself from outputs/ps5-mod-bridge/windows-services/verify_srw.py; originals unchanged).
No target execution, console contact, arbitrary DLL execution or runtime install.
Recorded native SDK provider declarations are component evidence only, never a target resolution claim."""
import argparse, importlib.util, json, os, platform, re, subprocess, uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
SERV = HERE.parents[2] / 'outputs/ps5-mod-bridge/windows-services'
EXEC = HERE.parents[2] / 'outputs/ps5-mod-bridge/windows-execution'
IMAGE = HERE.parents[2] / 'outputs/ps5-mod-bridge/windows-image'
SRC = HERE / 'sources'
_spec = importlib.util.spec_from_file_location('mb_windows_verify_helpers', SERV / 'verify.py')
_helpers = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_helpers)
ROOT, sha, data_layout = _helpers.ROOT, _helpers.sha, _helpers.data_layout


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host-only',action='store_true');p.add_argument('--sanitize',action='store_true')
    p.add_argument('--tsan',action='store_true');p.add_argument('--red-stub',action='store_true')
    p.add_argument('--output',type=Path,default=HERE/'receipts')
    a=p.parse_args();out=a.output.resolve()/uuid.uuid4().hex;out.mkdir(parents=True)
    sdk=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
    llvm=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    sources=[SRC/x for x in ['mb_win_mutex.h','mb_win_mutex.c','mb_win_mutex_abi.h','mb_win_mutex_abi.c','tests/test_win_mutex.c']]
    sources += [HERE/'verify_mutex.py',SERV/'verify.py',SERV/'mb_win_threads.h',SERV/'mb_win_threads.c']
    sources += [EXEC/'pe_module.h',IMAGE/'pe_image.h']
    rec={'status':'IN_PROGRESS','commands':[],'sources':{str(x):sha(x) for x in sources},'target_executed':False,'console_contact':False,'pe_fixture_executed':False}
    def save(): (out/'receipt.json').write_text(json.dumps(rec,indent=2)+'\n')
    def run(name,argv,env=None,expected=0):
        argv=list(map(str,argv));r=subprocess.run(argv,cwd=ROOT,env=env,capture_output=True,text=True,timeout=180)
        (out/(name+'.stdout')).write_text(r.stdout);(out/(name+'.stderr')).write_text(r.stderr)
        rec['commands'].append({'label':name,'argv':argv,'exit':r.returncode,'expected_exit':expected});save()
        if r.returncode!=expected:raise RuntimeError(name+' failed: '+r.stderr)
        return r.stdout
    flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin']
    arch=['-arch','x86_64'] if platform.system()=='Darwin' else []
    runner=['/usr/bin/arch','-x86_64'] if arch else []
    include=['-I',SRC,'-I',SERV,'-I',EXEC]
    try:
        owner_source=SRC/'mb_win_mutex.c'
        if a.red_stub:
            content=owner_source.read_text();start=content.index('{',content.index('mb_win_mutex_status mb_win_mutex_open('));end=start+1;depth=1
            while depth:
                depth += (content[end]=='{')-(content[end]=='}');end+=1
            owner_source=out/'open-stub.c';owner_source.write_text(content[:start]+'{(void)o;(void)c;(void)error;return MB_WIN_MUTEX_BAD_STATE;}'+content[end:])
            rec['negative_control']={'path':str(owner_source),'sha256':sha(owner_source),'mutation':'Only owner open returns BAD_STATE; expected real typed test rejects it.'}
        modes=[('host',[])]+([('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])] if a.sanitize and not a.red_stub else [])+([('tsan',['-fsanitize=thread','-fno-omit-frame-pointer'])] if a.tsan and not a.red_stub else [])
        for label,extra in modes:
            exe=out/(label+'-test')
            run(label+'-compile',['clang',*arch,*flags,*extra,*include,'-pthread',owner_source,SRC/'mb_win_mutex_abi.c',SERV/'mb_win_threads.c',SRC/'tests/test_win_mutex.c','-o',exe])
            rec[label+'_result']=run(label+'-run',[*runner,exe],expected=1 if a.red_stub else 0).strip()
        if a.red_stub:
            assert 'mb_win_mutex_open(&owner,&config,NULL)==MB_WIN_MUTEX_OK' in (out/'host-run.stderr').read_text()
            assert rec['sources']=={str(x):sha(x) for x in sources}
            rec['sources_stable']=True;rec['status']='EXPECTED_OPEN_STUB_REJECTION';save();print(json.dumps({'status':rec['status'],'receipt':str(out/'receipt.json')}));return
        if not a.host_only:
            cc=sdk/'bin/prospero-clang';env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'));nf=[*flags,'-fPIC','-march=x86-64',*include]
            bf=[*nf,'--target=x86_64-unknown-freebsd9','-fshort-wchar']
            for label,fl in [('native',nf),('bridge',bf)]:run(label+'-layout',[cc,*fl,'-DMB_WIN_MUTEX_LAYOUT','-S','-emit-llvm',SRC/'tests/test_win_mutex.c','-o',out/(label+'-layout.ll')],env)
            n=(out/'native-layout.ll').read_text();b=(out/'bridge-layout.ll').read_text()
            def layout(s):return list(map(int,re.findall(r'i64 (\d+)',next(x for x in s.splitlines() if x.startswith('@mb_mutex_layout =')))))
            expected_layout=[8,8,4,4,8,16,16,40,16,32]
            assert data_layout(n)==data_layout(b) and layout(n)==layout(b)==expected_layout, (layout(n),layout(b))
            run('native-owner',[cc,*nf,'-MD','-MF',out/'owner.d','-c',SRC/'mb_win_mutex.c','-o',out/'mb_win_mutex.o'],env)
            ir=out/'mutex.ll';run('bridge-ir',[cc,*bf,'-MD','-MF',out/'abi.d','-S','-emit-llvm',SRC/'mb_win_mutex_abi.c','-o',ir],env)
            source=ir.read_text();defs=[x for x in source.splitlines() if x.startswith('define ') and 'win64cc' in x]
            assert len(defs)==6 and data_layout(source)==data_layout(n)
            assert sum('win64cc ptr @' in x for x in defs)==3
            assert sum('win64cc i32 @' in x for x in defs)==3
            assert re.search(r'@create_mutex_w\(ptr [^,]*, i32 [^,]*, ptr [^)]*\)',source)
            assert re.search(r'@create_mutex_ex_w\(ptr [^,]*, ptr [^,]*, i32 [^,]*, i32 [^)]*\)',source)
            assert re.search(r'@open_mutex_w\(i32 [^,]*, i32 [^,]*, ptr [^)]*\)',source)
            assert re.search(r'@release_mutex\(ptr [^)]*\)',source)
            assert re.search(r'@wait_for_single_object\(ptr [^,]*, i32 [^)]*\)',source)
            assert re.search(r'@wait_for_single_object_ex\(ptr [^,]*, i32 [^,]*, i32 [^)]*\)',source)
            binder=next(x for x in source.splitlines() if '@mb_win_mutex_bind(' in x and x.startswith('define '))
            assert 'win64cc' not in binder
            run('bridge-backend',[cc,'-x','ir','-O2','-fPIC','-Werror','-Wno-override-module','-c',ir,'-o',out/'mb_win_mutex_abi.o'],env)
            rec['native']={'method':'Direct PS5 core; FreeBSD9 frontend -> unedited Win64 IR -> PS5 backend','layout':layout(n),'data_layout':data_layout(n),'ir_sha256':sha(ir),'win64_definitions':defs,'objects':[]}
            for name in ['mb_win_mutex','mb_win_mutex_abi']:
                obj=out/(name+'.o');undef=run(name+'-undefined',[llvm/'llvm-nm','--undefined-only','--format=posix',obj]);run(name+'-disassembly',[llvm/'llvm-objdump','-dr',obj])
                rec['native']['objects'].append({'path':str(obj),'sha256':sha(obj),'bytes':obj.stat().st_size,'imports':[x.split()[0] for x in undef.splitlines() if x.split()]})
            rec['native_dependencies']={}
            for dep in ['owner.d','abi.d']:
                for word in (out/dep).read_text().replace('\\\n',' ').split()[1:]:
                    f=Path(word)
                    if f.is_file():rec['native_dependencies'][str(f)]=sha(f)
            # Retained SDK export declarations only, never a target resolution claim.
            declarations={}
            for lib in ['libkernel_web.so','libSceLibcInternal.so']:
                path=sdk/'target/lib'/lib
                if path.exists():
                    text=run(lib+'-exports',[llvm/'llvm-nm','-D','--defined-only','--format=posix',path]);names={x.split()[0]:x for x in text.splitlines() if x.split()}
                    needed=set(rec['native']['objects'][0]['imports'])
                    declarations[lib]={'path':str(path),'sha256':sha(path),'symbols':{k:names[k] for k in sorted(needed&names.keys())},'target_resolution_proven':False}
            rec['sdk_declarations']=declarations
            declared={symbol for lib in declarations.values() for symbol in lib['symbols']}
            required={symbol for symbol in rec['native']['objects'][0]['imports'] if not symbol.startswith('mb_win_threads_')}
            assert required<=declared, sorted(required-declared)
        assert rec['sources']=={str(x):sha(x) for x in sources}
        rec['sources_stable']=True;rec['status']='MUTEX_PTHREAD_WIN64_AND_NATIVE_OBJECTS_PASSED' if not a.host_only else 'MUTEX_PTHREAD_WIN64_PASSED';save()
        print(json.dumps({'status':rec['status'],'receipt':str(out/'receipt.json')},indent=2))
    except Exception as e:rec['status']='FAILED';rec['error']=str(e);save();raise
if __name__=='__main__':main()
