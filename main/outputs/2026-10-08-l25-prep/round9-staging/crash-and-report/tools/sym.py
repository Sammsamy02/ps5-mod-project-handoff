import json,sys,bisect
d=json.load(open(sys.argv[1]))
syms=d['map_reconstructed']['symbols']
L=sorted(((int(s['addr'],16),int(s['size'],16),s['name']) for s in syms))
A=[x[0] for x in L]
base=0x80a8c000
for a in sys.argv[2:]:
    v=int(a,16); off=v-base
    i=bisect.bisect_right(A,off)-1
    s=L[i] if i>=0 else None
    inb = s and off < s[0]+s[1]
    print(a, hex(off), (s[2][:170] if s else None), '+'+hex(off-s[0]) if s else '', 'IN' if inb else 'OUT(size %s)'%hex(s[1]) )
