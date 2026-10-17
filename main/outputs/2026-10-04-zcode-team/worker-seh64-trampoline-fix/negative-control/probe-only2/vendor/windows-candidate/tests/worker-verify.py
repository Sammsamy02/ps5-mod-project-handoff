#!/usr/bin/env python3
"""Build an owned no-CRT Windows worker DLL; execute only through genuine services."""
from pathlib import Path
import argparse,hashlib,json,os,re,struct,subprocess
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
SPIKE=ROOT/'.superpowers/native-dependency-spike/windows-worker-contract-review-1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--fixture-only',action='store_true');ap.add_argument('--red-no-admission',action='store_true');ap.add_argument('--close-early',action='store_true');a=ap.parse_args()
 out=SPIKE/a.name;out.mkdir(exist_ok=False)
 llvm=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
 lld=ROOT/'.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link'
 source=[HERE/n for n in ['worker-fixture.c','worker-state.h','worker-kernel.def','worker-verify.py']]+[SPIKE/'CONTRACT.md',SPIKE/'retained-imports.json']
 services=HERE.parents[1]/'windows-services';execution=HERE.parents[1]/'windows-execution';image=HERE.parents[1]/'windows-image'
 units=[services/n for n in ['mb_win_workers.c','mb_win_workers_abi.c','mb_win_threads.c','mb_win_tls_bridge.c','mb_win_heap_bridge.c','mb_win_heap.c','mb_win_crt.c']]+[execution/'pe_module.c',image/'pe_image.c',HERE/'worker-runner.c',HERE.parent/'call_exports.c']
 if not a.fixture_only:
  source+=units+[services/n for n in ['mb_win_workers.h','mb_win_workers_abi.h','mb_win_threads.h','mb_win_tls_bridge.h','mb_win_heap_bridge.h','mb_win_heap.h','mb_win_crt.h','mb_win_crt_bind.h']]+[execution/'pe_module.h',image/'pe_image.h',HERE.parent/'call_exports.h']
 hashes={str(p.relative_to(ROOT)):sha(p)for p in source}
 receipt={'status':'IN_PROGRESS','source_hashes':hashes,'commands':[],'owned_pe_callback_executed':False,'owned_pe_export_executed':False,'console_contact':False,'target_execution':False,'mode':'fixture_only'if a.fixture_only else 'red_no_admission'if a.red_no_admission else 'close_early'if a.close_early else 'actual_worker'}
 def save(): (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 def run(label,argv,expected=0):
  r=subprocess.run(list(map(str,argv)),cwd=ROOT,capture_output=True,text=True,timeout=60)
  (out/(label+'.stdout')).write_text(r.stdout);(out/(label+'.stderr')).write_text(r.stderr);receipt['commands'].append({'label':label,'argv':list(map(str,argv)),'exit':r.returncode});save()
  if r.returncode!=expected:raise RuntimeError(label+' unexpected exit '+str(r.returncode)+': '+r.stderr[-2000:])
  return r.stdout
 try:
  flags=['-std=c11','-Wall','-Wextra','-Werror','-O2','-ffreestanding','-fno-builtin']
  run('import-library',[lld,'/lib','/def:'+str(HERE/'worker-kernel.def'),'/machine:x64','/out:'+str(out/'kernel-worker.lib')])
  run('compile-worker-pe',[llvm/'clang','--target=x86_64-pc-windows-msvc',*flags,*(['-DMB_WORKER_CLOSE_EARLY=1']if a.close_early else []),'-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-c',HERE/'worker-fixture.c','-o',out/'worker.obj'])
  run('link-worker-pe',[lld,'/dll','/noentry','/nodefaultlib','/align:16384','/dynamicbase','/nxcompat','/timestamp:0','/out:'+str(out/'worker.dll'),out/'worker.obj',out/'kernel-worker.lib'])
  metadata=run('inspect-worker-pe',[llvm/'llvm-readobj','--file-headers','--sections','--coff-imports','--coff-exports','--coff-basereloc',out/'worker.dll'])
  sym=run('inspect-internal-worker',[llvm/'llvm-nm','--defined-only','--format=posix',out/'worker.obj'])
  names=re.findall(r'Symbol: ([^ ]+) ',metadata);expected={'CreateThread','WaitForSingleObject','GetExitCodeThread','CloseHandle','Sleep','GetLastError','SetLastError'}
  assert set(names)==expected and len(names)==7 and metadata.count('Name: KERNEL32.dll')==1
  exports=[re.search(r'Name: ([^\n]+)',v).group(1)for v in re.findall(r'Export \{(.*?)\n\}',metadata,re.S)]
  assert set(exports)=={'WorkerBegin','WorkerCollect'} and len(exports)==2
  assert re.search(r'^mb_worker_internal_start t ',sym,re.M)
  b=(out/'worker.dll').read_bytes();pe=struct.unpack_from('<I',b,60)[0];assert b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,pe+4)[0]==0x8664
  opt=pe+24;assert struct.unpack_from('<H',b,opt)[0]==0x20b and struct.unpack_from('<I',b,opt+16)[0]==0
  directory=lambda n:struct.unpack_from('<II',b,opt+112+n*8)
  assert directory(3)==(0,0) and directory(9)==(0,0) and all(directory(5))
  receipt['pe']={'path':str((out/'worker.dll').relative_to(ROOT)),'bytes':len(b),'sha256':sha(out/'worker.dll'),'imports':sorted(names),'exports':sorted(exports),'internal_start_nonexported':True,'entry_rva':0,'exception_directory':[0,0],'static_tls_directory':[0,0],'relocation_directory':list(directory(5))}
  if not a.fixture_only:
   run('compile-host-worker-runner',['clang','-arch','x86_64',*flags,'-pthread',*units,'-o',out/'worker-runner'])
   result=run('execute-worker-pe',['/usr/bin/arch','-x86_64',out/'worker-runner',out/'worker.dll','red-no-admission'if a.red_no_admission else 'close-early'if a.close_early else 'green'],1 if a.red_no_admission else 0)
   receipt['result']=json.loads(result);receipt['owned_pe_export_executed']=True
   if a.red_no_admission:
    assert receipt['result']['begin_result']==-2 and not receipt['result']['worker_started'];receipt['status']='EXPECTED_RED_NO_CREATION_ADMISSION'
   else:
    assert receipt['result']['worker_started'] and receipt['result']['worker_returned'] and receipt['result']['code_lease_retired'];receipt['owned_pe_callback_executed']=True;receipt['status']='OWNED_WINDOWS_WORKER_HOST_EXECUTION_PASSED'
  else:receipt['status']='OWNED_WINDOWS_WORKER_PE_COMPILED'
  assert all(sha(ROOT/p)==h for p,h in hashes.items())
  receipt['sources_stable']=True;receipt['artifacts']={str(p.relative_to(ROOT)):sha(p)for p in out.iterdir()if p.is_file()and p.name!='receipt.json'};save();print(json.dumps({'status':receipt['status'],'receipt':str(out/'receipt.json')},indent=2))
 except Exception as e:
  receipt['status']='FAILED';receipt['error']=str(e);save();raise
if __name__=='__main__':main()
