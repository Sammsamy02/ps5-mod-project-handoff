#!/usr/bin/env python3
"""Read-only ELF-layout binding. Never emits an executable or crash sidecar."""
import argparse,contextlib,hashlib,io,json,os,re,struct,sys
from pathlib import Path
sys.dont_write_bytecode=True
HOME=Path(__file__).resolve().parent;ROOT=HOME.parents[3]
ARTIFACT_CAP=64*1024*1024;JSON_CAP=8*1024*1024;MAX64=(1<<64)-1
WRAPPER=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/install_app/make_fself.py'
SOURCE_PINS={
 WRAPPER:'d76a78f4bdf214d2f6a7508797dcede7d680c8b3d4ca0a4fe0a4d1d436cc9275',
 ROOT/'.superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/finalize_sdk.py':'fef9eaf5b2bb67bddc0a865c01f39d73b486d0641734628039499c1f1307bda2',
 ROOT/'outputs/2026-10-08-l26-package-A/package_prx_l19.py':'eb879e2cfea523a04093053d02e29169a186fde29a3522b2149073bd608e869c',
 ROOT/'outputs/2026-10-12-caps-prep-chatgpt/cap05/final-symbol-producer-audit/staged/Program.cs':'56a95adb56c056e3bf3e4709b9dc99697fea5f1f6ca84491f5bc3d02b88700ed',
 ROOT/'outputs/2026-10-12-caps-prep-chatgpt/cap05/final-symbol-producer-audit/staged/DynamicWriter.cs':'c331ec7a36806e9591589c6720a53bb553b908aacc5998b94424fe7d556e1241'}
CONVENTION='final ELF virtual address; runtime module-relative only after base convention validation'
TIER='FINAL-LINK-BOUNDED-FUNCTION'
def need(ok,why):
 if not ok:raise ValueError(why)
def sha(data):return hashlib.sha256(data).hexdigest()
def hexsha(v):return isinstance(v,str) and re.fullmatch('[0-9a-f]{64}',v) is not None
def read_stable(path,cap,refs,pin=None):
 p=Path(path).resolve()
 def ident(s):return [s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns]
 before=p.stat();need(p.is_file() and 0<before.st_size<=cap,'input size bound')
 with p.open('rb') as f:
  need(ident(os.fstat(f.fileno()))==ident(before),'input replaced before read');data=f.read(cap+1);ended=os.fstat(f.fileno())
 after=p.stat();need(len(data)==before.st_size<=cap and ident(before)==ident(ended)==ident(after),'input changed during read')
 digest=sha(data);need(pin is None or hexsha(pin) and digest==pin,'input SHA mismatch')
 ref={'path':str(p),'bytes':len(data),'sha256':digest,'identity':ident(after),'read_cap':cap}
 need(str(p) not in refs or refs[str(p)]==ref,'input changed after use');refs[str(p)]=ref;return data

def recheck(refs):
 for r in list(refs.values()):read_stable(r['path'],r['read_cap'],refs,r['sha256'])
def json_unique(data):
 def pairs(items):
  out={}
  for k,v in items:need(k not in out,'duplicate JSON key');out[k]=v
  return out
 return json.loads(data,object_pairs_hook=pairs)

