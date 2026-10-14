"""Owned status-host build only. No device dispatch or server execution."""
import argparse
import concurrent.futures
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import threading

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
PRODUCT=ROOT/'outputs/ps5-mod-bridge/native-host'
PROJECT=PRODUCT.parent
PROFILE=HERE.parent/'native-https-profile-experiment-2'
SEED=HERE.parent/'native-https-real-source-experiment-1'
TARGET=HERE.parent/'native-https-console-fixture-1/target-inputs.json'
DESCRIPTOR=PROFILE/'native-profile-descriptor.json'


def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dump(p,x): Path(p).write_text(json.dumps(x,indent=2,sort_keys=True)+'\n')


def recipe():
    d=json.loads(DESCRIPTOR.read_text())
    for p,h in d['config_inputs'].items(): assert digest(p)==h,p
    assert digest(d['source_archive_path'])==d['source_archive_sha256']
    for v in d['archives'].values(): assert digest(v['path'])==v['sha256'] and v['config_id']==d['config_id']
    return d,Path(d['source_path'])


def command_runner(out,env):
    commands=[];lock=threading.Lock()
    def run(argv,label):
        cp=subprocess.run([str(v) for v in argv],cwd=ROOT,capture_output=True,text=True,env=env,timeout=60)
        with lock:
            commands.append({'label':label,'argv':[str(v) for v in argv], 'cwd':str(ROOT),
                'returncode':cp.returncode,'stdout':cp.stdout,'stderr':cp.stderr,
                'LLVM_CONFIG':env.get('LLVM_CONFIG')})
            dump(out/'commands.json',commands)
        if cp.returncode: raise RuntimeError(label+': '+cp.stderr[:4000])
        return cp.stdout
    return run


