#!/usr/bin/env python3
"""Read immutable producer inputs; compile and emit verification only in this audit directory."""
from pathlib import Path
import hashlib,json,os,subprocess,sys
root=Path.cwd();own=Path(__file__).resolve().parent;source=root/'outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=json.loads((source/'SOURCE-HASHES.json').read_text())
for name,row in manifest['files'].items():
 p=Path(row['path']);assert p==source/name and sha(p)==row['sha256'] and p.stat().st_size==row['bytes'],name
script=(source/'validate_totals.py').read_text();old='OUT=Path(__file__).resolve().parent'
assert script.count(old)==1
script=script.replace(old,'OUT=Path('+repr(str(source))+');OWN=Path(__file__).resolve().parent')
assert script.count("(OUT/'VERIFICATION.json').write_text")==1
script=script.replace("(OUT/'VERIFICATION.json').write_text","(OWN/'REPLAY-VERIFICATION.json').write_text")
(own/'producer_validator_replay.py').write_text(script)
r=subprocess.run([sys.executable,str(own/'producer_validator_replay.py')],cwd=root,text=True,capture_output=True)
(own/'REPLAY.log').write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
commands=json.loads((source/'COMPILE-COMMANDS-FINAL.json').read_text());out=own/'recompiled';out.mkdir(exist_ok=True);results=[]
for c in commands:
 argv=list(c['argv']);idx=argv.index('-o')+1;original=Path(argv[idx]);argv[idx]=str(out/(c['name']+'.o'))
 env=dict(os.environ,LLVM_CONFIG=c['LLVM_CONFIG']);r=subprocess.run(argv,cwd=root,env=env,text=True,capture_output=True)
 (out/(c['name']+'.log')).write_text(r.stdout+r.stderr)
 assert r.returncode==0,(c['name'],r.stderr)
 assert sha(argv[idx])==sha(original),(c['name'],'object byte mismatch')
 results.append({'name':c['name'],'argv':argv,'LLVM_CONFIG':c['LLVM_CONFIG'],'exit_code':r.returncode,'object_sha256':sha(argv[idx]),'matches_frozen_object_byte_for_byte':True})
# Reconfirm the producer freeze after replay/compiles; it was never an output destination.
for name,row in manifest['files'].items():assert sha(source/name)==row['sha256'],name
result={'pass':True,'manifest_files_verified':len(manifest['files']),'producer_manifest_sha256':sha(source/'SOURCE-HASHES.json'),'fact_source_sha256':sha(source/'OFFICIAL-TOTAL-FACTS.json'),'replay_transformations':['Fix input OUT to immutable producer directory','Route only VERIFICATION write to owned audit directory'],'replay_source_sha256':sha(own/'producer_validator_replay.py'),'recompiled_controls':results,'producer_frozen_files_unchanged':True}
(own/'REPLAY-RECEIPT.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'pass':True,'frozen_files':len(manifest['files']),'PS5_recompiled_byte_identical_objects':len(results)}))