def elf_loads(data):
 need(isinstance(data,bytes) and 64<=len(data)<=ARTIFACT_CAP,'ELF size bound')
 h=struct.unpack_from('<16sHHIQQQIHHHHHH',data)
 need(h[0][:7]==b'\x7fELF\x02\x01\x01' and h[1] in (2,0xfe00,0xfe10,0xfe18) and h[2:4]==(62,1),'unsupported ELF64 header')
 off=h[5];n=h[10];need(h[8:10]==(64,56) and 0<n<=256 and off>=64 and off+n*56<=len(data),'ELF program-header bound')
 loads=[];total=0
 for i in range(n):
  typ,flags,at,va,pa,size,mem,align=struct.unpack_from('<II6Q',data,off+i*56)
  need(at<=len(data) and size<=len(data)-at,'ELF file range bound');total+=size
  need(total<=ARTIFACT_CAP,'ELF total segment budget')
  if typ!=1:continue
  need(0<=flags<=7 and size<=mem and mem<=MAX64-va,'ELF load flags/size/overflow')
  need(align in (0,1) or align&(align-1)==0 and at%align==va%align,'ELF load alignment')
  need(all(va+mem<=x['vaddr'] or va>=x['vaddr']+x['memsz'] for x in loads),'overlapping ELF loads')
  need(all(at+size<=x['offset'] or at>=x['offset']+x['filesz'] for x in loads),'overlapping ELF file-backed loads')
  loads.append({'index':i,'vaddr':va,'filesz':size,'memsz':mem,'flags':flags,'offset':at,'sha256':sha(data[at:at+size])})
 need(loads and any(x['flags']&1 and x['filesz'] for x in loads),'missing executable load')
 return loads

def wrapper_bytes(sdk,refs):
 # Exact normal packaging invocation has input/output only, so all wrapper options are defaults.
 # Execute verified retained implementation in a namespace, without its __main__ CLI.
 raw=None
 for p,pin in SOURCE_PINS.items():
  data=read_stable(p,512*1024,refs,pin)
  if p==WRAPPER:raw=data
 ns={'__name__':'package_binding_verified_wrapper','__file__':str(WRAPPER)}
 exec(compile(raw,str(WRAPPER),'exec'),ns)
 obj=ns['ElfFile'](ignore_shdrs=True);obj.load(io.BytesIO(sdk));out=io.BytesIO()
 with contextlib.redirect_stdout(io.StringIO()):ns['SignedElfFile'](obj).save(out)
 data=out.getvalue();need(len(data)<=ARTIFACT_CAP+1024*1024,'wrapper output bound');return data

def layout(original,sdk,packaged,refs):
 ob=read_stable(original,ARTIFACT_CAP,refs);sb=read_stable(sdk,ARTIFACT_CAP,refs);pb=read_stable(packaged,ARTIFACT_CAP+1024*1024,refs)
 ol=elf_loads(ob);sl=elf_loads(sb)
 # File offsets may change; executable VA extent/permissions/bytes may not.
 def executable(ls):return [{k:v for k,v in x.items() if k not in ('offset','index')} for x in ls if x['flags']&1]
 need(executable(ol)==executable(sl),'executable ELF bytes/VMA/flags not preserved')
 need(wrapper_bytes(sb,refs)==pb,'exact retained wrapper reproduction mismatch')
 out={'schema':1,'label':'ELF_LAYOUT_BOUND_ONLY','producer_status':'INPUT_REQUIRED','symbols_admitted':0,'functions':[],
      'artifacts':{k:refs[str(Path(p).resolve())] for k,p in [('bare_original',original),('bare_sdk',sdk),('packaged',packaged)]},
      'executable_loads':executable(ol),'bare_to_sdk_executable_preserved':True,'sdk_to_packaged_exact_wrapper_reproduction':True,
      'packaging_profile':{'invocation':'make_fself.py <bare-sdk> <packaged>; no optional flags','options':'exact retained default options','source':'package_prx_l19.py:905-911'},
      'runtime_base_convention_proven':False,'capture_build_pairing':False,'crash_sidecar_emitted':False,'runtime_execution_proven':False,
      'producer_execution_authenticated':False,'current_B7_dependency':False,
      'limits':'Byte/layout binding only. Does not authenticate producer/build execution, SDK stamping semantics, runtime load bias, module-base convention, capture pairing, FUNC provenance or symbol completeness. Source/assembly hashes in producer are declarations; this tool cannot bind them to an actual build record.'}
 return ob,ol,out

def verify_layout(original,sdk,packaged):
 refs={};_,_,out=layout(original,sdk,packaged,refs);recheck(refs);out['sources']=list(refs.values());out['sources_unchanged']=True;return out

