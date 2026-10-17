#!/usr/bin/env python3
"""Focused source capture fixtures; native mode compiles/links and never executes."""
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
    p.add_argument('mode',choices=('red','host','native'))
    p.add_argument('--output-dir',type=Path)
    a=p.parse_args()
    here=Path(__file__).resolve().parent; project=here.parent.parent; workspace=project.parent.parent
    vendor=project/'third_party'; tests=here/'source_tests'
    out=(a.output_dir or tests/'build'/(a.mode+'-'+str(time.time_ns()))).resolve()
    if not out.is_relative_to((tests/'build').resolve()): p.error('Output must stay in owned source_tests/build')
    if out.exists(): p.error('Use a fresh output directory; prior evidence is retained')
    sdk=here.parent/'build/sdk-v0.43/ps5-payload-sdk'
    sdk_archive=project/'native-probe/build/deps/sdk.zip'
    llvm=project/'native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    resource=workspace/'.superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21'
    selected_path=here.parent/'llvm_selected_manifest.json'
    manifest=json.loads((vendor/'manifest.json').read_text())
    deps=[e for e in manifest['files'] if e['path'].startswith(('nlohmann/','picosha2/'))]
    for e in deps:
        if digest(vendor/e['path'])!=e['sha256']: raise SystemExit('Pinned dependency differs: '+e['path'])
    native=a.mode=='native'; red=a.mode=='red'
    if native:
        if digest(sdk_archive)!=SDK_SHA: raise SystemExit('SDK archive differs')
        if digest(selected_path)!=LLVM_SELECTED_SHA: raise SystemExit('Approved LLVM resource manifest differs')
        selected=json.loads(selected_path.read_text())
        for name,sha in TOOLS.items():
            if digest(llvm/name)!=sha: raise SystemExit('Invoked tool differs: '+name)
    impl=tests/'red_stub.c' if red else here/'source.c'
    inputs=[here/'source.h',here/'source_verify.py',impl,here/'metadata.h',here/'metadata.cpp',
            tests/'c_caller.c',vendor/'manifest.json',*[vendor/e['path'] for e in deps]]
    if not native: inputs += [here/'source_test.h',tests/'source_test.cpp']
    else: inputs += [selected_path]
    identities=[{'path':str(path),'sha256':digest(path)} for path in inputs]
    out.mkdir(parents=True)
    receipt={'target':'ps5-compile-link-only' if native else 'host-red' if red else 'host-fixture',
        'target_executed':False,'native_provider_available':False,'directory_enumeration':False,
        'installed_observed':False,'runtime_supported':False,'build_fingerprint_available':False,
        'installation_supported':False,'inputs':identities,'commands':[],
        'sdk_commit':'d9c9519116944a7f1c22d262012c53b80b3520b7','dependencies':deps}
    env=dict(os.environ); cc='/usr/bin/clang'; cxx='/usr/bin/clang++'
    common=['-O2','-Wall','-Wextra','-Werror','-I'+str(here),'-isystem',str(vendor),'-isystem',str(vendor/'picosha2')]
    if native:
        cc=str(sdk/'bin/prospero-clang'); cxx=str(sdk/'bin/prospero-clang++')
        env['LLVM_CONFIG']=str(llvm/'llvm-config'); common += ['-resource-dir',str(resource)]
        receipt.update(verified_tool_sha256=TOOLS,llvm_config=env['LLVM_CONFIG'],resource_dir=str(resource),
            llvm_selected_manifest={'path':str(selected_path),'sha256':LLVM_SELECTED_SHA,
                'archive_sha256':selected['archive_sha256'],'package':selected['package']})
    def run(command,name,allowed=(0,)):
        command=[str(x) for x in command]
        try:
            r=subprocess.run(command,env=env,capture_output=True,timeout=60)
        except subprocess.TimeoutExpired as e:
            (out/name).write_bytes((e.stdout or b'')+(e.stderr or b''))
            receipt['commands'].append({'argv':command,'timeout_seconds':60,'log':name})
            (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
            raise SystemExit('Focused command timed out: '+name)
        text=r.stdout+r.stderr; (out/name).write_bytes(text)
        receipt['commands'].append({'argv':command,'exit_code':r.returncode,'log':name})
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        if r.returncode not in allowed: raise SystemExit(text.decode(errors='replace')[-8000:] or name+' failed')
        return text.decode(errors='replace')
    run([cxx,'--version'],'compiler.log')
    dependency_paths=set()
    def compile(source,cpp,test=False):
        obj=out/(source.stem+'.o'); dep=obj.with_suffix('.d')
        run([cxx if cpp else cc,'-std=c++20' if cpp else '-std=c11',*common,
            *(['-DMB_TITLE_SOURCE_HOST_TESTING=1'] if test else []),'-MD','-MF',dep,'-c',source,'-o',obj],
            source.stem+'-compile.log')
        dependency_paths.update(Path(x).resolve() for x in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]))
        return obj
    source_obj=compile(impl,False,test=not native)
    metadata_obj=compile(here/'metadata.cpp',True)
    caller_obj=compile(tests/'c_caller.c',False)
    if native:
        binary=out/'source_caller.elf'
        run([cxx,*common,source_obj,metadata_obj,caller_obj,'-Wl,-z,now','-Wl,-Map,'+str(out/'link.map'),'-o',binary],'link.log')
        elf=run([llvm/'llvm-readobj','--file-headers','--needed-libs','--dynamic-table',binary],'elf.log')
        symbols=run([llvm/'llvm-nm','--dynamic','--undefined-only',binary],'imports.log')
        source_symbols=run([llvm/'llvm-nm','--defined-only',source_obj],'source-symbols.log')
        source_import_text=run([llvm/'llvm-nm','--undefined-only',source_obj],'source-imports.log')
        defined_text=run([llvm/'llvm-nm','--defined-only',binary],'elf-defined-symbols.log')
        if 'Format: elf64-x86-64' not in elf or 'BIND_NOW' not in elf or 'OS/ABI: FreeBSD' not in elf:
            raise SystemExit('Target ELF differs')
        if 'mb_title_source_capture_fixture_v1' in source_symbols: raise SystemExit('Native arbitrary-root seam exported')
        imports=sorted(line.split()[-1] for line in symbols.splitlines() if line.strip())
        source_imports=sorted(line.split()[-1] for line in source_import_text.splitlines() if line.strip())
        defined=set(line.split()[-1] for line in defined_text.splitlines() if line.strip())
        required={'open','openat','lstat','fstat','fstatat','read','close'}
        if not required.issubset(source_imports) or not required.issubset(set(imports)|defined):
            raise SystemExit('Actual ordinary source closure incomplete')
        statically_resolved=sorted(required & defined)
        link_map=(out/'link.map').read_text()
        for name in statically_resolved:
            if 'libc.a(syscalls.o):(.text.'+name+')' not in link_map:
                raise SystemExit('Source static resolution differs: '+name)
        if any(re.search(r'^(socket|connect|send|recv|write|chmod|chown|unlink|mkdir)$',s) for s in source_imports):
            raise SystemExit('Unexpected write/network/permission import')
        closure=[]
        with zipfile.ZipFile(sdk_archive) as z:
            for path in sorted(dependency_paths):
                if path.is_relative_to(sdk):
                    if path.read_bytes()!=z.read('ps5-payload-sdk/'+path.relative_to(sdk).as_posix()):
                        raise SystemExit('Used SDK header differs: '+str(path))
                    kind='sdk_archive_verified'
                elif path.is_relative_to(vendor): kind='pinned_vendor'
                elif path.is_relative_to(here): kind='owned_source'
                elif path.is_relative_to(resource):
                    member='lib/clang/21/'+path.relative_to(resource).as_posix()
                    if digest(path)!=selected['members'].get(member): raise SystemExit('Used resource header differs: '+member)
                    kind='selected_manifest_verified_resource'
                else: raise SystemExit('Foreign/host target header: '+str(path))
                closure.append({'path':str(path),'kind':kind,'sha256':digest(path)})
            for name in ('bin/prospero-clang','bin/prospero-clang++','bin/prospero-llvm-config','bin/prospero-lld',
                'target/lib/libc.a','target/lib/libc++.a','target/lib/libc++abi.a','target/lib/libunwind.a'):
                if (sdk/name).read_bytes()!=z.read('ps5-payload-sdk/'+name): raise SystemExit('Used SDK input differs: '+name)
        receipt.update(imports=imports,source_object_imports=source_imports,
            source_apis_statically_resolved_in_pinned_sdk_libc=statically_resolved,
            include_closure=closure,sdk_used_inputs_archive_matched=True,bind_now=True,
            fixture_symbol_absent=True,artifact={'path':str(binary),'bytes':binary.stat().st_size})
    else:
        fixture_base=out/'fixtures'; fixture_base.mkdir()
        test_obj=compile(tests/'source_test.cpp',True,True)
        for name,obj,args in (('source_test',test_obj,[fixture_base]),('source_caller',caller_obj,[])):
            binary=out/name
            run([cxx,*common,source_obj,metadata_obj,obj,'-o',binary],name+'-link.log')
            text=run([binary,*args],name+'-run.log',(1,) if red else (0,))
            if name=='source_test':
                if red and ('GROUP ' not in text or 'FAIL' not in text): raise SystemExit('RED lacks actual behavior')
                if not red:
                    first=(fixture_base/'case-1/Any_Custom.Title-42/param.json')
                    matches=re.findall(r'^SOURCE_METADATA_SHA256 ([0-9a-f]{64})$',text,re.M)
                    if matches!=[digest(first)]: raise SystemExit('Captured metadata digest differs')
                    receipt['fixture_metadata_sha256']=matches[0]
            print(text.strip())
    receipt['inputs_unchanged']=all(digest(Path(i['path']))==i['sha256'] for i in identities)
    if not receipt['inputs_unchanged']: raise SystemExit('Inputs changed during focused checks')
    receipt['result']='EXPECTED_RED' if red else 'PASS'
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'result':receipt['result'],'target':receipt['target'],'target_executed':False,
        'native_provider_available':False,'output':str(out)}))

if __name__=='__main__': main()
