#!/usr/bin/env python3
"""Hash-gated bounded image import audit. No NID reversal or external lookup."""
import sys
sys.dont_write_bytecode=True
import argparse,capstone,hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];P=R/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';SHA='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';ALPH='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'
p=argparse.ArgumentParser();p.add_argument('--verify',action='store_true');args=p.parse_args()
def ident(s):
 n=0
 for c in s:n=n*64+ALPH.index(c)
 return n
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()==SHA and b[:6]==b'\x7fELF\x02\x01'
 po=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ps=[struct.unpack_from('<IIQQQQQQ',b,po+i*pe) for i in range(pn)]
 def off(v,n):
  for t,fl,o,a,_,fs,_,_ in ps:
   if t==1 and a<=v and v+n<=a+fs:return o+v-a
  raise ValueError('not file-backed '+hex(v))
 def block(v,n):
  o=off(v,n);raw=b[o:o+n];return {'vaddr':hex(v),'file_offset':hex(o),'bytes':raw.hex(),'sha256':hashlib.sha256(raw).hexdigest()}
 dy=next(x for x in ps if x[0]==2);tags={};dynamic=[]
 for o in range(dy[2],dy[2]+dy[5],16):
  t,v=struct.unpack_from('<qQ',b,o)
  if not t:break
  dynamic.append((t,v,o));tags[t]=v
 assert tags[9]==tags[11]==24 and tags[20]==7 and tags[2]%24==0 and tags[2]<1024*1024
 def string(no):
  assert 0<=no<tags[10];o=off(tags[5]+no,1);end=b.find(b'\0',o,o+tags[10]-no);assert end>=o
  raw=b[o:end+1];return raw[:-1].decode('ascii'),{'offset_in_strtab':hex(no),'file_offset':hex(o),'bytes':raw.hex()}
 modules={};libs={}
 for t,v,o in dynamic:
  if t in (0x61000045,0x61000049):
   name,sr=string(v&0xffffffff);row={'encoded_numeric_id':v>>48,'name':name,'version_bits':hex((v>>32)&0xffff),'dynamic_file_offset':hex(o),'dynamic_bytes':b[o:o+16].hex(),'string_record':sr};(modules if t==0x61000045 else libs)[v>>48]=row
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
 direct=list(md.disasm(b[off(0x6ecca60,0x2a):off(0x6ecca60,0x2a)+0x2a],0x6ecca60));call=next(i for i in direct if i.address==0x6ecca7f);assert call.mnemonic=='call';dst=call.operands[0].imm;assert dst==0x7479870
 stubs=[0x7472030,0x7472040,0x7472050,0x74720a0,dst];gots={};stubblocks={}
 for va in stubs:
  rec=block(va,16);raw=bytes.fromhex(rec['bytes']);ins=list(md.disasm(raw,va));assert len(ins)==3 and ins[0].mnemonic=='jmp' and raw[:2]==b'\xff\x25'
  got=va+6+struct.unpack_from('<i',raw,2)[0];gots[got]=va;stubblocks[va]=rec
 rows=[]
 for idx in range(tags[2]//24):
  va=tags[23]+idx*24;o=off(va,24);target,info,add=struct.unpack_from('<QQq',b,o)
  if target not in gots:continue
  assert info&0xffffffff==7;si=info>>32;sva=tags[6]+si*tags[11];so=off(sva,24);no,inf,oth,section,val,size=struct.unpack_from('<IBBHQQ',b,so);name,sr=string(no);parts=name.split('#');assert len(parts)==3
  lid,mid=ident(parts[1]),ident(parts[2]);assert lid in libs and mid in modules
  rows.append({'requested_routine':'0x6ecca60' if gots[target]==dst else hex(gots[target]),'plt_stub':hex(gots[target]),'plt_record':stubblocks[gots[target]],'got_slot':hex(target),'jmprel_index':idx,'relocation':block(va,24),'relocation_type':7,'symbol_index':si,'symbol_record':block(sva,24),'symbol_binding':inf>>4,'symbol_type':inf&15,'symbol_section':section,'symbol_value':hex(val),'symbol_size':size,'encoded_import_name':name,'name_record':sr,'library':libs[lid],'module':modules[mid],'name_status':'encoded identifier only; no plaintext API name established'})
 assert len(rows)==5 and len({r['got_slot'] for r in rows})==5
 # DT_HASH supplies a bounded symbol count independently of absent/stale section headers.
 ho=off(tags[4],8);nb,nc=struct.unpack_from('<II',b,ho);assert 0<nc<100000 and 0<nb<100000
 off(tags[6],nc*24);defined_matches=[]
 for si in range(nc):
  va=tags[6]+si*24;o=off(va,24);no,inf,oth,section,val,size=struct.unpack_from('<IBBHQQ',b,o)
  if section and (val==0x6ecca60 or (size and val<=0x6ecca60<val+size)):
   name,sr=string(no);defined_matches.append({'symbol_index':si,'name':name,'value':hex(val),'size':size,'symbol_record':block(va,24),'name_record':sr})
 shoff=struct.unpack_from('<Q',b,40)[0];she,shn=struct.unpack_from('<HH',b,58);shvalid=shoff<=len(b) and she*shn<=len(b)-shoff
 receipt={'label':'Fresh Mac-only bounded image import metadata; no ABI-name or shipping admission','image_path':str(P.relative_to(R)),'image_sha256':SHA,'image_bytes':len(b),'capstone':capstone.__version__,'load_segments':[{'flags':x[1],'offset':hex(x[2]),'vaddr':hex(x[3]),'filesz':hex(x[5])} for x in ps if x[0]==1],'dynamic_bounds':{'file_offset':hex(dy[2]),'bytes':dy[5]},'dynamic_table_records':[{'tag':hex(t),'value':hex(v),'file_offset':hex(o),'bytes':b[o:o+16].hex()} for t,v,o in dynamic if t in (2,4,5,6,9,10,11,20,23)],'direct_wrapper':block(0x6ecca60,0x2a),'direct_wrapper_call':{'site':hex(call.address),'bytes':call.bytes.hex(),'target':hex(dst)},'imports':sorted(rows,key=lambda r:int(r['plt_stub'],16)),'hash_symbol_count':{'buckets':nb,'symbols':nc,'header':block(tags[4],8)},'defined_dynsym_covering_direct_wrapper':defined_matches,'section_header_geometry':{'offset':shoff,'entry_bytes':she,'count':shn,'file_backed_valid':shvalid},'scope':'five PLT/GOT relocations and relevant symbols/providers; DT_HASH-bounded dynamic symbol search only; no NID dictionary, SDK or external research'}
 out=H/'import-receipt.json'
 if args.verify:
  expected=json.loads(out.read_text());assert receipt==expected,'receipt changed'
  (H/'verification.json').write_text(json.dumps({'label':'Fresh Mac-only replay PASS','image_sha256':SHA,'imports':len(rows),'dynamic_symbols_checked':nc,'direct_wrapper_calls':1,'plaintext_api_names_established':0,'receipt_sha256':hashlib.sha256(out.read_bytes()).hexdigest()},indent=2)+'\n')
  print('PASS:5 PLT/GOT→JMPREL→DYNSYM identities,1 wrapper call,',nc,'dynamic symbols checked;0 plaintext API names')
 else:
  out.write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps([{'routine':r['requested_routine'],'plt':r['plt_stub'],'symbol':r['encoded_import_name'],'library':r['library']['name'],'module':r['module']['name']} for r in receipt['imports']],indent=2))
