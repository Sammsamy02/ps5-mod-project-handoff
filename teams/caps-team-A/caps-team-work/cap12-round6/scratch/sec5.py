import sys,os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import struct
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'sec2.py')).read().split('found=[]')[0])
# scan for section-table starts: i32 count (1..64) then section with base 0 basev 0
res=[]
o=0
while o < L-80:
    c=i32(o)
    if 1<=c<=64:
        s=parse(o+4) if (d[o+6]|(d[o+7]<<8))<nmat and u32(o+8)==0 else None
        if s and s['basev']==0 and 1<=s['maxinf']<=16 and s['strip'] in ('0501','0001','0000','0101','0500'):
            # try parse all
            p=o+4; ok=True; secs=[]
            for k in range(c):
                t=parse(p)
                if not t: ok=False;break
                p=t['end']; t['disabled']=u32(p); p+=4; secs.append(t)
            if ok:
                tot=sum(t['ntri'] for t in secs)
                res.append((o,c,tot,[ (t['mat'],t['ntri'],t['nverts']) for t in secs]))
    o+=1
for r in res: print(r[0], 'nsec',r[1],'tris',r[2], r[3])
