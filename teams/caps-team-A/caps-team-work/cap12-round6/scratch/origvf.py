import sys,os,json,re
sys.dont_write_bytecode=True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from graph import *
import reparent_plan as rp
sys.path.insert(0,'/WORKSPACE/.codex/worktrees/translation-core/caps-team-A/caps-team-work/cap12-round6/work/tools')
import mesh_sections as ms, invisible_route as ir
EV=W+'/outputs/2026-10-08-l25-prep/evidence/cap12-20261012/'
checked=[r for p in ('round1-check.json','round2-check.json','round3-check.json') for r in json.load(open(EV+p))['checked'] if 'verified' in r]
whole=rp.fetched_packages([F+'/headers-r1',F+'/headers-r2',F+'/headers-r3'],checked)
plan=json.load(open('plan-r6-try.json'))
for r in plan['invisible_route']['rows']:
    if not r['users'] or r['result']=='reparent': continue
    p=r['mod_package']; chain=[p]; cur=p
    while True:
        par=[x for x in imp.get(cur,()) if cls(x) in ('Material','MaterialInstanceConstant')]
        if len(par)!=1: break
        cur=par[0]; chain.append(cur)
        if cls(cur)=='Material': break
    master=chain[-1] if cls(chain[-1])=='Material' else None
    toks=None
    if master and master in whole:
        z=whole[master]; tops=[b for e,b in ms.export_slices(z) if e['outer'][0]=='Null']
        toks=sorted(ir.vf_tokens(tops[0])) if len(tops)==1 else None
    print(p.split('/')[-1], r['users'], '-> master', (master or '?').split('/')[-1], 'whole' if master in whole else 'NOT FETCHED', toks)
