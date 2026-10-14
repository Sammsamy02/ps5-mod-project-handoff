import sys,os,json
sys.dont_write_bytecode=True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graph import *
EV=W+'/outputs/2026-10-08-l25-prep/evidence/cap12-20261012/'
checked={}
for p in ('round1-check.json','round2-check.json','round3-check.json'):
    for r in json.load(open(EV+p))['checked']: checked[r['package']]=r
names=[a['package'] for a in reg.assets if any(k.lower() in a['package'].lower() for k in sys.argv[1:])]
for n in names:
    h='%016x.hdr.bin'%cityhash.ue_name_hash(n)
    loc=[d for d in ('headers-r1','headers-r2','headers-r3') if os.path.exists(F+'/'+d+'/'+h)]
    st=store.get(cityhash.ue_name_hash(n))
    print(n.split('/')[-1], cls(n), loc, (checked.get(n) or {}).get('verified'), 'store' if st else '-')
