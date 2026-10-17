import struct, collections
def containers(blob):
    s=0
    while True:
        i=blob.find(b'DXBC',s)
        if i<0: return
        s=i+4
        if i+32>len(blob): continue
        ver,total,n=struct.unpack_from('<III',blob,i+20)
        if ver!=1 or not (32<total<=len(blob)-i) or not (0<n<64): continue
        yield i, blob[i:i+total]
def chunks(c):
    n=struct.unpack_from('<I',c,28)[0]
    for k in range(n):
        off=struct.unpack_from('<I',c,32+4*k)[0]
        if off+8>len(c): return
        fcc=c[off:off+4]; sz=struct.unpack_from('<I',c,off+4)[0]
        yield fcc, c[off+8:off+8+sz]
def program(sh):
    vt,ln=struct.unpack_from('<II',sh,0)
    ptype=vt>>16
    toks=struct.unpack_from('<%dI'%min(ln,len(sh)//4), sh, 0)
    p=2; ins=[]
    while p<len(toks):
        t=toks[p]; op=t&0x7ff
        if op==0x35:  # customdata: length in next dword
            L=toks[p+1]
        else:
            L=(t>>24)&0x7f
        if L==0: break
        ins.append((op, toks[p:p+L])); p+=L
    return ptype, ins
DISCARD=0x0d; RET=0x3e
def discard_profile(ins):
    """first discard: unconditional? (immediate operand that always passes the test)"""
    out=[]
    for op,t in ins:
        if op==DISCARD:
            nz=(t[0]>>18)&1; opnd=t[1]
            otype=(opnd>>12)&0xff
            if otype==4 and len(t)>=3:  # immediate32
                v=t[2]; always=(v!=0) if nz else (v==0)
                out.append('always' if always else 'never')
            else: out.append('conditional')
    return out
def summarize(blob):
    rows=collections.Counter(); ex=[]
    for at,c in containers(blob):
        for fcc,data in chunks(c):
            if fcc in (b'SHDR',b'SHEX'):
                ptype,ins=program(data)
                if ptype!=0: rows['non-pixel']+=1; continue
                dp=discard_profile(ins)
                ops=[op for op,_ in ins if op not in (0x35,)]
                # instructions before first unconditional discard that are not declarations (dcl opcodes 0x58..0x6a, 0x8f.. etc are > 0x57)
                first=None
                for k,(op,t) in enumerate(ins):
                    if op==DISCARD and discard_profile([(op,t)])==['always']: first=k; break
                key=('pixel', 'unconditional-discard' if 'always' in dp else ('conditional-discard' if 'conditional' in dp else 'no-discard'))
                rows[key]+=1
    return rows
