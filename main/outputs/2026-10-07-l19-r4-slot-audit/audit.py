#!/usr/bin/env python3
"""Independent saved-image byte, relocation and native semantic slot audit."""
from pathlib import Path
import capstone,hashlib,json,struct,sys
ROOT=Path.cwd();OWN=Path(__file__).resolve().parent;P=Path(sys.argv[1]).resolve();D=json.loads(P.read_text())
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert {(f['key'],f['value'],f['width_bytes']) for f in D['facts']}=={('UEngine.LoadMap.vtable_byte_offset','0x4f8',8),('AGameModeBase.InitGameState.vtable_byte_offset','0x7b8',8)} and len(D['facts'])==2
assert all(f['status']=='validated-static' and f['confidence']=='high' and f['live_validation'] is False for f in D['facts'])
B=(ROOT/D['image']['path']).read_bytes();assert hashlib.sha256(B).hexdigest()==D['image']['sha256']=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';assert len(B)==D['image']['size_bytes']
H=struct.unpack_from('<16sHHIQQQIHHHHHH',B);assert H[0][:6]==b'\x7fELF\x02\x01';PH=[struct.unpack_from('<IIQQQQQQ',B,H[5]+i*H[9]) for i in range(H[10])];LOADS=[p for p in PH if p[0]==1]
def offset(v,n):
 m=[p for p in LOADS if p[3]<=v and v+n<=p[3]+p[5]];assert len(m)==1,(hex(v),n);return m[0][2]+v-m[0][3]
def bytes_at(v,n):o=offset(v,n);return B[o:o+n]
MD=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);MD.detail=True
prior_path=ROOT/'outputs/2026-10-07-l19-worker-engine-slots/ENGINE-TICK-FACTS.json';PRIOR=json.loads(prior_path.read_text())
assert D['sources'][str(prior_path.relative_to(ROOT))]==sha(prior_path)
for d in (D,PRIOR):
 for a,row in d['instruction_evidence'].items():
  v=int(row['elf_image_vaddr'],0);b=bytes.fromhex(row['instruction_bytes']);assert v==int(a,0);assert offset(v,len(b))==int(row['file_byte_offset'],0);assert bytes_at(v,len(b))==b
  decoded=list(MD.disasm(b,v));assert len(decoded)==1 and decoded[0].size==len(b);ins=decoded[0];assert ins.mnemonic+' '+ins.op_str==row['instruction']
  rip=[ins.address+ins.size+x.mem.disp for x in ins.operands if x.type==capstone.x86.X86_OP_MEM and x.mem.base==capstone.x86.X86_REG_RIP]
  if 'rip_targets' in row:assert rip==[int(x,0) for x in row['rip_targets']]
 for row in d['data_evidence']:
  v=int(row['elf_image_vaddr'],0);b=bytes.fromhex(row['bytes']);assert offset(v,len(b))==int(row['file_byte_offset'],0);assert bytes_at(v,len(b))==b
  if row.get('encoding')=='UTF16LE':assert b.endswith(b'\0\0') and b[:-2].decode('utf-16le')==row['text']
  if row.get('encoding')=='ASCII':assert b.endswith(b'\0') and b[:-1].decode('ascii')==row['text']
 pins=d['sources'].items() if isinstance(d['sources'],dict) else ((x['path'],x['sha256']) for x in d['sources'])
 for relative,pin in pins:assert sha(ROOT/relative)==pin,relative
DYN={}
for p in PH:
 if p[0]!=2:continue
 for o in range(p[2],p[2]+p[5],16):
  tag,val=struct.unpack_from('<QQ',B,o)
  if tag==0:break
  DYN.setdefault(tag,[]).append(val)
for tag in (7,8,9):assert len(DYN[tag])==1
assert DYN[9][0]==24
needed={int(x['r_offset'],0) for d in (D,PRIOR) for x in d['relocations']}
for base,count in ((0x8489de0,19),(0x849ad80,10)):needed.update(base+8*i for i in range(count))
rows={};ro=offset(DYN[7][0],DYN[8][0]);assert DYN[8][0]%24==0
for o in range(ro,ro+DYN[8][0],24):
 target,info,addend=struct.unpack_from('<QQq',B,o)
 if target in needed:rows.setdefault(target,[]).append((o,info,addend))
