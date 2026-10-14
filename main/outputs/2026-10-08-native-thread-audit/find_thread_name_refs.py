from pathlib import Path
import re,struct,json,hashlib
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent;IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b=IMG.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)]
def va(off):
 for t,f,o,v,p,fs,ms,a in ss:
  if t==1 and o<=off<o+fs:return v+off-o
 raise ValueError(hex(off))
names=[]
for enc in ['ascii','utf-16le']:
 q='GameThread'.encode(enc);step=1 if enc=='ascii' else 2;start=0
 while (off:=b.find(q,start))>=0:
  start=off+len(q)
  if b[off-step:off]!=bytes(step) or b[off+len(q):off+len(q)+step]!=bytes(step):continue
  names.append(dict(encoding=enc,file_offset=hex(off),image_vaddr=hex(va(off)),bytes=(q+bytes(step)).hex()))
refs=[];targets={int(x['image_vaddr'],16) for x in names}
for t,flags,off,v,p,fs,ms,a in ss:
 if t!=1 or not(flags&1):continue
 for m in re.finditer(rb'[\x48-\x4f]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]....',b[off:off+fs],re.DOTALL):
  dest=v+m.start()+7+struct.unpack_from('<i',m.group(),3)[0]
  if dest in targets:refs.append(dict(image_vaddr=hex(v+m.start()),file_offset=hex(off+m.start()),bytes=m.group().hex(),rip_target=hex(dest)))
(OUT/'THREAD-NAME-REF-CANDIDATES.json').write_text(json.dumps(dict(image_sha256=sha,names=names,lea_pattern_refs=refs,scope='Exact standaloneGameThread strings and literal64B RIPLEArefs; not threadID proof, no CFG assumption'),indent=2)+'\n');print(json.dumps({'standalone_names':names,'rip_lea_refs':refs}))
