"""SDK dependency-only inventory for immutable source-policy/header consumers."""
from pathlib import Path
import importlib.util,json,concurrent.futures
r=Path(__file__).resolve().parent;sp=r.parent;original=sp/'ue4ss-native-scan-context-rebuild-1/inventory.py';spec=importlib.util.spec_from_file_location('retained_inventory',original);inv=importlib.util.module_from_spec(spec);spec.loader.exec_module(inv)
inv.r=r;inv.base=sp/'ue4ss-unreal-native-build-1/consolidated-19/result.json';inv.out=r/'inventory-1';inv.out.mkdir(exist_ok=False)
original_command=inv.command_for
prefix=['-I',str(r/'include'),'-I',str(sp/'ue4ss-native-mapped-candidates-1/include')]
def command_for(u):
 a,receipt=original_command(u);a=a[:1]+prefix+a[1:];return a,receipt
inv.command_for=command_for
base=json.loads(inv.base.read_text());rows=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 for f in concurrent.futures.as_completed([pool.submit(inv.probe,u)for u in base['units']]):
  row=f.result();dep=Path(row['dependency_file']);deps=inv.dependencies(dep)if row['dependency_exit']==0 else [];hits=[p for p in deps if Path(p).name in ('OwnedMappedScanSource.hpp','OwnedMappedCandidateSelection.hpp')];row['policy_header_dependencies']={p:inv.sha(p)for p in hits};row['affected']=bool(hits);rows.append(row)
  if len(rows)%50==0:print('SDK dependency probes',len(rows),'/',len(base['units']),flush=True)
rows.sort(key=lambda x:x['name']);failed=[x['name']for x in rows if x['dependency_exit']!=0];affected=[x['name']for x in rows if x['affected']]
result={'baseline':str(inv.base),'baseline_sha256':inv.sha(inv.base),'all_production_units':len(rows),'all_dependency_probes_passed':not failed,'failed_units':failed,'affected_units':affected,'affected_count':len(affected),'shared_psscan_config_and_original_report_unchanged':True,'units':rows}
(r/'affected-unit-inventory.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items()if k!='units'},indent=2));assert not failed,failed
