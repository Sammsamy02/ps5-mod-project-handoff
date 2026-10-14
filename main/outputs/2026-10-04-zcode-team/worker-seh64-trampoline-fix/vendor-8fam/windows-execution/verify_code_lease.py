#!/usr/bin/env python3
"""Focused PE code-address lease checks and direct PS5 native-C compilation.

No PE code is executed; callers retain and externally serialize the real module.
"""
import argparse,hashlib,json,os,struct,subprocess,uuid
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2];IMAGE=HERE.parent/'windows-image'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--expect-red',action='store_true');ap.add_argument('--sanitize',action='store_true');ap.add_argument('--host-only',action='store_true');ap.add_argument('--host-cc',default='clang');ap.add_argument('--output',type=Path,default=ROOT/'.superpowers/native-dependency-spike/windows-pe-code-lease-1');a=ap.parse_args()
 out=a.output.resolve()/uuid.uuid4().hex;out.mkdir(parents=True);receipt=dict(status='IN_PROGRESS',commands=[],pe_instructions_executed=False,target_executed=False,console_contact=False)
 def save():(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 def run(label,argv,env=None,expected_failure=False):
  p=subprocess.run(list(map(str,argv)),cwd=ROOT,env=env,text=True,capture_output=True);(out/(label+'.stdout')).write_text(p.stdout);(out/(label+'.stderr')).write_text(p.stderr);receipt['commands'].append(dict(label=label,argv=list(map(str,argv)),exit=p.returncode));save()
  if expected_failure:
   if not p.returncode or 'mb_pe_module_acquire_code(&module,storage+0x1020,&worker,&error)==MB_PE_MODULE_OK' not in p.stderr:raise RuntimeError('Red did not fail at missing code lease')
  elif p.returncode:raise RuntimeError(label+' failed: '+p.stderr[-2000:])
  return p.stdout.strip()
 sources=[HERE/'pe_module.c',HERE/'pe_module.h',IMAGE/'pe_image.c',IMAGE/'pe_image.h',HERE/'tests/test_pe_module.c',HERE/'tests/test_pe_code_lease.c',Path(__file__).resolve()]
 receipt['sources']={str(p.relative_to(ROOT)):sha(p) for p in sources};save()
 try:
  flags=['-std=c11','-Wall','-Wextra','-Werror'];source=[IMAGE/'pe_image.c',HERE/'pe_module.c',HERE/'tests/test_pe_code_lease.c']
  run('host-compile',[a.host_cc,*flags,*source,'-o',out/'test-code-lease']);receipt['host_result']=run('host-test',[out/'test-code-lease'],expected_failure=a.expect_red)
  if a.expect_red:receipt['status']='EXPECTED_MISSING_CODE_LEASE'
  else:
   # Existing owner checks are the focused regression for shared export/lifetime semantics.
   run('module-regression-compile',[a.host_cc,*flags,*source[:2],HERE/'tests/test_pe_module.c','-o',out/'test-module']);receipt['module_regression_result']=run('module-regression',[out/'test-module'])
   if a.sanitize:
    run('sanitized-compile',[a.host_cc,*flags,'-fsanitize=address,undefined','-fno-omit-frame-pointer',*source,'-o',out/'test-code-lease-sanitized']);receipt['sanitized_result']=run('sanitized-test',[out/'test-code-lease-sanitized'])
   if not a.host_only:
    sdk=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk';llvm=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin';compiler=sdk/'bin/prospero-clang';obj=out/'pe_module.o';env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
    run('target-compile',[compiler,*flags,'-O2','-fPIC','-MD','-MF',out/'pe_module.d','-c',HERE/'pe_module.c','-o',obj],env)
    raw=obj.read_bytes();assert raw[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<HH',raw,16)==(1,62)
    names={line.split()[0] for line in run('target-imports',[llvm/'llvm-nm','--undefined-only','--format=posix',obj]).splitlines() if line.strip()}
    assert names<={'mb_pe_inspect','mb_pe_prepare','mb_pe_visit_exports','memcmp','memcpy','memset'},names
    exports={line.split()[0]:line.split()[1] for line in run('target-exports',[llvm/'llvm-nm','--defined-only','--extern-only','--format=posix',obj]).splitlines() if line.strip()};assert exports['mb_pe_module_acquire_code']=='T'
    deps=(out/'pe_module.d').read_text().replace('\\\n',' ').split()[1:];receipt['native_dependencies']={p:sha(p)for p in deps if Path(p).is_file()};receipt['native_object']=dict(path=str(obj),bytes=len(raw),sha256=sha(obj),compiler_sha256=sha(compiler),imports=sorted(names),exports=exports)
   receipt['status']='CODE_LEASE_HOST_PASS' if a.host_only else 'CODE_LEASE_HOST_AND_PS5_COMPILE_PASS'
  for p,h in receipt['sources'].items():assert sha(ROOT/p)==h,p
  receipt['artifacts']={str(p):sha(p) for p in out.iterdir() if p.is_file() and p.name.startswith('test-')}
  save();print(json.dumps(dict(receipt=str(out/'receipt.json'),status=receipt['status']),indent=2))
 except Exception as exc:receipt['status']='FAILED';receipt['error']=str(exc);save();raise
if __name__=='__main__':main()
