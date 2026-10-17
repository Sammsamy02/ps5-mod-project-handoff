#!/usr/bin/env python3
"""Select O1 fmt opaque-stdio object, retain completed host evidence, and package replacements."""
from pathlib import Path
import hashlib
import json
import os
import subprocess

R=Path(__file__).resolve().parent
W=R.parents[2]
T=W/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
V=R/'verification-3'
O=R/'final-native-1'
O.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
v=json.loads((V/'receipt.json').read_text())
for field in ['compile_dependencies','tools','local_scripts','original_native_inputs']:
    for p,h in v[field].items():assert sha(p)==h,p
commands=json.loads((V/'commands.json').read_text())
inputs=json.loads((R/'replacement-inputs.json').read_text())
units=[]
for old in v['replacement_units']:
    row=next(x for x in inputs['units'] if x['unit']['name']==old['name'])
    rec=dict(row['unit'])
    rec.update(source=old['source'],source_sha256=old['source_sha256'],
               object=old['object'],object_sha256=old['object_sha256'],exit=0,
               original_object=old['original_object'],original_object_sha256=old['original_object_sha256'])
    c=next(c for c in commands if c['name']=='native-'+old['name'])
    if old['name']=='fmt':
        args=[str(O/'fmt.o') if a==old['object'] else str(O/'fmt.d') if a==str(V/'fmt.d')
              else '-O1' if a=='-O0' else a for a in c['argv']]
        env=dict(os.environ,LLVM_CONFIG=str(T/'llvm-config'))
        for k in ['CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH']:env.pop(k,None)
        p=subprocess.run(args,env=env,capture_output=True,text=True,timeout=60)
        (O/'fmt-build.stdout').write_text(p.stdout);(O/'fmt-build.stderr').write_text(p.stderr)
        assert p.returncode==0,p.stderr
        rec.update(object=str(O/'fmt.o'),object_sha256=sha(O/'fmt.o'),
                   native_optimization='O1',fmt_use_fallback_file=True)
        assert sha(O/'fmt.o')==sha(R/'fmt-o1-candidate.o')
        c=dict(argv=args,exit=0)
    else:c=dict(argv=c['argv'],exit=c['exit'],original_native_compile_reused=True)
    rec['object_bytes']=Path(rec['object']).stat().st_size
    cmd=O/(old['name']+'-command.json');cmd.write_text(json.dumps(c,indent=2)+'\n')
    rec['command']=str(cmd);units.append(rec)
provider=v['owned_fgetc']
assert sha(provider['object'])==provider['object_sha256']
pc=next(c for c in commands if c['name']=='native-owned-fgetc')
(O/'owned-fgetc-command.json').write_text(json.dumps(dict(argv=pc['argv'],exit=pc['exit'],original_native_compile_reused=True),indent=2)+'\n')
added=dict(name='owned-fgetc',source=provider['source'],source_sha256=provider['source_sha256'],
           object=provider['object'],object_sha256=provider['object_sha256'],
           object_bytes=Path(provider['object']).stat().st_size,exit=0,
           command=str(O/'owned-fgetc-command.json'),defined=['fgetc'],undefined=['fread'],hidden=True)
group=O/'replacement-stdio-consumers.o'
args=[str(T/'ld.lld'),'-r',*[u['object'] for u in units],added['object'],'-o',str(group)]
p=subprocess.run(args,capture_output=True,text=True,check=True)
(O/'group-link-command.json').write_text(json.dumps(dict(argv=args,exit=p.returncode),indent=2)+'\n')
def nm(path,undefined):
    a=[str(T/'llvm-nm'),'--format=posix','--extern-only','--undefined-only' if undefined else '--defined-only',str(path)]
    p=subprocess.run(a,capture_output=True,text=True,check=True)
    (O/(Path(path).stem+('-undefined' if undefined else '-defined')+'.stdout')).write_text(p.stdout)
    return {l.split()[0]:l.split()[1] for l in p.stdout.splitlines()}
before=set();after=set()
for u in units:
    before.update(nm(u['original_object'],True));after.update(nm(u['object'],True))
    bd=nm(u['original_object'],False);ad=nm(u['object'],False)
    assert {n for n,k in bd.items() if k in ['T','D','B','R','S']}<=set(ad)
before_current=set(nm(group,True))
assert not {'__isthreaded','__srget','getc','putc_unlocked','flockfile','funlockfile','fgetc'}&before_current
assert set(nm(added['object'],True))=={'fread'}
cross=json.loads((R/'fmt-o1-cross-consumer-check.json').read_text())
assert cross['strong_definitions_preserved'] and not cross['removed_symbols_required_by_other_stage27_units']
recipe=dict(units=units,additional_objects=[added],
           source_receipt=str(V/'receipt.json'),source_receipt_sha256=sha(V/'receipt.json'),
           production_native_rebuilds=3,integrated=False)
(O/'integration-units.json').write_text(json.dumps(recipe,indent=2)+'\n')
final=dict(target='x86_64-sie-ps5',integration_recipe=str(O/'integration-units.json'),
           integration_recipe_sha256=sha(O/'integration-units.json'),
           replacement_group=str(group),replacement_group_sha256=sha(group),
           source_units=3,owned_fgetc=added,strong_definitions_preserved=True,
           per_unit_removed_undefined=sorted(before-after),per_unit_added_undefined=sorted(after-before),
           group_undefined=sorted(before_current),new_unknown_public_runtime_requirements=[],
           group_absent=['__isthreaded','__srget','getc','putc_unlocked','flockfile','funlockfile','fgetc'],
           host_evidence_receipt=str(V/'receipt.json'),host_evidence_sha256=sha(V/'receipt.json'),
           host_adapted_checks_pass=True,host_original_fmt_control_pass=False,
           host_original_fmt_control_note='Both original FILE-optimization controls emitted assertion at format-inl.h:1673, then timed out during abort/reaping; fallback passes public-contract checks.',
           fmt_removed_weak_definitions_needed_by_other_stage27_units=[],
           fmt_cross_consumer_survey=str(R/'fmt-o1-cross-consumer-check.json'),
           fmt_host_and_selected_native_optimization='O1',
           native_dependencies={str(Path(x)):sha(Path(x)) for x in (O/'fmt.d').read_text().replace('\\\n','').split(':',1)[1].split()},
           preserved_original_input_hashes=v['original_native_inputs'],
           native_execution=False,console_contact=False,shared_sdk_modified=False,stage27_modified=False,
           integrated=False,finalization_script_sha256=sha(__file__))
(O/'receipt.json').write_text(json.dumps(final,indent=2)+'\n')
print(json.dumps(dict(integration_recipe=final['integration_recipe'],replacement_group_sha256=final['replacement_group_sha256'],
    replacements=[dict(name=u['name'],object=u['object'],sha256=u['object_sha256']) for u in units],
    owned_fgetc_sha256=added['object_sha256'],absent=final['group_absent'],
    removed=final['per_unit_removed_undefined'],added=final['per_unit_added_undefined']),indent=2))
