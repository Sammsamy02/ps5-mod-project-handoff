#!/usr/bin/env python3
"""Byte-semantic host checks plus actual PS5-target C object compilation. No PE/native code execution."""
import argparse, hashlib, json, os, struct, subprocess, uuid
from pathlib import Path
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--host-cc', default='clang')
    ap.add_argument('--sdk', type=Path, default=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk')
    ap.add_argument('--llvm-bin', type=Path, default=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin')
    ap.add_argument('--lld-link', type=Path, default=ROOT/'.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link')
    ap.add_argument('--output', type=Path, default=ROOT/'.superpowers/native-dependency-spike/windows-image-preparation-1')
    ap.add_argument('--host-only', action='store_true')
    args=ap.parse_args(); out=args.output.resolve()/uuid.uuid4().hex;out.mkdir(parents=True)
    receipt={'status':'IN_PROGRESS','commands':[],'source_files':{},'pe_code_executed':False,'native_code_executed':False,'console_contact':False,'execution_ready':False}
    def save(): (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    def run(label,argv,env=None):
        argv=list(map(str,argv));result=subprocess.run(argv,cwd=ROOT,env=env,text=True,capture_output=True)
        (out/(label+'.stdout')).write_text(result.stdout);(out/(label+'.stderr')).write_text(result.stderr)
        receipt['commands'].append({'label':label,'argv':argv,'exit':result.returncode});save()
        if result.returncode: raise RuntimeError(f'{label}: {out/(label+".stderr")}')
        return result.stdout.strip()
    try:
        receipt['source_files']={str(p.relative_to(HERE)):sha(p) for p in sorted(HERE.rglob('*')) if p.is_file() and '__pycache__' not in p.parts}
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I',HERE]
        run('host-compile',[args.host_cc,*flags,HERE/'pe_image.c',HERE/'tests/test_pe_image.c','-o',out/'test-pe'])
        receipt['host_result']=run('host-test',[out/'test-pe'])
        run('provider-coff-compile',[args.llvm_bin/'clang','--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-stack-protector','-c',HERE/'tests/compiler_provider.c','-o',out/'provider.obj'])
        run('provider-pe-link',[args.lld_link,'/dll','/noentry','/nodefaultlib',out/'provider.obj','/def:'+str(HERE/'tests/compiler_provider.def'),'/out:'+str(out/'FixtureProvider.dll'),'/implib:'+str(out/'provider.lib')])
        run('fixture-coff-compile',[args.llvm_bin/'clang','--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-stack-protector','-c',HERE/'tests/compiler_image.c','-o',out/'fixture.obj'])
        run('fixture-pe-link',[args.lld_link,'/dll','/noentry','/nodefaultlib','/base:0x180000000',out/'fixture.obj',out/'provider.lib','/out:'+str(out/'fixture.dll')])
        run('compiler-test-compile',[args.host_cc,*flags,HERE/'pe_image.c',HERE/'tests/test_compiler_image.c','-o',out/'test-compiler-pe'])
        receipt['compiler_fixture_result']=run('compiler-fixture-test',[out/'test-compiler-pe',out/'fixture.dll'])
        run('compiler-fixture-independent-read',[args.llvm_bin/'llvm-readobj','--file-headers','--sections','--coff-imports','--coff-exports','--coff-basereloc',out/'fixture.dll'])
        receipt['compiler_fixture']={'sha256':sha(out/'fixture.dll'),'bytes':(out/'fixture.dll').stat().st_size,'execution':False}
        if not args.host_only:
            compiler=args.sdk.resolve()/'bin/prospero-clang';env=dict(os.environ,LLVM_CONFIG=str(args.llvm_bin.resolve()/'llvm-config'))
            obj=out/'pe_image.o'
            run('native-compile',[compiler,*flags,'-O2','-fPIC','-MD','-MF',out/'pe_image.d','-c',HERE/'pe_image.c','-o',obj],env)
            data=obj.read_bytes()
            if data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<HH',data,16)!=(1,62): raise RuntimeError('Expected AMD64 ET_REL only')
            text=run('native-undefined',[args.llvm_bin/'llvm-nm','--undefined-only','--format=posix',obj])
            names={x.split()[0] for x in text.splitlines() if len(x.split())>=2}
            if not names <= {'memcpy','memset'}: raise RuntimeError('Unexpected native imports: '+repr(names))
            receipt['native']={'object_sha256':sha(obj),'bytes':len(data),'imports':sorted(names),'compiler_sha256':sha(compiler)}
            receipt['native_dependencies']={}
            for name in (out/'pe_image.d').read_text().replace('\\\n',' ').split()[1:]:
                p=Path(name)
                if p.is_file(): receipt['native_dependencies'][str(p)]=sha(p)
        receipt['status']='BYTE_PREPARATION_AND_NATIVE_COMPILE_PASSED' if not args.host_only else 'BYTE_PREPARATION_PASSED'
        save();print(json.dumps({'receipt':str(out/'receipt.json'),'status':receipt['status']},indent=2))
    except Exception as e:
        receipt['status']='FAILED';receipt['error']=str(e);save();raise
if __name__=='__main__': main()
