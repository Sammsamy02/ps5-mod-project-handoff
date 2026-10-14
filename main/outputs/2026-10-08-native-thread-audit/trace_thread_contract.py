from pathlib import Path
import struct, hashlib, json, re, capstone, importlib.machinery
import sys
sys.dont_write_bytecode=True
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what'); OUT=Path(__file__).resolve().parent
IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b=IMG.read_bytes(); sha=hashlib.sha256(b).hexdigest(); assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
ph=struct.unpack_from('<Q',b,32)[0]; pe,pn=struct.unpack_from('<HH',b,54); ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)]
def fo(a):
 for t,f,o,v,p,fs,ms,al in ss:
  if t==1 and v<=a<v+fs:return o+a-v
 raise ValueError(hex(a))
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
targets={0x9358a98,0x9358a9c,0x9358aa4}; refs=[]; calls=[]
# Bounded opcode family: scalar RIP MOV/LEA/CMP/TEST stores and loads, including
# immediate instructions. This is not a complete arbitrary-address alias census.
rx=re.compile(rb'[\x8d\x8b\x89\x39\x3b\xc7\xc6\x83\x81\x85\xff][\x05\x0d\x15\x1d\x25\x2d\x35\x3d]....',re.DOTALL)
for t,f,o,v,p,fs,ms,al in ss:
 if t!=1 or not(f&1):continue
 text=b[o:o+fs]
 for m in rx.finditer(text):
  op=m.group()[0]; extra=4 if op in (0xc7,0x81) else 1 if op in (0xc6,0x83) else 0
  target=v+m.start()+6+extra+struct.unpack_from('<i',m.group(),2)[0]
  if target not in targets:continue
  start=m.start()-1 if m.start() and 0x40<=text[m.start()-1]<=0x4f else m.start()
  i=next(md.disasm(text[start:start+16],v+start,count=1),None)
  if not i:continue
  if not any(op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP and i.address+i.size+op.mem.disp==target for op in i.operands):continue
  refs.append(dict(image_vaddr=hex(i.address),file_offset=hex(o+start),bytes=i.bytes.hex(),instruction=i.mnemonic+' '+i.op_str,target=hex(target)))
 for m in re.finditer(rb'\xe8....',text,re.DOTALL):
  dest=v+m.start()+5+struct.unpack_from('<i',m.group(),1)[0]
  if dest in (0xa262e0,0x74723a0,0x3a14720):calls.append(dict(image_vaddr=hex(v+m.start()),file_offset=hex(o+m.start()),bytes=m.group().hex(),target=hex(dest),boundary_status='literal direct-call candidate; not whole-image instruction-boundary proof'))
dy=next(s for s in ss if s[0]==2); tags={}; dynamic=[]
for i in range(dy[5]//16):
 t,value=struct.unpack_from('<QQ',b,dy[2]+i*16)
 if not t:break
 tags[t]=value;dynamic.append(dict(tag=hex(t),value=hex(value),file_offset=hex(dy[2]+i*16)))
imports=[]
for i in range(tags[2]//24):
 a,info,add=struct.unpack_from('<QQq',b,fo(tags[23])+i*24)
 if a==0x9132598:
  si=info>>32; symbol=struct.unpack_from('<IBBHQQ',b,fo(tags[6])+si*24); so=fo(tags[5])+symbol[0]; name=b[so:b.index(0,so)].decode()
  imports.append(dict(got_image_vaddr=hex(a),relocation_file_offset=hex(fo(tags[23])+i*24),relocation_bytes=b[fo(tags[23])+i*24:fo(tags[23])+(i+1)*24].hex(),symbol_index=si,symbol_file_offset=hex(fo(tags[6])+si*24),symbol_bytes=b[fo(tags[6])+si*24:fo(tags[6])+(si+1)*24].hex(),name=name,name_file_offset=hex(so)))
nid_script=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-nid'
m=importlib.machinery.SourceFileLoader('native_thread_nid',str(nid_script)).load_module();nid=m.nid_encode('scePthreadGetthreadid');assert imports[0]['name'].split('#')[0]==nid
(OUT/'THREAD-CONTRACT-REFS.json').write_text(json.dumps(dict(image_sha256=sha,scalar_rip_refs=refs,direct_call_candidates=calls,import_evidence=imports,nid_function='scePthreadGetthreadid',nid=nid,nid_script_sha256=hashlib.sha256(nid_script.read_bytes()).hexdigest(),dynamic=dynamic,scope='exact native references, bounded literal instruction families; does not establish all writers/aliases'),indent=2)+'\n')
print(json.dumps(dict(refs=refs,calls_to_name=[c for c in calls if c['target']=='0xa262e0'],current_id_direct_call_candidates=sum(c['target']=='0x74723a0' for c in calls),calls_to_initializer=[c for c in calls if c['target']=='0x3a14720'],import_name=imports[0]['name'])))
