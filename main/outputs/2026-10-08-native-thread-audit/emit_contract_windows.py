from pathlib import Path
import struct,hashlib,json,capstone
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent
b=(ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin').read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)]
def fo(a):return next(o+a-v for t,f,o,v,p,fs,ms,al in ss if t==1 and v<=a<v+fs)
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
groups=[('ThreadId-import-stub',0x74723a0,0x10),('GameThread-id-and-ready-producer',0x3a14e8e,0x2c),('Ready-consumer-one',0xb8f20,0x150),('Ready-consumer-two',0xb9090,0x85),('Ready-consumer-three',0xbc450,0xa7),('Thread-name-render-id-caller',0xa0a783,0x47),('Initializer-entry-caller',0x3a145c0,0x50),('GameThread-label-contract',0xa262e0,0x13c)]
rows={};ranges=[];strings=[]
for name,start,length in groups:
 out=[]
 for i in md.disasm(b[fo(start):fo(start)+length],start):
  row=dict(image_vaddr=hex(i.address),file_offset=hex(fo(i.address)),bytes=i.bytes.hex(),instruction=i.mnemonic+' '+i.op_str)
  for op in i.operands:
   if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:
    target=i.address+i.size+op.mem.disp;row.setdefault('rip_targets',[]).append(hex(target))
  rows[hex(i.address)]=row;out.append(row)
 (OUT/(name+'.asm')).write_text('\n'.join(f"{x['image_vaddr']} file{x['file_offset']} {x['bytes']} {x['instruction']}"+(' rip->'+','.join(x['rip_targets']) if x.get('rip_targets') else '') for x in out)+'\n')
 ranges.append(dict(name=name,start=hex(start),byte_limit=hex(length),instructions=len(out),scope='linear exact-byte window, boundary/completeness qualified individually in REPORT'))
for name,a in [('GameThread',0x76d6dec),('RenderThread',0x765beba)]:
 o=fo(a);n=0
 while b[o+2*n:o+2*n+2]!=b'\0\0':n+=1
 strings.append(dict(value=name,image_vaddr=hex(a),file_offset=hex(o),bytes=b[o:o+2*n+2].hex(),decoded=b[o:o+2*n].decode('utf-16le')))
(OUT/'THREAD-CONTRACT-WINDOWS.json').write_text(json.dumps(dict(image_sha256=sha,instruction_evidence=rows,data_evidence=strings,ranges=ranges),indent=2)+'\n');print(json.dumps({'instructions':len(rows),'strings':len(strings)}))
