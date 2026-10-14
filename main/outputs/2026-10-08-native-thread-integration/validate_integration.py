from pathlib import Path
import hashlib,json,sys,subprocess
w=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check(ok,why):
 if not ok:raise SystemExit('REFUSE '+why)
r=json.loads((w/'FINAL-RECEIPT.json').read_text())
check(sha(w/'SOURCE-SELECTION.json')==r['source_selection']['sha256'],'manifest identity')
s=json.loads((w/'SOURCE-SELECTION.json').read_text())
for p in s['owned_files']+s['external_inputs']:
 check(Path(p['path']).is_file() and Path(p['path']).stat().st_size==p['bytes'] and sha(p['path'])==p['sha256'],'pin '+p['path'])
red=json.loads((w/'tests/RED2/commands.json').read_text());green=json.loads((w/'tests/GREEN/commands.json').read_text())
check(red[0]['exit']==0 and red[1]['exit']==1 and 'checks=23 failures=20' in red[1]['stdout'],'behavior RED')
check(green[0]['exit']==0 and green[1]['exit']==0 and 'checks=23 failures=0' in green[1]['stdout'],'behavior GREEN')
target=json.loads((w/'target-v2/commands.json').read_text())
check(len(target)==3 and all(t['exit']==0 for t in target),'actual target 3')
check('scePthreadGetthreadid U 0 0' in (w/'target-v2/NativeThreadAuthority.undefined.stdout').read_text(),'actual static import')
old=Path(r['initializer_origin']['path']).read_text();new=(w/'candidate/UnrealInitializer.cpp').read_text()
needle='                if(!facts.ok) throw std::runtime_error("[BuildProfile] collect-all preflight refused: "+failures);'
expected='#include "SelectedThreadAuthorityAdapter.hpp"\n#include "ThreadProfileCandidate.hpp"\n'+old.replace(needle,needle+'\n                NativeThreadIntegration::require_selected_authority(UnrealConfig,NativeThreadIntegration::kThreadCandidate,rc::runtimeprofile::kDigest,now_ms);')
check(new==expected,'bounded initializer overlay')
check(new.index('require_selected_authority(UnrealConfig')<new.index('SeedArming::consume_required_slot_seeds(std::chrono::seconds'),'pre-publication guard order')
check(r['current_production_preparation']=='REFUSE' and not r['selected_sources_modified'] and not r['native_execution'],'scope/current refusal')
print(json.dumps(dict(pass_=True,owned_pins=len(s['owned_files']),external_pins=len(s['external_inputs']),host_checks=23,target_whole_tus=3,current_production_preparation='REFUSE',selected_sources_modified=False,native_execution=False),sort_keys=True))
