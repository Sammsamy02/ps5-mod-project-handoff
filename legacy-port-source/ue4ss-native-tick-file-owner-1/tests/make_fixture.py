"""Owned ELF bytes for static resolver tests; no function is executed."""
from pathlib import Path
import struct
TEXT,RO,DATA,RELA=0x410000,0x610000,0x810000,0x910000
b=bytearray(0x5200)
def put(fmt,p,*v):struct.pack_into('<'+fmt,b,p,*v)
def ph(i,t,f,off,va,size):put('IIQQQQQQ',64+i*56,t,f,off,va,va,size,size,1)
b[:7]=b'\x7fELF\x02\x01\x01';put('HHI',16,3,62,1);put('Q',32,64);put('HHH',52,64,56,5)
ph(0,1,5,0x400,TEXT,0x1400);ph(1,1,4,0x2000,RO,0x1000);ph(2,1,6,0x3000,DATA,0x1000);ph(3,1,4,0x4000,RELA,0x1200);ph(4,2,4,0x5100,RELA+0x1100,64)
b[0x400:0x1800]=b'\xcc'*0x1400
def code(at,c):p=0x400+at-TEXT;b[p:p+len(c)]=c
def wide(at,s):p=0x2000+at-RO;c=(s+'\0').encode('utf-16le');b[p:p+len(c)]=c
def emit(c,at,target,op):c.extend(op);c.extend(struct.pack('<i',target-(at+len(c)+4)))
for at in [TEXT+0x10,TEXT+0x20,TEXT+0x30]:code(at,b'\xc3')
for off,s in [(0,'causeevent='),(0x80,'UEngine::Tick'),(0x100,'Pure virtual not implemented (%s)'),(0x200,'GameEngine'),(0x280,'/Script/Engine')]:wide(RO+off,s)
c=bytearray();emit(c,TEXT+0x100,RO+0x80,b'\x48\x8d\x0d');emit(c,TEXT+0x100,RO+0x100,b'\x48\x8d\x15');emit(c,TEXT+0x100,TEXT+0x10,b'\xe8');c+=b'\xc3';code(TEXT+0x100,c)
c=bytearray();emit(c,TEXT+0x300,RO,b'\x48\x8d\x35');c+=b'\xc3';code(TEXT+0x300,c)
c=bytearray();emit(c,TEXT+0x500,DATA+0x300,b'\x48\x8d\x05');c+=b'\x48\x89\x07\xc3';code(TEXT+0x500,c)
c=bytearray();emit(c,TEXT+0x700,TEXT+0x500,b'\x4c\x8d\x3d');emit(c,TEXT+0x700,RO+0x280,b'\x48\x8d\x3d');emit(c,TEXT+0x700,RO+0x200,b'\x48\x8d\x35');c+=b'\x41\x57';emit(c,TEXT+0x700,TEXT+0x10,b'\xe8');c+=b'\x48\x83\xc4\x08\xc3';code(TEXT+0x700,c)
rows=[]
for i in range(20):rows.append((DATA+0x100+i*8,TEXT+(0x100 if i==12 else 0x10)))
for i in range(20):rows.append((DATA+0x300+i*8,TEXT+(0x300 if i==12 else 0x30 if i==0 else 0x10)))
rows.append((DATA+0x800,TEXT+0x700))
for i,(dst,value) in enumerate(rows):put('QQQ',0x4000+i*24,dst,8,value)
for i,(tag,value) in enumerate([(7,RELA),(8,len(rows)*24),(9,24),(0,0)]):put('QQ',0x5100+i*16,tag,value)
Path(__file__).with_name('owned-tick.elf').write_bytes(b)

# An owned SCE executable shape, not captured game bytes: execute-only flags,
# file-only metadata, unrelated loader-managed TLS/import state and BSS.
put('H',16,0xfe10);b[7]=9;put('H',56,7);put('I',68,1);put('I',64+3*56+4,0)
ph(5,7,4,0x1800,0x850000,16)
ph(6,1,6,0x1800,0x850000,0x400);put('Q',64+6*56+40,0x1000)
# An unrelated symbol-bearing JMPREL writer is excluded from all Tick proof.
ph(4,2,4,0x5100,RELA+0x1100,128)
for i,(tag,value) in enumerate([(7,RELA),(8,len(rows)*24),(9,24),(23,RELA+0x900),(2,24),(20,7),(1,1),(0,0)]):put('QQ',0x5100+i*16,tag,value)
put('QQQ',0x4900,0x850008,0x100000007,0)
Path(__file__).with_name('owned-tick-sce.elf').write_bytes(b)
