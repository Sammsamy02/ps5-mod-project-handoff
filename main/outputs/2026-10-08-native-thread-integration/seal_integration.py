from pathlib import Path
import json,hashlib,shlex
w=Path(__file__).resolve().parent;root=w.parent.parent
if (w/'FINAL-RECEIPT.json').exists():raise SystemExit('already sealed; producer refuses overwrite')
def pin(p):
 p=Path(p).absolute();b=p.read_bytes();return dict(path=str(p),bytes=len(b),sha256=hashlib.sha256(b).hexdigest())
refs=json.loads((w/'reports/SOURCE-CONTRACTS.json').read_text())
ext={r['path'] for r in refs}
for p in (w/'target-v2').glob('*.d'):
 text=p.read_text().replace('\\\n','');ext.update(shlex.split(text.split(': ',1)[1]))
for p in ['outputs/2026-10-08-native-thread-audit/THREAD-PROFILE-CANDIDATE.json','outputs/2026-10-08-native-thread-audit/FINAL-RECEIPT.json','outputs/2026-10-08-native-thread-audit/SOURCE-SELECTION.json','outputs/2026-10-08-native-thread-audit/LIBKERNEL-IMPORT-IDENTITY.json','outputs/2026-10-08-native-thread-audit/THREAD-SUPPORT.json','outputs/2026-10-08-native-thread-audit/sdk-libkernel-symbols.stdout','outputs/2026-10-07-l16-local-handles/receipts/public-api/OpenOrbis-libkernel.h','outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json','outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/lib/libkernel.so']:
 ext.add(str(root/p))
for row in json.loads((w/'target-v2/commands.json').read_text()):ext.add(row['argv'][0]);ext.add(row['LLVM_CONFIG'])
for row in json.loads((w/'target-v2/NM-COMMANDS.json').read_text()):ext.add(row['argv'][0])
owned=[pin(p) for p in sorted(w.rglob('*')) if p.is_file()]
external=[pin(p) for p in sorted(ext) if not Path(p).is_relative_to(w)]
manifest=dict(schema='isolated-thread-authority-integration-source-freeze-v1',scope='LOCAL_ONLY; owned candidate, no selected/shared edits',owned_files=owned,external_inputs=external)
(w/'SOURCE-SELECTION.json').write_text(json.dumps(manifest,indent=2)+'\n')
original=root/'outputs/2026-10-07-l19-local-profile-r4/units/UnrealInitializer.cpp'
receipt=dict(schema='isolated-thread-authority-integration-receipt-v1',review_status='independent_review_pending',source_selection=pin(w/'SOURCE-SELECTION.json'),report=pin(w/'reports/REPORT.md'),initializer_origin=pin(original),candidate_profile=pin(root/'outputs/2026-10-08-native-thread-audit/THREAD-PROFILE-CANDIDATE.json'),current_production_preparation='REFUSE',current_missing_contracts=['actual mapping-lifetime/protection owner; VmScan Retention::None','independently admitted static scePthreadGetthreadid/libkernel provider/NID/import'],native_execution=False,selected_sources_modified=False,console_network_contact=False,host_checks=dict(total=23,failures=0,real_owned_memory=True,genuine_saved_image_authentication='once per GREEN process; actual Cache::prepare',game_byte_mapping=False,native_getter_execution=False),actual_Config_host_harness=dict(compile='REFUSE: original Prospero/wchar_t target guards',executed_checks=0),target_whole_tus=3,target_unique_dependency_paths=1449,callback_reader_wired=False,usable_menu_claim=False)
(w/'FINAL-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(dict(source_selection=pin(w/'SOURCE-SELECTION.json'),final=pin(w/'FINAL-RECEIPT.json'),owned_files=len(owned),external_inputs=len(external)),indent=2))
