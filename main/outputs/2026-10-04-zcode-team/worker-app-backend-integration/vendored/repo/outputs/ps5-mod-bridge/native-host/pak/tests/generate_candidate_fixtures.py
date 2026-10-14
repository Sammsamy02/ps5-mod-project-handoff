"""Generate bounded offline candidate fixtures; no archive/device/account access."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zlib as fixture_zlib

if len(sys.argv)!=2:
    raise SystemExit('usage: generate_candidate_fixtures.py OUTPUT_DIRECTORY')
synthetic=Path(sys.argv[1])
synthetic.mkdir(parents=True,exist_ok=True)

def fstring(value):
    encoded=value.encode('utf-8')+b'\0'
    return struct.pack('<i',len(encoded))+encoded

def container(payload,encoded_blob,count,directory_bytes):
    # Complete v11 candidate fixtures, not the older primary-only empty control.
    path_bytes=struct.pack('<i',count)+b''.join(struct.pack('<Qi',i,i*28)
        for i in range(count))+struct.pack('<i',0)
    mount=fstring('/Fixture/')
    primary_bytes=len(mount)+4+8+4+36+4+36+4+len(encoded_blob)+4
    primary_offset=len(payload)
    path_offset=primary_offset+primary_bytes
    directory_offset=path_offset+len(path_bytes)
    primary=mount+struct.pack('<iQ',count,0x12345678)+struct.pack('<IQQ',1,path_offset,len(path_bytes))
    primary+=hashlib.sha1(path_bytes).digest()
    primary+=struct.pack('<IQQ',1,directory_offset,len(directory_bytes))+hashlib.sha1(directory_bytes).digest()
    primary+=struct.pack('<i',len(encoded_blob))+encoded_blob+struct.pack('<i',0)
    if len(primary)!=primary_bytes:
        raise RuntimeError('synthetic primary length mismatch')
    footer=bytes(16)+b'\0'+struct.pack('<IIQQ',0x5a6f12e1,11,primary_offset,len(primary))
    footer+=hashlib.sha1(primary).digest()+b'Zlib'+bytes(28)+bytes(32*4)
    if len(footer)!=221:
        raise RuntimeError('synthetic footer length mismatch')
    return payload+primary+path_bytes+directory_bytes+footer

empty=container(b'',b'',0,struct.pack('<i',0))
blocks=[bytes((i*17+3)%256 for i in range(64)),b'final-block-17!!!']
if len(blocks[1])!=17:
    raise RuntimeError('second block size mismatch')
stored=[fixture_zlib.compress(b,9) for b in blocks]
stored_total=b''.join(stored)

def zlib_candidate(streams):
    stored_bytes=b''.join(streams)
    header_bytes=89
    physical=struct.pack('<QQQI',0,len(stored_bytes),sum(map(len,blocks)),1)
    physical+=hashlib.sha1(stored_bytes).digest()+struct.pack('<I',2)
    at=header_bytes
    for stream in streams:
        physical+=struct.pack('<QQ',at,at+len(stream))
        at+=len(stream)
    physical+=b'\0'+struct.pack('<I',64)
    if len(physical)!=header_bytes:
        raise RuntimeError('synthetic physical header length mismatch')
    encoded=struct.pack('<IIIIII',0xe08000bf,64,0,sum(map(len,blocks)),len(stored_bytes),len(streams[0]))
    encoded+=struct.pack('<I',len(streams[1]))
    directory=struct.pack('<i',1)+fstring('')+struct.pack('<i',1)+fstring('block.bin')+struct.pack('<i',0)
    return container(physical+stored_bytes,encoded,1,directory)

two_block=zlib_candidate(stored)
truncated=zlib_candidate([stored[0],stored[1][:-1]])
for name,data in (('empty.pak',empty),('two-block.pak',two_block),
                  ('two-block-truncated.pak',truncated),('two-block.bin',b''.join(blocks))):
    (synthetic/name).write_bytes(data)
oracle={'compression_generator':'Python stdlib zlib; decoder independently built from pinned zlib 1.3.2',
    'generator_runtime':fixture_zlib.ZLIB_RUNTIME_VERSION,'block_bytes':[len(b) for b in blocks],
    'stored_bytes':[len(b) for b in stored], 'stored_sha1':hashlib.sha1(stored_total).hexdigest(),
    'decoded_sha1':hashlib.sha1(b''.join(blocks)).hexdigest()}
oracle['fixtures']=[{'name':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
    for p in sorted(synthetic.glob('*')) if p.name!='oracle.json']
(synthetic/'oracle.json').write_text(json.dumps(oracle,indent=2)+'\n')
print(json.dumps(oracle,sort_keys=True))
