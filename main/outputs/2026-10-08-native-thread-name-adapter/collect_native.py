from pathlib import Path
import struct,hashlib,json,capstone
w=Path(__file__).resolve().parent;root=w.parent.parent
path=root/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b=path.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54)
ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)]
def off(a):return next(o+a-v for t,f,o,v,p,fs,ms,al in ss if t==1 and v<=a<v+fs)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
ranges=[('ThreadNameAccessor',0xa262e0,0x11b),('ThreadNameManager',0xbd500,0x131),('ThreadManagerSingleton',0xbd640,0x240),('ThreadManagerRegistration-bounded',0xbd000,0x500),('AccessorCaller',0xa0a783,0x47)]
rows={};groups=[]
for name,start,count in ranges:
 out=[]
 for i in md.disasm(b[off(start):off(start)+count],start):
  r=dict(image_vaddr=hex(i.address),file_offset=hex(off(i.address)),bytes=i.bytes.hex(),instruction=i.mnemonic+' '+i.op_str)
  for op in i.operands:
   if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:r.setdefault('rip_targets',[]).append(hex(i.address+i.size+op.mem.disp))
  rows[hex(i.address)]=r;out.append(r)
 (w/'evidence'/(name+'.asm')).write_text('\n'.join(f"{r['image_vaddr']} file{r['file_offset']} {r['bytes']} {r['instruction']}"+(' rip->'+','.join(r['rip_targets']) if r.get('rip_targets') else '') for r in out)+'\n')
 groups.append(dict(name=name,start=hex(start),bytes=count,instructions=len(out)))
data=[]
for name,a in [('GameThread',0x76d6dec),('RenderThread',0x765beba)]:
 o=off(a);n=0
 while b[o+n:o+n+2]!=b'\0\0':n+=2
 data.append(dict(value=name,image_vaddr=hex(a),file_offset=hex(o),bytes=b[o:o+n+2].hex()))
segments=[]
for name,a,n in [('accessor',0xa262e0,0x11b),('GameThread-storage',0x9402ac8,16),('render-storage',0x9402ae0,16),('unknown-storage',0x932a5a0,16)]:
 hits=[dict(index=i,flags=f,vaddr=hex(v),file_offset=hex(o),filesz=hex(fs),memsz=hex(ms),file_backed=a+n<=v+fs) for i,(t,f,o,v,p,fs,ms,al) in enumerate(ss) if t==1 and v<=a and a+n<=v+ms]
 assert len(hits)==1;segments.append(dict(role=name,address=hex(a),width=n,load=hits[0]))
(w/'evidence/NATIVE-WINDOWS.json').write_text(json.dumps(dict(image_sha256=sha,instructions=rows,data=data,ranges=groups,segments=segments),indent=2)+'\n')
print(json.dumps(dict(instructions=len(rows),groups=groups,segments=segments)))
