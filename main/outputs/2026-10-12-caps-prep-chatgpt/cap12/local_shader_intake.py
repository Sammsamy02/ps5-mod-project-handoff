#!/usr/bin/env python3
"""Intake already-local UCAS chunks only; no acquisition or shader decoding.

A JSON job supplies hash-pinned parser, TOC, UCAS and explicit type8/9 IDs.
Source hashing is capped at 64MiB, each chunk at 4MiB, job at 8MiB. A hash-pinned
local slice can declare its physical origin; no full-UCAS identity is implied.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path

MAX_SOURCE=64*1024*1024
MAX_CHUNK=4*1024*1024
MAX_TOTAL=8*1024*1024

def demand(ok, why):
    if not ok: raise ValueError(why)

def file_hash(handle, cap):
    handle.seek(0); h=hashlib.sha256(); n=0
    while True:
        b=handle.read(min(1024*1024,cap-n+1))
        if not b: break
        n+=len(b); demand(n<=cap,'source exceeds hash/read cap'); h.update(b)
    return h.hexdigest()

def extract_verified(toc,row,handle,source_sha256,blake3_20,*,max_source_bytes=MAX_SOURCE,max_chunk_bytes=MAX_CHUNK,physical_base=0):
    demand(0<=row<len(toc.ids) and (toc.ids[row][11]&63) in (8,9),'explicit shader-related row required')
    demand(toc.partition_count==1 and toc.partition_size==(1<<64)-1,'unbounded single local partition required')
    demand(type(physical_base) is int and physical_base>=0,'invalid physical slice origin')
    before=os.fstat(handle.fileno()); demand(0<before.st_size<=max_source_bytes,'source exceeds size cap')
    demand(file_hash(handle,max_source_bytes)==source_sha256,'source sha256 mismatch')
    logical,length=toc.offset_length(row)
    demand(0<length<=max_chunk_bytes and toc.block_size>0,'invalid/capped chunk length')
    remaining=length; cursor=logical; data=bytearray()
    while remaining:
        index,skip=divmod(cursor,toc.block_size)
        demand(0<=index<toc.block_count,'logical range outside block table')
        physical,stored,decoded,method=toc.block(index)
        demand(method==0 and 0<stored==decoded<=toc.block_size,'unsupported compressed block')
        demand(0<=skip<decoded and physical>=physical_base and physical+stored<=physical_base+before.st_size,'block outside provided source')
        take=min(remaining,decoded-skip)
        # A short intermediate block would require a different logical geometry.
        demand(take==remaining or decoded==toc.block_size,'chunk spans short intermediate block')
        handle.seek(physical-physical_base+skip); part=handle.read(take)
        demand(len(part)==take,'short local bounded read')
        data.extend(part); remaining-=take; cursor+=take
    payload=bytes(data); meta=toc.meta(row)
    demand(len(meta)==24 and meta[20] in (0,2),'unsupported/missing IoHash metadata')
    demand(blake3_20(payload)==meta[:20],'chunk IoHash mismatch')
    # Detect source replacement/mutation after hashing and bounded reconstruction.
    after=os.fstat(handle.fileno())
    demand((before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns)==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns),'source changed during intake')
    demand(file_hash(handle,max_source_bytes)==source_sha256,'source changed after extraction')
    return payload

def checked(ref,cap):
    p=Path(ref['path']); demand(p.is_file(),'local source missing')
    demand(p.stat().st_size<=cap,'local source exceeds cap')
    with p.open('rb') as f: demand(file_hash(f,cap)==ref['sha256'],'pinned file hash mismatch')
    return p

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('job',type=Path); ap.add_argument('--out-dir',required=True,type=Path); a=ap.parse_args()
    home=Path(__file__).resolve().parent; dest=a.out_dir.resolve()
    demand(dest.is_relative_to(home) and dest!=home,'output must be a new directory under cap12')
    demand(not dest.exists(),'existing output directory refused')
    job=json.loads(a.job.read_text()); demand(job.get('schema')==1,'unsupported job schema')
    pp=checked(job['parser'],2*1024*1024); tp=checked(job['toc'],64*1024*1024); up=checked(job['ucas'],MAX_SOURCE)
    demand(up.suffix=='.ucas','only explicitly provided local UCAS/full-or-slice paths accepted')
    kind=job['ucas'].get('kind','full'); demand(kind in ('full','slice'),'unknown source scope')
    physical_base=job['ucas'].get('physical_base',0)
    demand(kind=='slice' or physical_base==0,'full source cannot declare nonzero origin')
    if kind=='slice': demand('physical_base' in job['ucas'],'slice origin missing')
    demand(not any(p.resolve().is_relative_to(dest) for p in (pp,tp,up,a.job)),'output contains input path')
    spec=importlib.util.spec_from_file_location('pinned_local_toc_parser',pp); mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    # Relax reserved header bytes only; keep parser admission and perfect-hash
    # lookup validation before selecting/extracting any source payload.
    toc=mod.parse_toc(tp.read_bytes(),strict_admission=False)
    mod.validate_lookup(toc)
    ids=job.get('chunk_ids'); demand(isinstance(ids,list) and 0<len(ids)<=16 and len(set(ids))==len(ids),'invalid/zero/duplicate selected chunk ids')
    lookup={cid.hex():i for i,cid in enumerate(toc.ids)}; accepted=[]; total=0
    with up.open('rb') as handle:
        for cid in ids:
            demand(cid in lookup,'chunk id absent from pinned TOC'); row=lookup[cid]
            _,length=toc.offset_length(row); total+=length; demand(total<=MAX_TOTAL,'job output cap exceeded')
            data=extract_verified(toc,row,handle,job['ucas']['sha256'],mod.blake3_20,physical_base=physical_base)
            accepted.append((cid,row,data))
    # Nothing is written until every selected payload validates. Exclusive files
    # preserve earlier outputs and source artifacts.
    dest.mkdir()
    receipts=[]
    for cid,row,data in accepted:
        with (dest/(cid+'.bin')).open('xb') as f: f.write(data)
        receipts.append({'chunk_id':cid,'toc_row':row,'chunk_type':toc.ids[row][11]&63,
                         'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),
                         'iohash_20':toc.meta(row)[:20].hex(),'iohash_verified':True,'source_scope':kind,'source_physical_base':physical_base})
    receipt={'schema':1,'label':'Mac-only','result':'LOCAL_PAYLOAD_VERIFIED','job_sha256':hashlib.sha256(a.job.read_bytes()).hexdigest(),
             'sources':{k:job[k] for k in ('parser','toc','ucas')},'chunks':receipts,'items_checked':len(receipts),
             'limits':'Provided full file or slice hash verified, but no full original UCAS hash/identity claim for slices. Stored bytes reconstructed and hash-verified only; no shader map, manifest semantics, code boundary or instruction decoded.'}
    with (dest/'receipt.json').open('x') as f: json.dump(receipt,f,indent=2); f.write('\n')
    print(json.dumps({'result':receipt['result'],'items_checked':len(receipts),'bytes':total}))

if __name__=='__main__': main()
