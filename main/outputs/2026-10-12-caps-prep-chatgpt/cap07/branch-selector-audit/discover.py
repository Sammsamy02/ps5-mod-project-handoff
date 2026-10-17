import sys
sys.dont_write_bytecode=True
import hashlib,json,mmap,struct
from pathlib import Path
H=Path(__file__).resolve().parent;R=H.parents[3];P=R/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
with P.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
 assert hashlib.sha256(b).hexdigest()=='3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
 po=struct.unpack_from('<Q',b,32)[0];pe,pn=struct.unpack_from('<HH',b,54);ps=[struct.unpack_from('<IIQQQQQQ',b,po+i*pe) for i in range(pn)]
 def off(v,n):
  for p in ps:
   if p[0]==1 and p[3]<=v and v+n<=p[3]+p[5]:return p[2]+v-p[3]
  raise ValueError(hex(v))
 d=next(p for p in ps if p[0]==2);tags={}
 for o in range(d[2],d[2]+d[5],16):
  t,v=struct.unpack_from('<qQ',b,o)
  if not t:break
  tags[t]=v
 assert tags[9]==24
 n=min(tags[8],0x40000);o=off(tags[7],n);rels=[]
 for k in range(0,n-n%24,24):
  dst,info,add=struct.unpack_from('<QQq',b,o+k)
  if 0x7ebcab0<=dst<0x7ebcb68:rels.append({'offset':hex(o+k),'bytes':b[o+k:o+k+24].hex(),'destination':hex(dst),'target':hex(add),'type':info&0xffffffff})
 raw=b[off(0,0x40000):off(0,0x40000)+0x40000];hits=[]
 for i in range(len(raw)-7):
  for ilen,disp in ((6,2),(7,3)):
   if i+ilen<=len(raw) and i+ilen+struct.unpack_from('<i',raw,i+disp)[0]==0x92dc07c:hits.append({'candidate':hex(i),'bytes':raw[max(0,i-8):i+16].hex()})
 out={'bounded_rela_bytes':n,'relocs':rels,'raw_reference_candidates_bounds':['0x0','0x40000'],'raw_reference_candidates':hits,'strings':{hex(v):b[off(v,1024):off(v,1024)+1024].decode('utf-16-le').split('\0',1)[0] for v in (0x78781e0,0x79c0910)},'config_data':{'vaddr':'0x92dc078','file_offset':hex(off(0x92dc078,16)),'bytes':b[off(0x92dc078,16):off(0x92dc078,16)+16].hex()}}
 (H/'discovery.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
