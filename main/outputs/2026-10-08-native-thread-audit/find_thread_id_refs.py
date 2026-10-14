from pathlib import Path
import struct,re,json,hashlib
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent;IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin';b=IMG.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ss=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)];rows=[];targets={0x9358a98,0x9358a9c}
for t,f,o,v,p,fs,ms,a in ss:
 if t!=1 or not(f&1):continue
 # RIP LEA address references only: direct scalar MOV/CMP RIP forms may be absent from this bounded index.
 for m in re.finditer(rb'[\x48-\x4f]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]....',b[o:o+fs],re.DOTALL):
  dest=v+m.start()+7+struct.unpack_from('<i',m.group(),3)[0]
  if dest in targets:rows.append(dict(image_vaddr=hex(v+m.start()),file_offset=hex(o+m.start()),bytes=m.group().hex(),target=hex(dest)))
(OUT/'THREAD-ID-LEA-REF-CANDIDATES.json').write_text(json.dumps(dict(image_sha256=sha,targets=[hex(x) for x in sorted(targets)],references=rows,scope='literal64B RIPLEA xrefs only, not complete globalsxref census'),indent=2)+'\n');print(json.dumps({'refs':len(rows),'game_refs':[x['image_vaddr'] for x in rows if x['target']=='0x9358a98'],'render_refs':[x['image_vaddr'] for x in rows if x['target']=='0x9358a9c']}))
