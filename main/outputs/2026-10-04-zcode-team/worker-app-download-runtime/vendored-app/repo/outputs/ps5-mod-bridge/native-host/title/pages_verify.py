#!/usr/bin/env python3
"""Pure page fixtures and pinned PS5 compile/link; native artifacts never execute."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import time
import zipfile

SDK_SHA='a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb'
LLVM_SELECTED_SHA='7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896'
TOOLS={'clang':'8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
       'ld.lld':'0c4381c70e4229b9988e120710de371c031cedd1d6da8f415a9aa7bac21a3620',
       'llvm-config':'39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5',
       'llvm-nm':'a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426',
       'llvm-readobj':'73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30'}
REASONS=('platform_provider_unavailable','provider_error','invalid_provider_capture',
         'inventory_resource_limit','source_changed','provider_observation_incomplete')

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda:stream.read(1024*1024),b''): h.update(data)
    return h.hexdigest()

def canonical(value):
    return json.dumps(value,ensure_ascii=True,sort_keys=True,separators=(',',':'),allow_nan=False).encode('ascii')

def fixtures(out):
    """Independent Python schema/packing oracle for the C caller's fixed data."""
    checks=0
    def require(value,message):
        nonlocal checks
        checks+=1
        if not value: raise SystemExit('Python oracle: '+message)
    def metadata(value,scope,status='observed'):
        return {'source':scope if status=='observed' else None,'status':status,
                'value':value if status=='observed' else None}
    def items(count,scope):
        return [{'title_key':f'{n+2:032x}',
                 'title_id':metadata(f'Any.Custom-{n:03}',scope),
                 'display_name':metadata('Café 🧭 "quote" \\ tail' if n==0 else 'é'*128,scope),
                 'build_version':metadata('1.🔧' if n==0 else '\x81'*64,scope),
                 'build_fingerprint':None} for n in range(count)]
    def snapshot(scope,status='observed',issues=None,reason=None):
        return {'status':status,'complete':status=='observed','source_scope':scope,
                'snapshot_id':f'{1:032x}' if status=='observed' else None,
                'issues':issues or [],'reason':reason}
    def page(s,rows,index=0,count=1,offset=0,total=0,cursor=None):
        return {'schema_version':2,'kind':'title_inventory_page',
                'snapshot':dict(s,issues=s['issues'] if index==0 else []),
                'items':rows,'page':None if not s['complete'] else {
                    'index':index,'page_count':count,'item_offset':offset,
                    'total_count':total,'next_cursor':cursor}}
    def packed(s,rows):
        if not s['complete']: return [page(s,[])],None
        def partition(assumed):
            bounds=[]; start=0
            if not rows: return [(0,0)]
            while start<len(rows):
                end=start
                while end<len(rows) and end-start<8:
                    candidate=page(s,rows[start:end+1],len(bounds),assumed,start,len(rows),
                                   None if end+1==len(rows) else 'f'*32)
                    if len(canonical(candidate))>8191: break
                    end+=1
                require(end>start,'single row fit'); bounds.append((start,end)); start=end
            return bounds
        assumed=1
        for _ in range(5):
            bounds=partition(assumed)
            if len(bounds)==assumed: break
            assumed=len(bounds)
        else: raise SystemExit('Oracle page convergence')
        return [page(s,rows[a:b],i,len(bounds),a,len(rows),
                     None if i+1==len(bounds) else f'{1000+i:032x}')
                for i,(a,b) in enumerate(bounds)], {
                    'schema_version':2,'kind':'title_inventory_current',
                    'snapshot_id':s['snapshot_id'],'current':True}
    private='system_private_metadata_cache'; user='user_metadata_cache'
    data={
        'generic128':(snapshot(private,issues=[{'code':REASONS[n%6]} for n in range(32)]),items(128,private)),
        'empty':(snapshot(user),[]),
        'optional-empty':(snapshot(user),items(1,user)),
        'optional-error':(snapshot(user),items(1,user)),
        'trim-policy':(snapshot(user),items(1,user)),
        'partial':(snapshot(private,'partial',[{'code':'source_changed'}],
                            'provider_observation_incomplete'),items(2,private)),
        'unavailable':(snapshot(user,'unavailable',reason='platform_provider_unavailable'),[]),
    }
    for prefix in ('optional-empty','optional-error'):
        status='unavailable' if prefix=='optional-empty' else 'error'
        for field in ('display_name','build_version'):
            data[prefix][1][0][field]=metadata(None,user,status)
    data['trim-policy'][1][0]['display_name']=metadata('\ufeffOK\x81',user)
    data['trim-policy'][1][0]['build_version']=metadata(None,user,'error')
    manifest={}
    for prefix,(s,rows) in data.items():
        pages,current=packed(s,rows)
        for i,value in enumerate(pages):
            name=f'{prefix}-page-{i:03}.json'; actual=(out/name).read_bytes(); expected=canonical(value)
            require(actual==expected,name+' exact Python bytes')
            require(len(actual)<=8191,name+' byte cap')
            require(json.loads(actual)==value,name+' owning schema')
            require(all(item['build_fingerprint'] is None for item in value['items']),name+' no build authority')
        names=sorted(out.glob(prefix+'-page-*.json'))
        require(len(names)==len(pages),prefix+' exact page count')
        if current:
            require((out/(prefix+'-current.json')).read_bytes()==canonical(current),prefix+' exact current bytes')
        else:
            require(not (out/(prefix+'-current.json')).exists(),prefix+' no terminal current')
        capture={'schema_version':2,'kind':'title_inventory_capture','snapshot':s,'items':rows}
        manifest[prefix]={'page_count':len(pages),'record_count':len(rows) if s['complete'] else 0,
                          'logical_capture_bytes':len(canonical(capture)),
                          'required_backing_bytes':sum(len(canonical(x)) for x in pages)+(len(canonical(current)) if current else 0)}
    (out/'oracle.json').write_bytes(canonical(manifest))
    return checks,manifest

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode',choices=('red','host','native')); p.add_argument('--output-dir',type=Path)
    a=p.parse_args(); here=Path(__file__).resolve().parent; tests=here/'pages_tests'
    project=here.parent.parent; workspace=project.parent.parent
    out=(a.output_dir or tests/'build'/(a.mode+'-'+str(time.time_ns()))).resolve()
    if not out.is_relative_to((tests/'build').resolve()): p.error('Output must remain in owned pages_tests/build')
    if out.exists(): p.error('Use a fresh output directory')
    native=a.mode=='native'; red=a.mode=='red'
    impl=tests/'red_stub.c' if red else here/'pages.c'
    web=project/'web/app.js'
    inputs=[here/'pages.h',here/'pages_verify.py',here/'catalog.h',here/'source.h',here/'metadata.h',
            impl,tests/('native_caller.c' if native else 'c_caller.c')]
    if not native and not red: inputs += [tests/'cross_language.cjs',web]
    ids=[{'path':str(path),'sha256':digest(path)} for path in inputs]
    out.mkdir(parents=True)
    receipt={'mode':a.mode,'native_executed':False,'provider_available':False,
        'runtime_supported':False,'installation_supported':False,'build_fingerprint_available':False,
        'inputs':ids,'commands':[]}
    env=dict(os.environ); env['PYTHONDONTWRITEBYTECODE']='1'
    def run(command,name,allowed=(0,)):
        command=[str(x) for x in command]
        try: r=subprocess.run(command,env=env,capture_output=True,timeout=60)
        except subprocess.TimeoutExpired as e:
            (out/name).write_bytes((e.stdout or b'')+(e.stderr or b''))
            receipt['commands'].append({'argv':command,'timeout_seconds':60,'log':name})
            (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
            raise SystemExit('Focused command timeout: '+name)
        text=r.stdout+r.stderr; (out/name).write_bytes(text)
        receipt['commands'].append({'argv':command,'exit_code':r.returncode,'log':name})
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        if r.returncode not in allowed: raise SystemExit(text.decode(errors='replace')[-8000:] or name+' failed')
        return text.decode(errors='replace')
    sdk=here.parent/'build/sdk-v0.43/ps5-payload-sdk'
    archive=project/'native-probe/build/deps/sdk.zip'
    llvm=project/'native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    resource=workspace/'.superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21'
    selected_file=here.parent/'llvm_selected_manifest.json'
    common=['-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(here)]
    cc='/usr/bin/clang'
    if native:
        if digest(archive)!=SDK_SHA or digest(selected_file)!=LLVM_SELECTED_SHA:
            raise SystemExit('Pinned SDK/resource manifest differs')
        selected=json.loads(selected_file.read_text())
        for name,sha in TOOLS.items():
            if digest(llvm/name)!=sha: raise SystemExit('Invoked tool differs: '+name)
        cc=str(sdk/'bin/prospero-clang'); env['LLVM_CONFIG']=str(llvm/'llvm-config')
        common+=['-resource-dir',str(resource)]
        receipt.update(sdk_commit='d9c9519116944a7f1c22d262012c53b80b3520b7',sdk_archive_sha256=SDK_SHA,
                       tool_sha256=TOOLS,resource_manifest_sha256=LLVM_SELECTED_SHA)
    run([cc,'--version'],'compiler.log')
    deps=set()
    def compile(source,name):
        obj=out/(name+'.o'); dep=obj.with_suffix('.d')
        run([cc,*common,'-MD','-MF',dep,'-c',source,'-o',obj],name+'-compile.log')
        deps.update(Path(x).resolve() for x in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]))
        return obj
    pages_obj=compile(impl,'pages'); caller=compile(inputs[-1] if native else tests/'c_caller.c','caller')
    binary=out/('pages_caller.elf' if native else 'pages_test')
    run([cc,*common,pages_obj,caller,*(['-Wl,-z,now','-Wl,-Map,'+str(out/'link.map')] if native else []),'-o',binary],'link.log')
    if native:
        elf=run([llvm/'llvm-readobj','--file-headers','--needed-libs','--dynamic-table',binary],'elf.log')
        symbols=run([llvm/'llvm-nm','--dynamic','--undefined-only',binary],'imports.log')
        object_symbols=run([llvm/'llvm-nm','--undefined-only',pages_obj],'pages-imports.log')
        if 'Format: elf64-x86-64' not in elf or 'OS/ABI: FreeBSD' not in elf or 'BIND_NOW' not in elf:
            raise SystemExit('Pinned target ELF differs')
        imports=sorted(line.split()[-1] for line in object_symbols.splitlines() if line.strip())
        if not set(imports).issubset({'memset','memcpy','memcmp','strlen','memmove','__stack_chk_fail'}):
            raise SystemExit('Unexpected page-object I/O/allocation/source import: '+str(imports))
        closure=[]
        with zipfile.ZipFile(archive) as z:
            for path in sorted(deps):
                if path.is_relative_to(sdk):
                    if path.read_bytes()!=z.read('ps5-payload-sdk/'+path.relative_to(sdk).as_posix()):
                        raise SystemExit('Used SDK header differs: '+str(path))
                    kind='sdk_archive_verified'
                elif path.is_relative_to(resource):
                    member='lib/clang/21/'+path.relative_to(resource).as_posix()
                    if digest(path)!=selected['members'].get(member): raise SystemExit('Resource header differs')
                    kind='selected_manifest_verified_resource'
                elif path.is_relative_to(here): kind='source_header'
                else: raise SystemExit('Foreign/host target header: '+str(path))
                closure.append({'path':str(path),'kind':kind,'sha256':digest(path)})
            used=[]
            for name in ('bin/prospero-clang','bin/clang','bin/prospero-llvm-config','bin/prospero-lld',
                         'target/lib/crt1.o','target/lib/libc.a','target/lib/libkernel_web.so',
                         'target/lib/libSceLibcInternal.so','target/lib/libSceNet.so','ldscripts/elf_x86_64.x'):
                path=sdk/name
                if path.read_bytes()!=z.read('ps5-payload-sdk/'+name): raise SystemExit('Used SDK input differs: '+name)
                used.append({'path':str(path),'sha256':digest(path)})
        receipt.update(include_closure=closure,sdk_used_inputs=used,pages_object_imports=imports,
                       imports=sorted(line.split()[-1] for line in symbols.splitlines() if line.strip()),bind_now=True)
    else:
        fixture_dir=out/'fixtures'; fixture_dir.mkdir()
        text=run([binary,fixture_dir],'caller-run.log',(1,) if red else (0,))
        print(text.strip())
        match=re.search(r'RESULT (\d+) checks (\d+) failures',text)
        if not match: raise SystemExit('No actual compiled behavior result')
        receipt['c_checks']=int(match[1]); receipt['c_failures']=int(match[2])
        if red:
            if not receipt['c_failures']: raise SystemExit('RED caller unexpectedly passed')
        else:
            count,oracle=fixtures(fixture_dir); receipt['python_oracle_checks']=count
            for name,expected in oracle.items():
                line=re.search(r'^FIXTURE '+re.escape(name)+r' pages=(\d+) rows=(\d+) logical=(\d+) backing=(\d+)$',text,re.M)
                if not line or tuple(map(int,line.groups()))!=tuple(expected.values()):
                    raise SystemExit('Plan differs from independent Python oracle: '+name)
            node=Path('/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node')
            print(run([node,tests/'cross_language.cjs',fixture_dir,web],'javascript-run.log').strip())
    receipt['inputs_unchanged']=all(digest(Path(i['path']))==i['sha256'] for i in ids)
    if not receipt['inputs_unchanged']: raise SystemExit('Source changed during focused proof')
    receipt['artifact']={'path':str(binary),'bytes':binary.stat().st_size,'sha256':digest(binary)}
    receipt['result']='EXPECTED_RED' if red else 'PASS'
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'result':receipt['result'],'mode':a.mode,'output':str(out),'native_executed':False}))

if __name__=='__main__': main()
