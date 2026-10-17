"""Recompile the ABI probes against the final native target source/config, read-only."""
from pathlib import Path
import difflib,hashlib,json,os,re,subprocess
H=Path(__file__).resolve().parents[1];W=H/'cross-work';T=H.parent/'cap10-port-closure/target-work';S=H/'target-config-final';S.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
expected='9f0324038b43cb4e01a20bb00b3e8c8e2a8083d9a5895aa8cbe5c6d032c34ee6'
assert sha(T/'config.h')==expected
for rel in ['config.h','mono/eglib/eglib-config.h']:
 p=S/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((T/rel).read_bytes())
with (H/'evidence/TARGET-CONFIG-FINAL.diff').open('x') as f:f.write(''.join(difflib.unified_diff((H/'target-config/config.h').read_text().splitlines(True),(S/'config.h').read_text().splitlines(True),'initial-target-config.h','final-target-config.h')))
original=json.loads((H/'evidence/OFFSETS-COMPLETE.json').read_text());inv=json.loads((H.parent/'cap10-port-research/LOCAL-INVENTORY.json').read_text());env=os.environ.copy();env['LLVM_CONFIG']=str(Path(inv['llvm_bin'])/'llvm-config')
def values(p):return {n:int(v) for n,v in re.findall(r'@(\w+) = .*?constant i64 (\d+)',p.read_text())}
rows=[]
for c in original['compilations']:
 part=c['part'];src=H/f'artifacts/{part}-offsets-complete.c';dest=H/f'artifacts/{part}-offsets-final-target.c';ir=dest.with_suffix('.ll')
 dest.write_text(src.read_text().replace(str(W),str(T)))
 cmd=[a.replace(str(H/'target-config'),str(S)).replace(str(W),str(T)).replace(str(src),str(dest)).replace(str(src.with_suffix('.ll')),str(ir)) for a in c['ir_command']]
 with (H/f'logs/FINAL-TARGET-ABI-{part.upper()}.log').open('x') as f:r=subprocess.run(cmd,env=env,cwd=H,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,(part,r.returncode)
 before=values(src.with_suffix('.ll'));after=values(ir);assert len(before)>0 and before==after,(part,'layout mismatch')
 rows.append({'part':part,'command':cmd,'exit_code':r.returncode,'constant_count':len(before),'all_offsets_sizes_alignments_equal':True,'ir_sha256':sha(ir)})
assert sha(T/'config.h')==expected
out={'final_config_sha256':sha(S/'config.h'),'original_config_sha256':sha(H/'target-config/config.h'),'final_eglib_config_sha256':sha(S/'mono/eglib/eglib-config.h'),'final_target_source':str(T),'source_commit':'0cbf0e290c31adb476f9de0fa44b1d8829affa40','compilations':rows,'target_execution':False}
with (H/'evidence/FINAL-TARGET-ABI-AUDIT.json').open('x') as f:json.dump(out,f,indent=2);f.write('\n')
print(json.dumps({'final_config_sha256':out['final_config_sha256'],'layout_constants_equal':{r['part']:r['constant_count'] for r in rows}},indent=2))
