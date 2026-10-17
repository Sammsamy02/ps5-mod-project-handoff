#!/usr/bin/env python3
"""Rebuild just the three production stdio consumers and compare actual host behavior."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parent
WORK=ROOT.parents[2]
BASE=ROOT.parent/'ue4ss-unreal-native-build-1'
LUA=BASE/'source-dependencies/ue4ss/deps/first/LuaRaw'
FMT=BASE/'source-dependencies/fmt'
TOOLS=WORK/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
OUT=ROOT/(sys.argv[1] if len(sys.argv)>1 else 'verification-1')
OUT.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
commands=[]
environment=dict(os.environ,LLVM_CONFIG=str(TOOLS/'llvm-config'))
for k in ['CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH']:environment.pop(k,None)
def run(name,args,required=True):
    if name.endswith('-test-run'):
        stdout=OUT/(name+'.stdout');stderr=OUT/(name+'.stderr')
        with stdout.open('w') as so,stderr.open('w') as se:
            p=subprocess.Popen([str(a) for a in args],env=environment,text=True,stdout=so,stderr=se)
            deadline=time.monotonic()+10
            while p.poll() is None and time.monotonic()<deadline:time.sleep(.05)
            status=p.poll()
            if status is None:
                p.kill()
                commands.append(dict(name=name,argv=[str(a) for a in args],exit=None,
                                     timed_out=True,pid=p.pid,kill_sent=True))
                (OUT/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
                return subprocess.CompletedProcess(args,124,stdout.read_text(),stderr.read_text())
        v=subprocess.CompletedProcess(args,status,stdout.read_text(),stderr.read_text())
        commands.append(dict(name=name,argv=[str(a) for a in args],exit=v.returncode))
        (OUT/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        if required and v.returncode:raise RuntimeError(f'{name}: {v.stderr}')
        return v
    v=subprocess.run([str(a) for a in args],env=environment,text=True,capture_output=True,timeout=60)
    (OUT/(name+'.stdout')).write_text(v.stdout)
    (OUT/(name+'.stderr')).write_text(v.stderr)
    commands.append(dict(name=name,argv=[str(a) for a in args],exit=v.returncode))
    (OUT/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    if required and v.returncode:raise RuntimeError(f'{name}: {v.stderr}')
    return v
def symbols(path,undefined=False):
    args=[TOOLS/'llvm-nm','--format=posix','--extern-only',
          '--undefined-only' if undefined else '--defined-only',path]
    v=run(Path(path).stem+('-undefined' if undefined else '-defined'),args)
    return {parts[0]:parts[1] for l in v.stdout.splitlines() if len(parts:=l.split())>=2}

inputs=json.loads((ROOT/'replacement-inputs.json').read_text())
replacements=[]
for row in inputs['units']:
    unit=row['unit'];name=unit['name'];source=Path(row['patched_source'])
    assert sha(unit['source'])==unit['source_sha256']
    assert sha(unit['object'])==unit['object_sha256']
    assert sha(source)==row['patched_source_sha256']
    old=json.loads(Path(unit['command']).read_text())
    obj=OUT/(name+'.o')
    args=[str(source) if x==unit['source'] else str(obj) if x==unit['object'] else x for x in old['argv']]
    args[1:1]=row.get('additional_native_flags',[])
    args[1:1]=['-MD','-MF',str(OUT/(name+'.d'))]
    run('native-'+name,args)
    before=symbols(unit['object'],True);after=symbols(obj,True)
    bd=symbols(unit['object']);ad=symbols(obj)
    strong={n for n,k in bd.items() if k in ['T','D','B','R','S']}
    assert strong<=set(ad),(name,sorted(strong-set(ad)))
    assert not {'__isthreaded','__srget','getc','putc_unlocked'}&set(after),(name,after)
    rec=dict(name=name,original_object=unit['object'],original_object_sha256=unit['object_sha256'],
             object=str(obj),object_sha256=sha(obj),source=str(source),source_sha256=sha(source),
             original_source=unit['source'],original_source_sha256=unit['source_sha256'],
             removed_undefined=sorted(set(before)-set(after)),added_undefined=sorted(set(after)-set(before)),
             strong_definitions_preserved=True,removed_weak_implementation_symbols=sorted(set(bd)-set(ad)),
             command=str(OUT/'commands.json'),exit=0)
    replacements.append(rec)

sdk=WORK/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
owned=OUT/'owned-fgetc.o'
run('native-owned-fgetc',[TOOLS/'clang','--target=x86_64-sie-ps5','-std=c11','-O2',
    '-ffreestanding','-fno-builtin','-fPIC','-fvisibility=hidden','-fno-stack-protector',
    '-nostdinc','-isystem',sdk/'target/include','-MD','-MF',OUT/'owned-fgetc.d',
    '-c',ROOT/'owned_fgetc.c','-o',owned])
assert set(symbols(owned,True))=={'fread'}
assert set(symbols(owned))=={'fgetc'}
owned_s=run('owned-fgetc-symbols',[TOOLS/'llvm-readelf','-s',owned]).stdout
assert 'GLOBAL HIDDEN' in owned_s
group=OUT/'replacement-stdio-consumers.o'
run('replacement-group-link',[TOOLS/'ld.lld','-r',*[r['object'] for r in replacements],owned,'-o',group])
group_u=symbols(group,True)
assert not {'__isthreaded','__srget','getc','putc_unlocked'}&set(group_u)
assert 'fgetc' not in group_u
run('replacement-group-header',[TOOLS/'llvm-readelf','-h',group])
fmt_symbols=run('fmt-file-symbols',[TOOLS/'llvm-nm','--defined-only',OUT/'fmt.o']).stdout
assert 'apple_file' not in fmt_symbols and 'glibc_file' not in fmt_symbols
gate=json.loads((ROOT.parent/'ue4ss-prx-import-contract-1/local-deps-audit-1/typed-gated-result.json').read_text())
fgetc=dict(name='fgetc',status='local_hidden_implementation',
           object=str(owned),object_sha256=sha(owned),required_imports=['fread'],
           qualification='Owned implementation using ISO C one-byte fread semantics; no fgetc import identity is claimed.')
(OUT/'fgetc-existing-identity-evidence.json').write_text(json.dumps(fgetc,indent=2)+'\n')

fixtures=OUT/'fixtures';fixtures.mkdir()
for n,d in {'plain.lua':b'return 42','bom.lua':b'\xef\xbb\xbfreturn 42',
 'shebang.lua':b'#!/usr/bin/lua\nreturn 42','bom-shebang.lua':b'\xef\xbb\xbf#!/bin/lua\nreturn 42',
 'empty':b'','lines':b'alpha\nbeta\n\nlast','numbers':b'12.5 -7 0xffX',
 'bytes':b'\xff\x00\x80','incomplete-bom.lua':b'\xef\xbbreturn 42',
 'long-line':b'z'*17000+b'\ntail'}.items():(fixtures/n).write_bytes(d)

# Full actual Lua host implementation supplies the dependencies needed to
# exercise the two rebuilt consumers through public Lua APIs. Native compile
# remains restricted to the three changed production units.
lua_native=json.loads((ROOT.parent/'ue4ss-native-luaraw-closure-1/native-1/result.json').read_text())
lua_units=lua_native['units']
host=[]
for arch in ['arm64','x86_64']:
    hf=OUT/arch;hf.mkdir()
    host_owned=hf/'owned-fgetc.o'
    run(arch+'-owned-fgetc-build',['/usr/bin/clang','-arch',arch,'-std=c11','-O1',
        '-fno-builtin','-fvisibility=hidden','-fsanitize=undefined','-fno-sanitize-recover=all',
        '-c',ROOT/'owned_fgetc.c','-o',host_owned])
    for tested in [False,True]:
        mode='owned' if tested else 'system-reference'
        exe=hf/('fgetc-'+mode+'-test')
        run(arch+'-fgetc-'+mode+'-test-build',['/usr/bin/clang','-arch',arch,'-std=c11','-O1',
            '-fno-builtin','-fsanitize=undefined','-fno-sanitize-recover=all',ROOT/'fgetc_stream_test.c',
            *([host_owned] if tested else []),'-o',exe])
        v=run(arch+'-fgetc-'+mode+'-test-run',[exe,fixtures/('fgetc-'+arch+'-'+mode+'-write-only')],False)
        host.append(dict(architecture=arch,consumer='fgetc',mode=mode,passed=v.returncode==0,
                         exit=v.returncode,stdout=v.stdout,stderr=v.stderr))
    common=[]
    for u in lua_units:
        if Path(u['source']).name in ['lauxlib.c','liolib.c']:continue
        obj=hf/(u['name']+'.o')
        run(arch+'-lua-common-'+u['name'],['/usr/bin/clang','-arch',arch,'-std=c11','-O1',
            '-D_XOPEN_SOURCE=700','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all',
            '-I',LUA/'include','-c',u['source'],'-o',obj])
        common.append(obj)
    for adapted in [False,True]:
        mode='adapted' if adapted else 'original'
        luapart=[]
        for n in ['lauxlib.c','liolib.c']:
            obj=hf/(mode+'-'+n+'.o')
            run(arch+'-lua-'+mode+'-'+n,['/usr/bin/clang','-arch',arch,'-std=c11','-O1',
                '-D_XOPEN_SOURCE=700','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all',
                '-I',LUA/'include','-c',(ROOT/'patched'/n if adapted else LUA/'src'/n),'-o',obj])
            luapart.append(obj)
        exe=hf/('lua-'+mode+'-test')
        run(arch+'-lua-'+mode+'-test-build',['/usr/bin/clang','-arch',arch,'-std=c11','-O1',
            '-fsanitize=undefined','-fno-sanitize-recover=all','-pthread','-I',LUA/'include',
            ROOT/'lua_stdio_test.c',*common,*luapart,*([host_owned] if adapted else []),'-o',exe])
        v=run(arch+'-lua-'+mode+'-test-run',[exe,fixtures],False)
        host.append(dict(architecture=arch,consumer='Lua',mode=mode,passed=v.returncode==0,
                         exit=v.returncode,stdout=v.stdout,stderr=v.stderr))
        fmtobj=hf/('fmt-'+mode+'.o')
        fmtflags=['-DFMT_USE_FALLBACK_FILE=1'] if adapted else []
        run(arch+'-fmt-'+mode+'-build',['/usr/bin/clang++','-arch',arch,'-std=c++20','-O1',
            '-fsanitize=undefined','-fno-sanitize-recover=all','-I',FMT/'include',*fmtflags,
            '-c',FMT/'src/format.cc','-o',fmtobj])
        fmtexe=hf/('fmt-'+mode+'-test')
        run(arch+'-fmt-'+mode+'-test-build',['/usr/bin/clang++','-arch',arch,'-std=c++20','-O1',
            '-fsanitize=undefined','-fno-sanitize-recover=all','-pthread','-I',FMT/'include',
            ROOT/'fmt_stdio_test.cpp',fmtobj,'-o',fmtexe])
        v=run(arch+'-fmt-'+mode+'-test-run',[fmtexe,fixtures/'plain.lua'],False)
        host.append(dict(architecture=arch,consumer='fmt',mode=mode,passed=v.returncode==0,
                         exit=v.returncode,stdout=v.stdout,stderr=v.stderr))
assert all(h['passed'] for h in host if h['consumer']!='fmt'),host
deps=set()
for p in OUT.glob('*.d'):
    deps.update(Path(x) for x in p.read_text().replace('\\\n','').split(':',1)[1].split())
receipt=dict(replacement_units=replacements,native_changed_units=3,
    replacement_group=str(group),replacement_group_sha256=sha(group),
    replacement_group_undefined=sorted(group_u),
    target='x86_64-sie-ps5',four_targeted_imports_absent=True,
    fgetc_identity_evidence=str(OUT/'fgetc-existing-identity-evidence.json'),
    fgetc_identity_scope='Local hidden implementation; only existing fread remains required',
    owned_fgetc=dict(source=str(ROOT/'owned_fgetc.c'),source_sha256=sha(ROOT/'owned_fgetc.c'),
                     object=str(owned),object_sha256=sha(owned),undefined=['fread']),
    host_checks=host,compile_dependencies={str(p):sha(p) for p in sorted(deps)},
    original_native_inputs={str(Path(r['unit']['object'])):r['unit']['object_sha256'] for r in inputs['units']},
    sdk_stdio_header=str(WORK/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/stdio.h'),
    tools={str(p):sha(p) for p in [TOOLS/'clang',TOOLS/'ld.lld',TOOLS/'llvm-nm',TOOLS/'llvm-readelf','/usr/bin/clang','/usr/bin/clang++']},
    original_lua_pin='e3ba1016562d6c0868c410d0a71e88bfcdbf691b',fmt_version='11.2.0',
    local_scripts={str(p):sha(p) for p in [ROOT/'build_verify.py',ROOT/'lua_stdio_test.c',ROOT/'fmt_stdio_test.cpp',ROOT/'owned_fgetc.c',ROOT/'fgetc_stream_test.c']},
    native_execution=False,console_contact=False,shared_sdk_modified=False,stage27_modified=False,integrated=False)
(OUT/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
integration=dict(units=replacements,additional_objects=[dict(name='owned-fgetc',object=str(owned),object_sha256=sha(owned))],source_receipt=str(OUT/'receipt.json'),source_receipt_sha256=sha(OUT/'receipt.json'),
                 production_native_rebuilds=3,integrated=False)
(OUT/'integration-units.json').write_text(json.dumps(integration,indent=2)+'\n')
print(json.dumps(dict(native_changed_units=3,replacements=[{k:r[k] for k in ['name','object','object_sha256','removed_undefined','added_undefined']} for r in replacements],host_checks=host),indent=2))
