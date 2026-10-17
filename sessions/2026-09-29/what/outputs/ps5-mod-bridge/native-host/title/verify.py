#!/usr/bin/env python3
"""Focused offline title decoding; native mode compiles/links and never executes."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import zipfile

SDK_SHA='a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb'
LLVM_SELECTED_SHA='7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896'
TOOLS={'clang':'8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
       'clang++':'8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
       'ld.lld':'0c4381c70e4229b9988e120710de371c031cedd1d6da8f415a9aa7bac21a3620',
       'llvm-config':'39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5',
       'llvm-nm':'a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426',
       'llvm-readobj':'73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30'}

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda:stream.read(1024*1024),b''): h.update(data)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output-dir',type=Path,required=True)
    p.add_argument('--ps5-sdk',type=Path,required=True,help='Already verified extracted SDK; used fixture is read-only')
    p.add_argument('--llvm-config',type=Path)
    p.add_argument('--resource-dir',type=Path)
    p.add_argument('--sdk-archive',type=Path)
    p.add_argument('--native',action='store_true')
    p.add_argument('--red',action='store_true')
    a=p.parse_args()
    if a.native and (a.red or not a.llvm_config or not a.resource_dir or not a.sdk_archive):
        p.error('native needs exact LLVM/resource/SDK archive paths and forbids RED')
    here=Path(__file__).resolve().parent; project=here.parent.parent
    vendor=project/'third_party'; sdk=a.ps5_sdk.resolve(); out=a.output_dir.resolve()
    fixture=sdk/'samples/install_app/FAKE02932/sce_sys/param.json'
    manifest=json.loads((vendor/'manifest.json').read_text())
    deps=[entry for entry in manifest['files'] if entry['path'].startswith(('nlohmann/','picosha2/'))]
    for entry in deps:
        if digest(vendor/entry['path'])!=entry['sha256']: raise SystemExit('Pinned dependency differs: '+entry['path'])
    tools=a.llvm_config.resolve().parent if a.llvm_config else None
    selected_path=here.parent/'llvm_selected_manifest.json'
    selected=None
    if a.native:
        if digest(a.sdk_archive.resolve())!=SDK_SHA: raise SystemExit('SDK release archive identity differs')
        if digest(selected_path)!=LLVM_SELECTED_SHA: raise SystemExit('Approved selected LLVM manifest differs')
        selected=json.loads(selected_path.read_text())
        for name,sha in TOOLS.items():
            if digest(tools/name)!=sha: raise SystemExit('Pinned tool differs: '+name)
    impl=here/'tests/red_stub.cpp' if a.red else here/'metadata.cpp'
    inputs=[here/'metadata.h',here/'verify.py',impl,fixture,vendor/'manifest.json',
        *[vendor/entry['path'] for entry in deps]]
    if a.native: inputs += [here/'tests/c_caller.c',selected_path]
    if not a.native: inputs += [here/'tests'/name for name in ('metadata_test.cpp','allocator_test.cpp','c_caller.c')]
    identities=[{'path':str(path),'sha256':digest(path)} for path in inputs]
    out.mkdir(parents=True,exist_ok=True)
    receipt={'target':'ps5-compile-link-only' if a.native else 'host-red' if a.red else 'host-green',
        'target_executed':False,'native_provider_available':False,'runtime_supported':False,
        'build_fingerprint_available':False,'installation_supported':False,'inputs':identities,
        'dependencies':deps,'commands':[],'sdk_commit':'d9c9519116944a7f1c22d262012c53b80b3520b7'}
    env=dict(os.environ); cc='/usr/bin/clang'; cxx='/usr/bin/clang++'
    common=['-O2','-Wall','-Wextra','-Werror','-I'+str(here),'-isystem',str(vendor),
        '-isystem',str(vendor/'picosha2')]
    if a.native:
        cc=str(sdk/'bin/prospero-clang'); cxx=str(sdk/'bin/prospero-clang++')
        env['LLVM_CONFIG']=str(a.llvm_config.resolve())
        common += ['-resource-dir',str(a.resource_dir.resolve())]
        receipt['llvm_config']=env['LLVM_CONFIG']; receipt['resource_dir']=str(a.resource_dir.resolve())
        receipt['llvm_selected_manifest']={'path':str(selected_path),'sha256':LLVM_SELECTED_SHA,
            'archive_sha256':selected['archive_sha256'],'package':selected['package']}
        receipt['verified_tool_sha256']=TOOLS
    def run(command,name,allowed=(0,)):
        r=subprocess.run([str(x) for x in command],env=env,capture_output=True,timeout=60)
        text=r.stdout+r.stderr; (out/name).write_bytes(text)
        receipt['commands'].append({'argv':[str(x) for x in command],'exit_code':r.returncode,'log':name})
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        if r.returncode not in allowed: raise SystemExit(text.decode(errors='replace')[-8000:] or 'command failed')
        return r.returncode,text.decode(errors='replace')
    run([cxx,'--version'],'compiler.log')
    objects={}; dependency_paths=set()
    def compile(source,cpp):
        if source in objects: return objects[source]
        obj=out/(source.stem+'.o'); dep=obj.with_suffix('.d')
        run([cxx if cpp else cc,'-std=c++20' if cpp else '-std=c11',*common,
            '-MD','-MF',dep,'-c',source,'-o',obj],source.stem+'-compile.log')
        dependency_paths.update(Path(x).resolve() for x in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]))
        objects[source]=obj; return obj
    impl_obj=compile(impl,True)
    if a.native:
        caller=compile(here/'tests/c_caller.c',False); binary=out/'metadata_caller.elf'
        run([cxx,*common,impl_obj,caller,'-Wl,-z,now','-Wl,-Map,'+str(out/'link.map'),'-o',binary],'link.log')
        _,elf=run([tools/'llvm-readobj','--file-headers','--needed-libs','--dynamic-table',binary],'elf.log')
        _,symbols=run([tools/'llvm-nm','--dynamic','--undefined-only',binary],'imports.log')
        if 'Format: elf64-x86-64' not in elf or 'BIND_NOW' not in elf or 'OS/ABI: FreeBSD' not in elf:
            raise SystemExit('Target ELF differs')
        receipt['imports']=sorted(line.split()[-1] for line in symbols.splitlines() if line.strip())
        closure=[]
        with zipfile.ZipFile(a.sdk_archive) as z:
            for path in sorted(dependency_paths):
                if path.is_relative_to(sdk):
                    if path.read_bytes()!=z.read('ps5-payload-sdk/'+path.relative_to(sdk).as_posix()):
                        raise SystemExit('Actual target SDK header differs: '+str(path))
                    kind='sdk_archive_verified'
                elif path.is_relative_to(vendor): kind='pinned_vendor'
                elif path.is_relative_to(here): kind='owned_source'
                elif path.is_relative_to(a.resource_dir.resolve()):
                    member='lib/clang/21/'+path.relative_to(a.resource_dir.resolve()).as_posix()
                    if digest(path)!=selected['members'].get(member):
                        raise SystemExit('Used resource header differs from approved pin: '+member)
                    kind='selected_manifest_verified_resource'
                else: raise SystemExit('Foreign/host target header: '+str(path))
                closure.append({'path':str(path),'kind':kind,'sha256':digest(path)})
            for name in ('bin/prospero-clang','bin/prospero-clang++','bin/prospero-llvm-config',
                'bin/prospero-lld','target/lib/libc.a','target/lib/libc++.a','target/lib/libc++abi.a','target/lib/libunwind.a'):
                path=sdk/name
                if path.read_bytes()!=z.read('ps5-payload-sdk/'+name): raise SystemExit('Used SDK input differs: '+name)
        receipt['include_closure']=closure; receipt['sdk_used_inputs_archive_matched']=True; receipt['bind_now']=True
        receipt['artifact']={'path':str(binary),'bytes':binary.stat().st_size}
    else:
        outcomes=[]
        for name,cpp in (('metadata_test',True),('allocator_test',True),('c_caller',False)):
            obj=compile(here/'tests'/(name+('.cpp' if cpp else '.c')),cpp); binary=out/name
            run([cxx,*common,impl_obj,obj,'-o',binary],name+'-link.log')
            command=[binary,*([fixture] if name=='metadata_test' else [])]
            rc,text=run(command,name+'-run.log',(1,) if a.red else (0,)); outcomes.append({'name':name,'exit_code':rc})
            if name=='metadata_test':
                if a.red and ('GROUP' not in text or 'FAIL' not in text): raise SystemExit('RED is not compiled behavior')
                if not a.red:
                    matches=re.findall(r'^SDK_SHA256 ([0-9a-f]{64})$',text,re.M)
                    if matches!=[digest(fixture)]: raise SystemExit('Actual metadata-only digest differs')
                    receipt['fixture_metadata_sha256']=matches[0]
        receipt['host_outcomes']=outcomes
    receipt['inputs_unchanged']=all(digest(Path(i['path']))==i['sha256'] for i in identities)
    if not receipt['inputs_unchanged']: raise SystemExit('Inputs changed during focused check')
    receipt['result']='EXPECTED_RED' if a.red else 'PASS'
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({key:receipt[key] for key in ('result','target','target_executed','native_provider_available','build_fingerprint_available')}))

if __name__=='__main__': main()