def verify(receipt,receipt_sha,original,sdk,packaged):
 need(hexsha(receipt_sha),'explicit producer receipt SHA required');refs={}
 p=json_unique(read_stable(receipt,JSON_CAP,refs,receipt_sha));need(type(p) is dict and type(p.get('schema')) is int and p['schema']==1 and p.get('kind')=='final-layout-symbol-producer','producer schema')
 need(p.get('address_convention')==CONVENTION and p.get('runtime_execution_proven') is False,'producer convention/runtime assertion')
 need(p.get('packaged_prx_binding')=='UNBOUND; owner must validate transforms and attach exact packaged PRX SHA','producer package declaration')
 for k in ['bare_elf_sha256','input_object_sha256','contract_sha256','packager_assembly_sha256','link_writer_assembly_sha256']:need(hexsha(p.get(k)),'producer SHA declaration')
 ob,loads,out=layout(original,sdk,packaged,refs)
 need(type(p.get('bare_elf_bytes')) is int and p['bare_elf_bytes']==len(ob) and p['bare_elf_sha256']==sha(ob),'producer bare ELF SHA/length mismatch')
 rows=p.get('functions');need(isinstance(rows,list) and 0<len(rows)<=65536,'producer function count bound')
 for row in rows:
  need(type(row) is dict and set(row)=={'name','addr','size','tier'},'producer function schema')
  need(isinstance(row['name'],str) and 0<len(row['name'].encode())<=4096 and '\0' not in row['name'] and row['tier']==TIER,'producer FUNC declaration')
  for k in ['addr','size']:need(isinstance(row[k],str) and re.fullmatch('0x[0-9a-f]{1,16}',row[k]) is not None,'producer final VMA hex')
  a,n=int(row['addr'],16),int(row['size'],16);need(n>0 and n<=MAX64-a,'producer range overflow/zero')
  need(sum(1 for x in loads if x['flags']&1 and x['vaddr']<=a and a+n<=x['vaddr']+x['filesz'])==1,'producer range outside executable file-backed load')
 out.update({'producer_status':'PROVIDED_DECLARATIONS_LAYOUT_VERIFIED','producer_receipt':refs[str(Path(receipt).resolve())],'producer_declarations':{k:p[k] for k in ['input_object_sha256','contract_sha256','packager_assembly_sha256','link_writer_assembly_sha256']},'symbols_admitted':len(rows),'functions':rows,'symbol_admission_scope':'Declared FUNC rows bounded in final executable ELF loads only; not crash-compatible runtime symbols'})
 recheck(refs);out['sources']=list(refs.values());out['sources_unchanged']=True;need(len(json.dumps(out).encode())<=JSON_CAP,'output bound');return out

def write_output(result,path):
 p=Path(path).resolve();need(p.is_relative_to(HOME) and not p.exists(),'new output inside package-binding required')
 need(p not in {Path(r['path']).resolve() for r in result['sources']},'input clobber refused')
 raw=json.dumps(result,indent=2)+'\n';need(len(raw.encode())<=JSON_CAP,'output bound')
 with p.open('x') as f:f.write(raw)
def main():
 a=argparse.ArgumentParser(description=__doc__)
 for name in ['bare-original','bare-sdk','packaged','out']:a.add_argument('--'+name,type=Path,required=True)
 a.add_argument('--producer',type=Path);a.add_argument('--producer-sha');x=a.parse_args();need(bool(x.producer)==bool(x.producer_sha),'producer and explicit SHA must be paired')
 out=verify(x.producer,x.producer_sha,x.bare_original,x.bare_sdk,x.packaged) if x.producer else verify_layout(x.bare_original,x.bare_sdk,x.packaged)
 write_output(out,x.out);print(json.dumps({k:out[k] for k in ['label','producer_status','symbols_admitted','runtime_base_convention_proven','capture_build_pairing']}))
if __name__=='__main__':main()