def relocation(v):
 matches=rows.get(v,[]);assert len(matches)==1,(hex(v),matches);o,info,target=matches[0];assert info==8;return target
for d in (D,PRIOR):
 for row in d['relocations']:
  v=int(row['r_offset'],0);o,info,target=rows[v][0];assert len(rows[v])==1;assert o==int(row.get('relocation_file_offset',row.get('file_offset')),0);assert info==int(row['r_info'],0)==8;assert target==int(row.get('r_addend',row.get('addend')),0);assert B[o:o+24].hex()==row.get('entry_bytes',row.get('bytes'))
# Native named root tables. These are actual relocation targets, never a PC table.
assert relocation(0x82315c0+0x4f8)==relocation(0x8477708+0x4f8)==0x384d460
assert relocation(0x848a0b0+0x7b8)==0x2fdb2b0
instructions={**PRIOR['instruction_evidence'],**D['instruction_evidence']}
def ins(addr,text):assert instructions[hex(addr)]['instruction']==text,(hex(addr),text)
def rip(addr,target):
 r=instructions[hex(addr)];decoded=next(MD.disasm(bytes.fromhex(r['instruction_bytes']),addr));assert any(x.type==capstone.x86.X86_OP_MEM and x.mem.base==capstone.x86.X86_REG_RIP and decoded.address+decoded.size+x.mem.disp==target for x in decoded.operands)
# Prior named Engine/GameEngine constructor evidence is itself freshly byte verified.
for addr,target in ((0x2faae27,0x78b59b4),(0x2faae12,0x2faad80),(0x2a6d16e,0x782706e),(0x2a6d167,0x2a6d110),(0x2faad94,0x8477708)) :rip(addr,target)
ins(0x2faad8f,'call 0x2fa6f20');ins(0x2a6d11a,'call 0x2fa6f20');rip(0x2fa6f45,0x82315b0);ins(0x2fa6f5e,'lea rcx, [rax + 0x10]')
# Corrected named GameModeBase registration: class getter RCX, constructor R15.
rip(0x2fda7f7,0x783a91a);rip(0x2fda805,0x2fda7a0)
ctor_rows=[(int(a,0),r) for a,r in D['instruction_evidence'].items() if 0x2fda7a0<=int(a,0)<0x2fda880]
assert any(r['instruction'].startswith('lea r15,') and int(r['rip_targets'][0],0)==0x2fda880 for a,r in ctor_rows)
assert any(r['instruction']=='push r15' for a,r in ctor_rows)
ins(0x2fda88a,'call 0x2fdac20');ins(0x2fdac3b,'mov rbx, rdi');rip(0x2fdac85,0x848a0b0);ins(0x2fdac97,'mov qword ptr [rbx], rax')
# Read actual ClassParam property-array topology, packed count and descriptor identity.
assert relocation(0x848a060)==0x2fc8fd0;ins(0x2fc8fd4,'call 0x2fda8a0');rip(0x2fda8f7,0x783a91a)
assert relocation(0x849ae90)==0x2fdaa40;rip(0x2fdaa97,0x76c22e0)
assert {(x['owner'],x['name'],x['offset'],x['kind']) for x in D['property_semantics']}=={
 ('GameModeBase','GameStateClass','0x2c0','0x11'),('GameModeBase','GameState','0x308','0x52'),('GameModeBase','SpectatorClass','0x2e8','0x11'),
 ('GameStateBase','GameModeClass','0x2a8','0x11'),('GameStateBase','AuthorityGameMode','0x2b0','0x52'),('GameStateBase','SpectatorClass','0x2b8','0x11')} and len(D['property_semantics'])==6
properties=[]
for owner,param,array,count in [('GameModeBase',0x848a060,0x8489de0,19),('GameStateBase',0x849ae90,0x849ad80,10)]:
 assert relocation(param+0x28)==array
 packed=struct.unpack('<I',bytes_at(param+0x38,4))[0];assert ((packed>>15)&0x7ff)==count
 entries=[relocation(array+8*i) for i in range(count)]
 for fact in [x for x in D['property_semantics'] if x['owner']==owner]:
  desc=int(fact['descriptor'],0);entry=int(fact['array_entry'],0);assert entry in range(array,array+8*count,8);assert relocation(entry)==desc and desc in entries
  nameptr=relocation(desc);n=len(fact['name'])+1;assert bytes_at(nameptr,n)==fact['name'].encode()+b'\0'
  off=struct.unpack('<H',bytes_at(desc+0x32,2))[0];kind=bytes_at(desc+0x18,1)[0];assert off==int(fact['offset'],0) and kind==int(fact['kind'],0)
  properties.append(dict(owner=owner,name=fact['name'],descriptor=hex(desc),offset=hex(off),kind=hex(kind)))
