from pathlib import Path
import struct,json,hashlib,collections,capstone
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent;IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b=IMG.read_bytes();raw=json.loads((OUT/'RAW-D8-CALL-PATTERNS.json').read_text());assert hashlib.sha256(b).hexdigest()==raw['image_sha256'];ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)]
def fo(a):
 for t,f,o,v,p,fs,ms,al in ss:
  if t==1 and v<=a<v+fs:return o+a-v
 raise ValueError(hex(a))
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
regs={x:y for y,ls in {'rax':['rax','eax','ax','al','ah'],'rbx':['rbx','ebx','bx','bl','bh'],'rcx':['rcx','ecx','cx','cl','ch'],'rdx':['rdx','edx','dx','dl','dh'],'rdi':['rdi','edi','di','dil'],'rsi':['rsi','esi','si','sil'],'rbp':['rbp','ebp','bp','bpl'],'rsp':['rsp','esp','sp','spl']}.items() for x in ls}
for i in range(8,16):
 for x in [f'r{i}',f'r{i}d',f'r{i}w',f'r{i}b']:regs[x]=f'r{i}'
def rn(r):return regs.get(md.reg_name(r),md.reg_name(r))
vol={'rax','rcx','rdx','rsi','rdi','r8','r9','r10','r11'}
rows=[];amb=[]
for row in raw['candidates']:
 ff=int(row['image_vaddr_ff'],16);ffo=fo(ff);start=ff-1 if 0x40<=b[ffo-1]<=0x4f else ff
 call=next(md.disasm(b[fo(start):fo(start)+16],start,count=1))
 assert call.mnemonic=='call' and call.operands[0].type==capstone.x86.X86_OP_MEM and call.operands[0].mem.disp==0xd8
 # Only a local syntactic boundary heuristic, not CFG or function completeness evidence.
 lo=max(0,ffo-0x5000);pos=b.rfind(bytes.fromhex('554889e5'),lo,ffo);ins=[];boundary=False
 if pos>=lo:
  va=ff-(ffo-pos)
  for x in md.disasm(b[pos:fo(start)+call.size],va):
   ins.append(x)
   if x.address==start:boundary=True;break
 if not boundary:
  # Report unresolved rather than forcing a guessed decoding boundary.
  cls='unresolved-no-aligned-local-prologue';trace=[]
 else:
  base=rn(call.operands[0].mem.base);reg=base;trace=[];cls='unresolved-register-origin'
  for x in reversed(ins[:-1]):
   if x.mnemonic=='call' and reg in vol:cls='unresolved-intervening-call';trace.append(hex(x.address));break
   _,writes=x.regs_access()
   if reg not in [rn(z) for z in writes]:continue
   trace.append(hex(x.address)+' '+x.mnemonic+' '+x.op_str)
   if x.mnemonic=='mov' and x.operands[0].type==capstone.x86.X86_OP_REG and rn(x.operands[0].reg)==reg:
    op=x.operands[1]
    if op.type==capstone.x86.X86_OP_REG:reg=rn(op.reg);continue
    if op.type==capstone.x86.X86_OP_MEM:
     mem=op.mem
     if mem.base and mem.base!=capstone.x86.X86_REG_RIP and not mem.index and mem.disp==0:cls='vtable-shaped-zero-dereference'
     else:cls='unresolved-member-or-global-load'
     break
   cls='unresolved-other-base-definition';break
  else:cls='unresolved-entry-or-preserved-register'
 row=dict(row,call_image_vaddr=hex(start),call_bytes=call.bytes.hex(),call_instruction=call.mnemonic+' '+call.op_str,base_register=rn(call.operands[0].mem.base),local_boundary_heuristic=boundary,classification=cls,origin_trace=trace)
 if start==0x326b25:row['classification']='confirmed-UFunction-Func-Invoke';row['identity']='Frame90 set toUFunction self at326B1E beforecall; restore326B2C';cls=row['classification']
 rows.append(row)
 if cls!='vtable-shaped-zero-dereference':
  before=ins[max(0,len(ins)-30):]
  amb.append('CALL '+row['call_image_vaddr']+' '+cls+'\n'+'\n'.join(f'{x.address:x} {x.bytes.hex()} {x.mnemonic} {x.op_str}' for x in before)+'\n')
result=dict(scope='all literal FF /2 mod10 memoryCALL disp32+D8 patterns in executable PT_LOAD; local origin classifier, not complete native call graph',image_sha256=raw['image_sha256'],count=len(rows),counts=dict(collections.Counter(x['classification'] for x in rows)),calls=rows,limits=['Patterns embedded in code/data cannot all be declared instructions from this local boundary heuristic.','Vtable-shaped dereference0 is syntactic strong exclusion evidence, not authenticated receiver-class identity.','mov register,[function+D8];call register, computedLEA/functionwrapper and nonliteral variants are outside this census.','No proof all actual Func entry paths set frame90; unresolved candidates are retained.'])
(OUT/'D8-CALL-CENSUS.json').write_text(json.dumps(result,indent=2)+'\n');(OUT/'NON-VTABLE-CONTEXTS.asm').write_text('\n'.join(amb));print(json.dumps({'literal_patterns':len(rows),'classifications':result['counts']}))
