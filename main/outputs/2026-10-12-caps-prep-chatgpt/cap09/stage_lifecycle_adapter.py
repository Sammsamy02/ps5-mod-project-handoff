#!/usr/bin/env python3
"""Exact-once derived source staging only. Never writes original/runtime files."""
import argparse,hashlib,json
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]

def derive(source, expected_sha):
    if hashlib.sha256(source).hexdigest()!=expected_sha:raise ValueError('source_hash_mismatch')
    pairs=[(b'start_mod()',b'mb_cppmod_source_start_mod()'),(b'uninstall_mod(',b'mb_cppmod_source_uninstall_mod(')]
    if b'mb_cppmod_source_' in source:raise ValueError('already_derived')
    for old,new in pairs:
        if source.count(old)!=1:raise ValueError('export_anchor_count')
    result=source
    for old,new in pairs:result=result.replace(old,new,1)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--mod',choices=['EventViewerMod','KismetDebuggerMod'],required=True);parser.add_argument('--stage-dir',type=Path)
    args=parser.parse_args();plan=json.loads((HERE/'LIFECYCLE-STAGE-PLAN.json').read_text());row=plan['mods'][args.mod]
    source=(ROOT/row['path']).read_bytes();derived=derive(source,row['sha256']);wrapper=(HERE/'staged/lifecycle_adapter.cpp').read_bytes()
    if hashlib.sha256(wrapper).hexdigest()!=plan['wrapper_sha256']:raise ValueError('wrapper_hash_mismatch')
    receipt={'label':'design-only source staging, Mac-only checks','runtime_ready':False,'original_source':row['path'],'original_sha256':row['sha256'],'derived_target':row['derived_target'],'derived_sha256':hashlib.sha256(derived).hexdigest(),'wrapper_sha256':plan['wrapper_sha256'],'applied_to_original':False,'stage_created':False}
    if args.stage_dir:
        stage=args.stage_dir.absolute()
        # Derived output must remain in our cap09 tree; no existing/symlink targets.
        if stage.exists() or stage.is_symlink():raise ValueError('stage_directory_already_exists')
        if HERE.resolve() not in stage.parent.resolve().parents and stage.parent.resolve()!=HERE.resolve():raise ValueError('stage_directory_outside_cap09')
        stage.mkdir(parents=False,exist_ok=False);target=stage/row['derived_target'];target.parent.mkdir(parents=True);target.write_bytes(derived);(stage/'lifecycle_adapter.cpp').write_bytes(wrapper)
        receipt['stage_created']=True;(stage/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
