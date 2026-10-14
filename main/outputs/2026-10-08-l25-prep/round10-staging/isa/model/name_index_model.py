#!/usr/bin/env python3
"""Host-side model of the [R9-NAME] lookup vs the GUObjectArray walk (Mac-only reasoning aid).
Models: slots (object id or None, unreachable flag), object name comparison index, number,
outer; delete listener -> Dirty; exact Sync (dirty + watched + appended); bucket chains;
rename sweep. Checks that for random workloads the index answer == walk answer, except for
in-place renames not yet swept (counted separately)."""
import random

class World:
    def __init__(s): s.slots=[]; s.objs={}; s.next_id=1; s.dirty=[]
    def new(s, cmp, num, outer, reuse=None):
        oid=s.next_id; s.next_id+=1; s.objs[oid]=dict(cmp=cmp,num=num,outer=outer,unreach=False)
        if reuse is not None: s.slots[reuse]=oid
        else: s.slots.append(oid)
        return oid
    def free(s, i):
        oid=s.slots[i]; s.slots[i]=None; del s.objs[oid]; s.dirty.append(i)
        for o in s.objs.values():  # children lose outer (engine frees children first; model: reparent to None)
            if o['outer']==oid: o['outer']=None
    def live(s, i):
        oid=s.slots[i] if i < len(s.slots) else None
        return oid if oid is not None and not s.objs[oid]['unreach'] else None

def parts_match(w, oid, parts):
    n=0; p=oid
    while p is not None:
        if w.objs[p]['cmp'] not in parts: return False
        p=w.objs[p]['outer']; n+=1
    return n==len(parts)

def walk(w, pred):
    for i in range(len(w.slots)):
        oid=w.live(i)
        if oid is not None and pred(oid): return oid
    return None

class Index:
    def __init__(s): s.objat=[]; s.namecmp={}; s.buckets={}; s.watch=set(); s.synced=0; s.sweep=0
    def link(s,i,cmp):
        old=s.namecmp.get(i)
        if old==cmp: return False
        if old is not None: s.buckets[old].discard(i)
        s.namecmp[i]=cmp; s.buckets.setdefault(cmp,set()).add(i); return old is not None
    def unlink(s,i):
        old=s.namecmp.pop(i,None)
        if old is not None: s.buckets[old].discard(i)
    def observe(s,w,i):
        while len(s.objat)<=i: s.objat.append(None)
        oid=w.live(i)
        if oid is None: s.objat[i]=None; s.unlink(i); s.watch.add(i); return
        s.link(i,w.objs[oid]['cmp']); s.objat[i]=oid
    def sync(s,w):
        while len(s.objat)<len(w.slots): s.objat.append(None)
        for i in w.dirty: s.objat[i]=None; s.watch.add(i)
        w.dirty.clear(); wl=list(s.watch); s.watch.clear()
        for i in wl: s.observe(w,i)
        for i in range(s.synced,len(w.slots)): s.observe(w,i)
        s.synced=len(w.slots)
    def sweep_step(s,w,budget):
        n=s.synced
        for _ in range(min(budget,n)):
            i=s.sweep; o=s.objat[i]
            if o is not None and w.live(i)==o: s.link(i,w.objs[o]['cmp'])
            s.sweep=(i+1)%n
    def lookup(s,w,cmps,pred,budget):
        s.sync(w); s.sweep_step(w,budget)
        c=sorted(i for cmp in set(cmps) for i in s.buckets.get(cmp,()) if s.objat[i] is not None and w.live(i)==s.objat[i])
        for i in c:
            if pred(s.objat[i]): return s.objat[i]
        return None

def run(seed, steps=1500, renames=True):
    r=random.Random(seed); w=World(); ix=Index(); ncmp=40
    for _ in range(300): w.new(r.randrange(ncmp), r.randrange(3), None if r.random()<.3 or not w.slots else w.slots[r.randrange(len(w.slots))])
    ix.sync(w)
    mism=0; renamed_pending=set(); checks=0
    for _ in range(steps):
        a=r.random()
        live=[i for i in range(len(w.slots)) if w.slots[i] is not None]
        if a<.25 and live: w.free(r.choice(live))
        elif a<.5:
            free=[i for i in range(len(w.slots)) if w.slots[i] is None]
            outer=None if r.random()<.3 or not live else w.slots[r.choice(live)]
            w.new(r.randrange(ncmp), r.randrange(3), outer, reuse=(r.choice(free) if free and r.random()<.7 else None))
        elif a<.55 and live:
            o=w.objs[w.slots[r.choice(live)]]; o['unreach']=not o['unreach']
        elif a<.58 and live and renames:  # in-place rename: no delete/create event
            i=r.choice(live); w.objs[w.slots[i]]['cmp']=r.randrange(ncmp); renamed_pending.add(i)
        elif a<.62 and live:  # outer-only rename (Rename(nullptr, NewOuter))
            i=r.choice(live); me=w.slots[i]; older=[w.slots[j] for j in live if w.slots[j]<me]  # outers stay acyclic (engine invariant)
            w.objs[me]['outer']=None if r.random()<.5 or not older else r.choice(older)
        else:
            parts=set(r.randrange(ncmp) for _ in range(r.randrange(1,4)))
            if r.random()<.5:   # StaticFindObject_InternalNoToStringFromNames shape
                cmps=list(parts); pred=lambda o,parts=parts: parts_match(w,o,parts)
            else:               # FindObject searcher shape (name first, then a top-level filter)
                c=next(iter(parts)); cmps=[c]; pred=lambda o,c=c: w.objs[o]['cmp']==c and w.objs[o]['outer'] is None
            got=ix.lookup(w,cmps,pred,budget=64); want=walk(w,pred); checks+=1
            if got!=want: mism+=1
            if ix.sweep==0: renamed_pending.clear()
    return checks, mism

if __name__=='__main__':
    tot=0; bad=0
    for seed in range(12):
        c,m=run(seed, renames=False); tot+=c; bad+=m
    print('no in-place renames: %d lookups, %d mismatches (must be 0)' % (tot,bad))
    tot=0; bad=0
    for seed in range(12):
        c,m=run(seed, renames=True); tot+=c; bad+=m
    print('with in-place renames (eventual via sweep): %d lookups, %d mismatches (expected small, >0 possible)' % (tot,bad))
