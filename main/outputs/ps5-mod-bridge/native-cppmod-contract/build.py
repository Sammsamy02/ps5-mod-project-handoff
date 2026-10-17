#!/usr/bin/env python3
"""Build a local genuine C++ plugin/host export contract; never load either module."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SPIKE = ROOT / '.superpowers/native-dependency-spike'
GATE = SPIKE / 'ue4ss-prx-import-contract-1'

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path): return json.loads(Path(path).read_text())
def save(path, value): Path(path).write_text(json.dumps(value, indent=2) + '\n')

def elf_symbols(path):
    """Read exact native symbol records, retaining index/type/definition evidence."""
    blob = Path(path).read_bytes()
    if blob[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', blob, 18)[0] != 62:
        raise ValueError('expected x86_64 ELF64 input')
    offset = struct.unpack_from('<Q', blob, 40)[0]
    stride, count = struct.unpack_from('<HH', blob, 58)
    if count == 0: count = struct.unpack_from('<Q', blob, offset+32)[0]
    sections = [struct.unpack_from('<IIQQQQIIQQ', blob, offset+i*stride) for i in range(count)]
    rows = []
    for table in sections:
        if table[1] != 2: continue
        strings = sections[table[6]]
        names = blob[strings[4]:strings[4]+strings[5]]
        for index in range(table[5]//table[9]):
            nameoff, info, other, section, value, size = struct.unpack_from('<IBBHQQ', blob, table[4]+index*table[9])
            end = names.find(b'\0', nameoff)
            name = names[nameoff:end].decode('ascii')
            if name and info >> 4 in (1, 2):
                rows.append(dict(name=name, type=info & 15, binding=info >> 4,
                                 section=section, index=index, value=value, size=size))
    return rows

def type_evidence(path, row):
    return dict(name=row['name'], symbol_type={1:'OBJECT',2:'FUNC'}[row['type']],
                symbol_table='.symtab', symbol_index=row['index'],
                evidence=dict(source=str(Path(path).resolve()), sha256=sha(path)))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compile-receipt', type=Path, default=SPIKE/'native-coherent-runtime-1/coherent-full-1/cppmod.command.json')
    parser.add_argument('--host-runtime', type=Path, default=SPIKE/'native-coherent-cpp-loader-1/build-1/coherent-cpp-loader-runtime.o')
    parser.add_argument('--output', type=Path, default=SPIKE/'native-cppmod-child-lifecycle-1/packages')
    args = parser.parse_args()
    out = args.output.resolve()/uuid.uuid4().hex
    out.mkdir(parents=True, exist_ok=False)
    dotnet = SPIKE/'sharpprospero-host-tool-1/dotnet/dotnet'
    typed_source = SPIKE/'ue4ss-native-prx-import-types-1/source'
    typed = out/'writer-tool'
    sdk = ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
    llvm = ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    env = dict(os.environ, LLVM_CONFIG=str(llvm/'llvm-config'), DOTNET_ROOT=str(dotnet.parent),
               DOTNET_CLI_HOME=str(out/'dotnet-home'), DOTNET_CLI_TELEMETRY_OPTOUT='1',
               DOTNET_NOLOGO='1', DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1',
               DOTNET_MULTILEVEL_LOOKUP='0', DOTNET_CLI_USE_MSBUILD_SERVER='0')
    records=[]
    result=dict(status='building', output=str(out), target_execution=False,
                console_contact=False, runnable_game_runtime=False, commands=records)
    def call(label, argv, expected=0):
        argv=list(map(str,argv))
        p=subprocess.run(argv,cwd=ROOT,env=env,capture_output=True,text=True)
        (out/(label+'.stdout')).write_text(p.stdout);(out/(label+'.stderr')).write_text(p.stderr)
        records.append(dict(label=label,argv=argv,exit=p.returncode))
        save(out/'commands.json',records)
        if p.returncode != expected: raise RuntimeError(label+' failed: '+p.stderr[-2000:]+p.stdout[-500:])
        return p.stdout
    spec=importlib.util.spec_from_file_location('compile_contract',GATE/'compile_contract.py')
    compiler=importlib.util.module_from_spec(spec);spec.loader.exec_module(compiler)
    try:
        # Reproduce the narrow writer successor without editing its retained base.
        writer_source=out/'writer-source'
        shutil.copytree(typed_source,writer_source,ignore=shutil.ignore_patterns('bin','obj','.git'))
        call('apply-child-writer',[ 'patch','-d',writer_source,'-p1','-i',HERE/'child-writer.patch'])
        writer_project=writer_source/'tools/SharpProspero.Link/SharpProspero.Link.csproj'
        call('restore-child-writer',[dotnet,'restore',writer_project,'--configfile',HERE/'offline-nuget.config',
             '--packages',SPIKE/'sharpprospero-host-tool-1/nuget-packages','-p:NuGetAudit=false'])
        call('build-child-writer',[dotnet,'build',writer_project,'-c','Release','--no-restore','--nologo',
             '-o',typed,'-m:1','-nr:false','-p:UseSharedCompilation=false'])
        call('compile-real-plugin',[sys.executable,HERE.parent/'native-cppmod-source/build.py',
             '--compile-receipt',args.compile_receipt,'--host-runtime',args.host_runtime,'--output',out/'source-compile'])
        audit_path=next((out/'source-compile').glob('*/audit.json'));audit=read(audit_path)
        plugin=Path(audit['plugin_object']);host=args.host_runtime.resolve()
        plugin_rows=elf_symbols(plugin);host_rows=elf_symbols(host)
        host_defs={x['name']:x for x in host_rows if x['section'] and x['type'] in (1,2)}
        requested=sorted({x['name'] for x in plugin_rows if not x['section']} & set(host_defs))
        if not {'_ZN2RC14CppUserModBaseC2Ev','_ZN2RC14CppUserModBaseD2Ev','_ZTIN2RC14CppUserModBaseE'} <= set(requested):
            raise ValueError('genuine base definitions absent from selected host')
        names=out/'host-exports.txt';names.write_text('\n'.join(requested)+'\n')
        plugin_names=out/'plugin-exports.txt';plugin_names.write_text('start_mod\nuninstall_mod\nmodbridge_cppmod_counters\nmodule_start\nmodule_stop\n')
        csargs=[f'-p:TypedTool={typed}',f'-p:BaseIntermediateOutputPath={out}/obj/']
        call('restore-host-tool',[dotnet,'restore',HERE/'ContractHost.csproj','--configfile',HERE/'offline-nuget.config',
             '--packages',SPIKE/'sharpprospero-host-tool-1/nuget-packages','-p:NuGetAudit=false',*csargs])
        call('build-host-tool',[dotnet,'build',HERE/'ContractHost.csproj','-c','Release','--no-restore',
             '--nologo','-o',out/'tool','-m:1','-nr:false','-p:UseSharedCompilation=false',*csargs])
        tool=[dotnet,out/'tool/ContractHost.dll']
        evidence=read(GATE/'successor-audit-1/sdk-type-declarations.json')
        for row in evidence['records']:
            name=Path(row['evidence']['source']).name
            if name not in {'libSceLibcInternal.so','libScePad.so','libSceUserService.so','libkernel_web.so'}:
                raise ValueError('unexpected SDK type-evidence source')
            source=sdk/'target/lib'/name
            if sha(source)!=row['evidence']['sha256']: raise ValueError('SDK type-evidence hash mismatch')
            row['evidence']['source']=str(source)
        # qsort came from the current composition's additional observed provider;
        # its FUNC type is independently checked in the existing SDK ELF declaration.
        qsort_lib=sdk/'target/lib/libSceLibcInternal.so'
        qsort=[x for x in elf_symbols(qsort_lib) if x['name']=='qsort' and x['section'] and x['type']==2]
        if len(qsort)!=1: raise ValueError('qsort exact SDK FUNC declaration missing/ambiguous')
        evidence['records'].append(type_evidence(qsort_lib,qsort[0]))
        rmdir_lib=sdk/'target/lib/libkernel.so'
        rmdir=[x for x in elf_symbols(rmdir_lib) if x['name']=='rmdir' and x['section'] and x['type']==2]
        if len(rmdir)!=1: raise ValueError('rmdir exact SDK FUNC declaration missing/ambiguous')
        evidence['records'].append(type_evidence(rmdir_lib,rmdir[0]))
        # These declarations are read from the exact retained SDK ELF. Runtime
        # provider identity comes separately from the strict original-export read.
        for name,lib in [('__cxa_finalize',qsort_lib),
                         ('sceKernelLoadStartModule',rmdir_lib),('sceKernelDlsym',rmdir_lib),('sceKernelStopUnloadModule',rmdir_lib)]:
            rows=[x for x in elf_symbols(lib) if x['name']==name and x['section'] and x['type']==2]
            if len(rows)!=1: raise ValueError('exact SDK FUNC declaration missing/ambiguous: '+name)
            evidence['records'].append(type_evidence(lib,rows[0]))
        evidence['records'] += [type_evidence(host,host_defs[n]) for n in requested]
        ep=out/'declared-types.json';save(ep,evidence)
        declarations=read(SPIKE/'native-coherent-runtime-1/composition-1/provider-contract-input.json')['declarations']
        declarations+=read(SPIKE/'native-cpp-module-api-evidence-1/declarations.json')['declarations']
        declarations.append(read(SPIKE/'native-runtime-finalize-identity-1/identity-and-lua-test.json')['provider_declaration'])
        def contract(label,obj,decls):
            resolved=out/(label+'-resolved.json')
            call(label+'-resolve',[*tool,'resolve',obj,resolved,ep])
            actual=read(resolved);report=compiler.compile_contract(actual['required'],decls)
            report.update(input_object=actual['input_object'],actual_resolved_requirements=actual['raw_required'],
                          reconciled_type_evidence=actual['reconciled_type_evidence'])
            path=out/(label+'-contract.json');save(path,report)
            if report['contract'] is None:raise ValueError(label+' incomplete contract: '+str(report['missing_strong_names'])+str(report['type_conflict_names']))
            return path
        hp=contract('host',host,declarations)
        bare_host=out/'modbridge_ue4ss_host_contract.prx'
        call('host-export-package',[*tool,'write',host,hp,bare_host,names])
        hostdecl=out/'actual-host-exports.json'
        call('inspect-actual-host-exports',[*tool,'exports',bare_host,names,hostdecl])
        exported=read(hostdecl)['declarations']
        for entry in exported:
            if entry['symbol_type'] != {1:'OBJECT',2:'FUNC'}[host_defs[entry['name']]['type']]:
                raise ValueError('emitted host export type differs from original definition')
        pp=contract('plugin',plugin,exported+declarations)
        bare_plugin=out/'modbridge_lifecycle_plugin.prx'
        call('plugin-package',[*tool,'write',plugin,pp,bare_plugin,plugin_names,'--child-lifecycle','module_start','module_stop'])
        call('inspect-plugin-exports',[*tool,'exports',bare_plugin,plugin_names,out/'actual-plugin-exports.json'])
        call('plugin-dynamic-layout',[llvm/'llvm-readelf','-h','-d','-r','--dyn-syms',bare_plugin])
        call('check-child-package',[sys.executable,HERE/'check_child_package.py',bare_plugin])
        result.update(status='HOST_EXPORT_AND_PLUGIN_IMPORT_PACKAGED',host=dict(path=str(bare_host),sha256=sha(bare_host)),
                      plugin=dict(path=str(bare_plugin),sha256=sha(bare_plugin)),
                      runtime_object=dict(path=str(host),sha256=sha(host)),actual_host_exports=exported,
                      plugin_contract=str(pp),source_compile_audit=str(audit_path),
                      child_lifecycle=True,
                      source_hashes={str(p.relative_to(ROOT)):sha(p) for p in [HERE/'build.py',HERE/'ContractHost.cs',
                          HERE/'child-writer.patch',HERE/'check_child_package.py',
                          HERE.parent/'native-cppmod-source/build.py',HERE.parent/'native-cppmod-source/lifecycle_mod.cpp',
                          HERE.parent/'native-cppmod-source/child_runtime.cpp',HERE.parent/'native-cppmod-source/child_runtime.h']},
                      writer_source=dict(baseline=sha(typed_source/'tools/SharpProspero.Link/DynamicWriter.cs'),
                          successor=sha(writer_source/'tools/SharpProspero.Link/DynamicWriter.cs')),
                      boundary='Local SCE module packaging and host-owned lifecycle checks only. No target constructor/loader/start_mod execution.')
    except Exception as error:
        result.update(status='FAILED',error=str(error));raise
    finally:
        save(out/'receipt.json',result)
    print(json.dumps(dict(receipt=str(out/'receipt.json'),status=result['status']),indent=2))

if __name__=='__main__':main()
