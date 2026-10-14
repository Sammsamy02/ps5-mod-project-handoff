#!/usr/bin/env python3
"""Focused offline format checks; PS5 mode compiles/links without execution."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mbedtls-source',type=Path,required=True)
    p.add_argument('--zlib-source',type=Path,required=True)
    p.add_argument('--output-dir',type=Path,required=True)
    p.add_argument('--red',action='store_true')
    p.add_argument('--ps5-sdk',type=Path)
    p.add_argument('--llvm-config',type=Path)
    p.add_argument('--resource-dir',type=Path)
    a=p.parse_args()
    if bool(a.ps5_sdk)!=bool(a.llvm_config and a.resource_dir) or (a.red and a.ps5_sdk):
        p.error('target mode needs SDK, LLVM and resource paths and forbids --red')
    here=Path(__file__).resolve().parent; lib=here.parent/'pak'
    m=a.mbedtls_source.resolve(); z=a.zlib_source.resolve(); out=a.output_dir.resolve()
    out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ); cc='/usr/bin/clang'
    flags=['-std=c11','-O2','-fno-strict-aliasing','-Wall','-Wextra','-Wpedantic','-Werror',
        '-DZ_SOLO','-DNO_GZIP','-DZLIB_CONST','-DMBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"',
        '-I'+str(here),'-I'+str(lib),'-I'+str(z),'-I'+str(m/'include'),'-I'+str(m/'library')]
    if a.ps5_sdk:
        cc=str(a.ps5_sdk.resolve()/'bin/prospero-clang'); env['LLVM_CONFIG']=str(a.llvm_config.resolve())
        flags += ['-resource-dir',str(a.resource_dir.resolve())]
    sources=[lib/name for name in ('pak_candidate.c','pak_candidate_io.c','pak_candidate_reconcile.c',
        'pak_primary_capture.c','pak_secondary_integrity.c','pak_directory_parser.c','pak_reference_graph.c',
        'pak_physical_header.c','pak_reconciled_payload.c','pak_reader.c','pak_entry_decoder.c','bounded_zlib.c')]
    sources += [z/name for name in ('inflate.c','inftrees.c','inffast.c','adler32.c','zutil.c')]
    sources += [m/'library'/name for name in ('sha1.c','platform_util.c')]
    sources += [here/'tests/red_stub.c'] if a.red else [here/'dispatch.c',here/'pak_adapter.c']
    sources += [here/'tests'/('native_caller.c' if a.ps5_sdk else 'dispatch_test.c')]
    lock=json.loads((lib/'dependency-lock.json').read_text())
    for kind,root in (('mbedtls',m),('zlib',z)):
        for item in lock[kind]['files']:
            if hashlib.sha256((root/item['path']).read_bytes()).hexdigest()!=item['sha256']:
                raise SystemExit('Pinned dependency differs: '+item['path'])
    identity=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    inputs=[{'path':str(path),'sha256':identity(path)} for path in sources+list(lib.glob('*.h'))+
        [here/'dispatch.h',here/'verify.py',lib/'dependency-lock.json']]
    receipt={'target':'ps5-link-only' if a.ps5_sdk else 'host-red' if a.red else 'host-green',
        'target_executed':False,'commands':[],'inputs':inputs,'runtime_supported':False,'installation_supported':False}
    def run(argv,name,allowed=(0,)):
        r=subprocess.run([str(x) for x in argv],env=env,capture_output=True,timeout=60)
        output=r.stdout+r.stderr; (out/name).write_bytes(output)
        receipt['commands'].append({'argv':[str(x) for x in argv],'exit_code':r.returncode,'log':name})
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        if r.returncode not in allowed:
            raise SystemExit(output.decode(errors='replace')[-6000:] or 'command failed')
        return r.returncode,output.decode(errors='replace')
    _,receipt['compiler']=run([cc,'--version'],'compiler.log')
    objects=[]
    for i,s in enumerate(sources):
        obj=out/f'{i:02d}-{s.stem}.o'; run([cc,*flags,'-c',s,'-o',obj],f'{i:02d}-{s.stem}.log');objects.append(obj)
    binary=out/('native_caller.elf' if a.ps5_sdk else 'dispatch_test')
    run([cc,*flags,*objects,*(['-Wl,-z,now'] if a.ps5_sdk else []),'-o',binary],'link.log')
    if a.ps5_sdk:
        tool_dir=a.llvm_config.resolve().parent
        _,elf=run([tool_dir/'llvm-readobj','--file-headers','--needed-libs','--dynamic-table',binary],'elf.log')
        _,symbols=run([tool_dir/'llvm-nm','--dynamic','--undefined-only',binary],'imports.log')
        imports=sorted(line.split()[-1] for line in symbols.splitlines() if line.strip())
        if 'Format: elf64-x86-64' not in elf or 'BIND_NOW' not in elf or 'OS/ABI: FreeBSD' not in elf:
            raise SystemExit('Target ELF/link contract differs')
        if imports!=['free','memcmp','memcpy','memmove','memset','snprintf','strcmp','strlen']:
            raise SystemExit('Unexpected target imports: '+repr(imports))
        receipt['imports']=imports; receipt['bind_now']=True
        receipt['llvm_config']=str(a.llvm_config.resolve())
        receipt['resource_dir']=str(a.resource_dir.resolve())
    if not a.ps5_sdk:
        run(['python3',lib/'tests/generate_candidate_fixtures.py',out/'synthetic'],'generate.log')
        rc,output=run([binary,lib/'tests/fixtures',out/'synthetic'],'tests.log',(1,) if a.red else (0,))
        if a.red and ('GROUP' not in output or 'FAIL' not in output):
            raise SystemExit('RED must be a compiled behavioral failure')
        if not a.red:
            reports=[json.loads(line[7:]) for line in output.splitlines() if line.startswith('REPORT ')]
            if len(reports)!=1: raise SystemExit('Missing actual JSON report')
            report=reports[0]
            if report['format_candidate']['state']!='supported' or report['compatibility_state']!='unverified':
                raise SystemExit('Unexpected actual format scope')
            if any(report[k] for k in ('runtime_supported','installation_supported','authenticity_verified','game_acceptance_verified')):
                raise SystemExit('Unexpected authority assertion')
            receipt['actual_report']=report
            probes=[json.loads(line[13:]) for line in output.splitlines() if line.startswith('PROBE_REPORT ')]
            if len(probes)!=1: raise SystemExit('Missing actual probe JSON report')
            probe=probes[0]
            if (probe['format_candidate']!={'state':'unverified','reason':'candidate_requires_validation','scope':[]}
                or probe['operation_state']!='unverified' or probe['compatibility_state']!='unverified'
                or probe['engine_build']['state']!='unverified' or probe['asset_platform']['state']!='unverified'
                or probe['install_authority']['state']!='unsupported'
                or any(probe[k] for k in ('runtime_supported','installation_supported','authenticity_verified','game_acceptance_verified'))):
                raise SystemExit('Unexpected actual probe scope')
            receipt['actual_probe_report']=probe
        receipt['host_behavior_exit_code']=rc
    receipt['inputs_unchanged']=all(identity(Path(i['path']))==i['sha256'] for i in inputs)
    receipt['artifact']={'path':str(binary),'bytes':binary.stat().st_size}
    receipt['result']='EXPECTED_RED' if a.red else 'PASS'
    if not receipt['inputs_unchanged']: raise SystemExit('Source changed during check')
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({key:receipt[key] for key in ('result','target','target_executed','runtime_supported','installation_supported')}))

if __name__=='__main__': main()
