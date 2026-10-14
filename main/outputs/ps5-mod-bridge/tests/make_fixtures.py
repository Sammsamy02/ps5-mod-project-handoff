"""Generate exclusively project-authored structural fixtures (never executable tests)."""
import io, json, pathlib, struct, sys, tarfile, zipfile, warnings
warnings.filterwarnings("ignore", message="Duplicate name:.*", category=UserWarning)
root=pathlib.Path(sys.argv[1] if len(sys.argv)>1 else 'build/fixtures'); root.mkdir(parents=True,exist_ok=True)
def pe(managed=False):
 b=bytearray(2048)
 def w(o,f,*v):struct.pack_into('<'+f,b,o,*v)
 b[:2]=b'MZ'; w(0x3c,'I',0x80);b[0x80:0x84]=b'PE\0\0';w(0x84,'HHIIIHH',0x8664,1,0,0,0,240,0x2022)
 o=0x98;w(o,'H',0x20b);w(o+24,'Q',0x140000000);w(o+60,'I',0x200);w(o+108,'I',16)
 # One .rdata section maps RVA 0x1000 to offset 0x200.
 s=o+240;b[s:s+8]=b'.rdata\0\0';w(s+8,'IIII',0x600,0x1000,0x600,0x200)
 def d(i,r,size):w(o+112+i*8,'II',r,size)
 d(1,0x1000,40);w(0x200,'IIIII',0x1060,0,0,0x1040,0x1060);b[0x240:0x24d]=b'KERNEL32.dll\0';w(0x260,'QQQ',0x1080,0x8000000000000007,0);w(0x280,'H',3);b[0x282:0x28e]=b'CreateFileW\0'
 d(3,0x10a0,12);w(0x2a0,'III',0x1000,0x1010,0x10b0);b[0x2b0]=1
 d(5,0x10c0,12);w(0x2c0,'IIHH',0x1000,12,0xa008,0)
 d(9,0x1100,40);w(0x300,'QQQQII',0,0,0,0x140001140,0,0);w(0x340,'QQ',0x140001180,0)
 if managed:
  d(14,0x1200,72);w(0x400,'IHHII',72,2,5,0x1250,64);w(0x410,'I',1);b[0x450:0x454]=b'BSJB';w(0x454,'HHII',1,1,0,12);b[0x460:0x46c]=b'v4.0.30319\0\0';w(0x46c,'HH',0,0)
 return bytes(b)
def elf():
 b=bytearray(512);b[:16]=b'\x7fELF\x02\x01\x01'+bytes(9)
 struct.pack_into('<HHIQQQIHHHHHH',b,16,3,62,1,0,64,128,0,64,56,1,64,2,0)
 struct.pack_into('<IIQQQQQQ',b,64,1,5,0,0,0,len(b),len(b),4096)
 struct.pack_into('<IIQQQQIIQQ',b,192,0,4,0,0,320,24,0,0,8,24)
 struct.pack_into('<QQq',b,320,0x1000,(2<<32)|8,0)
 return bytes(b)
files={'asset.uasset':b'\xc1\x83\x2a\x9e'+bytes(12),'logic.lua':b'local x = require("module"); load("computed")\n','native.dll':pe(),'managed.dll':pe(True),'native.elf':elf(),'fake.dll':b'not a DLL','blob.bin':bytes(range(32)),'archive.pak':b'opaque authored bytes'}
manifest={'metadata':{'provider_id':'fixture-only'},'requirements':[{'id':'example.runtime','consumer':'logic.lua'}],'dependencies':[{'from':'logic.lua','to':'module','type':'runtime'},{'from':'asset.uasset','to':'logic.lua','type':'order'}]}
files['modbridge.json']=json.dumps(manifest).encode()
for name,data in files.items():(root/name).write_bytes(data)
with zipfile.ZipFile(root/'mixed.zip','w',zipfile.ZIP_DEFLATED) as z:
 for name,data in files.items():z.writestr(name,data)
with tarfile.open(root/'mixed.tar','w') as t:
 for name,data in files.items():
  info=tarfile.TarInfo(name);info.size=len(data);t.addfile(info,io.BytesIO(data))
 info=tarfile.TarInfo('link.lua');info.type=tarfile.SYMTYPE;info.linkname='../outside.lua';t.addfile(info)
for name,members in {'unsafe.zip':['../escape','/absolute','C:\\escape','a/../alias'],'collision.zip':['Folder/File','folder/file'],'duplicate.zip':['same','same'],'overflow.zip':['large']}.items():
 with zipfile.ZipFile(root/name,'w') as z:
  for m in members:z.writestr(m,b'x'*128)
(root/'truncated.pe').write_bytes(pe()[:300]);(root/'truncated.elf').write_bytes(elf()[:100])
b=bytearray(elf());struct.pack_into('<Q',b,40,0xffffffffffffffff);(root/'malformed.elf').write_bytes(b)
(root/'empty.zip').write_bytes(b'PK\x05\x06'+bytes(18))
with tarfile.open(root/'directories.tar','w') as t:
 for name in ['a/','b/','c/']:
  info=tarfile.TarInfo(name);info.type=tarfile.DIRTYPE;t.addfile(info)
