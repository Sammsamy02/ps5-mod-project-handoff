#!/usr/bin/env python3
"""Build only an owned SDK payload ELF candidate; never dispatch or rename it as PRX."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import uuid
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
EXEC=HERE.parent/'windows-execution'
SERV=HERE.parent/'windows-services'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def layout(ir):return next(x for x in ir.splitlines() if x.startswith('target datalayout ='))
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--host-only',action='store_true')
 p.add_argument('--entry',action='store_true',help='Owned DllMain consumer/helper profile; default retains no-entry fixtures.')
 p.add_argument('--clock',action='store_true',help='With --entry, exercise real monotonic QPC/QPF in owned DLL attach, exports, and detach.')
 p.add_argument('--workers',action='store_true',help='With --entry, execute an internal DLL worker through native thread services.')
 p.add_argument('--sync',action='store_true',help='With --workers, exercise critical sections, SRW/condition variables and address waits through actual PE imports.')
 p.add_argument('--init-once',action='store_true',help='With --sync, exercise one-time initialization (InitOnceBeginInitialize/InitOnceComplete) through actual PE imports.')
 p.add_argument('--mutex',action='store_true',help='With --sync, exercise the KERNEL32 mutex family (create/open/release/waits) through actual PE imports.')
 p.add_argument('--reject-worker-start',action='store_true',help='Host control with --sync: internal worker receives an invalid parameter and must retire without hanging readiness.')
 p.add_argument('--reject-attach',action='store_true',help='With --entry, compile consumer DllMain to return FALSE after initialization.')
 p.add_argument('--report-path',default='/tmp/modbridge-owned-dll-candidate.jsonl',help='Target-side profile file path; empty disables file output. Build never opens it.')
 p.add_argument('--no-stdout',action='store_true',help='Disable target stdout result delivery.')
 p.add_argument('--output',type=Path,default=ROOT/'.superpowers/native-dependency-spike/owned-dll-sdk-candidate-1')
 p.add_argument('--sdk',type=Path,default=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk')
 p.add_argument('--llvm-bin',type=Path,default=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin')
 p.add_argument('--lld-link',type=Path,default=ROOT/'.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link')
 a=p.parse_args();out=a.output.resolve()/uuid.uuid4().hex;out.mkdir(parents=True)
 if a.clock and not a.entry:p.error('--clock requires --entry')
 if a.workers and not a.entry:p.error('--workers requires --entry')
 if a.sync and not a.workers:p.error('--sync requires --workers')
 if a.init_once and not a.sync:p.error('--init-once requires --sync')
 if a.mutex and not a.sync:p.error('--mutex requires --sync')
 if a.reject_worker_start and (not a.sync or not a.host_only or a.reject_attach):p.error('--reject-worker-start requires --sync --host-only and successful attach')
 if a.reject_attach and not a.entry:p.error('--reject-attach requires --entry')
 if not a.report_path.isascii() or '\0' in a.report_path or len(a.report_path)>512:p.error('report path must be at most 512 ASCII characters with no NUL')
 if not a.report_path and a.no_stdout:p.error('at least one result destination must be configured')
 profile=out/'owned_profile.h';profile.write_text('#define MB_OWNED_REPORT_PATH '+json.dumps(a.report_path)+'\n#define MB_OWNED_BUILD_ID '+json.dumps(out.name)+'\n#define MB_OWNED_STDOUT '+str(int(not a.no_stdout))+'\n#define MB_OWNED_ENTRY '+str(int(a.entry))+'\n#define MB_OWNED_CLOCK '+str(int(a.clock))+'\n#define MB_OWNED_WORKERS '+str(int(a.workers))+'\n#define MB_OWNED_SYNC '+str(int(a.sync))+'\n#define MB_OWNED_INIT_ONCE '+str(int(a.init_once))+'\n#define MB_OWNED_MUTEX '+str(int(a.mutex))+'\n#define MB_OWNED_REJECT_WORKER_START '+str(int(a.reject_worker_start))+'\n#define MB_OWNED_REJECT_ATTACH '+str(int(a.reject_attach))+'\n')
 native_c=[EXEC/'pe_module.c',EXEC/'pe_dependencies.c',EXEC/'pe_loader.c',EXEC.parent/'windows-image/pe_image.c',SERV/'mb_win_heap.c',SERV/'mb_win_threads.c',HERE/'owned_runtime.c',HERE/'result_report.c',HERE/'result_sink.c']
 abi_c=[SERV/'mb_win_crt.c',SERV/'mb_win_heap_bridge.c',SERV/'mb_win_tls_bridge.c',SERV/'mb_win_loader_bridge.c',HERE/'call_exports.c']
 if a.entry:abi_c.append(SERV/'mb_win_entry.c')
 if a.clock:
  native_c.append(SERV/'mb_win_clock_native.c');abi_c.append(SERV/'mb_win_clock.c')
 if a.workers:
  native_c.append(SERV/'mb_win_workers.c');abi_c += [SERV/'mb_win_workers_abi.c',HERE/'call_workers.c']
 if a.sync:
  native_c += [SERV/('mb_win_'+name+'.c') for name in ['critical','address_wait','srw']]
  abi_c += [SERV/('mb_win_'+name+'_abi.c') for name in ['critical','address_wait','srw']]
 if a.init_once:
  native_c.append(SERV/'mb_win_init_once.c');abi_c.append(SERV/'mb_win_init_once_abi.c')
 if a.mutex:
  native_c.append(SERV/'mb_win_mutex.c');abi_c.append(SERV/'mb_win_mutex_abi.c')
 sources=[*native_c,*abi_c,HERE/'owned_runtime.h',HERE/'call_exports.h',HERE/'sdk_driver.c',HERE/'test_owned_runtime.c',Path(__file__).resolve(),
  EXEC/'pe_ps5_memory.c',EXEC/'pe_ps5_memory.h',EXEC/'pe_loader.h',EXEC/'pe_dependencies.h',EXEC/'pe_module.h',EXEC.parent/'windows-image/pe_image.h']
 sources += [HERE/x for x in ['README.md','result_report.h','result_sink.h','sdk_driver.h','test_result_report.c','test_result_sink.c','test_result_close.c','test_sdk_driver.c']]
 sources += [EXEC/'tests/ps5_memory_backend.h',HERE/'fixtures/clock_state.h',HERE/'fixtures/kernel_clock.def']
 if a.clock:sources += [SERV/'mb_win_clock.h',SERV/'tests/clock_layout.c']
 if a.workers:sources += [SERV/'mb_win_workers.h',SERV/'mb_win_workers_abi.h',HERE/'call_workers.h',HERE/'test_worker_candidate_failures.c',HERE/'tests/worker-fixture.c',HERE/'tests/worker-state.h',HERE/'tests/worker-kernel.def']
 if a.sync:
  sources += [SERV/('mb_win_'+name+suffix) for name in ['critical','address_wait','srw'] for suffix in ['.h','_abi.h']]
  sources += [HERE/'tests'/name for name in ['worker-sync.h','worker-sync-kernel.def','worker-sync-address.def']]
 if a.init_once:sources += [SERV/'mb_win_init_once.h',SERV/'mb_win_init_once_abi.h',HERE/'tests/worker-initonce.h',HERE/'tests/worker-initonce-kernel.def']
 if a.mutex:sources += [SERV/'mb_win_mutex.h',SERV/'mb_win_mutex_abi.h',HERE/'tests/worker-mutex.h',HERE/'tests/worker-mutex-kernel.def']
 sources += [HERE/'test_entry_candidate.c',HERE/'fixtures/entry_state.h',HERE/'fixtures/entry_consumer.c',HERE/'fixtures/entry_helper.c',SERV/'mb_win_entry.h']
 sources += [SERV/x for x in ['mb_win_crt.h','mb_win_crt_bind.h','mb_win_heap.h','mb_win_threads.h','mb_win_heap_bridge.h','mb_win_tls_bridge.h','mb_win_loader_bridge.h','tests/loader_bridge_layout.c']]
 sources += [EXEC/'tests'/x for x in ['loader_helper.c','loader_consumer.c','loader_helper.def','kernel_loader.def','crt_heap.def','kernel_tls.def']]
 receipt={'status':'IN_PROGRESS','commands':[],'source_files':{str(x.relative_to(ROOT)):sha(x) for x in sources},
  'console_contact':False,'target_execution':False,'downloaded_dll_executed':False,'prx_created':False,
  'source_scope':'Freshly compiled owned fixture bytes embedded and acquired lazily; independent previous host fixture proves disk intake'}
 receipt['result_profile']={'path':a.report_path,'stdout':not a.no_stdout,'build_id':out.name,'header_sha256':sha(profile),'target_path_opened_by_build':False}
 receipt['clock_profile']={'enabled':a.clock,'provider':'clock_gettime(CLOCK_MONOTONIC)' if a.clock else None,'frequency_scale':1000000000 if a.clock else None}
 receipt['entry_profile']={'enabled':a.entry,'consumer_rejects_attach':a.reject_attach,'entry_adapter':'mb_win_call_entry' if a.entry else None}
 receipt['worker_profile']={'enabled':a.workers,'started_from':'active PE export only' if a.workers else None,'asynchronous_dllmain_supported':False}
 receipt['synchronization_profile']={'enabled':a.sync,'critical_section_bytes':40 if a.sync else None,'srw_condition_bytes':8 if a.sync else None,'actual_pe_imports':21 if a.sync else 0,'target_execution':False}
 receipt['init_once_profile']={'enabled':a.init_once,'init_once_bytes':8 if a.init_once else None,'actual_pe_imports':2 if a.init_once else 0,'target_execution':False}
 receipt['mutex_profile']={'enabled':a.mutex,'handle_bytes':8 if a.mutex else None,'actual_pe_imports':5 if a.mutex else 0,'target_execution':False}
 receipt['worker_parameter_control']={'enabled':a.reject_worker_start,'host_only':True,'bounded_readiness':True}
 def save():(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 def run(label,cmd,env=None):
  cmd=list(map(str,cmd));r=subprocess.run(cmd,cwd=ROOT,env=env,text=True,capture_output=True,timeout=60)
  (out/(label+'.stdout')).write_text(r.stdout);(out/(label+'.stderr')).write_text(r.stderr)
  receipt['commands'].append({'label':label,'argv':cmd,'exit':r.returncode});save()
  if r.returncode:raise RuntimeError(label+' failed: '+str(out/(label+'.stderr')))
  return r.stdout
 try:
  flags=['-std=c11','-Wall','-Wextra','-Werror','-O1','-ffreestanding','-fno-builtin']
  for name,definition in [('loader','kernel_loader.def'),('heap','crt_heap.def'),('tls','kernel_tls.def')]:
   run(name+'-lib',[a.lld_link,'/lib','/def:'+str(EXEC/'tests'/definition),'/machine:x64','/out:'+str(out/(name+'.lib'))])
  if a.clock:run('clock-lib',[a.lld_link,'/lib','/def:'+str(HERE/'fixtures/kernel_clock.def'),'/machine:x64','/out:'+str(out/'clock.lib')])
  if a.workers:run('workers-lib',[a.lld_link,'/lib','/def:'+str(HERE/'tests/worker-kernel.def'),'/machine:x64','/out:'+str(out/'workers.lib')])
  if a.sync:
   for name in ['kernel','address']:
    run('sync-'+name+'-lib',[a.lld_link,'/lib','/def:'+str(HERE/'tests'/('worker-sync-'+name+'.def')),'/machine:x64','/out:'+str(out/('sync-'+name+'.lib'))])
  if a.init_once:run('initonce-lib',[a.lld_link,'/lib','/def:'+str(HERE/'tests/worker-initonce-kernel.def'),'/machine:x64','/out:'+str(out/'initonce.lib')])
  if a.mutex:run('mutex-lib',[a.lld_link,'/lib','/def:'+str(HERE/'tests/worker-mutex-kernel.def'),'/machine:x64','/out:'+str(out/'mutex.lib')])
  arrays=['#ifndef MB_OWNED_FIXTURES_H','#define MB_OWNED_FIXTURES_H']
  receipt['fixture_entry_rva']={}
  for kind in ['helper','consumer']:
   fixture=HERE/'fixtures'/('entry_'+kind+'.c') if a.entry else EXEC/'tests'/('loader_'+kind+'.c')
   reject=['-DMB_FIXTURE_REJECT_ATTACH=1'] if a.reject_attach and kind=='consumer' else []
   run(kind+'-compile',[a.llvm_bin/'clang','--target=x86_64-pc-windows-msvc',*flags,*reject,'-DMB_OWNED_CLOCK='+str(int(a.clock)),'-DMB_OWNED_WORKERS='+str(int(a.workers)),'-DMB_OWNED_SYNC='+str(int(a.sync)),'-DMB_OWNED_INIT_ONCE='+str(int(a.init_once)),'-DMB_OWNED_MUTEX='+str(int(a.mutex)),'-DMB_OWNED_REJECT_WORKER_START='+str(int(a.reject_worker_start)),'-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-c',fixture,'-o',out/(kind+'.obj')])
   extra=['/def:'+str(EXEC/'tests/loader_helper.def'),out/'heap.lib',out/'tls.lib'] if kind=='helper' else [out/'loader.lib']
   if a.entry:extra += [out/'loader.lib'] if kind=='helper' else [out/'heap.lib',out/'tls.lib']
   if a.clock:extra += [out/'clock.lib']
   if a.workers and kind=='consumer':extra += [out/'workers.lib']
   if a.sync and kind=='consumer':extra += [out/'sync-kernel.lib',out/'sync-address.lib']
   if a.init_once and kind=='consumer':extra += [out/'initonce.lib']
   if a.mutex and kind=='consumer':extra += [out/'mutex.lib']
   dll=out/('owned_'+kind+'.dll')
   run(kind+'-link',[a.lld_link,'/dll','/entry:DllMain' if a.entry else '/noentry','/nodefaultlib','/align:16384','/dynamicbase','/nxcompat','/timestamp:0','/out:'+str(dll),out/(kind+'.obj'),*extra])
   run(kind+'-metadata',[a.llvm_bin/'llvm-readobj','--file-headers','--coff-imports','--coff-exports','--coff-basereloc',dll])
   data=dll.read_bytes();opt=struct.unpack_from('<I',data,0x3c)[0]+24;entry=struct.unpack_from('<I',data,opt+16)[0]
   if bool(entry)!=a.entry:raise RuntimeError('Unexpected PE entry admission profile')
   for directory in [3,7,8,9,10,11,13,14,15]:
    if any(struct.unpack_from('<II',data,opt+112+directory*8)):raise RuntimeError('Unsupported directory in owned entry fixture')
   receipt['fixture_entry_rva'][kind]=entry
   arrays.append('static const unsigned char mb_owned_'+kind+'_bytes[]={')
   arrays += [','.join(str(x) for x in data[i:i+24])+',' for i in range(0,len(data),24)];arrays.append('};')
  arrays.append('#endif');header=out/'owned_fixtures.h';header.write_text('\n'.join(arrays)+'\n')
  receipt['fixture_sha256']={k:sha(out/('owned_'+k+'.dll')) for k in ['consumer','helper']};receipt['embedded_header_sha256']=sha(header)
  if not a.entry:
   run('host-runtime-fault-calls',['clang','-arch','x86_64',*flags,'-pthread','-I',out,'-Dmb_win_threads_leave=mb_test_threads_leave','-Dpthread_mutex_init=mb_test_mutex_init','-c',HERE/'owned_runtime.c','-o',out/'host-runtime.o'])
   run('host-candidate-compile',['clang','-arch','x86_64',*flags,'-pthread','-I',out,*[x for x in native_c if x.name!='owned_runtime.c'],out/'host-runtime.o',*abi_c,HERE/'test_owned_runtime.c','-o',out/'host-candidate'])
   receipt['host_normal']=run('host-owned-run',['/usr/bin/arch','-x86_64',out/'host-candidate']).strip()
   receipt['host_cleanup_retry']=run('host-owned-cleanup-retry',['/usr/bin/arch','-x86_64',out/'host-candidate','fail-release']).strip()
   receipt['host_leave_retry']=run('host-owned-leave-retry',['/usr/bin/arch','-x86_64',out/'host-candidate','leave-failure']).strip()
   receipt['host_init_failure']=run('host-owned-init-failure',['/usr/bin/arch','-x86_64',out/'host-candidate','mutex-failure']).strip()
   for name in ['report','sink']:
    extra=[HERE/'result_sink.c'] if name=='sink' else []
    run('host-result-'+name+'-compile',['clang','-arch','x86_64',*flags,HERE/'result_report.c',*extra,HERE/('test_result_'+name+'.c'),'-o',out/('test-result-'+name)])
    receipt['host_result_'+name]=run('host-result-'+name,['/usr/bin/arch','-x86_64',out/('test-result-'+name)]).strip()
   run('host-close-fault-compile',['clang','-arch','x86_64',*flags,'-Dclose=mb_test_report_close','-c',HERE/'result_sink.c','-o',out/'host-sink-close.o'])
   run('host-result-close-compile',['clang','-arch','x86_64',*flags,HERE/'result_report.c',out/'host-sink-close.o',HERE/'test_result_close.c','-o',out/'test-result-close'])
   receipt['host_result_close']=run('host-result-close',['/usr/bin/arch','-x86_64',out/'test-result-close']).strip()
  run('host-sdk-driver-compile',['clang','-arch','x86_64',*flags,'-I',out,'-DMB_OWNED_DRIVER_TEST','-Dsleep=mb_test_sleep','-c',HERE/'sdk_driver.c','-o',out/'host-sdk-driver.o'])
  host_native=list(native_c)
  if a.workers:
   run('host-runtime-worker-fault-calls',['clang','-arch','x86_64',*flags,'-pthread','-I',out,'-Dmb_win_workers_leave_call=mb_test_worker_leave','-c',HERE/'owned_runtime.c','-o',out/'host-worker-runtime.o'])
   host_native=[x for x in native_c if x.name!='owned_runtime.c']+[out/'host-worker-runtime.o']
  run('host-sdk-controls-compile',['clang','-arch','x86_64',*flags,'-I',out,'-DMB_PE_PS5_MEMORY_TEST','-pthread',*host_native,*abi_c,EXEC/'pe_ps5_memory.c',out/'host-sdk-driver.o',HERE/('test_entry_candidate.c' if a.entry else 'test_sdk_driver.c'),'-o',out/'test-sdk-driver'])
  receipt['host_result_records']={}
  for mode in (['reject','reject-release'] if a.reject_attach else ['normal','release'] if a.entry else ['normal','deny','release']):
   path=out/('host-'+mode+'.jsonl')
   receipt['host_driver_'+mode]=run('host-driver-'+mode,['/usr/bin/arch','-x86_64',out/'test-sdk-driver',mode,path]).strip()
   records=[json.loads(line) for line in path.read_text().splitlines()]
   if [r['phase'] for r in records]!=[1,2,3,*([4] if 'release' in mode else []),5]:raise RuntimeError('Missing driver result transition')
   if any(r['marker']!='modbridge.owned-dll' or r['schema']!=1 or r['build_id']!=out.name or r['sequence']!=i for i,r in enumerate(records)):raise RuntimeError('Result schema/sequence mismatch')
   final=records[-1]
   if final['cleanup']!={'pending':0,'retired':1}:raise RuntimeError('Final cleanup not retired')
   if mode=='deny':
    if final['execution']['dll_calls_started']!=0 or final['memory']['first_failure']['native_result']!=-2147352563 or final['memory']['first_failure']['operation']!=2:raise RuntimeError('Native denial not preserved')
   elif mode.startswith('reject'):
    if final['execution']['dll_calls_started'] or final['first_failure']['stage']!=5 or final['first_failure']['module_cause']!=13:raise RuntimeError('Rejected entry cause or zero export calls not preserved')
   elif final['results']!=[86,93] or final['execution']['dll_calls_returned']!=(1 if a.reject_worker_start else 3):raise RuntimeError('Actual DLL results absent')
   if a.reject_worker_start and (final['execution']['completed'] or final['first_failure']['stage']!=24 or final['first_failure']['native_status']!=0xffffffd8):raise RuntimeError('Invalid worker parameter was not bounded or exact first failure changed')
   if 'release' in mode and (records[-2]['first_failure']!=final['first_failure'] or final['memory']['first_failure']['operation']!=3):raise RuntimeError('Original cleanup failure changed')
   if a.entry:
    expected_calls=[1,0 if a.reject_attach else 1]
    if final['entry']['attach_calls']!=expected_calls or final['entry']['detach_calls']!=expected_calls or final['entry']['contract_errors'] or final['entry']['detach_result']!=[0,0]:raise RuntimeError('DllMain lifecycle evidence mismatch')
    if final['entry']['attach_result']!=([0,0] if a.reject_attach else [7,7]):raise RuntimeError('DllMain BOOL was not preserved')
   if a.clock and final.get('clock')!={'enabled':1,'session_begins':expected_calls,'session_ends':expected_calls}:raise RuntimeError('Clock sessions did not retire with module lifecycle')
   if a.workers:
    worker=final.get('worker',{});expected_worker_calls=0 if a.reject_attach else 1 if a.reject_worker_start else 2
    if worker.get('enabled')!=1 or worker.get('calls_started')!=expected_worker_calls or worker.get('calls_returned')!=expected_worker_calls:raise RuntimeError('Owned worker export calls did not complete')
    if not a.reject_attach and not a.reject_worker_start and (worker.get('begin',[0,0,0,0])[2:]!=[258,259] or worker.get('result',[0])[0]!=0x81234567 or worker.get('result',[0,0,0,1])[3]!=0):raise RuntimeError('Actual DLL worker timeout/result proof absent')
   elif 'worker' in final:raise RuntimeError('Default result unexpectedly advertises worker')
   if a.sync and final.get('synchronization')!={'enabled':1}:raise RuntimeError('Synchronization profile missing')
   if not a.sync and 'synchronization' in final:raise RuntimeError('Default result unexpectedly advertises synchronization')
   if a.init_once and final.get('init_once')!={'enabled':1}:raise RuntimeError('InitOnce profile missing')
   if not a.init_once and 'init_once' in final:raise RuntimeError('Default result unexpectedly advertises init_once')
   if a.mutex and final.get('mutex')!={'enabled':1}:raise RuntimeError('Mutex profile missing')
   if not a.mutex and 'mutex' in final:raise RuntimeError('Default result unexpectedly advertises mutex')
   if not a.clock and 'clock' in final:raise RuntimeError('Default result unexpectedly advertises clock')
   receipt['host_result_records'][mode]={'path':str(path),'sha256':sha(path),'records':len(records),'final':final}
  if a.workers and not a.reject_attach and not a.reject_worker_start:
   run('host-worker-cleanup-fault-compile',['clang','-arch','x86_64',*flags,'-I',out,'-DMB_PE_PS5_MEMORY_TEST','-pthread',*host_native,*abi_c,EXEC/'pe_ps5_memory.c',out/'host-sdk-driver.o',HERE/'test_worker_candidate_failures.c','-o',out/'test-worker-failure'])
   receipt['host_worker_cleanup_failures']={}
   for mode in ['failure','failure-release']:
    path=out/('host-worker-'+mode+'.jsonl')
    output=run('host-worker-'+mode,['/usr/bin/arch','-x86_64',out/'test-worker-failure',mode,path]).strip()
    records=[json.loads(line) for line in path.read_text().splitlines()]
    if [r['phase'] for r in records]!=[1,2,3,*([4] if 'release' in mode else []),5]:raise RuntimeError('Missing worker cleanup failure transition')
    if any(r['build_id']!=out.name or r['sequence']!=i for i,r in enumerate(records)):raise RuntimeError('Worker cleanup failure identity mismatch')
    final=records[-1]
    if final['cleanup']!={'pending':0,'retired':1} or final['first_failure']['stage']!=25 or final['first_failure']['native_status']!=4:raise RuntimeError('Pending worker admission did not retire or first failure changed')
    if final['execution']['dll_calls_returned']!=1 or final['worker']['calls_returned']!=2 or final['worker']['result'][0]!=0x81234567:raise RuntimeError('DLL worker was repeated or not completed before pending admission')
    receipt['host_worker_cleanup_failures'][mode]={'path':str(path),'sha256':sha(path),'output':output,'final':final}
   receipt['host_owned_worker_pe_executed']=True
  if not a.host_only:
   sdk=a.sdk.resolve();llvm=a.llvm_bin.resolve();cc=sdk/'bin/prospero-clang';env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
   nflags=[*flags,'-O2','-fPIC','-march=x86-64','-I',out]
   bflags=[*nflags,'--target=x86_64-unknown-freebsd9','-fshort-wchar']
   probe=SERV/'tests/loader_bridge_layout.c'
   run('native-layout',[cc,*nflags,'-S','-emit-llvm',probe,'-o',out/'native-layout.ll'],env)
   run('bridge-layout',[cc,*bflags,'-S','-emit-llvm',probe,'-o',out/'bridge-layout.ll'],env)
   nir=(out/'native-layout.ll').read_text();bir=(out/'bridge-layout.ll').read_text()
   values=lambda s:re.findall(r'i64 (\d+)',next(x for x in s.splitlines() if x.startswith('@mb_loader_bridge_layout =')))
   if layout(nir)!=layout(bir) or values(nir)!=values(bir):raise RuntimeError('Native/bridge public ABI mismatch')
   receipt['structure_layout']=list(map(int,values(nir)))
   if a.clock:
    for prefix,probe_flags in [('native',nflags),('bridge',bflags)]:
     run(prefix+'-clock-layout',[cc,*probe_flags,'-S','-emit-llvm',SERV/'tests/clock_layout.c','-o',out/(prefix+'-clock-layout.ll')],env)
    cn=(out/'native-clock-layout.ll').read_text();cb=(out/'bridge-clock-layout.ll').read_text()
    clock_values=lambda s:re.findall(r'i64 (\d+)',next(x for x in s.splitlines() if x.startswith('@mb_win_clock_layout =')))
    if layout(cn)!=layout(cb) or clock_values(cn)!=clock_values(cb):raise RuntimeError('Clock native/bridge public ABI mismatch')
    receipt['clock_structure_layout']=list(map(int,clock_values(cn)))
   objects=[];deps=[]
   for source in [*native_c,EXEC/'pe_ps5_memory.c',HERE/'sdk_driver.c']:
    obj=out/(source.stem+'.o');dep=out/(source.stem+'.d');objects.append(obj);deps.append(dep)
    # owned_runtime.c contains the ms_abi combined_close_handle dispatch; the
    # default sie-ps5 target rejects ms_abi, compile with the FreeBSD9 frontend
    flags=bflags if source.name=='owned_runtime.c' else nflags
    run('native-'+source.stem,[cc,*flags,'-MD','-MF',dep,'-c',source,'-o',obj],env)
   expected={'mb_win_crt':14,'mb_win_heap_bridge':5,'mb_win_tls_bridge':6,'mb_win_loader_bridge':8,'call_exports':0,'mb_win_entry':0,'mb_win_clock':2,'mb_win_workers_abi':5,'call_workers':0,'mb_win_critical_abi':7,'mb_win_address_wait_abi':3,'mb_win_srw_abi':11,'mb_win_init_once_abi':2,'mb_win_mutex_abi':6}
   for source in abi_c:
    ir=out/(source.stem+'.ll');obj=out/(source.stem+'.o');dep=out/(source.stem+'.d');objects.append(obj);deps.append(dep)
    run('ir-'+source.stem,[cc,*bflags,'-MD','-MF',dep,'-S','-emit-llvm',source,'-o',ir],env)
    text=ir.read_text()
    if layout(text)!=layout(nir) or sum(x.startswith('define ') and 'win64cc' in x for x in text.splitlines())!=expected[source.stem]:raise RuntimeError('Windows ABI definitions changed: '+source.stem)
    if source.stem=='call_exports' and sum(bool(re.search(r'\bcall win64cc\b',x)) for x in text.splitlines())!=3:raise RuntimeError('Exact owned export call ABI missing')
    if source.stem=='mb_win_entry' and sum(bool(re.search(r'\b(?:tail )?call win64cc\b',x)) for x in text.splitlines())!=1:raise RuntimeError('DLL entry typed Windows call missing')
    if source.stem=='call_workers' and sum(bool(re.search(r'\b(?:tail )?call win64cc\b',x)) for x in text.splitlines())!=1:raise RuntimeError('Owned worker export typed Windows call missing')
    run('backend-'+source.stem,[cc,'-x','ir','-O2','-fPIC','-Werror','-Wno-override-module','-c',ir,'-o',obj],env)
   layout_py=HERE.parent/'native-host/archive/link_layout.py'
   spec=importlib.util.spec_from_file_location('candidate_link_layout',layout_py);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
   script=module.write_linker_script(sdk,out);artifact=out/'owned-dll-candidate.elf'
   run('sdk-elf-link',[cc,'-nostartfiles','-Wl,-T,'+script['caller_script'],'-Wl,-e,_start','-fno-lto','-fPIC','-pthread','-Wl,--no-undefined','-Wl,-Map,'+str(out/'link.map'),*objects,sdk/'target/lib/crt1.o','-o',artifact],env)
   data=artifact.read_bytes()
   if data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<HH',data,16)!=(3,62):raise RuntimeError('Expected SDK AMD64 ELF ET_DYN')
   receipt['dynamic_layout']=module.validate_dynamic_layout(artifact)
   undefined=run('elf-undefined',[llvm/'llvm-nm','-u',artifact])
   run('elf-headers',[llvm/'llvm-readelf','-h','-l','-d','-r',artifact])
   defined=run('elf-symbols',[llvm/'llvm-nm','--defined-only',artifact])
   run('elf-startup',[llvm/'llvm-objdump','--disassemble-symbols=_start,__crt_start,mb_owned_ps5_start,mb_owned_ps5_run,mb_owned_ps5_cleanup',artifact])
   names=sorted(set(line.split()[-1] for line in undefined.splitlines() if line.strip()));providers={}
   for library in ['libkernel_web.so','libSceLibcInternal.so','libSceNet.so']:
    symbols=run('provider-'+library,[llvm/'llvm-nm','-D','--defined-only',sdk/'target/lib'/library]);providers[library]=set(line.split()[-1] for line in symbols.splitlines() if line.strip())
   mappings={name:[lib for lib,syms in providers.items() if name in syms] for name in names}
   if not all(mappings.values()):raise RuntimeError('SDK does not declare every dynamic import')
   if a.clock and 'clock_gettime' not in names:raise RuntimeError('Real native monotonic clock import was not retained')
   if 'sceKernelMprotect' not in names:raise RuntimeError('Exact native range-protection import was not retained')
   receipt['import_providers']=mappings;receipt['artifact']={'path':str(artifact),'sha256':sha(artifact),'bytes':len(data),'startup':'Genuine SDK payload crt1 _start, not a PRX module entry','protection':'Direct native sceKernelMprotect exact-range syscall API; EXEC permission remains untested on target'}
   receipt['native_dependencies']={str(layout_py):sha(layout_py),str(cc):sha(cc),str(llvm/'clang'):sha(llvm/'clang')}
   for dep in deps:
    for word in dep.read_text().replace('\\\n',' ').split()[1:]:
     path=Path(word)
     if path.is_file():receipt['native_dependencies'][str(path)]=sha(path)
   for name in ['crt1.o','libc.a','libpthread.a','libkernel_web.so','libSceLibcInternal.so','libSceNet.so']:
    path=sdk/'target/lib'/name;receipt['native_dependencies'][str(path)]=sha(path)
   receipt['native_objects']={str(x.name):sha(x) for x in objects}
  for name,digest in receipt['source_files'].items():
   if sha(ROOT/name)!=digest:raise RuntimeError('Source changed: '+name)
  receipt['warnings']={c['label']:(out/(c['label']+'.stderr')).read_text() for c in receipt['commands'] if (out/(c['label']+'.stderr')).read_text()}
  receipt['status']='OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED' if not a.host_only else 'OWNED_CANDIDATE_HOST_CONTROLS_PASSED';save()
  print(json.dumps({'status':receipt['status'],'receipt':str(out/'receipt.json')}))
 except Exception as e:
  receipt.update(status='FAILED',error=str(e));save();raise
if __name__=='__main__':main()
