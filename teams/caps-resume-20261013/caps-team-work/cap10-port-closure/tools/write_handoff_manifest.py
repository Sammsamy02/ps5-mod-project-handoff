#!/usr/bin/env python3
"""Select already-verified source/build inputs for handoff; never changes a build."""
from pathlib import Path
import hashlib,json
HERE=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
selected=set()
for name in ['README.md','LOCAL-INPUTS.json']:
 selected.add(HERE/name)
for directory in ['tools','patches','pristine','licenses','handoff/runtime-headers']:
 selected.update(p for p in (HERE/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
# Keep exact derived patch targets and the generated configuration as inspection inputs.
for row in json.loads((HERE/'evidence/CONFIGURATION-STAGE.json').read_text())['bindings']:
 selected.add(HERE/'derived'/row['path'])
selected.update((HERE/'evidence').glob('*.json'))
selected.update((HERE/'evidence').glob('*.patch'))
selected.update((HERE/'evidence').glob('*.d'))
for name in ['COMPILER-MODE.txt','CORLIB-ASSEMBLER-MODE.txt','OWNER-ROOTING-RELOCATIONS.txt','FINAL-ELF-RELOCATIONS.txt','EGLIB-TARGET-ELF.txt']:
 selected.add(HERE/'evidence'/name)
selected.update(p for p in (HERE/'logs').iterdir() if p.is_file())
for name in ['LinkOnlyObserver.cpp','LinkOnlyProvider.c','ELF-IMPORTS.txt']:
 selected.add(HERE/'owner-link'/name)
items=[]
for p in sorted(selected):
 items.append(dict(source=str(p),packet_path=str(p.relative_to(HERE)),sha256=sha(p),bytes=p.stat().st_size))
for rel,dest in [('target-work/mono/mini/.libs/libmonosgen-2.0.a','handoff/lib/libmonosgen-2.0.a'),('target-work/data/config','handoff/runtime-config/mono/config')]:
 p=HERE/rel;items.append(dict(source=str(p),packet_path=dest,sha256=sha(p),bytes=p.stat().st_size))
extra=[]
for rel in ['artifacts/mscorlib.full-interp.ps5.o','managed-input/mscorlib.dll','evidence/AOT-ARTIFACT.json']:
 p=HERE.parent/'cap10-cross-aot'/rel
 extra.append(dict(source=str(p),sha256=sha(p),bytes=p.stat().st_size,owner='cap10-cross-aot',note='Parent packet must include the matching target corlib input/evidence; kept under its owning component'))
result=dict(version=1,status='Mac source/build/static-link closure complete for exact minimal owner; console admission absent',files=items,count=len(items),total_selected_bytes=sum(p['bytes'] for p in items),external_matching_corlib_inputs=extra,excluded=['target-work full source/object tree except selected library/config','owner-link/mono-owner-link-only.elf never executed','large runtime/corlib raw relocation dumps; hashes/counts are in audit receipt','owner-link full symbol dump/link map; output hashes bound in link receipt'],no_prx_build=True,no_target_execution=True,no_production_admission=True)
(HERE/'HANDOFF-MANIFEST.json').write_text(json.dumps(result,indent=2)+'\n')
print('Selected',result['count'],'files',result['total_selected_bytes'],'bytes; external matching corlib inputs',len(extra))
