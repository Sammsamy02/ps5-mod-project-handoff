"""Extract members from a GNU/SysV ar archive (macOS ar cannot read 'name.o/' rows).
usage: python3 -I arx.py ARCHIVE OUTDIR [MEMBER ...]   (no member = list)"""
import sys
from pathlib import Path
def members(blob):
    if not blob.startswith(b'!<arch>\n'): raise SystemExit('not an ar archive')
    pos=8; longnames=b''
    while pos<len(blob):
        h=blob[pos:pos+60]; size=int(h[48:58]); name=h[:16].decode().strip(); data=blob[pos+60:pos+60+size]
        if name=='//': longnames=data
        elif name in ('/','/SYM64/','__.SYMDEF','__.SYMDEF SORTED'): pass
        else:
            if name.startswith('#1/'):
                n=int(name[3:]); name=data[:n].rstrip(b'\0').decode(); data=data[n:]
            elif name.startswith('/'):
                at=int(name[1:]); stop=longnames.find(b'/\n',at); name=longnames[at:stop].decode()
            else: name=name.removesuffix('/')
            yield name,data
        pos+=60+size+(size%2)
a=Path(sys.argv[1]).read_bytes(); out=Path(sys.argv[2]); want=set(sys.argv[3:])
out.mkdir(parents=True,exist_ok=True); n=0
for name,data in members(a):
    if not want: print(name); continue
    if name in want:
        (out/name).write_bytes(data); n+=1
print('extracted',n,file=sys.stderr)
