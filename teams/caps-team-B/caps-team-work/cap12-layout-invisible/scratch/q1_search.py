"""Table-free derivation: try every monotone interleaving of 6 X and 6 Y bits for the
16-byte-element 64 KiB tile (924 candidates) and rank by mip-to-mip consistency."""
import sys, itertools
exec(open(sys.argv[1]+'/q1_test.py').read())
z,d,bulk=tex(sys.argv[3])
vals={}
for m in (3,4):
    data=mip_bytes(z,bulk,m); vals[m]=[val_bc5_mean(data[i:i+16]) for i in range(0,len(data),16)]
def grid_from_seq(seq, bw, V):
    peq=[[(a,int(b))] for a,b in seq]
    tw,th=block_dims(peq); tpr=bw//tw
    return [[V[addr_of(peq,x,y,tw,th,tpr,16,4)] for x in range(bw)] for y in range(bw)]
res=[]
for pos in itertools.combinations(range(12),6):
    seq=[];xi=yi=0
    for k in range(12):
        if k in pos: seq.append(('X',xi)); xi+=1
        else: seq.append(('Y',yi)); yi+=1
    G3=grid_from_seq(seq,32*4,vals[3]); G4=grid_from_seq(seq,64,vals[4])
    res.append((corr(downsample(G3),G4), ''.join(a+str(b) for a,b in seq)))
res.sort(reverse=True)
print('candidates', len(res))
for r in res[:6]: print(round(r[0],4), r[1])
print('median', round(res[len(res)//2][0],4))
