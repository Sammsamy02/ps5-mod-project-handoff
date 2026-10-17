#!/usr/bin/env python3
"""Local global-cache metadata hypotheses; does not decode shader instructions."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

def census(records, bound, prefix_bytes=96):
    if bound<=0 or prefix_bytes<=0 or prefix_bytes%4:
        raise ValueError('positive bound and aligned prefix required')
    columns={}; total=0
    for image, starts, objects_at in records:
        valid=[s for s in starts if s is not None]
        if any(type(s) is not int or not objects_at<=s<len(image) for s in valid):
            raise ValueError('object start outside frozen object region')
        unique=sorted(set(valid)); next_start={s:unique[i+1] if i+1<len(unique) else len(image) for i,s in enumerate(unique)}
        for start in valid:
            total+=1
            # Bound scan to the next object start; subdata remains unclassified.
            limit=min(prefix_bytes,next_start[start]-start)
            for rel in range(0,limit-3,4):
                columns.setdefault(rel,Counter())[struct.unpack_from('<I',image,start+rel)[0]]+=1
    if total==0: raise ValueError('zero shader objects checked')
    rows=[]
    for rel, values in sorted(columns.items()):
        count=sum(values.values())
        rows.append({'relative_offset':rel,'objects_sampled':count,'objects_total':total,
                     'all_objects_sampled':count==total,'min_value':min(values),'max_value':max(values),
                     'unique_values':len(values),'within_hypothesized_bound':sum(n for v,n in values.items() if v<bound),
                     'all_values_in_bound':max(values)<bound,'degenerate':len(values)==1})
    return {'label':'Mac-only / hypothesis','decoded_resource_index':False,'instruction_bytes_decoded':0,
            'objects_checked':total,'bound_hypothesis':bound,'prefix_bytes':prefix_bytes,
            'limits':'Integer range matches do not establish field identity. Object-start gaps are not object sizes; subdata may occur in scanned prefixes.',
            'candidates':[r for r in rows if r['all_values_in_bound'] and r['all_objects_sampled']], 'all_offsets':rows}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input',type=Path); p.add_argument('--codec',required=True,type=Path)
    p.add_argument('--bound-hypothesis',required=True,type=int)
    p.add_argument('--out',required=True,type=Path); a=p.parse_args()
    dest=a.out.resolve()
    if not dest.is_relative_to(Path(__file__).resolve().parent) or dest in (a.input.resolve(),a.codec.resolve()):
        raise ValueError('output must stay in cap12 and cannot overwrite input/codec')
    spec=importlib.util.spec_from_file_location('existing_gsc_codec',a.codec); module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    raw=a.input.read_bytes(); model=module.decode(raw)
    records=[]
    for s in model['sections']:
        if s['share_code']!=1 or not s.get('frozen'):
            raise ValueError('unsupported inline code or opaque section; refusing partial census')
        f=s['frozen']; image=raw[s['frozen_offset']:s['frozen_offset']+s['frozen_size']]
        records.append((image,f['shader_object_offsets'],f['objects_at']))
    result=census(records,a.bound_hypothesis)
    result.update(input_sha256=hashlib.sha256(raw).hexdigest(), codec_sha256=hashlib.sha256(a.codec.read_bytes()).hexdigest(),sections_checked=len(records))
    a.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'objects_checked':result['objects_checked'],'candidate_offsets':[r['relative_offset'] for r in result['candidates']],'decoded_resource_index':False}))

if __name__=='__main__': main()
