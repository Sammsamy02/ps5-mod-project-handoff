#!/usr/bin/env python3
"""Check emitted child entry ownership; reads local ELF bytes only."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct

# The existing SCE NID transform, also used by the retained SharpProspero reader.
def nid(name):
    suffix = bytes.fromhex('518d64a635ded8c1e6b039b1c3e55230')
    return base64.b64encode(hashlib.sha1(name.encode()+suffix).digest()[:8][::-1]).decode().rstrip('=').replace('/', '-')

def check(path):
    b = Path(path).read_bytes()
    assert b[:6] == b'\x7fELF\x02\x01', 'expected ELF64 little endian'
    phoff = struct.unpack_from('<Q',b,32)[0]
    entsize,count = struct.unpack_from('<HH',b,54)
    ph = [struct.unpack_from('<IIQQQQQQ',b,phoff+i*entsize) for i in range(count)]
    def at(va, size):
        match=[p for p in ph if p[0]==1 and p[3]<=va and va+size<=p[3]+p[5]]
        assert len(match)==1, 'address is not uniquely file backed'
        return match[0][2]+va-match[0][3]
    dyn=[p for p in ph if p[0]==2]
    assert len(dyn)==1, 'missing unique dynamic table'
    d={}
    for pos in range(dyn[0][2],dyn[0][2]+dyn[0][5],16):
        tag,value=struct.unpack_from('<QQ',b,pos)
        if tag==0:break
        if tag in (4,5,6,10,11,12,13,25,26,27,28):
            assert tag not in d or d[tag]==value, 'conflicting dynamic tag'
        d[tag]=value
    # Standard tables emitted by this retained writer, with an exact SysV count.
    buckets,nchain=struct.unpack_from('<II',b,at(d[4],8))
    assert buckets and nchain and d[11]==24
    strings=b[at(d[5],d[10]):at(d[5],d[10])+d[10]]
    found={}
    for i in range(nchain):
        no,info,other,section,value,size=struct.unpack_from('<IBBHQQ',b,at(d[6]+i*24,24))
        name=strings[no:strings.index(0,no)].decode()
        for plain in ('module_start','module_stop','start_mod','uninstall_mod'):
            if name.split('#')[0]==nid(plain) and section and info&15==2:
                assert plain not in found, 'duplicate lifecycle export'
                found[plain]=value
    assert set(found)=={'module_start','module_stop','start_mod','uninstall_mod'}, 'child lifecycle exports absent'
    assert d[12]==found['module_start'] and d[13]==found['module_stop'], 'DT_INIT/DT_FINI do not select real child entries'
    assert all(d.get(tag,0)==0 for tag in (25,26,27,28)), 'loader arrays would duplicate child-owned traversal'
    assert struct.unpack_from('<Q',b,24)[0]==0, 'library unexpectedly has executable entry'
    shoff=struct.unpack_from('<Q',b,40)[0]
    shstride,shcount,shnames=struct.unpack_from('<HHH',b,58)
    sections=[struct.unpack_from('<IIQQQQIIQQ',b,shoff+i*shstride) for i in range(shcount)]
    names_section=sections[shnames];names=b[names_section[4]:names_section[4]+names_section[5]]
    named={names[s[0]:names.index(0,s[0])].decode():s for s in sections}
    frame,header=named['.eh_frame'],named['.eh_frame_hdr']
    assert frame[5] and frame[5]<=16*1024*1024 and at(frame[3],frame[5])==frame[4]
    pos=0;cies=set();fdes=[]
    while pos<frame[5]:
        assert frame[5]-pos>=8, 'truncated frame record'
        length,cie=struct.unpack_from('<II',b,frame[4]+pos)
        assert 4<=length<0xffffffff and length<=frame[5]-pos-4, 'early terminator/invalid frame length'
        if cie==0: cies.add(pos)
        else:
            assert pos+4-cie in cies, 'FDE does not identify a preceding CIE'
            fdes.append(frame[3]+pos)
        pos+=4+length
    frame_end=frame[3]+frame[5]
    terminator=(frame_end+3)&~3
    assert b[at(terminator,4):at(terminator,4)+4]==b'\0'*4, 'missing bounded frame terminator'
    h=header[4]
    assert b[h:h+4]==bytes((1,0x1b,3,0x3b)), 'unexpected frame-index encoding'
    assert header[3]+4+struct.unpack_from('<i',b,h+4)[0]==frame[3], 'frame index does not name section start'
    indexed=struct.unpack_from('<I',b,h+8)[0]
    assert indexed==len(fdes) and header[5]==12+8*indexed, 'frame index/section FDE count differs'
    indexed_fdes=[header[3]+struct.unpack_from('<i',b,h+12+8*i+4)[0] for i in range(indexed)]
    assert sorted(indexed_fdes)==sorted(fdes), 'frame index does not cover the exact section records'
    return dict(status='CHILD_ENTRIES_AND_SINGLE_ARRAY_OWNER_VERIFIED',entries=found,
                eh_frame=dict(start=frame[3],end=frame_end,terminator=terminator,fde_count=indexed),
                artifact_sha256=hashlib.sha256(b).hexdigest(),target_execution=False)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('elf',type=Path)
    print(json.dumps(check(p.parse_args().elf),indent=2))
