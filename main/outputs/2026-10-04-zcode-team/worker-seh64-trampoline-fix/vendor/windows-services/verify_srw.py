#!/usr/bin/env python3
"""Real pthread/Win64 SRW and condition-variable controls; direct PS5 owner plus unchanged win64cc IR.
No target execution, console contact, arbitrary DLL execution or runtime install.
"""
import argparse, json, os, platform, re, subprocess, uuid
from pathlib import Path
from verify import HERE, ROOT, sha, data_layout

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host-only',action='store_true');p.add_argument('--sanitize',action='store_true')
    p.add_argument('--tsan',action='store_true');p.add_argument('--red-stub',action='store_true')
    p.add_argument('--output',type=Path,default=ROOT/'.superpowers/native-dependency-spike/windows-srw-services-1')
    a=p.parse_args();out=a.output.resolve()/uuid.uuid4().hex;out.mkdir(parents=True)
    sdk=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
    llvm=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    sources=[HERE/x for x in ['mb_win_srw.h','mb_win_srw.c','mb_win_srw_abi.h','mb_win_srw_abi.c','mb_win_threads.h','mb_win_threads.c','tests/test_win_srw.c','verify_srw.py','verify.py']]
    sources += [HERE.parent/'windows-execution/pe_module.h',HERE.parent/'windows-image/pe_image.h']
    rec={'status':'IN_PROGRESS','commands':[],'sources':{str(x):sha(x) for x in sources},'target_executed':False,'console_contact':False,'pe_fixture_executed':False}
    def save(): (out/'receipt.json').write_text(json.dumps(rec,indent=2)+'\n')
    def run(name,argv,env=None,expected=0):
        argv=list(map(str,argv));r=subprocess.run(argv,cwd=ROOT,env=env,capture_output=True,text=True,timeout=120)
        (out/(name+'.stdout')).write_text(r.stdout);(out/(name+'.stderr')).write_text(r.stderr)
        rec['commands'].append({'label':name,'argv':argv,'exit':r.returncode,'expected_exit':expected});save()
        if r.returncode!=expected:raise RuntimeError(name+' failed: '+r.stderr)
        return r.stdout
    flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin']
    arch=['-arch','x86_64'] if platform.system()=='Darwin' else []
    runner=['/usr/bin/arch','-x86_64'] if arch else []
    try:
        owner_source=HERE/'mb_win_srw.c'
        if a.red_stub:
            content=owner_source.read_text();start=content.index('{',content.index('mb_win_srw_status mb_win_srw_open('));end=start+1;depth=1
            while depth:
                depth += (content[end]=='{')-(content[end]=='}');end+=1
            owner_source=out/'open-stub.c';owner_source.write_text(content[:start]+'{(void)o;(void)c;(void)error;return MB_WIN_SRW_BAD_STATE;}'+content[end:])
            rec['negative_control']={'path':str(owner_source),'sha256':sha(owner_source),'mutation':'Only owner open returns BAD_STATE; expected real typed test rejects it.'}
        modes=[('host',[])]+([('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])] if a.sanitize and not a.red_stub else [])+([('tsan',['-fsanitize=thread','-fno-omit-frame-pointer'])] if a.tsan and not a.red_stub else [])
        for label,extra in modes:
            exe=out/(label+'-test')
            run(label+'-compile',['clang',*arch,*flags,*extra,'-I',HERE,'-pthread',owner_source,HERE/'mb_win_srw_abi.c',HERE/'mb_win_threads.c',HERE/'tests/test_win_srw.c','-o',exe])
            rec[label+'_result']=run(label+'-run',[*runner,exe],expected=1 if a.red_stub else 0).strip()
        if a.red_stub:
            assert 'mb_win_srw_open(&owner,&config,NULL)==MB_WIN_SRW_OK' in (out/'host-run.stderr').read_text()
            assert rec['sources']=={str(x):sha(x) for x in sources}
            rec['sources_stable']=True;rec['status']='EXPECTED_OPEN_STUB_REJECTION';save();print(json.dumps({'status':rec['status'],'receipt':str(out/'receipt.json')}));return
        if not a.host_only:
            cc=sdk/'bin/prospero-clang';env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'));nf=[*flags,'-fPIC','-march=x86-64']
            bf=[*nf,'--target=x86_64-unknown-freebsd9','-fshort-wchar']
            for label,fl in [('native',nf),('bridge',bf)]:run(label+'-layout',[cc,*fl,'-DMB_WIN_SRW_LAYOUT','-S','-emit-llvm',HERE/'tests/test_win_srw.c','-o',out/(label+'-layout.ll')],env)
            n=(out/'native-layout.ll').read_text();b=(out/'bridge-layout.ll').read_text()
            def layout(s):return list(map(int,re.findall(r'i64 (\d+)',next(x for x in s.splitlines() if x.startswith('@mb_srw_layout =')))))
            assert data_layout(n)==data_layout(b) and layout(n)==layout(b) and layout(n)[:3]==[8,8,4]
            run('native-owner',[cc,*nf,'-MD','-MF',out/'owner.d','-c',HERE/'mb_win_srw.c','-o',out/'mb_win_srw.o'],env)
            ir=out/'srws.ll';run('bridge-ir',[cc,*bf,'-MD','-MF',out/'abi.d','-S','-emit-llvm',HERE/'mb_win_srw_abi.c','-o',ir],env)
            source=ir.read_text();defs=[x for x in source.splitlines() if x.startswith('define ') and 'win64cc' in x]
            assert len(defs)==11 and data_layout(source)==data_layout(n)
            assert sum('win64cc void ' in x for x in defs)==8
            assert re.search(r'@sleep_condition\(ptr [^,]*, ptr [^,]*, i32 [^,]*, i32 [^)]*\)',source)
            assert sum(bool(re.search(r'win64cc(?: zeroext)? i8 ',x)) for x in defs)==2
            assert sum(bool(re.search(r'win64cc i32 ',x)) for x in defs)==1
            run('bridge-backend',[cc,'-x','ir','-O2','-fPIC','-Werror','-Wno-override-module','-c',ir,'-o',out/'mb_win_srw_abi.o'],env)
            rec['native']={'method':'Direct PS5 core; FreeBSD9 frontend -> unedited Win64 IR -> PS5 backend','layout':layout(n),'data_layout':data_layout(n),'ir_sha256':sha(ir),'win64_definitions':defs,'objects':[]}
            for name in ['mb_win_srw','mb_win_srw_abi']:
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
        rec['sources_stable']=True;rec['status']='SRW_PTHREAD_WIN64_AND_NATIVE_OBJECTS_PASSED' if not a.host_only else 'SRW_PTHREAD_WIN64_PASSED';save()
        print(json.dumps({'status':rec['status'],'receipt':str(out/'receipt.json')},indent=2))
    except Exception as e:rec['status']='FAILED';rec['error']=str(e);save();raise
if __name__=='__main__':main()
