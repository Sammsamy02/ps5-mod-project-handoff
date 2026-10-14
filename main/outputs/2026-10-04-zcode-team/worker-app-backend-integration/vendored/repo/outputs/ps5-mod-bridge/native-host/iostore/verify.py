#!/usr/bin/env python3
"""Focused immutable-span IoStore v8 host fixtures or PS5 compile/link only."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import struct
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        while data := stream.read(1048576):
            h.update(data)
    return h.hexdigest()


def oracle(fixture):
    path = Path(fixture['path'])
    with path.open('rb') as stream:
        p = stream.read(144)
        chunks, blocks, width, methods, name_width, block_bytes, directory_bytes, parts = struct.unpack_from('<8I',p,24)
        seeds = struct.unpack_from('<I',p,84)[0]
        overflow = struct.unpack_from('<I',p,96)[0]
        ids = 144
        offsets = ids+12*chunks
        block_table = offsets+10*chunks+4*seeds+4*overflow
        method_table = block_table+width*blocks
        directory = method_table+methods*name_width
        metadata = directory+directory_bytes
        end = metadata+chunks*24
        all_rows = fixture['bytes'] <= 65536
        selected = fixture['selected_chunk']
        stream.seek(offsets+10*selected)
        row = stream.read(10)
        offset = int.from_bytes(row[:5],'big')
        length = int.from_bytes(row[5:],'big')
        indices = range(blocks) if all_rows or blocks <= 3 else (0,blocks//2,blocks-1)
        stored = 0
        for index in indices:
            stream.seek(block_table+12*index+5)
            stored += int.from_bytes(stream.read(3),'little')
    if p[:16] != b'-==--==--==--==-' or p[16] != 8 or width != 12 or parts != 1 or selected >= chunks:
        raise ValueError('fixture outside explicit oracle profile')
    sampled_chunks = range(chunks) if all_rows or chunks <= 3 else (0,chunks//2,chunks-1)
    return {'chunks':chunks,'blocks':blocks,'seeds':seeds,'overflow':overflow,'block_table':block_table,
        'method_table':method_table,'directory':directory,'entry_metadata':metadata,'layout_end':end,
        'chunk_rows_checked':len(sampled_chunks)+int(selected not in sampled_chunks),
        'block_rows_checked':blocks if all_rows or blocks <= 3 else 3,'all_rows_checked':int(all_rows),
        'stored_bytes_checked':stored,'selected_chunk':selected,'selected_offset':offset,'selected_bytes':length,
        'selected_first_block':offset//block_bytes if length else 0,
        'selected_last_block':(offset+length-1)//block_bytes if length else 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--fixture-profile',type=Path)
    parser.add_argument('--cc',default='clang')
    parser.add_argument('--ps5-sdk',type=Path)
    parser.add_argument('--llvm-config',type=Path)
    parser.add_argument('--resource-dir',type=Path)
    args = parser.parse_args()
    native = bool(args.ps5_sdk)
    if native != bool(args.llvm_config and args.resource_dir) or (not native and (args.llvm_config or args.resource_dir)):
        parser.error('SDK, llvm-config and resource-dir required together')
    if native and args.fixture_profile:
        parser.error('native target is compile/link only; no corpus execution')
    if not native and not args.fixture_profile:
        parser.error('host fixture profile required')
    lock_path = HERE/'schema-lock.json'
    lock = json.loads(lock_path.read_text())
    references = [ROOT/lock['cue4parse_directory']/name for name in lock['cue4parse_files']]
    for path in references:
        if digest(path) != lock['cue4parse_files'][path.name]:
            raise SystemExit('retained schema mismatch: '+str(path))
    for row in lock['retoc_writers']:
        path = ROOT/row['path']; references.append(path)
        if digest(path) != row['sha256']:
            raise SystemExit('retained construction source mismatch')
    out = args.output_dir.resolve(); out.mkdir(parents=True,exist_ok=False)
    env = dict(os.environ); compiler = args.cc
    flags = ['-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(HERE)]
    caller = HERE/'tests'/('native_caller.c' if native else 'reader_test.c')
    inputs = [HERE/'toc_reader.c',HERE/'toc_reader.h',caller,Path(__file__).resolve(),lock_path,*references]
    receipt = {'result':'INCOMPLETE','target':'ps5-compile-link-only' if native else 'host-captured-metadata',
        'target_executed':False,'full_toc_validated':False,'payload_verified':False,
        'runtime_supported':False,'installation_supported':False,'commands':[]}
    fixtures = []
    if not native:
        profile = args.fixture_profile.resolve(); inputs.append(profile)
        fixtures = json.loads(profile.read_text())['fixtures']
        if not 1 <= len(fixtures) <= 8:
            raise SystemExit('focused fixture count exceeded')
        for row in fixtures:
            path = Path(row['path']).resolve(); inputs.append(path)
            if not 144 <= row['bytes'] <= 268435456 or path.stat().st_size != row['bytes'] or digest(path) != row['sha256']:
                raise SystemExit('captured fixture identity mismatch')
            if not 0 <= row['paired_length'] <= (1<<64)-1 or not 0 <= row['selected_chunk'] < 1048576:
                raise SystemExit('fixture scalar outside range')
        receipt['fixture_profile'] = str(profile)
        receipt['ucas_payload_bytes_read'] = 0
    else:
        compiler = str(args.ps5_sdk.resolve()/'bin/prospero-clang')
        llvm_bin = args.llvm_config.resolve().parent
        selected = HERE.parent/'llvm_selected_manifest.json'
        expected = {Path(compiler):'3dcf3139662db7b0f796c2073bc8f673a5557e48bc24d2e413896d33847a69db',
            args.llvm_config.resolve():'39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5',
            llvm_bin/'clang':'8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
            llvm_bin/'llvm-readobj':'73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30',
            llvm_bin/'llvm-nm':'a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426',
            selected:'7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896'}
        for path,expected_sha in expected.items():
            if digest(path) != expected_sha:
                raise SystemExit('pinned tool/resource manifest mismatch: '+str(path))
        inputs += list(expected)
        env['LLVM_CONFIG'] = str(args.llvm_config.resolve())
        for name in ('CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','SDKROOT'):
            env.pop(name,None)
        flags += ['-resource-dir',str(args.resource_dir.resolve())]
        receipt['environment_overrides'] = {'LLVM_CONFIG':env['LLVM_CONFIG']}
    receipt['inputs'] = [{'path':str(p),'sha256':digest(p)} for p in inputs]

    def save():
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')

    def run(name,argv):
        argv = [str(a) for a in argv]; start = time.monotonic()
        p = subprocess.run(argv,env=env,capture_output=True,timeout=60)
        (out/(name+'.stdout')).write_bytes(p.stdout); (out/(name+'.stderr')).write_bytes(p.stderr)
        receipt['commands'].append({'argv':argv,'exit':p.returncode,'elapsed_seconds':round(time.monotonic()-start,6),
            'stdout':name+'.stdout','stderr':name+'.stderr'})
        save()
        if p.returncode:
            raise RuntimeError(name+' failed: '+p.stdout.decode(errors='replace')[-2000:]+p.stderr.decode(errors='replace')[-2000:])
        return p.stdout

    try:
        save(); run('compiler',[compiler,'--version'])
        objects = []
        for source in [HERE/'toc_reader.c',caller]:
            obj = out/(source.stem+'.o'); objects.append(obj)
            run(source.stem,[compiler,*flags,'-MD','-MF',out/(source.stem+'.d'),'-c',source,'-o',obj])
        binary = out/('reader.elf' if native else 'reader-test')
        run('link',[compiler,*flags,*objects,*(['-Wl,-z,now'] if native else []),'-o',binary])
        if not native:
            argv = [binary]
            for row in fixtures:
                argv += [row['path'],str(row['paired_length']),str(row['selected_chunk'])]
            text = run('reader-test',argv).decode()
            rows = [json.loads(line) for line in text.splitlines() if line.startswith('{')]
            if len(rows) != len(fixtures):
                raise RuntimeError('actual fixture receipts missing')
            for i,(observed,fixture) in enumerate(zip(rows,fixtures)):
                expected = oracle(fixture)
                if observed['fixture'] != i or any(observed[k] != v for k,v in expected.items()):
                    raise RuntimeError('independent metadata geometry mismatch: '+str(i))
            summary = re.search(r'iostore-reader groups=(\d+) checks=(\d+) failures=0 ',text)
            if not summary:
                raise RuntimeError('focused success summary missing')
            receipt['groups'],receipt['checks'] = map(int,summary.groups())
            receipt['fixture_observations'] = rows
            receipt['independent_geometry_matches'] = True
        else:
            resource = args.resource_dir.resolve()
            members = json.loads(selected.read_text())['members']; dependencies = set()
            for dep in out.glob('*.d'):
                text = dep.read_text().replace('\\\n',' ')
                dependencies.update(Path(p).resolve() for p in shlex.split(text.split(':',1)[1]))
            rows = []
            for path in sorted(dependencies):
                pin = path.is_relative_to(resource); value = digest(path)
                if pin and members.get('lib/clang/21/'+path.relative_to(resource).as_posix()) != value:
                    raise RuntimeError('used resource header mismatch')
                rows.append({'path':str(path),'sha256':value,'selected_resource_pin_verified':pin})
            receipt['used_dependencies'] = rows
            info = run('elf',[llvm_bin/'llvm-readobj','--file-headers','--dynamic-table','--dyn-symbols',binary]).decode()
            if any(x not in info for x in ('EM_X86_64','Type: SharedObject','OS/ABI: FreeBSD','BIND_NOW')):
                raise RuntimeError('expected native ELF contract missing')
            imports = []
            for block in info.split('Symbol {')[1:]:
                name = re.search(r'Name: (\S+)',block)
                if name and 'Section: Undefined' in block:
                    imports.append(name.group(1))
            if not set(imports) <= {'memcmp','memcpy','memset'}:
                raise RuntimeError('unexpected native import: '+repr(imports))
            receipt['imports'] = sorted(imports)
            symbols = run('reader-symbols',[llvm_bin/'llvm-nm','-u',objects[0]]).decode()
            if re.search(r'\b(open|read|write|socket|malloc|calloc|realloc|free|system|execve|__udivti3)\b',symbols):
                raise RuntimeError('reader unexpectedly depends on I/O/allocation/arithmetic helper')
            receipt['elf_contract'] = 'FreeBSD x86_64 ET_DYN/BIND_NOW; link only, entry/runtime unproved'
        receipt['artifact'] = {'path':str(binary),'bytes':binary.stat().st_size,'sha256':digest(binary)}
        receipt['inputs_unchanged'] = all(digest(Path(r['path'])) == r['sha256'] for r in receipt['inputs'])
        if not receipt['inputs_unchanged']:
            raise RuntimeError('input changed during check')
        receipt['result'] = 'PASS'
    except Exception as e:
        receipt['result'] = 'FAIL'; receipt['failure'] = str(e)
        raise
    finally:
        save()
    print(json.dumps({k:receipt[k] for k in ('result','target','target_executed')}))


if __name__ == '__main__':
    main()
