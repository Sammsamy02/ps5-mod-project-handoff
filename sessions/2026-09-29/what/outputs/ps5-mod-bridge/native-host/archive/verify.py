#!/usr/bin/env python3
"""Focused real-file archive proof. Every generated/build file stays in --output."""
import argparse, hashlib, io, json, os, pathlib, struct, subprocess, sys, tarfile, unicodedata, zipfile
from link_layout import write_linker_script, validate_dynamic_layout

ROOT = pathlib.Path(__file__).resolve().parents[4]
HERE = pathlib.Path(__file__).resolve().parent
CORPUS = ROOT / '.superpowers/native-observations/nexus-real-mods-2026-10-01'
NAMES = ['leon-2k-v1.1.zip', 'minimap-v3.1.zip', 'mod-menu-v6.zip',
         'infinite-stamina-v1.zip', 'ue4ss-v1.zip']

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(65536), b''):
            h.update(block)
    return h.hexdigest()

def select(rows):
    groups = {}
    for i, row in enumerate(rows):
        if row['kind'] != 1:
            continue
        p = pathlib.PurePosixPath(row['name'].replace('\\', '/'))
        ext = p.suffix.lower()
        if ext in ('.pak', '.utoc', '.ucas'):
            groups.setdefault((str(p.parent), p.stem), {}).setdefault(ext, []).append(i)
    complete = [g for g in groups.values() if all(len(g.get(x, [])) == 1
                 for x in ('.pak', '.utoc', '.ucas'))]
    return sorted(sum((sum(g.values(), []) for g in complete), [])) if complete else [
        i for i, r in enumerate(rows) if r['kind'] == 1]

def oracle(path, output):
    rows = []
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            for item in archive.infolist():
                h = hashlib.sha256()
                with archive.open(item) as f:
                    for block in iter(lambda: f.read(65536), b''):
                        h.update(block)
                # Pinned libarchive explicitly reports NFD names on Darwin
                # (archive_string.c:1263-1291). Keep the raw wire spelling in
                # the oracle separately; neither spelling becomes a sink path.
                reported=unicodedata.normalize('NFD',item.filename) if sys.platform=='darwin' else item.filename
                rows.append(dict(name=reported, wire_name=item.filename, size=item.file_size,
                                 kind=2 if item.is_dir() else 1, sha256=h.hexdigest()))
    else:
        with tarfile.open(path) as archive:
            for item in archive:
                h = hashlib.sha256()
                if item.isfile():
                    with archive.extractfile(item) as f:
                        for block in iter(lambda: f.read(65536), b''):
                            h.update(block)
                rows.append(dict(name=item.name.rstrip('/')+'/' if item.isdir() else item.name, size=item.size,
                                 kind=2 if item.isdir() else 1, sha256=h.hexdigest()))
    selection = select(rows)
    with output.open('w', encoding='utf-8') as f:
        f.write(f'{digest(path)} {path.stat().st_size}\n')
        for i, row in enumerate(rows):
            f.write(f"{int(i in selection)} {row['kind']} {row['size']} {row['sha256']} {row['name']}\n")
    return rows, selection

def run(argv, output, env=None):
    result = subprocess.run([str(x) for x in argv], stdout=subprocess.PIPE, stderr=subprocess.PIPE,env=env)
    output.with_suffix('.stdout').write_bytes(result.stdout)
    output.with_suffix('.stderr').write_bytes(result.stderr)
    return dict(argv=[str(x) for x in argv], exit=result.returncode)