ins(0x499704,'bextr eax, dword ptr [r13 + 0x38], eax');ins(0x499711,'add rcx, qword ptr [r13 + 0x28]');ins(0x49975d,'call 0x494470')
ins(0x4944aa,'movzx eax, byte ptr [r15 + 0x18]');ins(0x4944af,'and eax, 0x3f');ins(0x4944b2,'dec eax');rip(0x4944bd,0x7c3564c)
kind_branches={}
for kind in (0x11,0x52):
 branch=0x7c3564c+struct.unpack('<i',bytes_at(0x7c3564c+4*((kind&0x3f)-1),4))[0];kind_branches[hex(kind)]=hex(branch)
assert kind_branches=={'0x11':'0x494ad4','0x52':'0x494bc0'}
# Both native kinds reach the shared descriptor offset reader through the actual constructor graph.
def calls(lo,hi,target):return [hex(int(a,0)) for a,r in instructions.items() if lo<=int(a,0)<hi and r['instruction']=='call '+hex(target)]
assert calls(0x494ad4,0x494b0d,0x496380);assert calls(0x494bc0,0x494bfc,0x495dc0)
assert calls(0x496380,0x496500,0x402040);assert calls(0x495dc0,0x495f30,0x402160)
assert calls(0x402040,0x402100,0x418d60);assert calls(0x402160,0x402200,0x418d60);assert calls(0x418d60,0x419000,0x3e1e20)
ins(0x3e1fa4,'movzx eax, word ptr [r13 + 0x32]');ins(0x3e1fa9,'mov dword ptr [rbx + 0x44], eax')
# Native state lifecycle and initialization: exact named field relationships.
for addr,text in [(0x2fdb36b,'mov rbx, rdi'),(0x2fdb3a0,'mov r15, qword ptr [rbx + 0x2c0]'),(0x2fdb624,'mov qword ptr [rbx + 0x308], r15'),(0x2fdb650,'mov qword ptr [r15 + 0x2b0], rbx'),(0x2fdb6a3,'mov rdi, rbx'),(0x2fdb6a6,'mov rax, qword ptr [rbx]'),(0x2fdb6a9,'call qword ptr [rax + 0x7b8]'),(0x2fdc321,'mov rdi, rbx'),(0x2fdc324,'call qword ptr [rax + 0x7b8]'),(0x2fdb2bb,'mov r14, qword ptr [rdi + 0x10]'),(0x2fdb2bf,'mov r12, qword ptr [rdi + 0x308]'),(0x2fdb2e3,'mov qword ptr [r12 + 0x2a8], r14'),(0x2fdb2ff,'mov rdi, qword ptr [rbx + 0x2e8]'),(0x2fdb324,'mov qword ptr [r14 + 0x2b8], rdi')]:ins(addr,text)
# LoadMap native names, argument receivers and dedicated configuration identity.
assert relocation(0x82315c0+0x4e8)==0x384ae10 and relocation(0x82315c0+0x4f0)==0x384ca20
ins(0x384ca3b,'mov r14, rsi');ins(0x384ca3e,'mov r13, rdi')
# Independently decode Browse from its actual entry. Branch-aware backwards reaching
# definitions establish the call's R15/R13 as saved incoming Engine/context, not
# unrelated values assigned to those registers on branches that bypass this call.
lo,hi=0x384ae10,0x384ca20
native={i.address:i for i in MD.disasm(bytes_at(lo,hi-lo),lo)};pred={}
for a,i in native.items():
 successors=[]
 if i.mnemonic.startswith('j'):
  if i.operands[0].type==capstone.x86.X86_OP_IMM:successors.append(i.operands[0].imm)
  if i.mnemonic!='jmp':successors.append(a+i.size)
 elif i.mnemonic not in ('ret','ud2'):successors.append(a+i.size)
 for target in successors:
  if target in native:pred.setdefault(target,[]).append(a)
