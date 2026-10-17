#!/usr/bin/env python3
"""Focused host corpus comparison or PS5 compile/link-only archive grouping proof."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import stat
import subprocess
import time
import zipfile
from link_layout import write_linker_script, validate_dynamic_layout

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
ROOT = HERE.parents[3]
CORPUS = ROOT / '.superpowers/native-observations/nexus-real-mods-2026-10-01'
CORPUS_RECEIPTS = [
    ('leon', 'leon-download-receipt.json'),
    ('minimap', 'minimap-v3.1-receipt.json'),
    ('mod-menu', 'mod-menu-v6-receipt.json'),
    ('stamina', 'stamina-download-receipt.json'),
    ('ue4ss', 'ue4ss-v1-receipt.json'),
]
TOOLS = {
    'clang': '8501bb9d3deb0250f23422a759b94651c89479937a0af29a7c7f0b98eaf3ca5a',
    'llvm-readobj': '73efdbad7294e02ed570d7156d1d1c8d77ed922c2df281435a7a1f8ad90eeb30',
    'llvm-nm': 'a011020cab7ca848ebb10948c636cde63a83cf81e81156285f96f577ea70e426',
    'llvm-config': '39616d289c2c1da34a279ccd6e9478bff10b40ae7cd30901302dd8ffad6cede5',
    'prospero-clang': '3dcf3139662db7b0f796c2073bc8f673a5557e48bc24d2e413896d33847a69db',
}
SDK_ARCHIVE_SHA256 = 'a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb'
LLVM_MANIFEST_SHA256 = '7748ecf6bf71b73fb23ccdb157bd2a2b94468f28a97487aa4f8ac586e1829896'


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def c_string(value):
    data = value.encode('utf-8')
    if any(b == 0 or b in (9, 10, 13) for b in data):
        raise ValueError('corpus path contains NUL/tab/newline')
    out = '"'
    for b in data:
        if 0x20 <= b < 0x7f and b not in (ord('"'), ord('\\')):
            out += chr(b)
        elif b == ord('"'):
            out += '\\"'
        elif b == ord('\\'):
            out += '\\\\'
        else:
            out += '\\%03o' % b
    return out + '"'


def read_corpus_receipts(out):
    fixtures = []
    used = [HERE / 'groups.h', HERE / 'intake.h', HERE / 'groups_tests/groups_test.c',
            HERE / 'groups_tests/red_stub.c', HERE / 'groups_tests/native_caller.c', Path(__file__).resolve()]
    for name, receipt_name in CORPUS_RECEIPTS:
        receipt_path = CORPUS / receipt_name
        receipt = json.loads(receipt_path.read_text())
        archive = Path(receipt['archive']).resolve()
        if not archive.is_file() or archive.stat().st_size != int(receipt['bytes']):
            raise ValueError('retained archive size differs from its receipt: ' + name)
        if not re.fullmatch(r'[0-9a-f]{64}', receipt['sha256']):
            raise ValueError('invalid archive digest in receipt: ' + name)
        members = receipt.get('members')
        if not isinstance(members, list) or len(members) > 128:
            raise ValueError('receipt rows exceed focused test bound: ' + name)
        rows = []
        for member in members:
            path = member['name']
            if not isinstance(path, str) or not path or len(path.encode('utf-8')) > 4096:
                raise ValueError('receipt path outside row bound: ' + name)
            attr = member.get('external_attr')
            mode = ((int(attr) >> 16) & 0xffff) if isinstance(attr, int) else 0
            file_kind = stat.S_IFMT(mode)
            if file_kind == stat.S_IFREG or file_kind == 0 and not path.endswith('/'):
                kind = 1
            else:
                kind = 2
            rows.append({'path': path, 'kind': kind, 'size': int(member.get('bytes', 0))})
        fixtures.append({'name': name, 'receipt': receipt_path, 'archive': archive,
                         'archive_sha256': receipt['sha256'], 'rows': rows})
        used.append(receipt_path)

    lines = ['#ifndef MB_ARCHIVE_GROUPS_CORPUS_GENERATED_H',
             '#define MB_ARCHIVE_GROUPS_CORPUS_GENERATED_H',
             '#include <stdint.h>',
             'typedef struct groups_corpus_row { const char *path; uint32_t kind; uint64_t size; } groups_corpus_row;',
             'typedef struct groups_corpus_fixture { const char *name; const char *archive_sha256; uint32_t row_count; const groups_corpus_row *rows; } groups_corpus_fixture;']
    for fixture in fixtures:
        sym = 'rows_' + re.sub(r'[^a-z0-9]', '_', fixture['name'])
        lines.append('static const groups_corpus_row %s[] = {' % sym)
        for row in fixture['rows']:
            lines.append('    {%s,%du,UINT64_C(%d)},' % (c_string(row['path']), row['kind'], row['size']))
        if not fixture['rows']:
            lines.append('    {"",0u,UINT64_C(0)},')
        lines.append('};')
        fixture['symbol'] = sym
    lines.append('static const groups_corpus_fixture groups_corpus[] = {')
    for f in fixtures:
        lines.append('    {%s,%s,%du,%s},' % (c_string(f['name']), c_string(f['archive_sha256']), len(f['rows']), f['symbol']))
    lines += ['};', '#define GROUPS_CORPUS_COUNT ((uint32_t)(sizeof(groups_corpus)/sizeof(groups_corpus[0])))', '#endif']
    generated = out / 'corpus_generated.h'
    generated.write_text('\n'.join(lines) + '\n')
    used.append(generated)
    return fixtures, used


def run(name, argv, out, receipt, env=None, timeout=60, allowed=(0,)):
    argv = [str(x) for x in argv]
    started = time.monotonic()
    try:
        proc = subprocess.run(argv, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
        status = proc.returncode
        stdout, stderr = proc.stdout, proc.stderr
    except subprocess.TimeoutExpired as exc:
        status = 'timeout'
        stdout, stderr = exc.stdout or b'', exc.stderr or b''
    (out / (name + '.stdout')).write_bytes(stdout)
    (out / (name + '.stderr')).write_bytes(stderr)
    receipt['commands'].append({'name': name, 'argv': argv, 'exit': status,
        'elapsed_seconds': round(time.monotonic() - started, 6),
        'stdout': name + '.stdout', 'stderr': name + '.stderr'})
    (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if status not in allowed:
        raise RuntimeError(name + ' failed: ' + stdout.decode(errors='replace')[-3000:] + stderr.decode(errors='replace')[-3000:])
    return stdout, stderr, status


def expected_host_groups(report):
    if report.get('member_inventory_complete') is not True:
        raise RuntimeError('production host did not publish a complete member inventory')
    result = []
    for group in report.get('container_groups', []):
        result.append({
            'parent': group['parent'], 'stem': group['stem'], 'kind': group['kind'],
            'status': 1 if group['status'] == 'candidate' else 2,
            'reason_flags': sum({'container_member_missing': 1, 'container_case_ambiguity': 2,
                                 'container_partition_unresolved': 4}[r] for r in group['reason_codes']),
            'members_complete': int(group['members_complete']),
            'members': [(m['role'], m['path']) for m in group['members']],
        })
    return result


def compare_host_and_c(fixtures, c_stdout, out, receipt):
    parsed = {}; current_group = {}
    for line in c_stdout.decode(errors='strict').splitlines():
        fields = line.split('\t')
        if not fields or fields[0] not in {'CASE', 'GROUP', 'MEMBER'}:
            continue
        if fields[0] == 'CASE':
            parsed[fields[1]] = {'count': int(fields[2]), 'groups': []}
            current_group.pop(fields[1],None)
        elif fields[0] == 'GROUP':
            current_group[fields[1]] = {'parent': fields[2], 'stem': fields[3], 'kind': fields[4],
                'status': int(fields[5]), 'reason_flags': int(fields[6]),
                'members_complete': int(fields[7]), 'members': []}
            parsed[fields[1]]['groups'].append(current_group[fields[1]])
        else:
            if fields[1] not in current_group:
                raise RuntimeError('C corpus member appeared without its group')
            current_group[fields[1]]['members'].append((fields[2], fields[4]))

    binary = PROJECT / 'build/modbridge'
    if not binary.is_file():
        raise RuntimeError('existing production host executable is missing')
    receipt['host_reference_binary'] = {'path': str(binary), 'sha256': digest(binary),
        'source_inventory_sha256': digest(PROJECT / 'src/inventory.cpp'), 'built_or_modified': False}
    host_runs = []
    for fixture in fixtures:
        output, _, _ = run('host-' + fixture['name'], [binary, 'analyze', fixture['archive']], out,
                           receipt, timeout=180)
        report = json.loads(output)
        if report.get('package_sha256') != fixture['archive_sha256']:
            raise RuntimeError('host archive identity differs from retained receipt: ' + fixture['name'])
        expected = expected_host_groups(report)
        observed = parsed.get(fixture['name'])
        if observed is None or observed['count'] != len(observed['groups']):
            raise RuntimeError('C corpus result missing or internally inconsistent: ' + fixture['name'])
        if observed['groups'] != expected:
            raise RuntimeError('C/production grouping mismatch for ' + fixture['name'] + ': ' + repr((observed, expected)))
        if fixture['name'] == 'leon' and observed['count'] != 1:
            raise RuntimeError('retained Leon receipt should form one IoStore candidate')
        if fixture['name'] != 'leon' and observed['count'] != 0:
            raise RuntimeError('script/runtime receipt unexpectedly formed a container candidate: ' + fixture['name'])
        host_runs.append({'name': fixture['name'], 'archive_sha256': fixture['archive_sha256'],
            'rows': len(fixture['rows']), 'groups': observed['groups'], 'matched_production_host': True})
    receipt['corpus'] = host_runs


def verify_source_hashes(paths, expected):
    return all(digest(path) == expected[str(path)] for path in paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('red', 'host', 'native'), required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--ps5-sdk', type=Path)
    parser.add_argument('--llvm-config', type=Path)
    parser.add_argument('--resource-dir', type=Path)
    args = parser.parse_args()
    native = args.phase == 'native'
    if native != bool(args.ps5_sdk and args.llvm_config and args.resource_dir):
        parser.error('native phase requires --ps5-sdk, --llvm-config and --resource-dir together')
    if not native and any((args.ps5_sdk,args.llvm_config,args.resource_dir)):
        parser.error('SDK flags are only valid for native compile/link')
    out = args.output_dir.resolve()
    if out.exists():
        parser.error('use a fresh output directory')
    out.mkdir(parents=True)
    receipt = {'phase': args.phase, 'target_executed': False, 'installation_supported': False,
        'runtime_supported': False, 'format_validated': False, 'commands': [], 'inputs': [],
        'scope': 'pure metadata grouping; no extraction, write, install or runtime claim'}
    try:
        fixtures, input_paths = read_corpus_receipts(out) if not native else ([], [HERE/'groups.h', HERE/'intake.h',
            HERE/'groups_tests/native_caller.c', Path(__file__).resolve(), HERE/'link_layout.py'])
        input_paths += [HERE/'groups.c'] if args.phase != 'red' else [HERE/'groups_tests/red_stub.c']
        receipt['inputs'] = [{'path': str(Path(p).resolve()), 'sha256': digest(p)} for p in input_paths]
        if args.phase == 'red':
            # RED needs generated corpus declarations before compilation too.
            cc = '/usr/bin/clang'
            cmd = [cc,'-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(HERE),'-I'+str(out),
                   HERE/'groups_tests/groups_test.c',HERE/'groups_tests/red_stub.c','-o',out/'groups-test-red']
            run('host-red-compile',cmd,out,receipt)
            stdout, _, status = run('host-red-test',[out/'groups-test-red'],out,receipt,timeout=15,allowed=(1,))
            summary = re.search(rb'archive-groups cases=(\d+) checks=(\d+) failures=(\d+)',stdout)
            if not summary or int(summary.group(3))==0:
                raise RuntimeError('RED did not demonstrate a behavioral failure')
            receipt['red'] = {'cases':int(summary.group(1)), 'checks':int(summary.group(2)),
                              'expected_nonzero_exit':status, 'behavioral_failure_observed':True}
        elif args.phase == 'host':
            cc='/usr/bin/clang'; obj=out/'groups.o'; test_obj=out/'groups_test.o'
            common=['-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(HERE),'-I'+str(out)]
            run('host-compiler-version',[cc,'--version'],out,receipt)
            run('host-groups-compile',[cc,*common,'-MD','-MF',out/'groups.d','-c',HERE/'groups.c','-o',obj],out,receipt)
            run('host-test-compile',[cc,*common,'-MD','-MF',out/'groups_test.d','-c',HERE/'groups_tests/groups_test.c','-o',test_obj],out,receipt)
            binary=out/'groups-test'
            run('host-link',[cc,obj,test_obj,'-o',binary],out,receipt)
            stdout,_,_=run('host-groups-test',[binary],out,receipt,timeout=20)
            summary=re.search(rb'archive-groups cases=(\d+) checks=(\d+) failures=0 ',stdout)
            if not summary: raise RuntimeError('focused host summary missing')
            receipt['host_tests']={'cases':int(summary.group(1)),'checks':int(summary.group(2))}
            compare_host_and_c(fixtures,stdout,out,receipt)
            receipt['host_reference_inputs']=[{'path':str(f['receipt']),'sha256':digest(f['receipt'])} for f in fixtures]
            receipt['host_reference_inputs'].append(receipt['host_reference_binary'])
        else:
            sdk=args.ps5_sdk.resolve(); llvm_config=args.llvm_config.resolve(); resource=args.resource_dir.resolve()
            llvm_bin=llvm_config.parent
            compiler=sdk/'bin/prospero-clang'; sdk_archive=PROJECT/'native-probe/build/deps/sdk.zip'
            selected=HERE.parent/'llvm_selected_manifest.json'; selected_json=json.loads(selected.read_text())
            expected_tools={'prospero-clang':TOOLS['prospero-clang'],'llvm-config':TOOLS['llvm-config'],
                'clang':TOOLS['clang'],'llvm-readobj':TOOLS['llvm-readobj'],'llvm-nm':TOOLS['llvm-nm']}
            paths={compiler:expected_tools['prospero-clang'],llvm_config:expected_tools['llvm-config'],
                llvm_bin/'clang':expected_tools['clang'],llvm_bin/'llvm-readobj':expected_tools['llvm-readobj'],
                llvm_bin/'llvm-nm':expected_tools['llvm-nm'],selected:LLVM_MANIFEST_SHA256,sdk_archive:SDK_ARCHIVE_SHA256}
            for path,sha in paths.items():
                if not path.is_file() or digest(path)!=sha:
                    raise RuntimeError('pinned SDK/tool input mismatch: '+str(path))
            receipt['link_layout_profile']=write_linker_script(sdk,out)
            caller_script=Path(receipt['link_layout_profile']['caller_script'])
            receipt['inputs'] += [{'path':str(path),'sha256':digest(path)}
                for path in (sdk/'ldscripts/elf_x86_64.x',caller_script)]
            env=dict(os.environ); env['LLVM_CONFIG']=str(llvm_config)
            for key in ('CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','SDKROOT'):
                env.pop(key,None)
            flags=['-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(HERE),'-resource-dir',str(resource)]
            receipt['toolchain']={'compiler':str(compiler),'compiler_sha256':digest(compiler),
                'llvm_config':str(llvm_config),'llvm_config_sha256':digest(llvm_config),
                'selected_manifest':str(selected),'selected_manifest_sha256':digest(selected),
                'sdk_archive_sha256':digest(sdk_archive),'resource_dir':str(resource),
                'target_executed':False}
            run('native-compiler-version',[compiler,'--version'],out,receipt,env=env)
            objects=[]
            for source in (HERE/'groups.c',HERE/'groups_tests/native_caller.c'):
                obj=out/(source.stem+'.o'); dep=out/(source.stem+'.d')
                run(source.stem+'-native-compile',[compiler,*flags,'-MD','-MF',dep,'-c',source,'-o',obj],out,receipt,env=env)
                objects.append(obj)
            elf=out/'archive-groups.elf'
            run('native-link',[compiler,*flags,*objects,'-Wl,-z,now','-Wl,-T,'+str(caller_script),
                '-Wl,-Map,'+str(out/'link.map'),'-o',elf],out,receipt,env=env)
            dynamic_layout=validate_dynamic_layout(elf)
            readobj=llvm_bin/'llvm-readobj'; nm=llvm_bin/'llvm-nm'
            elf_out,elf_err,_=run('native-elf',[readobj,'--file-headers','--needed-libs','--dynamic-table','--dyn-symbols',elf],out,receipt,env=env)
            if elf_err: raise RuntimeError('llvm-readobj emitted stderr for native ELF')
            if b'Format: elf64-x86-64' not in elf_out or b'OS/ABI: FreeBSD' not in elf_out or b'BIND_NOW' not in elf_out:
                raise RuntimeError('target ELF contract missing')
            undefined,_,_=run('native-imports',[nm,'--dynamic','--undefined-only',elf],out,receipt,env=env)
            imports=sorted({line.split()[-1].decode() for line in undefined.splitlines() if line.strip()})
            forbidden={'open','openat','read','write','unlink','mkdir','socket','connect','execve','system','malloc','calloc','realloc','free'}
            if forbidden.intersection(imports): raise RuntimeError('unexpected I/O/allocator import: '+repr(sorted(forbidden.intersection(imports))))
            dependencies=set()
            for dep in out.glob('*.d'):
                dependencies.update(Path(p).resolve() for p in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]))
            with zipfile.ZipFile(sdk_archive) as archive:
                sdk_member='ps5-payload-sdk/'
                closure=[]
                for path in sorted(dependencies):
                    if path.is_relative_to(sdk):
                        member=sdk_member+path.relative_to(sdk).as_posix()
                        if archive.read(member)!=path.read_bytes(): raise RuntimeError('SDK header differs: '+str(path))
                        kind='verified_sdk_archive'
                    elif path.is_relative_to(resource):
                        rel='lib/clang/21/'+path.relative_to(resource).as_posix()
                        pinned=selected_json.get('members',{}).get(rel)
                        if not pinned or digest(path)!=pinned: raise RuntimeError('resource header differs from selected manifest: '+rel)
                        kind='selected_resource_manifest'
                    elif path.is_relative_to(HERE): kind='owned_source'
                    else: raise RuntimeError('unexpected non-SDK/host dependency: '+str(path))
                    closure.append({'path':str(path),'sha256':digest(path),'kind':kind})
            receipt['native']={'elf':str(elf),'elf_bytes':elf.stat().st_size,'elf_sha256':digest(elf),
                'imports':imports,'used_header_closure':closure,'readobj_stderr':elf_err.decode(errors='replace'),
                'dynamic_layout':dynamic_layout,
                'elf_contract':'FreeBSD x86_64 shared-object executable; compile/link only; not run'}
        all_inputs=[Path(item['path']) for item in receipt['inputs']]
        receipt['inputs_unchanged']=all(p.is_file() and digest(p)==item['sha256'] for p,item in zip(all_inputs,receipt['inputs']))
        if not receipt['inputs_unchanged']: raise RuntimeError('input changed during focused run')
        receipt['result']='PASS'
    except Exception as exc:
        receipt['result']='FAIL'; receipt['failure']=str(exc)
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        raise
    finally:
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'phase':args.phase,'result':receipt['result'],'target_executed':False}))


if __name__ == '__main__':
    main()
