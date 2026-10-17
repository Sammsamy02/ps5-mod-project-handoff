"""ELF64 section reader with extended numbering and duplicate-name retention."""
import struct

def sections(blob):
    if len(blob)<64 or blob[:6]!=b'\x7fELF\x02\x01':raise ValueError('not little-endian ELF64')
    off=struct.unpack_from('<Q',blob,40)[0]
    entsize,count,names=struct.unpack_from('<HHH',blob,58)
    if entsize!=64 or off>len(blob)-64:raise ValueError('invalid section table')
    zero=struct.unpack_from('<II4QII2Q',blob,off)
    if count==0:count=zero[5]
    if names==0xffff:names=zero[6]
    if not count or count>(len(blob)-off)//64 or names>=count:raise ValueError('section extent/index invalid')
    stringrow=struct.unpack_from('<II4QII2Q',blob,off+names*64)
    start,size=stringrow[4:6]
    if stringrow[1]!=3 or start>len(blob) or size>len(blob)-start:raise ValueError('bad section-name table')
    strings=blob[start:start+size]
    for i in range(count):
        row=struct.unpack_from('<II4QII2Q',blob,off+i*64)
        if row[0]>=len(strings):raise ValueError('section name out of bounds')
        end=strings.find(b'\0',row[0])
        if end<0:raise ValueError('unterminated section name')
        name=strings[row[0]:end].decode('utf-8')
        if row[1]!=8 and (row[4]>len(blob) or row[5]>len(blob)-row[4]):raise ValueError('section data out of bounds')
        yield {'index':i,'name':name,'type':row[1],'offset':row[4],'bytes':row[5],'link':row[6]}

def array_census(blob):
    result={name:[] for name in ('.init_array','.fini_array')}
    aliases={'.init_array':('.init_array','.ctors'),'.fini_array':('.fini_array','.dtors')}
    for row in sections(blob):
        for name in result:
            if any(row['name']==alias or row['name'].startswith(alias+'.') for alias in aliases[name]):
                if row['bytes']%8:raise ValueError('misaligned constructor/destructor array')
                result[name].append(row)
    return {name:{'bytes':sum(r['bytes'] for r in rows),'slots':sum(r['bytes'] for r in rows)//8,'sections':rows} for name,rows in result.items()}