def native(out):
    deps=ROOT/'.superpowers/native-dependency-spike'
    fresh=deps/'native-libarchive-lzma-experiment-1'
    sdk=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
    crypto=ROOT/'outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/native-038ce86825317d4d74716405'
    mbed=crypto/'sources/mbedtls/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5'
    tools=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    resource=deps/'tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21'
    config=tools/'llvm-config'
    env=os.environ.copy();env['LLVM_CONFIG']=str(config)
    libs=[fresh/'native/libarchive-build/.libs/libarchive.a',fresh/'native/xz-build/src/liblzma/.libs/liblzma.a',deps/'native-zip-experiment-1/native/zlib-build/libz.a',crypto/'libmbedcrypto.a']
    sources=[HERE/'private/posix_compat.c',HERE/'private/posix_compat.h',HERE/'intake.c',HERE/'intake.h',HERE/'groups.c',HERE/'groups.h',HERE/'tests/corpus_test.c',HERE/'tests/controls_test.c',pathlib.Path(__file__),HERE/'link_layout.py']
    inputs=[*sources,*libs,mbed/'include/mbedtls/sha256.h',crypto/'config/crypto_config.h',
            deps/'libarchive-release/libarchive/archive.h',deps/'libarchive-release/libarchive/archive_entry.h',
            fresh/'native/libarchive-build/config.h',fresh/'native/xz-build/config.h',sdk/'bin/prospero-clang',sdk/'bin/prospero-lld',config,
            sdk/'ldscripts/elf_x86_64.x']
    receipt={'scope':'native-compile-link-only','target_executed':False,'inputs_before':{str(x):digest(x) for x in inputs},'environment':{'LLVM_CONFIG':str(config)},'commands':[]}
    try:
        receipt['link_layout_profile']=write_linker_script(sdk,out)
    except (OSError,ValueError) as exc:
        receipt['link_layout_error']=str(exc)
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2))
        return 1
    caller_script=pathlib.Path(receipt['link_layout_profile']['caller_script'])
    inputs.append(caller_script)
    receipt['inputs_before'][str(caller_script)]=digest(caller_script)
    flags=[sdk/'bin/prospero-clang','-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror',
              '-resource-dir',resource,'-DMB_ARCHIVE_BUILTIN_XZ=1','-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"',
              '-I'+str(mbed/'include'),'-I'+str(crypto/'config'),'-I'+str(deps/'libarchive-release/libarchive')]
    for name in ('intake','groups','posix_compat','corpus_test','controls_test'):
        source=HERE/'private/posix_compat.c' if name=='posix_compat' else HERE/(name+'.c') if name in ('intake','groups') else HERE/'tests'/(name+'.c')
        argv=[*flags,'-MD','-MF',out/(name+'.d'),'-c',source,'-o',out/(name+'.o')]
        receipt['commands'].append(run(argv,out/('compile-'+name),env))
        if receipt['commands'][-1]['exit']:break
    for caller in ('corpus_test','controls_test') if not any(c['exit'] for c in receipt['commands']) else ():
        objects=[out/'intake.o',out/'posix_compat.o',out/(caller+'.o')]
        if caller=='corpus_test':objects.append(out/'groups.o')
        argv=[sdk/'bin/prospero-clang','-resource-dir',resource,*objects,*libs,
              '-Wl,--no-undefined,-z,now','-Wl,-T,'+str(caller_script),
              '-Wl,-Map='+str(out/(caller+'.map')),'-o',out/(caller+'.elf')]
        receipt['commands'].append(run(argv,out/('compile-'+caller),env))
        if receipt['commands'][-1]['exit']:
            break
        try:
            receipt.setdefault('dynamic_layout',{})[caller]=validate_dynamic_layout(out/(caller+'.elf'))
        except (OSError,ValueError) as exc:
            receipt['link_layout_error']=str(exc)
            break
        receipt['commands'].append(run([tools/'llvm-readobj','--file-headers','--program-headers','--dynamic-table',out/(caller+'.elf')],out/('elf-'+caller),env))
        if (out/('elf-'+caller+'.stderr')).stat().st_size:
            receipt['link_layout_error']='llvm-readobj emitted stderr for '+caller
            break
        receipt['commands'].append(run([tools/'llvm-nm','--dynamic','--undefined-only',out/(caller+'.elf')],out/('imports-'+caller),env))
        receipt.setdefault('artifacts',[]).append({'path':str(out/(caller+'.elf')),'bytes':(out/(caller+'.elf')).stat().st_size,'sha256':digest(out/(caller+'.elf'))})
    roots=[HERE,sdk,resource,mbed,crypto/'config',deps/'libarchive-release/libarchive']
    headers=set()
    for path in out.glob('*.d'):
        text=path.read_text().replace('\\\n',' ')
        for value in text.split(':',1)[1].split():
            p=pathlib.Path(value).resolve()
            if p.exists():headers.add(p)
    receipt['headers']={str(x):digest(x) for x in sorted(headers)}
    receipt['foreign_headers']=[str(x) for x in headers if not any(x==r.resolve() or r.resolve() in x.parents for r in roots)]
    receipt['inputs_after']={str(x):digest(x) for x in inputs}
    receipt['inputs_unchanged']=receipt['inputs_before']==receipt['inputs_after']
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2))
    bad=any(c['exit'] for c in receipt['commands']) or not receipt['inputs_unchanged'] or bool(receipt['foreign_headers']) or 'link_layout_error' in receipt
    print(json.dumps({'native_commands':len(receipt['commands']),'failed':bool(bad),'executed':False}))
    return int(bad)

