#!/usr/bin/env python3
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,struct,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1]
label=sys.argv[1] if len(sys.argv)>1 else 'GREEN'
run=root/'tests/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-CACHE-'+label+'-'+uuid.uuid4().hex[:8]);run.mkdir(parents=True)
good=bytearray(0x10bb8);good[:7]=b'\x7fELF\x02\x01\x01';struct.pack_into('<HHI',good,16,0xfe10,62,1);struct.pack_into('<Q',good,32,64);struct.pack_into('<HHH',good,52,64,56,1)
struct.pack_into('<IIQQQQQQ',good,64,1,1,0x400,0,0,0x10000,0x10000,0x4000)
good[0x700:0x704]=bytes.fromhex('554889e5');good[0x8700:0x8705]=bytes.fromhex('4157415641')
cases={name:bytes(good) for name in ['good','short_reads','wrong_sha','zero_base','resolved_base','wrong_base','wrong_extent','open_error','read_error_first','read_error_middle','read_error_eof','no_progress','close_reuse','close_minus1_errno0','close_positive','deadline','read_exception','duplicate_pin']}
def patch(name,offset,fmt,value):
 data=bytearray(good);struct.pack_into(fmt,data,offset,value);cases[name]=bytes(data)
patch('file_offset_overflow',72,'<Q',0xffffffffffffffff)
patch('vaddr_overflow',80,'<Q',0xfffffffffffffff0)
patch('filesz_gt_memsz',96,'<Q',0x10001)
patch('zero_fill_prefix',96,'<Q',0x8300)
patch('phoff_overflow',32,'<Q',0xffffffffffffffff)
patch('bad_phentsize',54,'<H',64)
patch('too_many_phdrs',56,'<H',0xffff)
patch('bad_machine',18,'<H',183)
data=bytearray(good);data[0x700]^=1;cases['prefix_mismatch']=bytes(data)
data=bytearray(good);struct.pack_into('<H',data,56,2);data[120:176]=data[64:120];cases['overlapping_loads']=bytes(data)
cases['truncated']=bytes(good[:-1]);cases['extra']=bytes(good)+b'X'
rows=[]
for mode,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
 exe=run/mode;cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1',*extra,'-I',str(root/'units'),str(root/'tests/test_code_cache.cpp'),'-o',str(exe)]
 c=subprocess.run(cmd,capture_output=True,text=True);row={'mode':mode,'compile_argv':cmd,'compile_exit':c.returncode,'compile_stderr':c.stderr,'cases':[]};print(mode,'compile',c.returncode)
 if c.returncode:print(c.stderr)
 else:
  for name,data in cases.items():
   p=run/(name+'.elf');p.write_bytes(data);sha=hashlib.sha256(data).hexdigest();argv=[str(exe),str(p),sha,name]
   r=subprocess.run(argv,capture_output=True,text=True,timeout=20);row['cases'].append({'scenario':name,'argv':argv,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(r.stdout.strip(),r.stderr)
 rows.append(row)
receipt={'scope':'standalone production cache through injected public I/O boundary; independently Python-hashed ELF fixtures','missing_feature_stub':not (root/'units/CodeImageCache.hpp').exists(),'runs':rows,'case_count':len(cases)}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');(root/'reports'/('CACHE-'+label+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if all(r['compile_exit']==0 and len(r['cases'])==len(cases) and all(c['exit']==0 for c in r['cases']) for r in rows) else 1)
