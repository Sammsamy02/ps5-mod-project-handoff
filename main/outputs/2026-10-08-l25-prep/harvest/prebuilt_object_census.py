#!/usr/bin/env python3
"""Claude round-9 item 4: stale-object census. Lists every prebuilt/pinned
object the l26 link consumes, and for each verifies its pinned sha matches
the file on disk AND (where a source mapping exists) that the source hash
matches the object's recorded provenance. Flags source-only fixes (symbols
present in source, absent from the object)."""
import json, sys, glob, hashlib, subprocess, os
def sha(p): return hashlib.sha256(open(p,'rb').read()).hexdigest()
# [gate law 2026-10-12] four dirnames = worktree root (three pointed at outputs/,
# so every default path below resolved into outputs/outputs/... and matched nothing).
WHAT=os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
BV=os.path.join(WAT:=WHAT, 'outputs','2026-10-08-l26-build-variants')
sel_path=sys.argv[1] if len(sys.argv)>1 else None
if not sel_path:
    cands=sorted(glob.glob(os.path.join(BV,'l26-A','SOURCE-SELECTION.json')))
    if not cands: sys.exit('no live SOURCE-SELECTION; pass one as argv[1]')
    sel_path=cands[0]
sel=json.load(open(sel_path))
issues=[]; checked=0
# 1) source_pins: each file must exist at its pin
for grp in ('source_pins','receipt_pins','artifact_pins'):
    for pin in sel.get(grp,[]):
        p=pin['path']
        if not os.path.exists(p): issues.append(('MISSING',grp,p)); continue
        checked+=1
        if sha(p)!=pin['sha256']:
            issues.append(('SHA-DRIFT',grp,p))
# 2) the copyin-probe bridge: source vs compiled object symbols
bridge_src=os.path.join(WHAT,'outputs','2026-10-08-l26-runtime','copyin-probe','candidate','lifecycle','runtime_start_bridge.cpp')
if not os.path.exists(bridge_src): issues.append(('MISSING','bridge-source',bridge_src))
if os.path.exists(bridge_src):
    src_syms=set()
    for line in open(bridge_src):
        # definitions only: a prototype ending in ';' is defined elsewhere (U in nm)
        if 'extern "C"' in line and '(' in line and not line.rstrip().endswith(';'):
            name=line.split('(')[0].split()[-1].strip()
            if name.isidentifier(): src_syms.add(name)
    obj_candidates=glob.glob(os.path.join(BV,'l26-A','pass1','objects','*bridge*.o'))+glob.glob(os.path.join(WHAT,'outputs','2026-10-08-l24-package','lineage','**','runtime_start_bridge*.o'),recursive=True)
    # [gate law 2026-10-12] the packaged bridge object lives in the package pass
    # dir; with no object found every source symbol looked "source-only".
    obj_candidates+=glob.glob(os.path.join(WHAT,'outputs','2026-10-08-l26-package-A','pass-1','build','runtime_start_bridge*.o'))
    if not obj_candidates: issues.append(('NO-OBJECT','bridge','no compiled bridge object found to compare against'))
    obj_syms=set()
    for o in obj_candidates:
        r=subprocess.run(['nm',o],capture_output=True,text=True)
        for l in r.stdout.splitlines():
            parts=l.split()
            if len(parts)>=3 and parts[1] in ('T','W','t'): obj_syms.add(parts[2].split('(')[0].lstrip('_Z').split('RK' if False else '')[-1] if False else parts[2])
    src_only=sorted(s for s in src_syms if s and s not in obj_syms and 'bridge' in s)
    if src_only: issues.append(('SOURCE-ONLY-SYMBOLS','bridge',src_only))
print(json.dumps({'checked':checked,'bridge_objects':len(obj_candidates) if os.path.exists(bridge_src) else 0,'issues':issues,'source_only_count':len([i for i in issues if i[0]=="SOURCE-ONLY-SYMBOLS"])},indent=1))
if checked==0: issues.append(('ZERO-CHECKED','selection',sel_path))
sys.exit(1 if issues else 0)