def fixtures(out):
    f=out/'fixtures';f.mkdir()
    payload=bytes((i*17+31)%256 for i in range(200003))
    def make(name, rows):
        with zipfile.ZipFile(f/name,'w',compression=zipfile.ZIP_STORED) as z:
            for path, data in rows:
                z.writestr(path,data)
    make('good.zip',[('wrapper/data.bin',payload),('wrapper/late.txt',b'late complete member')])
    make('unicode.zip',[('Wrapper/Caf\u00e9/\U0001f3ae.lua',b'unchanged UTF-8 member')])
    make('traversal.zip',[('ok.bin',b'ok'),('../late.bin',b'bad')])
    make('alias.zip',[('wrap/X.bin',b'a'),('wrap/./x.bin',b'b')])
    make('ancestor.zip',[('event',b'a'),('event-extra',b'b'),('event/file.bin',b'c')])
    bad=bytearray((f/'good.zip').read_bytes())
    # Flip only the final stored member payload; keep all structural bytes and
    # declared CRCs intact, independently from the production reader.
    with zipfile.ZipFile(f/'good.zip') as z:
        entry=z.infolist()[-1]
        off=entry.header_offset
        n,e=struct.unpack_from('<HH',bad,off+26)
        bad[off+30+n+e]^=1
    (f/'crc.zip').write_bytes(bad)
    (f/'truncated.zip').write_bytes((f/'good.zip').read_bytes()[:100])
    (f/'mutable.zip').write_bytes((f/'good.zip').read_bytes())
    for name in ('mutable.zip','good.zip','unicode.zip','traversal.zip','alias.zip','ancestor.zip','crc.zip','truncated.zip'):
        (f/(name+'.identity')).write_text(f'{digest(f/name)} {(f/name).stat().st_size}\n')
    with tarfile.open(f/'plain.tar','w',format=tarfile.USTAR_FORMAT) as t:
        entry=tarfile.TarInfo('wrapper/');entry.type=tarfile.DIRTYPE;t.addfile(entry)
        entry=tarfile.TarInfo('wrapper/data.bin');entry.size=len(payload);t.addfile(entry,io.BytesIO(payload))
        entry=tarfile.TarInfo('wrapper/empty.txt');entry.size=0;t.addfile(entry,io.BytesIO(b''))
    with tarfile.open(f/'link.tar','w',format=tarfile.USTAR_FORMAT) as t:
        entry=tarfile.TarInfo('unsafe');entry.type=tarfile.SYMTYPE;entry.linkname='../escape';t.addfile(entry)
    (f/'link.tar.identity').write_text(f'{digest(f/"link.tar")} {(f/"link.tar").stat().st_size}\n')
    return f

def verify_manifest(path, archive, rows, selection):
    data=path.read_bytes();off=0
    def take(n):
        nonlocal off
        if n>len(data)-off:raise AssertionError('truncated owning manifest')
        result=data[off:off+n];off+=n;return result
    def u64():return int.from_bytes(take(8),'little')
    assert take(8)==b'MBARMAN1'
    assert take(32).hex()==digest(archive)
    assert take(32)!=bytes(32)
    assert take(32)==bytes([0x51])*32
    assert u64()==len(selection)
    for i in selection:
        r=rows[i]
        assert u64()==i and u64()==r['size']
        assert take(32).hex()==r['sha256']
        assert take(u64()).decode()==f'member-{i:08d}.bin'
        assert take(u64()).decode()==r['name']
        assert take(u64()).decode()==r['name'].replace('\\','/').rstrip('/')
    assert off==len(data)