def reaching_defs(at,reg):
 seen=set();pending=list(pred[at]);definitions=set()
 while pending:
  a=pending.pop()
  if a in seen:continue
  seen.add(a);i=native[a]
  if i.operands and i.operands[0].type==capstone.x86.X86_OP_REG and i.reg_name(i.operands[0].reg)==reg and i.operands[0].access&capstone.CS_AC_WRITE:definitions.add(a)
  else:pending.extend(pred.get(a,[]))
 return definitions
assert reaching_defs(0x384b89b,'r15')=={0x384b2ac}
assert reaching_defs(0x384b89b,'r13')=={0x384b2a5}
assert native[0x384b2ac].mnemonic+' '+native[0x384b2ac].op_str=='mov r15, qword ptr [rbp - 0x2a0]'
assert native[0x384b2a5].mnemonic+' '+native[0x384b2a5].op_str=='mov r13, qword ptr [rbp - 0x298]'
ins(0x384ae2b,'mov qword ptr [rbp - 0x2a0], rdi');ins(0x384ae39,'mov rbx, rsi')
assert native[0x384ae6a].mnemonic+' '+native[0x384ae6a].op_str=='mov qword ptr [rbp - 0x298], rbx'
browse_receiver_evidence=[{'image_vaddr':hex(a),'file_offset':hex(offset(a,native[a].size)),'bytes':native[a].bytes.hex(),'instruction':native[a].mnemonic+' '+native[a].op_str} for a in (0x384ae2b,0x384ae39,0x384ae6a,0x384b2a5,0x384b2ac)]

for addr,target in ((0x384cb69,0x79b190c),(0x384c1ee,0x75ce11c),(0x384d4b8,0x793768c),(0x385c1d9,0x795ff94),(0x385c1e0,0x9553978),(0x385c1e7,0x7798d18)):rip(addr,target)
for addr,text in [(0x384b89b,'mov rax, qword ptr [r15]'),(0x384b8a5,'mov rdi, r15'),(0x384b8b0,'call qword ptr [rax + 0x4f8]'),(0x384d2e1,'mov rax, qword ptr [r13]'),(0x384d2e9,'mov rdi, r13'),(0x384d2ec,'mov rsi, r14'),(0x384d2ef,'mov rdx, r12'),(0x384d2f2,'call qword ptr [rax + 0x4f8]'),(0x384d489,'mov qword ptr [rbp - 0x178], rdi'),(0x384d482,'mov qword ptr [rbp - 0x168], rsi'),(0x384d490,'mov r15, rdx'),(0x384d47b,'mov qword ptr [rbp - 0x1a0], rcx'),(0x384d49e,'mov qword ptr [rbp - 0x180], r8')]:ins(addr,text)
for addr in (0x384df8c,0x384ec71):rip(addr,0x9553978)
result={'pass':True,'status':'ENGINE_SLOT_STATIC_AUDIT_PASS','scope':'Independent LOCAL saved-image native slots; semantic identity is explicit inference; no runtime/ABI/menu claim',
 'fact_receipt':{'path':str(P),'sha256':sha(P)},'image':{'sha256':hashlib.sha256(B).hexdigest(),'bytes':len(B)},
 'verified':{'selected_instruction_windows':len(D['instruction_evidence']),'selected_data_windows':len(D['data_evidence']),'selected_unique_DT_RELA':len(D['relocations']),'source_pins':len(D['sources']),'prior_ctor_instruction_windows':len(PRIOR['instruction_evidence']),'prior_ctor_data_windows':len(PRIOR['data_evidence']),'prior_unique_DT_RELA':len(PRIOR['relocations'])},
 'browse_receiver_CFG_evidence':browse_receiver_evidence,'native_properties':properties,'kind_dispatch_branches':kind_branches,'accepted_facts':D['facts'],
 'proof_limits':['LoadMap/InitGameState method identities are native semantic inferences, not literal method symbols.','No live execution or console observation.','Selected relocations do not prove entire vtable extent.','Superseded initial captures are diagnostic only.'],
 'audit_script_sha256':sha(Path(__file__))}
(OWN/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'pass':True,'verified':result['verified'],'accepted':[(f['key'],f['value']) for f in D['facts']]}))
