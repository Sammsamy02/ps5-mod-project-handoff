#!/usr/bin/env python3
"""Bounded local UTF16 class-name/RIP-LEA evidence capture, not a symbol classifier."""
from pathlib import Path
import hashlib,json,re,struct
import capstone as cs
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=ROOT/'outputs/2026-10-07-l19-class-size-evidence'
IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
D=IMG.read_bytes();H=hashlib.sha256(D).hexdigest()
assert H=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
phoff=struct.unpack_from('<Q',D,32)[0];ents,num=struct.unpack_from('<HH',D,54)
segments=[struct.unpack_from('<IIQQQQQQ',D,phoff+i*ents) for i in range(num)]
segments=[s for s in segments if s[0]==1]
def f2v(f):
 for s in segments:
  if s[2]<=f<s[2]+s[5]:return f-s[2]+s[3]
def v2f(v):
 for s in segments:
  if s[3]<=v<s[3]+s[5]:return v-s[3]+s[2]
names=['UObject','Object','UField','Field','UStruct','Struct','UFunction','Function','UClass','Class','FField','FFieldClass','FieldClass','FProperty','Property','Engine','GameModeBase']
targets={}
for name in names:
 raw=(name+'\0').encode('utf-16le');pos=0
 while True:
  pos=D.find(raw,pos)
  if pos<0:break
  if pos%2==0 and (pos==0 or D[pos-2:pos]==b'\0\0'):
   v=f2v(pos)
   if v is not None:targets.setdefault(v,[]).append(name)
  pos+=1
text=next(s for s in segments if s[1]&1)
refs=[]
pattern=re.compile(b'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]')
for m in pattern.finditer(D,text[2],text[2]+text[5]-7):
 f=m.start();v=f2v(f);target=v+7+struct.unpack_from('<i',D,f+3)[0]
 if target in targets:refs.append({'image_vaddr':hex(v),'file_offset':hex(f),'bytes':D[f:f+7].hex(),
                                    'target':hex(target),'names':targets[target]})
OUT.mkdir(exist_ok=True)
(OUT/'NAME-REFS.json').write_text(json.dumps({'image_sha256':H,'names':names,'targets':{hex(k):v for k,v in targets.items()},'rip_lea_refs':refs,
 'scope':'candidate RIP-LEA references only; instruction alignment and semantic identity require separate proof'},indent=2)+'\n')
md=cs.Cs(cs.CS_ARCH_X86,cs.CS_MODE_64)
lines=[]
for r in refs:
 v=int(r['image_vaddr'],16)
 if not 0x300000<=v<0x370000:continue
 f=v2f(v);start=D.rfind(b'\x55\x48\x89\xe5',max(text[2],f-0x240),f)
 if start<0:start=f
 lines.append('\nCANDIDATE '+','.join(r['names'])+' ref='+hex(v)+' target='+r['target']+' start='+hex(f2v(start)))
 for ins in md.disasm(D[start:min(f+0xb0,text[2]+text[5])],f2v(start)):
  lines.append(f'{ins.address:#x} file={v2f(ins.address):#x} {ins.bytes.hex():<24} {ins.mnemonic} {ins.op_str}')
(OUT/'CORE-NAME-REFS.asm').write_text('\n'.join(lines)+'\n')
print('name targets',len(targets),'refs',len(refs),'core refs',sum(0x300000<=int(r['image_vaddr'],16)<0x370000 for r in refs))
for r in refs:
 if 0x300000<=int(r['image_vaddr'],16)<0x370000:print(r)
