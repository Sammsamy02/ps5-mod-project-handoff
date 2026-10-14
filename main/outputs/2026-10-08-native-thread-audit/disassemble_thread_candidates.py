from pathlib import Path
import struct,json,hashlib,capstone
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent;IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';b=IMG.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)]
def fo(a):
 for t,f,o,v,p,fs,ms,al in ss:
  if t==1 and v<=a<v+fs:return o+a-v
 raise ValueError(hex(a))
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
refs=json.loads((OUT/'THREAD-NAME-REF-CANDIDATES.json').read_text());groups=[('GameEngineTick-bounded',0x2fac8c0,0x1000),('GameThreadId-low-native-region',0xbb180,0x120),('GameThreadId-consumer',0x3a14e40,0x160),('ThreadName-manager',0xbd500,0x140)]
for n,r in enumerate(refs['lea_pattern_refs']):
 a=int(r['image_vaddr'],16);f=fo(a);p=b.rfind(bytes.fromhex('554889e5'),max(0,f-0x600),f)
 start=a-(f-p) if p>=0 else a
 groups.append((f'GameThread-name-ref{n+1}',start,a-start+0x80))
for n,r in enumerate(json.loads((OUT/'THREAD-ID-LEA-REF-CANDIDATES.json').read_text())['references']):
 a=int(r['image_vaddr'],16);f=fo(a);p=b.rfind(bytes.fromhex('554889e5'),max(0,f-0x800),f)
 start=a-(f-p) if p>=0 else a
 groups.append((f'ThreadId-LEA-ref{n+1}',start,a-start+0x100))
E={};summ=[]
for name,start,limit in groups:
 seen={};work=[start];calls=[];rips=[]
 while work:
  a=work.pop()
  while start<=a<start+limit and a not in seen:
   i=next(md.disasm(b[fo(a):fo(a)+16],a,count=1),None)
   if i is None:break
   seen[a]=i
   if i.mnemonic in ['ret','int3','ud2']:break
   if i.mnemonic=='call' and i.operands[0].type==capstone.x86.X86_OP_IMM:calls.append(hex(i.operands[0].imm))
   if i.mnemonic.startswith('j') and i.operands[0].type==capstone.x86.X86_OP_IMM:
    target=i.operands[0].imm
    if start<=target<start+limit:work.append(target)
    if i.mnemonic=='jmp':break
   a+=i.size
 rows=[]
 for a,i in sorted(seen.items()):
  row=dict(image_vaddr=hex(a),file_offset=hex(fo(a)),bytes=i.bytes.hex(),instruction=i.mnemonic+' '+i.op_str)
  for op in i.operands:
   if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:
    target=a+i.size+op.mem.disp;row.setdefault('rip_targets',[]).append(hex(target));rips.append(hex(target))
  E[hex(a)]=row;rows.append(row)
 (OUT/(name+'.asm')).write_text('\n'.join(f"{x['image_vaddr']} file{x['file_offset']} {x['bytes']} {x['instruction']}"+(' rip->'+','.join(x['rip_targets']) if x.get('rip_targets') else '') for x in rows)+'\n')
 summ.append(dict(name=name,start=hex(start),limit=hex(limit),instructions=len(rows),direct_call_targets=list(dict.fromkeys(calls)),rip_targets=list(dict.fromkeys(rips)),function_completeness=False))
(OUT/'NATIVE-THREAD-CANDIDATE-WINDOWS.json').write_text(json.dumps({'image_sha256':sha,'instructions':E,'ranges':summ,'scope':'bounded native candidate windows; references do not establish thread identity'},indent=2)+'\n');print(json.dumps({'instruction_windows':len(E),'ranges':[(s['name'],s['instructions']) for s in summ]}))
