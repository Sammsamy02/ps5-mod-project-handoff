#!/usr/bin/env python3
"""Runs the actual probe control flow; replaces only inline syscall transport.
No console or raw host syscalls. Each generated host TU records its source pin.
"""
import pathlib,re,subprocess,tempfile,json,hashlib,sys
root=pathlib.Path(__file__).resolve().parents[1]
src=root/'KernelReadProbe-v2.c'
source=src.read_text()
failed=0
results=[]
for case in ['check-carry-zero','dr-carry-zero','high-bit-success','errno-lstar','efer-carry-zero','tee-log']:
 with tempfile.TemporaryDirectory() as td:
  d=pathlib.Path(td); log=d/'probe.log'
  s=source
  start=s.index('static uint64_t kekcall('); end=s.index('/* Sub-op numbers:',start)
  check_cf=int(case=='check-carry-zero'); dr_cf=int(case=='dr-carry-zero'); efer_cf=int(case=='efer-carry-zero')
  lstar='14' if case=='errno-lstar' else 'UINT64_C(0xffffffff82a5b000)'
  stub=f'''static uint64_t kekcall(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g,int*cf)
{{
 static int t=0; (void)b;(void)c;(void)d;(void)e;(void)f;
 *cf=0;
 if(g==UINT64_C(0xffffffff00000027)){{*cf={check_cf};return 0;}}
 if(g==UINT64_C(0x100000027)){{memset((void*)(uintptr_t)a,0,48);*cf={dr_cf};return 0;}}
 if(a==UINT64_C(0xc0000082))return {lstar};
 if(a==UINT64_C(0xc0000080)){{*cf={efer_cf};return UINT64_C(0x11d01);}}
 return UINT64_C(0x100000)+ ++t;
}}
'''
  s=s[:start]+stub+s[end:]
  s=re.sub(r'#define PROBE_LOGPATH .*', '#define PROBE_LOGPATH '+json.dumps(str(log)),s)
  (d/'test.c').write_text(s)
  subprocess.run(['clang','-arch','x86_64','-Wall','-Wextra','-Wno-unused-parameter',str(d/'test.c'),'-o',str(d/'test')],check=True,capture_output=True)
  p=subprocess.run([str(d/'test')],capture_output=True,text=True,timeout=5)
  if case=='check-carry-zero': ok=p.returncode==2 and 'further_checks_skipped' in p.stdout and 'check=RDMSR_LSTAR' not in p.stdout
  elif case=='dr-carry-zero':ok=p.returncode!=0 and 'check=READ_DR verdict=FAIL' in p.stdout
  elif case=='efer-carry-zero':ok=p.returncode!=0 and 'check=RDMSR_EFER verdict=FAIL' in p.stdout and 'summary planned_gates=5' in p.stdout
  elif case=='errno-lstar':ok=p.returncode!=0 and 'check=RDMSR_LSTAR verdict=FAIL' in p.stdout
  elif case=='tee-log':ok=p.returncode==0 and log.read_text()==p.stdout
  else:ok=p.returncode==0 and 'canonical_kernel=yes verdict=PASS' in p.stdout
  print(f'{case}: {"PASS" if ok else "FAIL"} exit={p.returncode}')
  results.append({'case':case,'passed':ok,'exit':p.returncode,'stdout':p.stdout,'file_log':log.read_text() if log.exists() else None})
  failed+=not ok
(root/'tests/latest.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'cases':results},indent=2)+'\n')
print(f'cases={len(results)} failed={failed}')
sys.exit(bool(failed))
