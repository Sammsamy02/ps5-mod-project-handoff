#!/usr/bin/env python3
"""Focused host behavior and pinned PS5 compile/link checks for the catalog."""
from pathlib import Path
import argparse
import hashlib
import json
import os
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
        for data in iter(lambda:stream.read(1024*1024),b''):h.update(data)
    return h.hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode',choices=('host','native'))
    parser.add_argument('--output-dir',type=Path)
    args=parser.parse_args()
    here=Path(__file__).resolve().parent
    project=here.parent.parent
    workspace=project.parent.parent
    vendor=project/'third_party'
    out=(args.output_dir or workspace/'.superpowers/native-dependency-spike/native-title-catalog-experiment-1'/
         (args.mode+'-'+str(time.time_ns()))).resolve()
    evidence_root=(workspace/'.superpowers/native-dependency-spike/native-title-catalog-experiment-1').resolve()
    if not out.is_relative_to(evidence_root):parser.error('output must stay in the fresh owned catalog evidence root')
    if out.exists():parser.error('use a fresh output directory; prior evidence is retained')
    out.mkdir(parents=True)
    native=args.mode=='native'
    sdk=here.parent/'build/sdk-v0.43/ps5-payload-sdk'
    sdk_archive=project/'native-probe/build/deps/sdk.zip'
    llvm=project/'native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    resource=workspace/'.superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21'
    selected_path=here.parent/'llvm_selected_manifest.json'
    test=here/'catalog_tests/catalog_test.c'
    native_caller=here/'catalog_tests/native_caller.c'
    sources=[here/'catalog.c',here/'source.c',here/'metadata.cpp',native_caller if native else test]
    vendor_manifest=json.loads((vendor/'manifest.json').read_text())
    deps=[e for e in vendor_manifest['files'] if e['path'].startswith(('nlohmann/','picosha2/'))]
    for entry in deps:
        if digest(vendor/entry['path'])!=entry['sha256']:raise SystemExit('Pinned vendor dependency differs: '+entry['path'])
    env=dict(os.environ)
    cc='/usr/bin/clang';cxx='/usr/bin/clang++'
    if native:
        if digest(sdk_archive)!=SDK_SHA:raise SystemExit('SDK archive differs from pinned SHA256')
        if digest(selected_path)!=LLVM_SELECTED_SHA:raise SystemExit('Approved LLVM resource manifest differs')
        selected=json.loads(selected_path.read_text())
        for name,sha in TOOLS.items():
            if digest(llvm/name)!=sha:raise SystemExit('Pinned LLVM tool differs: '+name)
        cc=str(sdk/'bin/prospero-clang');cxx=str(sdk/'bin/prospero-clang++')
        env['LLVM_CONFIG']=str(llvm/'llvm-config')
    identities=[{'path':str(path),'sha256':digest(path)} for path in
        [here/'catalog.h',here/'catalog.c',here/'catalog_test.h',here/'source.h',here/'source.c',
         here/'source_test.h',here/'metadata.h',here/'metadata.cpp',here/'catalog_verify.py',
         *( [native_caller] if native else [test] ),vendor/'manifest.json',
         *[vendor/e['path'] for e in deps]]]
    if native:identities.append({'path':str(selected_path),'sha256':LLVM_SELECTED_SHA})
    receipt={'target':'ps5-compile-link-only' if native else 'host-fixture','target_executed':False,
        'platform_provider_enabled':False,'installed_observed':False,'build_fingerprint_available':False,
        'source_scope':'metadata cache candidate observation only','inputs':identities,
        'commands':[],'dependencies':deps,'result':'IN_PROGRESS'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    common=['-O2','-Wall','-Wextra','-Werror','-I'+str(here),'-isystem',str(vendor),'-isystem',str(vendor/'picosha2')]
    if native:common += ['-resource-dir',str(resource)]
    dependency_paths=set()
    def run(argv,name,allowed=(0,)):
        argv=[str(x) for x in argv]
        try:r=subprocess.run(argv,env=env,capture_output=True,timeout=90)
        except subprocess.TimeoutExpired as exc:
            (out/name).write_bytes((exc.stdout or b'')+(exc.stderr or b''))
            receipt['commands'].append({'argv':argv,'timeout_seconds':90,'log':name})
            (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
            raise SystemExit('Focused command timed out: '+name)
        raw=r.stdout+r.stderr;(out/name).write_bytes(raw)
        receipt['commands'].append({'argv':argv,'exit_code':r.returncode,'log':name})
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        if r.returncode not in allowed:raise SystemExit(raw.decode(errors='replace')[-7000:] or name+' failed')
        return raw.decode(errors='replace')
    def compile_one(path,is_cpp=False,host_test=False):
        obj=out/(path.stem+'.o'); dep=obj.with_suffix('.d')
        run([cxx if is_cpp else cc,'-std=c++20' if is_cpp else '-std=c11',*common,
             *(['-DMB_TITLE_CATALOG_HOST_TESTING=1','-DMB_TITLE_SOURCE_HOST_TESTING=1'] if host_test else []),
             '-MD','-MF',dep,'-c',path,'-o',obj],path.stem+'-compile.log')
        text=dep.read_text().replace('\\\n',' ').split(':',1)[1]
        dependency_paths.update(Path(value).resolve() for value in shlex.split(text))
        return obj
    run([cxx,'--version'],'compiler.log')
    catalog_obj=compile_one(here/'catalog.c',host_test=not native)
    source_obj=compile_one(here/'source.c',host_test=not native)
    metadata_obj=compile_one(here/'metadata.cpp',is_cpp=True)
    caller_obj=compile_one(native_caller if native else test,host_test=not native)
    if native:
        binary=out/'catalog_caller.elf'
        run([cxx,*common,catalog_obj,source_obj,metadata_obj,caller_obj,'-Wl,-z,now',
             '-Wl,-Map,'+str(out/'link.map'),'-o',binary],'link.log')
        readobj=run([llvm/'llvm-readobj','--file-headers','--needed-libs','--dynamic-table',binary],'elf.log')
        imports=run([llvm/'llvm-nm','--dynamic','--undefined-only',binary],'imports.log')
        catalog_imports=run([llvm/'llvm-nm','--undefined-only',catalog_obj],'catalog-imports.log')
        catalog_defined=run([llvm/'llvm-nm','--defined-only',catalog_obj],'catalog-defined.log')
        defined_binary=run([llvm/'llvm-nm','--defined-only',binary],'elf-defined.log')
        if 'Format: elf64-x86-64' not in readobj or 'BIND_NOW' not in readobj or 'OS/ABI: FreeBSD' not in readobj:
            raise SystemExit('Native target ELF profile differs')
        if 'mb_title_catalog_scan_fixture_v1' in catalog_defined or 'mb_title_source_capture_fixture_v1' in catalog_defined:
            raise SystemExit('Host arbitrary-root fixture symbol entered the target')
        catalog_undefined=sorted(line.split()[-1] for line in catalog_imports.splitlines() if line.strip())
        binary_undefined=sorted(line.split()[-1] for line in imports.splitlines() if line.strip())
        defined=set(line.split()[-1] for line in defined_binary.splitlines() if line.strip())
        required={'lstat','open','openat','fstat','fstatat','close','fdopendir','readdir','closedir'}
        if not required.issubset(set(catalog_undefined)|defined):
            raise SystemExit('Native ordinary filesystem/enumeration symbol closure incomplete')
        static=sorted(required & defined)
        link_map=(out/'link.map').read_text()
        for symbol in static:
            if 'libc.a(syscalls.o):(.text.'+symbol+')' not in link_map:
                raise SystemExit('Expected SDK libc static resolution missing: '+symbol)
        closure=[]
        with zipfile.ZipFile(sdk_archive) as archive:
            for path in sorted(dependency_paths):
                if path.is_relative_to(sdk):
                    if path.read_bytes()!=archive.read('ps5-payload-sdk/'+path.relative_to(sdk).as_posix()):
                        raise SystemExit('Used SDK header differs: '+str(path))
                    kind='sdk_archive_verified'
                elif path.is_relative_to(vendor):kind='pinned_vendor'
                elif path.is_relative_to(here):kind='owned_source'
                elif path.is_relative_to(resource):
                    member='lib/clang/21/'+path.relative_to(resource).as_posix()
                    if digest(path)!=selected['members'].get(member):raise SystemExit('Resource header differs: '+member)
                    kind='selected_manifest_verified_resource'
                else:raise SystemExit('Foreign/host include in native dependency closure: '+str(path))
                closure.append({'path':str(path),'kind':kind,'sha256':digest(path)})
            for name in ('bin/prospero-clang','bin/prospero-clang++','bin/prospero-llvm-config','bin/prospero-lld',
                         'target/lib/libc.a','target/lib/libc++.a','target/lib/libc++abi.a','target/lib/libunwind.a'):
                if (sdk/name).read_bytes()!=archive.read('ps5-payload-sdk/'+name):raise SystemExit('SDK link input differs: '+name)
        receipt.update(imports=binary_undefined,catalog_object_imports=catalog_undefined,
            catalog_static_sdk_libc_resolution=static,include_closure=closure,sdk_archive_inputs_match=True,
            bind_now=True,host_fixture_symbols_absent=True,artifact={'path':str(binary),'bytes':binary.stat().st_size})
    else:
        binary=out/'catalog_test'
        run([cxx,*common,catalog_obj,source_obj,metadata_obj,caller_obj,'-o',binary],'link.log')
        test_output=run([binary],'test.log')
        receipt['host_output']=test_output.strip()
    receipt['inputs_unchanged']=all(digest(Path(item['path']))==item['sha256'] for item in identities)
    if not receipt['inputs_unchanged']:raise SystemExit('An owned/pinned source input changed during the focused build')
    receipt['result']='PASS'
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'result':'PASS','target':receipt['target'],'target_executed':False,'output':str(out)}))

if __name__=='__main__':main()
