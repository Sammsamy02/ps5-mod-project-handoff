from pathlib import Path
import struct,re,json,hashlib
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');OUT=Path(__file__).resolve().parent;IMG=ROOT/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
b=IMG.read_bytes();sha=hashlib.sha256(b).hexdigest();assert sha=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc';ph=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);segments=[struct.unpack_from('<IIQQQQQQ',b,ph+i*pe) for i in range(pn)];rows=[]
for t,flags,off,vaddr,pa,filesz,memsz,align in segments:
 if t!=1 or not(flags&1):continue
 for m in re.finditer(rb'\xff(?:[\x90-\x93\x95-\x97]\xd8\x00\x00\x00|\x94.\xd8\x00\x00\x00)',b[off:off+filesz],re.DOTALL):rows.append({'image_vaddr_ff':hex(vaddr+m.start()),'file_offset_ff':hex(off+m.start()),'bytes':m.group().hex()})
(OUT/'RAW-D8-CALL-PATTERNS.json').write_text(json.dumps({'image_sha256':sha,'scope':'executable PT_LOAD bytes: literalFF memoryCALL ModRM/2 mod10 disp32+D8; allbases andSIB; opcodealignment not yet established','count':len(rows),'candidates':rows},indent=2)+'\n');print(json.dumps({'raw_patterns':len(rows)}))