def host_libraries(d,src):
    out=HERE/'host-libraries'
    if (out/'descriptor.json').exists():
        old=json.loads((out/'descriptor.json').read_text())
        assert old['config_id']==d['config_id']
        for p,h in old['archives'].items(): assert digest(p)==h
        return
    out.mkdir(exist_ok=True)
    run=command_runner(out,{**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
    text=(src/'library/CMakeLists.txt').read_text()
    families={k:re.search(r'set\(src_'+k+r'\s+(.*?)\)',text,re.S).group(1).split() for k in ('crypto','x509','tls')}
    flags=['-std=c11','-Os','-fno-common','-ffunction-sections','-fdata-sections',
        '-I'+str(PROFILE),'-I'+str(src/'include'),'-I'+str(src/'library'),
        '-DMBEDTLS_CONFIG_FILE="tls_config.h"','-include',str(src/'library/common.h'),
        '-include',str(PROFILE/'tls_effective.h')]
    names=[n for v in families.values() for n in v]
    def compile_one(n):
        obj=out/(Path(n).stem+'.o')
        run(['/usr/bin/clang',*flags,'-c',src/'library'/n,'-o',obj,'-MD','-MF',str(obj)+'.d'],n)
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool: list(pool.map(compile_one,names))
    archives={}
    for family,names in families.items():
        archive=out/('libmbed'+('tls' if family=='tls' else family)+'.a')
        run(['/usr/bin/ar','rcs',archive,*[out/(Path(n).stem+'.o') for n in names]],'archive-'+family)
        archives[str(archive)]=digest(archive)
    closure={}
    for p in out.glob('*.o.d'):
        for name in shlex.split(p.read_text().replace('\\\n',' ').split(':',1)[1]):
            path=Path(name)
            if path.is_relative_to(src) or path.is_relative_to(PROFILE): closure[str(path)]=digest(path)
    dump(out/'descriptor.json',{'config_id':d['config_id'],'archives':archives,
         'compiled_units':108,'source_header_identities':closure,'native_libraries_reused':False})
    print(json.dumps({'matching_host_libraries_built':True,'config_id':d['config_id'],'units':108}),flush=True)


def elf_layout(path):
    data=path.read_bytes();e=struct.unpack_from('<16sHHIQQQIHHHHHH',data)
    assert data[:6]==b'\x7fELF\x02\x01' and e[2]==62
    loads=[]
    for i in range(e[10]):
        p=struct.unpack_from('<IIQQQQQQ',data,e[5]+i*e[9])
        if p[0]==1:
            assert p[2]+p[5]<=len(data) and p[5]<=p[6] and p[7]==16384
            loads.append({'offset':p[2],'vaddr':p[3],'filesz':p[5],'memsz':p[6],'flags':p[1],
                          'alignment':p[7],'file_end':p[2]+p[5]})
    end=e[6]+e[11]*e[12]
    for i in range(e[12]):
        s=struct.unpack_from('<IIQQQQIIQQ',data,e[6]+i*e[11])
        if s[1]!=8: end=max(end,s[4]+s[5])
    assert end<=len(data) and all(p['file_end']<=end for p in loads)
    return {'size':len(data),'sha256':digest(path),'machine':'x86_64','type':e[1],'entry':e[4],
            'loads':loads,'loader_prefix':end,'max_load_file_end':max(p['file_end'] for p in loads)}


def build(profile,inputs,d,src):
    native=profile=='native'
    target=json.loads(inputs.read_text());root=Path(target['root_der'])
    assert digest(root)==target['root_der_sha256'] and root.stat().st_size==target['root_der_bytes']<=4096
    for key in ('server_address','console_bind_address'):
        ip=ipaddress.IPv4Address(target[key]);assert ip.is_private and not(ip.is_loopback or ip.is_multicast or ip.is_unspecified or ip.is_link_local)
    assert target['console_host_port']==8085 and 1<=target['server_port']<=65535
    out=HERE/('native' if native else 'host');out.mkdir(exist_ok=True)
    cfg={'profile':'development_lan' if native else 'desktop_https_fixture',
         'address':target['console_bind_address'] if native else '127.0.0.1','port':8085,
         'lifetime':600,'native':native}
    fixture={'server_address':target['server_address'] if native else '127.0.0.1',
        'server_port':target['server_port'],'expected_hostname':target['hostname'],'root_der_sha256':target['root_der_sha256']}
    header=out/'runtime_config.h'
    header.write_text('#define HOST_NATIVE 1\n'+
        f'#define HOST_ENTRY_NATIVE {int(native)}\n#define HOST_ADDRESS "{cfg["address"]}"\n'+
        f'#define HOST_PROFILE "{cfg["profile"]}"\n#define HOST_LIFETIME 600\n'+
        f'#define TLS_SERVER_ADDRESS "{fixture["server_address"]}"\n#define TLS_SERVER_PORT {fixture["server_port"]}\n'+
        f'#define TLS_EXPECTED_HOSTNAME "{fixture["expected_hostname"]}"\n#define TLS_CONFIG_ID "{d["config_id"]}"\n'+
        f'#define TLS_SOURCE_ORIGIN "{"reviewed_kern_arnd" if native else "owned_boundary_fixture"}"\n')
    trust=out/'fixture_root.h'
    b=root.read_bytes();rows=[','.join(f'0x{x:02x}' for x in b[i:i+16]) for i in range(0,len(b),16)]
    trust.write_text('#ifndef OWNED_FIXTURE_ROOT_H\n#define OWNED_FIXTURE_ROOT_H\nstatic const unsigned char fixture_root_der[]={\n'+',\n'.join(rows)+'\n};\n#endif\n')
    prereq=json.loads(Path(d['approved_prerequisite_descriptor']).read_text())
    pins=prereq['identity']['tools']
    sdk=Path(d['sdk_path']);llvm=Path(d['LLVM_CONFIG']).parent
    tools={}
    if native:
        tools.update(d['tool_identities'])
        for n in ('bin/prospero-clang++','bin/clang++'):
            p=sdk/n;assert digest(p)==pins['sdk_members'][n];tools[str(p)]=digest(p)
        for p,h in tools.items(): assert digest(p)==h,p
        cc=sdk/'bin/prospero-clang';cpp=sdk/'bin/prospero-clang++'
        libraries=[Path(d['archives'][n]['path']) for n in ('tls','x509','crypto')]
    else:
        cc=Path('/usr/bin/clang');cpp=Path('/usr/bin/clang++')
        tools={str(p):digest(p) for p in (cc,cpp,Path('/usr/bin/ar'))}
        hd=json.loads((HERE/'host-libraries/descriptor.json').read_text())
        assert hd['config_id']==d['config_id']
        libraries=[HERE/'host-libraries'/('libmbed'+n+'.a') for n in ('tls','x509','crypto')]
        for p in libraries: assert digest(p)==hd['archives'][str(p)]
    library_inputs={str(p):digest(p) for p in libraries}
    if native:
        for name in ('libc.a','libc++.a','libc++abi.a','libunwind.a','libkernel_web.so','libSceLibcInternal.so','libSceNet.so','crt1.o'):
            p=sdk/'target/lib'/name;assert digest(p)==pins['sdk_members']['target/lib/'+name];library_inputs[str(p)]=digest(p)
    env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1','LLVM_CONFIG':d['LLVM_CONFIG']}
    run=command_runner(out,env)
    common=['-Wall','-Wextra','-Werror','-Os','-ffunction-sections','-fdata-sections',
        '-ffile-prefix-map='+str(ROOT)+'=.','-I'+str(HERE),'-I'+str(out),
        '-I'+str(PRODUCT),'-include',str(header)]
    cflags=['-std=c11','-fno-common',*common,'-I'+str(SEED),'-I'+str(PROFILE),
        '-I'+str(src/'include'),'-I'+str(src/'library'),'-DMBEDTLS_CONFIG_FILE="tls_config.h"',
        *(d['native_compile_options'] if native else ['-DTLS_ATTEMPT_TESTING=1']),
        '-include',str(PROFILE/'tls_effective.h')]
    cppflags=['-std=c++20',*common,'-I'+str(PROJECT/'include'),'-isystem',str(PROJECT/'third_party'),
              '-isystem',str(PROJECT/'third_party/picosha2')]
    sources=[HERE/'host.c',HERE/'http.c',HERE/'tls_attempt.c',SEED/'tls_seed_source.c',PRODUCT/'entropy.c']
    if not native: sources.append(HERE/'fixture_boundary.c')
    csources=sources[:]
    sources += [HERE/'core_bridge.cpp',PROJECT/'src/dds.cpp',PROJECT/'src/hash.cpp']
    objects=[]
    def compile_source(source,build_id):
        obj=out/(source.stem+'.o');is_c=source in csources
        extra=['-Dsysctl=fixture_sysctl','-I'+str(SEED/'include')] if not native and source==PRODUCT/'entropy.c' else []
        run([cc if is_c else cpp,*(cflags if is_c else cppflags),'-DBUILD_ID="'+build_id+'"',*extra,
             '-c',source,'-o',obj,'-MD','-MF',str(obj)+'.d'],source.stem+'-'+('identity' if build_id=='pending' else 'final'))
        return obj
    for source in sources: objects.append(compile_source(source,'pending'))
    closure={}
    for dep in out.glob('*.o.d'):
        for name in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
            p=Path(name);h=digest(p)
            if native and p.is_relative_to(sdk):
                assert pins['sdk_members'][p.relative_to(sdk).as_posix()]==h,p
                origin='sdk'
            elif p.is_relative_to(src): origin='pinned_mbed'
            elif p.is_relative_to(PROFILE): origin='matching_tls_config'
            elif p.is_relative_to(ROOT): origin='owned_or_reviewed_source'
            elif native:
                resource=Path(d['native_compile_options'][2])
                for base in (resource,llvm.parent/'lib/clang/21'):
                    if p.is_relative_to(base):
                        assert pins['llvm_selected_members']['lib/clang/21/'+p.relative_to(base).as_posix()]==h,p
                        origin='verified_llvm_resource';break
                else: raise AssertionError('native host header fallback: '+str(p))
            else: origin='host_system'
            closure[str(p)]={'sha256':h,'origin':origin}
    identity={'schema_version':1,'version':'0.1.0','profile':cfg,'tls_fixture':fixture,
        'tls_config_id':d['config_id'],'tls_descriptor_sha256':digest(DESCRIPTOR),
        'source_header_inputs':closure,'source_sha256':{str(p):digest(p) for p in sources},
        'tool_sha256':tools,'library_sha256':library_inputs,'trust_root_sha256':digest(root),
        'recipe_sha256':digest(__file__)}
    build_id=hashlib.sha256(json.dumps(identity,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    # BUILD_ID is a command-line stamp, not a self-referential header input.
    # Only host.c embeds it; all actual source/header/config bytes stay fixed.
    compile_source(HERE/'host.c',build_id)
    artifact=out/('modbridge-native-https-host-development-lan.elf' if native else 'modbridge-https-host-fixture')
    extra=['-nodefaultlibs','-Wl,--gc-sections','-Wl,-z,now','-Wl,-Map,'+str(out/'link.map'),
           '-lc++','-lc++abi','-lunwind','-lc','-lkernel_web','-lSceLibcInternal','-lSceNet'] if native else []
    run([cpp,*objects,*libraries,*extra,'-o',artifact],'link')
    manifest={**identity,'build_id':build_id,'artifact':artifact.name,'artifact_sha256':digest(artifact),
        'artifact_size':artifact.stat().st_size,'tls_descriptor':d,'console_executed':False,
        'owned_launch_contract':'socket_spawn_only','library_units_rebuilt':0,
        'host_boundary_injection':not native}
    if native:
        manifest['elf']=elf_layout(artifact)
        symbols=run([llvm/'llvm-nm',artifact],'symbols')
        for n in ('fixture_sysctl','tls_attempt_test_cleanup','mb_random_checkpoint','mb_drbg_source'):
            assert n not in symbols,n
        for n in ('mb_core_selftest','tls_attempt','mb_entropy_read32','exp_clock_time','mbedtls_platform_gmtime_r'):
            assert n in symbols,n
        imports=run([llvm/'llvm-nm','--dynamic','--undefined-only',artifact],'imports')
        assert 'sysctl' in imports and 'time' in imports and 'clock_gettime' in imports
        import_names={line.split()[-1] for line in imports.splitlines() if line.split()}
        # Compiler arithmetic helper availability is not guaranteed by the SDK
        # dynamic resolver. The supported portable Mbed division path must close
        # the concrete __udivti3 dependency and leave no related helper import.
        arithmetic_helpers=sorted(n for n in import_names if re.fullmatch(
            r'__(?:u?div|u?mod|u?mul|(?:add|sub|neg)v?|ashl|ashr|lshr|u?cmp|clz|ctz|ffs|parity|popcount|fix|float)[a-z0-9]*',n))
        assert not arithmetic_helpers,arithmetic_helpers
        manifest['arithmetic_compiler_helper_imports']=arithmetic_helpers
        readelf=run([llvm/'llvm-readelf','-h','-l','-d',artifact],'readelf')
        assert 'BIND_NOW' in readelf
    for p,h in library_inputs.items(): assert digest(p)==h,p
    for p,h in d['config_inputs'].items(): assert digest(p)==h,p
    for p,row in closure.items(): assert digest(p)==row['sha256'],p
    dump(out/'manifest.json',manifest)
    print(json.dumps({'artifact':str(artifact),'build_id':build_id,'artifact_sha256':manifest['artifact_sha256'],
                      'artifact_size':manifest['artifact_size'],'config_id':d['config_id'],'native_executed':False}),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--host-libraries',action='store_true')
    p.add_argument('--profile',choices=['host','native']);p.add_argument('--inputs',type=Path,default=TARGET)
    a=p.parse_args();d,src=recipe()
    if a.host_libraries: host_libraries(d,src)
    if a.profile: build(a.profile,a.inputs,d,src)