def main():
    p = argparse.ArgumentParser();p.add_argument('--output', type=pathlib.Path, required=True)
    p.add_argument('--phase', choices=['red', 'green'], required=True)
    p.add_argument('--native',action='store_true')
    a = p.parse_args();out=a.output.resolve();out.mkdir(parents=True, exist_ok=False)
    if a.native:return native(out)
    deps=ROOT/'.superpowers/native-dependency-spike'
    fresh=deps/'native-libarchive-lzma-experiment-1'
    crypto=ROOT/'outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/host-a4768e3a50d32394616f27f8'
    mbed=crypto/'sources/mbedtls/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5'
    libs=[fresh/'host/libarchive-build/.libs/libarchive.a',fresh/'host/xz-build/src/liblzma/.libs/liblzma.a',deps/'native-zip-experiment-1/host/zlib-build/libz.a',crypto/'libmbedcrypto.a']
    argv=['/usr/bin/clang','-std=c11','-O2','-Wall','-Wextra','-Wpedantic','-Werror',
          '-DMB_ARCHIVE_BUILTIN_XZ=1','-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"','-I'+str(mbed/'include'),'-I'+str(crypto/'config'),
          '-I'+str(deps/'libarchive-release/libarchive'),HERE/'intake.c',HERE/'private/posix_compat.c',HERE/'groups.c',HERE/'tests/corpus_test.c',*libs,'-o',out/'corpus_test']
    receipt={'phase':a.phase,'sources':{str(x):digest(x) for x in [HERE/'private/posix_compat.c',HERE/'private/posix_compat.h',HERE/'intake.c',HERE/'intake.h',HERE/'groups.c',HERE/'groups.h',HERE/'tests/corpus_test.c',pathlib.Path(__file__)]},'libraries':{str(x):digest(x) for x in libs},'commands':[]}
    receipt['commands'].append(run(argv,out/'compile'))
    if receipt['commands'][-1]['exit']:
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2));return 2
    local=fixtures(out) if a.phase=='green' else None
    cases=[CORPUS/name for name in NAMES]+([local/'plain.tar',local/'unicode.zip'] if local else [])
    for path in cases:
        name=path.name;fixture=out/name;fixture.mkdir();q=fixture/'quarantine';q.mkdir(mode=0o700)
        rows,selection=oracle(path,fixture/'oracle.txt')
        receipt['commands'].append(run([out/'corpus_test',path,fixture/'oracle.txt',q],fixture/'run'))
        receipt.setdefault('corpus',[]).append(dict(name=name,bytes=path.stat().st_size,sha256=digest(path),rows=len(rows),selected=selection))
        if a.phase=='green' and receipt['commands'][-1]['exit']==0:
            for i in selection:
                dest=q/f'member-{i:08d}.bin'
                if dest.stat().st_size!=rows[i]['size'] or digest(dest)!=rows[i]['sha256']:
                    raise AssertionError('materialized bytes do not match independent archive oracle')
            verify_manifest(q/'manifest-v1.bin',path,rows,selection)
    if local:
        control_argv=argv[:]
        control_argv.remove(HERE/'groups.c')
        control_argv[control_argv.index(HERE/'tests/corpus_test.c')]=HERE/'tests/controls_test.c'
        control_argv[-1]=out/'controls_test'
        receipt['commands'].append(run(control_argv,out/'compile-controls'))
        if not receipt['commands'][-1]['exit']:
            (out/'control-quarantine').mkdir(mode=0o700)
            receipt['commands'].append(run([out/'controls_test',local,out/'control-quarantine'],out/'controls'))
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2))
    failures=sum(c['exit']!=0 for c in receipt['commands'])
    print(json.dumps({'phase':a.phase,'cases':len(cases),'failed_commands':failures}))
    return 0 if (a.phase=='red' and failures==len(NAMES)) or (a.phase=='green' and failures==0) else 1

if __name__=='__main__':
    raise SystemExit(main())
