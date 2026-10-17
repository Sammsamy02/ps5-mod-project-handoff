#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,subprocess,os,shlex,collections
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];OUT=HERE/'combined-stop-pad';OUT.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p).resolve();return {'path':str(p),'sha256':sha(p),'size':p.stat().st_size}
primary_file=HERE/'COMPOSITION-RECEIPT.json';primary=json.loads(primary_file.read_text());command=json.loads((HERE/'LINK-COMMAND.json').read_text())['argv'];rows={r['name']:r for r in primary['selected_units']}
ackfile=ROOT/'outputs/2026-10-08-runtime-stop-candidate/target-cpp-with-pinned-toolchain/TARGET-CPP-RECEIPT.json';ackreceipt=json.loads(ackfile.read_text());ack=next(r for r in ackreceipt['records'] if r['name']=='runtime_ack_supplier')
padfile=ROOT/'outputs/2026-10-08-pad-recovery-candidate/target/TARGET-RECEIPT.json';pad=json.loads(padfile.read_text());replacements=[]
for name,record in [('module-runtime-ack-supplier',ack),('native-pad-PS5PadNative',pad)]:
 assert record['exit']==0;old=rows[name];assert sha(old['object'])==old['object_sha256']==record['baseline_object']['sha256']
 for key in ['source','object','dependency']:assert sha(record[key]['path'])==record[key]['sha256'],(name,key)
 dep=Path(record['dependency']['path']).read_text().replace('\\\n',' ');paths=shlex.split(dep.split(':',1)[1]);assert not any(t.endswith('/Unreal/UFunctionStructs.hpp') for t in paths)
 assert command.count(old['object'])==1;command[command.index(old['object'])]=record['object']['path']
 replacements.append({'name':name,'old_object':pin(old['object']),'new_object':pin(record['object']['path']),'new_source':pin(record['source']['path']),'actual_dependency':pin(record['dependency']['path']),'changed_header_excluded':True,'recorded_TU_undefined_added':record['undefined_added'],'recorded_TU_undefined_removed':record['undefined_removed']})
output=OUT/'runtime-r5-stop-pad.o';command[command.index('-o')+1]=str(output);inputs=[t for i,t in enumerate(command) if t.endswith(('.o','.a')) and i!=command.index('-o')+1];assert len(inputs)==len(set(inputs))==387
(OUT/'LINK-COMMAND.json').write_text(json.dumps({'argv':command,'ordered_inputs':inputs,'replacements':replacements,'primary_link_command':pin(HERE/'LINK-COMMAND.json')},indent=2)+'\n')
r=subprocess.run(command,capture_output=True,text=True,cwd=ROOT,timeout=240);(OUT/'link.stdout').write_text(r.stdout);(OUT/'link.stderr').write_text(r.stderr);assert r.returncode==0,r.stderr
plan=json.loads((HERE/'COMPILE-PLAN.json').read_text());unit=next(x for x in plan['units'] if x['name']=='unreal-ufunction');nm=Path(unit['argv'][0]).with_name('prospero-nm');env=dict(os.environ,LLVM_CONFIG=unit['LLVM_CONFIG']);sets={}
for label,o in [('primary',HERE/'runtime-r5.o'),('combined',output)]:
 x=subprocess.run([str(nm),'-u',str(o)],capture_output=True,text=True,env=env,check=True);(OUT/(label+'.undefined.nm')).write_text(x.stdout);sets[label]={l.split()[-1] for l in x.stdout.splitlines() if l.split()}
x=subprocess.run([str(nm),'--defined-only',str(output)],capture_output=True,text=True,env=env,check=True);(OUT/'combined.defined.nm').write_text(x.stdout);symbols=[l.split()[-1] for l in x.stdout.splitlines() if l.split()]
exports={s:symbols.count(s) for s in ['bridge_native_code_residence_required','bridge_native_pad_code_residence_required']};assert exports=={s:1 for s in exports}
receipt={'status':'COMBINED_ACK_STOP_PAD_RELOCATABLE_EXPERIMENT_PASS','scope':'Root ack+pad source copies in complete R5 runtime; C-entry/start-bridge consumer changes remain outside composition and required before qualification','primary_composition_receipt':pin(primary_file),'primary_source_selection':pin(HERE/'SOURCE-SELECTION.primary.json'),'root_ack_receipt':pin(ackfile),'root_pad_receipt':pin(padfile),'link_command':pin(OUT/'LINK-COMMAND.json'),'replacements':replacements,'object':pin(output),'full_link_inputs':387,'selected_units':370,'primary_undefined_count':len(sets['primary']),'combined_undefined_count':len(sets['combined']),'undefined_added':sorted(sets['combined']-sets['primary']),'undefined_removed':sorted(sets['primary']-sets['combined']),'C_exports_defined_once':exports,'thread_authority_selected':False,'pad_entry_consumer_integrated':False,'package_promoted':False,'console_contact':False}
(OUT/'COMPOSITION-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({'status':receipt['status'],'object':receipt['object'],'undefined_added':receipt['undefined_added'],'undefined_removed':receipt['undefined_removed'],'C_exports':exports}))
