#!/usr/bin/env python3
"""Bind current classifier output to the generic gate; no original edits."""
import argparse
import hashlib
import json
from pathlib import Path
from directive_gate import checked, evaluate, require

def reference(p):
    return {'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}

def coverage(c, s):
    rows=c['packages']
    require(len(rows)>0,'zero classified packages')
    matched=sum(t['matched_package_count'] for t in s['targets'])
    stored=sum(r.get('base_replaced') is True for r in rows)
    unmatched=sum(m['unmatched_package_count'] for m in s['mods'])
    require(matched+unmatched==len(rows),'selection and classification counts differ')
    return {'classified_packages':len(rows),'captured_path_overlaps':matched,
            'known_classifier_store_membership':stored,'store_membership_unknown':len(rows)-stored,
            'cross_container_overlap_gap':matched-stored,'unmatched_in_captured_indexes':unmatched,
            'unmatched_meaning':'UNKNOWN outside provided indexes; never inferred added',
            'catalogue_completeness_verified':s.get('catalogue_completeness_verified') is True}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('classification',type=Path); ap.add_argument('selection',type=Path)
    ap.add_argument('--receipts',type=Path); ap.add_argument('--out',required=True,type=Path)
    a=ap.parse_args()
    dest=a.out.resolve(); home=Path(__file__).resolve().parent
    require(dest.is_relative_to(home),'output must remain in cap08')
    require(dest not in [p.resolve() for p in (a.classification,a.selection,a.receipts) if p], 'input overwrite refused')
    c=json.loads(a.classification.read_text()); s=json.loads(a.selection.read_text())
    # Verify all discovery input bytes, not merely its summary labels.
    for item in s['bases']+s['mods']:
        checked(item)
    counts=coverage(c,s)
    m=json.loads(a.receipts.read_text()) if a.receipts else {'schema':1,'classification':reference(a.classification),'catalogue_complete':False,'containers':[],'packages':{}}
    # An incomplete observed catalogue cannot be promoted by a receipts flag.
    m['catalogue_complete']=m.get('catalogue_complete') is True and counts['catalogue_completeness_verified']
    result=evaluate(c,m)
    result.update(coverage=counts, classification_source=reference(a.classification), selection_source=reference(a.selection))
    a.out.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'result':result['result'],'actionable_count':result['actionable_count'],'coverage':counts}))
    return 0 if result['result']=='VALIDATED_DIRECTIVES' else 2

if __name__=='__main__': raise SystemExit(main())
