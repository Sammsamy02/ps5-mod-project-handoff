#!/usr/bin/env python3
"""Join frozen, local-only l19 fact artifacts; never modify inputs."""
import argparse
import configparser
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = ROOT / 'outputs/2026-10-07-l19-fact-summary-r2'
INV = ROOT / 'outputs/2026-10-07-l19-fact-inventory/FACTS.json'
STATIC_DIR = ROOT / 'outputs/2026-10-07-l19-static-layout-check'
PROFILE_DIR = ROOT / 'outputs/2026-10-07-l19-local-profile-r2'
PROFILE_PATH = PROFILE_DIR / 'profiles/townfall-build-facts.json'
HEADER = PROFILE_DIR / 'units/RuntimeBuildProfile.hpp'
INPUTS = [INV, INV.with_name('FACT-TABLE.md'), STATIC_DIR/'STATIC-FACTS.json',
          STATIC_DIR/'REFLECTION-FACTS.json', STATIC_DIR/'REPORT.md', STATIC_DIR/'REPORT-SUPPLEMENT.md',
          PROFILE_PATH, HEADER, PROFILE_DIR/'units/BuildFactsChecks.hpp',
          PROFILE_DIR/'units/SeedTable.h', PROFILE_DIR/'units/FNameAbiProfile.hpp',
          PROFILE_DIR/'reports/REPORT.md', PROFILE_DIR/'reports/EVIDENCE.json',
          PROFILE_DIR/'reports/FINAL-RECEIPT.json', PROFILE_DIR/'reports/SOURCE-SELECTION.json']

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read(path):
    return json.loads(path.read_text())

def rel(path):
    return str(Path(path).relative_to(ROOT))

args_parser = argparse.ArgumentParser(description='Join only a caller-pinned frozen R2 profile; no input edits.')
args_parser.add_argument('--profile-digest', required=True)
args_parser.add_argument('--profile-sha256', required=True)
args_parser.add_argument('--source-selection-sha256', required=True)
args = args_parser.parse_args()
assert sha(PROFILE_PATH) == args.profile_sha256, 'R2 profile is not the approved frozen source'
assert sha(PROFILE_DIR/'reports/SOURCE-SELECTION.json') == args.source_selection_sha256
inv = read(INV)
profile = read(PROFILE_PATH)
receipt = read(PROFILE_DIR/'reports/FINAL-RECEIPT.json')
source_paths = [STATIC_DIR/'STATIC-FACTS.json', STATIC_DIR/'REFLECTION-FACTS.json']
for candidate in profile['evidence']['sources']:
    path = ROOT/candidate
    if path.suffix == '.json' and path not in source_paths:
        document = read(path)
        if isinstance(document, dict) and 'facts' in document and 'evidence' in document:
            source_paths.append(path)
sources = [read(path) for path in source_paths]
source_names = [path.name for path in source_paths]
INPUTS += [path for path in source_paths if path not in INPUTS]
INPUTS += [ROOT/path for path in profile['evidence']['sources'] if ROOT/path not in INPUTS]
for source_path in source_paths:
    for name in ['REPORT.md','SOURCE-HASHES.json','VERIFICATION.json']:
        path = source_path.parent/name
        if path.exists() and path not in INPUTS:
            INPUTS.append(path)
INPUTS.append(OUT/'SUBTYPE-AUDIT.json')
INPUTS.append(OUT/'SOURCE-AUDIT.json')
INPUTS += sorted((PROFILE_DIR/'reports').glob('ACTIVE-WIDTHS*.json'))
if (PROFILE_DIR/'units/NativeLayoutActiveWidths.hpp').exists():
    INPUTS.append(PROFILE_DIR/'units/NativeLayoutActiveWidths.hpp')
member_path = Path(inv['actual_layout_provenance']['matching_local_member_candidate'])
vtable_path = Path(inv['actual_layout_provenance']['matching_local_vtable_candidate'])
INPUTS += [member_path, vtable_path]
profile_source_checks = []
for path,expected in profile['evidence']['sources'].items():
    actual=sha(ROOT/path)
    assert actual==expected, path
    profile_source_checks.append({'path':str(ROOT/path),'sha256':actual,'matches_profile':True})
source_selection=read(PROFILE_DIR/'reports/SOURCE-SELECTION.json')
selection_source_checks = []
for path, expected in source_selection['source_files'].items():
    actual = sha(PROFILE_DIR/path)
    assert actual == expected, path
    selection_source_checks.append({'path':str(PROFILE_DIR/path),'sha256':actual,'matches_selection':True})
    if PROFILE_DIR/path not in INPUTS:INPUTS.append(PROFILE_DIR/path)
selection_evidence_checks = []
for path, expected in source_selection['evidence_files'].items():
    actual = sha(ROOT/path)
    assert actual == expected, path
    selection_evidence_checks.append({'path':str(ROOT/path),'sha256':actual,'matches_selection':True})
    if ROOT/path not in INPUTS:INPUTS.append(ROOT/path)
assert len(selection_source_checks)==67 and len(selection_evidence_checks)==23
assert all(source_selection['evidence_files'][path]==h for path,h in profile['evidence']['sources'].items())
for name, expected in receipt['receipt_hashes'].items():
    path=PROFILE_DIR/'reports'/name
    assert sha(path)==expected, name
    if path not in INPUTS:INPUTS.append(path)
target_width_receipt_path=PROFILE_DIR/'reports/ACTIVE-WIDTHS-TARGET.json'
target_width_receipt=read(target_width_receipt_path)
assert target_width_receipt['exit']==0 and receipt['target_active_width_controls_pass'] is True
for path, expected in target_width_receipt['source_hashes'].items():
    source=Path(path)
    assert sha(source)==expected, path
    if source not in INPUTS:INPUTS.append(source)
input_hashes = {rel(p):sha(p) for p in INPUTS}

def ini(path):
    c = configparser.ConfigParser(interpolation=None)
    c.optionxform = str
    c.read(path)
    return c

member_ini, vtable_ini = ini(member_path), ini(vtable_path)
members = {p['name'].replace('::','.'):p for p in profile['required_members']}
vtables = {p['name'].replace('::','.'):p for p in profile['required_vtables']}
static_records, static_index = [], {}
for document, source_path in zip(sources,source_paths):
    for f in document['facts']:
        entry = dict(f)
        entry['source'] = str(source_path)
        entry['original_key'] = f['key']
        key = f['key'].removesuffix('.vtable_byte_offset')
        entry['key'] = key
        entry['instructions'] = [{**document['evidence'][k], 'evidence_key':k,
                                  'disassembly_artifact':str(source_path.parent/document['evidence'][k]['disassembly_artifact'])}
                                 for k in f['evidence_keys']]
        entry['data_evidence'] = {k:document['data_evidence'][k] for k in f.get('data_evidence_keys',[])}
        entry['normalized_direct_call_targets'] = {k:document['direct_call_targets'][k] for k in f['evidence_keys']
                                                   if k in document.get('direct_call_targets',{})}
        static_records.append(entry)
        static_index.setdefault(key,[]).append(entry)
assert len(static_records)>=39 and len(static_index)>=37
assert len(sources[0]['facts']) + len(sources[1]['facts']) == 39
assert len({f['key'] for doc in sources[:2] for f in doc['facts']}) == 37
duplicates = {k:len(v) for k,v in static_index.items() if len(v)>1}
assert duplicates.get('UFunction.ParmsSize') == 2 and duplicates.get('UFunction.ReturnValueOffset') == 2
for key, records in static_index.items():
    assert len({(json.dumps(f['value'],sort_keys=True),f['width_bytes']) for f in records})==1, 'Conflicting static fact: '+key
for pins in [members,vtables]:
    for key,pin in pins.items():
        if pin['offset'] is not None and key in static_index:
            fact=static_index[key][-1]
            assert pin['offset']==int(fact['value'],0), 'Profile differs from measured fact: '+key
            if fact['width_bytes'] is not None:assert pin['width']==fact['width_bytes'], key
static_window_checks = sum(len(doc['evidence']) for doc in sources)
unique_static_windows = {(e['elf_image_vaddr'],e['instruction_bytes']) for doc in sources for e in doc['evidence'].values()}

header_text = HEADER.read_text()
digest = re.search(r'kDigest="([0-9a-f]+)"',header_text).group(1)
assert digest==receipt['profile_digest']==args.profile_digest
header_checks = []
for cname, pins in [('kRequiredMembers',profile['required_members']),('kRequiredVtables',profile['required_vtables'])]:
    body = re.search(r'\b'+cname+r'\[\]=\{(.*?)\};',header_text,re.S).group(1)
    actual = re.findall(r'\{"([^"]+)",(true|false),(\d+),(\d+),"([^"]*)"\}',body)
    expected = [(p['name'],'true' if p['offset'] is not None else 'false',str(p['offset'] or 0),
                 str(p['width'] or 0),p['source']) for p in pins]
    assert actual==expected
    header_checks.append({'array':cname,'count':len(actual),'equal_to_json':True})
header_widths = re.search(r'kUnknownRequiredStaticFacts\[\]=\{(.*?)\};',header_text,re.S).group(1)
assert re.findall(r'"([^"]+)"',header_widths)==profile['unknown_required_static_facts']
known_members = sum(p['offset'] is not None for p in members.values())
known_vtables = sum(p['offset'] is not None for p in vtables.values())
assert (len(members),len(vtables))==(69,7)

identity_keys = {'target':'Target','pointer_size':'PointerSize','engine_major':'EngineMajor',
                 'engine_minor':'EngineMinor','engine_revision':'EngineRevision','build_id':'BuildId',
                 'non_shipping':'NonShipping','stats':'Stats','layout.format':'Format'}
metadata_values = {'image.sha256':profile['image']['sha256'],'image.base':profile['image']['base'],
                   'image.file_size':profile['image']['size'],'image.pt_load_extent':profile['image']['extent'],
                   'engine_major':profile['layout']['major'],'engine_minor':profile['layout']['minor'],
                   'engine_revision':profile['layout']['engine_revision'],'build_id':profile['layout']['build_id'],
                   'target':profile['layout']['target'],'pointer_size':profile['layout']['pointer_size'],
                   'layout.format':profile['layout']['format'],'engine_patch':1,
                   'non_shipping':False,'stats':False,
                   'FName.output_bytes':profile['fname']['output_bytes'],
                   'FName.output_alignment':profile['fname']['output_alignment'],
                   'FName.case_preserving':profile['fname']['case_preserving']}
static_alias = {'object_storage_dispatch':'object_storage.native_chunked_65536',
                'TUObjectArray.NumElementsPerChunk':'object_storage.native_chunked_65536',
                'FField_owner_variant_encoding':'FField.owner_variant_encoding',
                'FFieldVariant.active_encoding':'FField.owner_variant_encoding'}

def requirement(f):
    r=f['requirement']
    if r=='notqueriedontracedpath' or f['id'] in ['with_editoronly_data','UE_ENABLE_FUOBJECT_ITEM_PACKING']:
        return 'dormant'
    if r=='dynamicreflectiondiscovery': return 'dynamic'
    if r.startswith('metadata'): return 'metadata'
    if r=='conditional': return 'conditional'
    return 'required'

def saved_declaration(fid):
    if fid.startswith(('member:','vtable:')):
        kind,key=fid.split(':',1); section,name=key.split('.',1)
        c,path=(member_ini,member_path) if kind=='member' else (vtable_ini,vtable_path)
        present=c.has_option(section,name)
        return {'status':'saved-local-candidate' if present else 'absent-in-saved-local-candidate',
                'present':present,'value':int(c[section][name],0) if present else None,
                'path':str(path),'exact_current_installed_content_hash_proved':False}
    if fid in identity_keys:
        key=identity_keys[fid]
        a,b=member_ini['NativeLayout'][key],vtable_ini['NativeLayout'][key]
        assert a==b
        value=int(a,0) if a.isdigit() else a
        return {'status':'saved-local-candidate','present':True,'value':value,
                'paths':[str(member_path),str(vtable_path)],'exact_current_installed_content_hash_proved':False}
    return {'status':'not-an-INI-declaration','present':False,'value':None}

def measured(fid,key,pin,original):
    skey=static_alias.get(fid,key)
    if skey in static_index:
        f=static_index[skey][-1]
        value=f['value']
        if fid=='object_storage_dispatch': value='native chunked storage, 65536 entries/chunk'
        if fid=='TUObjectArray.NumElementsPerChunk': value=65536
        return {'status':'authenticated-saved-image-static','value':value,
                'width_bytes':f['width_bytes'],'confidence':f.get('confidence','high'),'static_fact_key':skey,
                'evidence':static_index[skey],'fresh_live_validation':False}
    if pin and pin['offset'] is not None:
        return {'status':'retained-target-pin-not-remeasured-in-l19-static-slice',
                'value':pin['offset'],'width_bytes':pin['width'],
                'confidence':original['confidence'],'evidence':original['evidence'],
                'profile_source':pin['source'],'fresh_live_validation':False}
    if fid in metadata_values:
        if fid=='layout.format':
            return {'status':'native-format-contract','value':1,'confidence':'selected contract',
                    'evidence':[{'path':str(PROFILE_DIR/'units/BuildFactsChecks.hpp'),'field':'profile.layout-abi'}],
                    'fresh_live_validation':False}
        return {'status':'retained-authenticated-image-or-ABI-evidence',
                'value':metadata_values[fid],'confidence':original.get('confidence','high'),
                'evidence':original.get('evidence',[])+[{'path':str(PROFILE_PATH),'field':fid}],
                'fresh_live_validation':False}
    if fid=='native_tchar_bytes':
        return {'status':'selected-ABI-with-retained-FName-roundtrip','value':2,
                'confidence':original['confidence'],'evidence':original['evidence'],'fresh_live_validation':False}
    if requirement(original)=='dynamic':
        return {'status':'dynamic-reflection-required','value':None,'confidence':'no witnessed value',
                'evidence':original['evidence'],'fresh_live_validation':False}
    if fid.startswith('mask:') or fid in ['platform_abi','FScriptArray.storage','FImplementedInterface.storage','UE4SS_R3_MARSHAL_MODE']:
        return {'status':'selected-port-contract-only','value':None,'selected_contract_value':original['current_value'],
                'confidence':'compiled contract; complete target behavior not established',
                'evidence':original['evidence'],'fresh_live_validation':False}
    return {'status':'unknown','value':None,'confidence':'unresolved',
            'evidence':original.get('evidence',[]),'fresh_live_validation':False}

def supplied(fid,pin):
    if pin:
        return {'status':'known' if pin['offset'] is not None else 'explicit-unknown',
                'value':pin['offset'],'width_bytes':pin['width'],'mandatory_pin':True,
                'source':pin['source'],'path':str(PROFILE_PATH),'generated_header':str(HEADER),
                'header_known':pin['offset'] is not None,'header_offset':pin['offset'] or 0,
                'header_width':pin['width'] or 0}
    if fid in metadata_values and fid!='engine_patch':
        value=metadata_values[fid]
        if fid=='non_shipping': value={'build_configuration_enum':1,'meaning':'Shipping','non_shipping':False}
        if fid=='stats': value={'stats_enum':1,'meaning':'Off','stats_enabled':False}
        return {'status':'known','value':value,'mandatory_pin':False,'path':str(PROFILE_PATH)}
    skey=static_alias.get(fid,fid)
    if skey in profile['evidence']['resolved_runtime_static_facts']:
        return {'status':'evidence-backed-static-fact','value':profile['evidence']['resolved_runtime_static_facts'][skey]['value'],
                'mandatory_pin':False,'path':str(PROFILE_PATH),'field':'evidence.resolved_runtime_static_facts.'+skey}
    return {'status':'not-in-RuntimeBuildProfile','value':None,'mandatory_pin':False}

rows=[]
for original in inv['facts']:
    fid=original['id']; key=fid.split(':',1)[-1]
    pin=members.get(key) if fid.startswith('member:') else vtables.get(key) if fid.startswith('vtable:') else None
    row={'id':fid,'origin':'traced-inventory','requirement':requirement(original),
         'requirement_condition':original['requirement'],'stage':original['stage'],
         'consumer':original['consumer'],'saved_staged_declaration':saved_declaration(fid),
         'measured_target':measured(fid,key,pin,original),'profile_supplied':supplied(fid,pin),
         'selected_port_value':original['current_value'],
         'binding_validator_width_bytes':original.get('accessor_width_bytes'),
         'inventory_record':original}
    if fid=='member:UFunction.Func':
        row['selected_active_accessor_width_bytes']=8
        row['historical_source_type_maximum_width_bytes']=16
        row['legacy_selected_target_provider_width_bytes']=None
        row['legacy_selected_target_provider_status']='unavailable; actual target provider returns null'
        row['binding_validator_width_bytes']=None
        row['r2_selected_accessor_width_bytes']=pin['width'] if pin else None
        row['resolution_note']='Func is target +0xD8 with an 8-byte native function pointer. Selected GetFunc419/GetFunc reads/writes a pointer reference (8B). Actual selected legacy target provider returns null; historical16 was a source-type maximum model, not demonstrated provider availability. R2 binding derives its width from the active public getter. Historical C8 identifies EventGraphFunction.'
    if fid=='member:UStruct.MinAlignment':
        row['v1_conservative_validator_width_bytes']=4
        row['legacy_selected_target_provider_width_bytes']=4
        row['legacy_selected_target_provider_status']='available; actual target provider returns4'
        row['r2_selected_accessor_width_bytes']=pin['width'] if pin else None
        row['resolution_note']='Target field is word +0x5C (2B). R2 binding selects the active public getter width; V1 conservative4 included alternate forms.'
    if fid in ['member:UClass.ClassCastFlags','member:FFieldClass.CastFlags']:
        row['legacy_selected_target_provider_width_bytes']=None
        row['legacy_selected_target_provider_status']='unavailable; actual target provider returns null'
        row['binding_validator_width_bytes']=None
        row['r2_selected_accessor_width_bytes']=pin['width'] if pin else None
        row['resolution_note']='Native operand width8 is independent of provider availability. Actual selected legacy target provider returns null; source enum:uint64 alone did not establish provider availability. R2 derives the selected accessor width from the public getter type.'
    if fid in ['non_shipping','stats']:
        row['measured_target']['confidence']='independent saved-image evidence; l19 C/Python agreement'
        row['resolution_note']='INI boolean0 differs from detector/profile enum1: Shipping for build configuration, Off for STATS. Both resolved independently; no fresh game launch in l19.'
    if fid=='with_editoronly_data':
        row['resolution_note']='No runtime Version-axis query on the traced path; Shipping does not prove WITH_EDITORONLY_DATA. Dormant compile policy, not an extra mandatory target pin.'
    if fid=='UE_ENABLE_FUOBJECT_ITEM_PACKING':
        row['resolution_note']='Global macro unknown. Actual 5.6 native path directly proves Object0/Flags8/stride18; 5.7+ packing branch is dormant here.'
    rows.append(row)

represented={r['measured_target'].get('static_fact_key') for r in rows}
for key in sorted(static_index.keys()-represented):
    f=static_index[key][-1]; fid='member:'+key if key.split('.')[0] in ['UFunction','UStruct','FProperty'] else key
    rows.append({'id':fid,'origin':'additional-static-evidence','requirement':'dormant',
                 'requirement_condition':'measured, but absent from the selected mandatory accessor census',
                 'stage':'diagnostic static context','consumer':[],
                 'saved_staged_declaration':saved_declaration(fid),'measured_target':{
                     'status':'authenticated-saved-image-static','value':f['value'],'width_bytes':f['width_bytes'],
                     'confidence':'high','static_fact_key':key,'evidence':static_index[key],'fresh_live_validation':False},
                 'profile_supplied':supplied(key,None),'binding_validator_width_bytes':None})
for fid in ['image.file_size','image.pt_load_extent','layout.format']:
    original={'id':fid,'requirement':'required','confidence':'authenticated saved-image/profile metadata',
              'evidence':[{'path':str(PROFILE_PATH),'field':fid}]}
    rows.append({'id':fid,'origin':'l19-preflight-metadata','requirement':'required',
                 'requirement_condition':'fresh-cache/profile or staged format comparison',
                 'stage':'l19 preflight before seeded consumers','consumer':[{'path':str(PROFILE_DIR/'units/BuildFactsChecks.hpp')}],
                 'saved_staged_declaration':saved_declaration(fid),'measured_target':measured(fid,fid,None,original),
                 'profile_supplied':supplied(fid,None),'binding_validator_width_bytes':None})
for fid in ['accessor_width.UClass.ClassCastFlags','accessor_width.FFieldClass.CastFlags']:
    native_key = fid.removeprefix('accessor_width.')
    width_record = profile['evidence']['resolved_runtime_static_facts'].get(native_key)
    width_known = fid not in profile['unknown_required_static_facts'] and width_record is not None
    assert width_known == (fid not in profile['unknown_required_static_facts']), 'Resolved width must have a frozen profile receipt'
    width_value = width_record['width_bytes'] if width_known else None
    if width_known:assert width_value==8
    rows.append({'id':fid,'origin':'l19-required-static-width','requirement':'required',
                 'requirement_condition':'selected cast-mask width proof, separately required from member offset',
                 'stage':'l19 preflight before seeded consumers','consumer':[{'path':str(HEADER)}],
                 'saved_staged_declaration':saved_declaration(fid),
                 'measured_target':{'status':'native-width-and-selected-compiled-accessor' if width_known else 'unknown',
                     'value':width_value,'confidence':'high' if width_known else 'unresolved',
                     'native_operand_fact_key':native_key,
                     'compiled_getter_width_receipt':{'path':str(target_width_receipt_path),
                         'sha256':sha(target_width_receipt_path),'exit':0,'static_assertions':target_width_receipt['static_assertions']},
                     'evidence':static_index.get(native_key,[]),'profile_width_receipt':width_record,'fresh_live_validation':False},
                 'profile_supplied':{'status':'known' if width_known else 'explicit-unknown','value':width_value,
                     'mandatory_static_width':True,'path':str(PROFILE_PATH),
                     'field':'evidence.resolved_runtime_static_facts.'+native_key+'.width_bytes'},
                 'binding_validator_width_bytes':width_value})
assert len({r['id'] for r in rows})==len(rows)
assert {r['measured_target'].get('static_fact_key') for r in rows if r['measured_target'].get('static_fact_key')}==set(static_index)

unknown_members=[p['name'] for p in profile['required_members'] if p['offset'] is None]
unknown_vtables=[p['name'] for p in profile['required_vtables'] if p['offset'] is None]
assert len(unknown_members)==len(members)-known_members and len(unknown_vtables)==len(vtables)-known_vtables
seedtext=(PROFILE_DIR/'units/SeedTable.h').read_text()
seedpins=[]
for m in re.finditer(r'inline constexpr SlotSeed (\w+)\{ (true|false), (true|false), (0x[0-9a-f]+), "([^"]+)", "((?:\\.|[^"\\])*)" \}; // slot (\d+) \((\w+)\)',seedtext):
    seedpins.append({'constant':m[1],'known':m[2]=='true','is_data':m[3]=='true','image_vaddr':m[4],
                     'label':m[5],'provenance':json.loads('"'+m[6]+'"'),'slot':int(m[7]),'kind':m[8],
                     'source':str(PROFILE_DIR/'units/SeedTable.h'),'source_line':seedtext[:m.start()].count('\n')+1,
                     'scope':'carried prior prerequisite; separate from joined static layout facts; no new live claim'})
assert len(seedpins)==4

summary={
    'schema':'l19-joined-facts-r2','scope':'LOCAL ONLY report synthesis; no source/build/frozen input edits; no console/network/contact/upload/game execution',
    'status':'final frozen-source synthesis; exact required target gaps remain',
    'image':profile['image'],'profile_digest':digest,'profile_file_sha256':sha(PROFILE_PATH),
    'counts':{'inventory_fact_rows':len(inv['facts']),'joined_fact_rows':len(rows),
              'required_member_pins':len(members),'known_member_pins':known_members,'unknown_member_pins':len(unknown_members),
              'required_vtable_pins':len(vtables),'known_vtable_pins':known_vtables,'unknown_vtable_pins':len(unknown_vtables),
              'unknown_required_cast_width_facts':len(profile['unknown_required_static_facts']),
              'required_cast_width_facts':2,'known_required_cast_width_facts':2-len(profile['unknown_required_static_facts']),
              'positive_static_records':len(static_records),'positive_static_unique_facts':len(static_index),
              'v1_positive_static_records':39,'v1_positive_static_unique_facts':37,
              'static_duplicate_keys':duplicates,'static_instruction_window_checks':static_window_checks,
              'unique_static_instruction_windows':len(unique_static_windows),'static_evidence_documents':len(sources),
              'saved_local_member_declarations':17,'saved_local_vtable_declarations':1,
              'dynamic_method_names':len(inv['dynamic_reflection']['methods']),
              'dynamic_object_class_paths':len(inv['dynamic_reflection']['object_class_paths']),
              'dynamic_struct_shapes':len(inv['dynamic_reflection']['structs']),
              'version_source_queries':len(inv['version_queries']),'assertion_throw_source_sites':len(inv['assertion_throw_audit']),
              'requirements_in_joined_rows':dict(Counter(r['requirement'] for r in rows))},
    'required_unresolved':{'member_values':unknown_members,'vtable_slots':unknown_vtables,
                           'cast_accessor_widths':profile['unknown_required_static_facts'],
                           'counting_policy':'Exact unresolved member offsets/totals, vtable slots, and separate selected accessor widths are reported from this frozen R2 profile. Width facts do not add member offsets; historical freeze77 and V1 counts are not R2 gap counts.'},
    'saved_candidate_required_declaration_omissions':{
        'member_keys':[r['id'].split(':',1)[1] for r in rows if r['id'].startswith('member:')
            and r['id'].split(':',1)[1] in members and not r['saved_staged_declaration']['present']],
        'vtable_keys':[r['id'].split(':',1)[1] for r in rows if r['id'].startswith('vtable:')
            and r['id'].split(':',1)[1] in vtables and not r['saved_staged_declaration']['present']],
        'scope':'Saved candidate omissions, distinct from unknown native facts. No current installed byte-hash proof.'},
    'current_staged_layout_provenance':inv['actual_layout_provenance'],
    'freshness_policy':'Saved local INIs are candidates, size-corroborated by the saved l18 loadlist; their exact current installed byte hashes are unproved. Profile known values and static image proof do not establish current game memory or publication. Fresh startup authenticates its own full image/current generation/base/deadline and reads staged bytes through NativeModSource.',
    'preflight_outcome':{'status':'REFUSE','basis':receipt['saved_maps'],
        'unknown_member_values':unknown_members,'unknown_vtable_slots':unknown_vtables},
    'superseded_inventory_gate':'The old inventory first-source-order FUObjectItem gap is resolved statically. Current l19 preflight collects all required declarations/measured values/widths before seeded engine consumers; no claim that FUObjectItem remains the first gap.',
    'axes_encoding':{'staged_INI':{'NonShipping':0,'Stats':0},'profile_and_detector':{'build_configuration':1,'build_meaning':'Shipping','stats':1,'stats_meaning':'Off'},'independent_axes':True},
    'facts':rows,'positive_static_facts':static_index,
    'static_evidence_documents':[{'path':str(path),'sha256':sha(path),'format':doc['format'],
        'address_model':doc.get('address_model'),'validation':doc.get('validation'),
        'accessor_width_implications':doc.get('accessor_width_implications'),
        'width_implications_interpretation':'Preserved source-type observations. Actual legacy target provider availability is separately checked; null provider results supersede any assumption that enum width alone makes it available.',
        'data_evidence':doc.get('data_evidence',{}),'direct_call_targets':doc.get('direct_call_targets',{})}
        for doc,path in zip(sources,source_paths)],
    'independent_subtype_audit':read(OUT/'SUBTYPE-AUDIT.json'),
    'independent_frozen_source_audit':read(OUT/'SOURCE-AUDIT.json'),
    'supplemental_dormant_distinctions':{
        'fully_measured_nonmandatory_member_fields':['FProperty.DestructorLinkNext','FProperty.PostConstructLinkNext',
            'UFunction.EventGraphCallOffset','UFunction.EventGraphFunction','UFunction.FirstPropertyToInit','UStruct.DestructorLink'],
        'partial_nonmandatory_member_offsets':[{'key':'UStruct.Script','offset':'0x60','exact_accessor_or_storage_width':None}],
        'nonmandatory_vtable_slots':[{'key':'UClass.CreateDefaultObject','slot':'0x400'}]},
    'dynamic_reflection':inv['dynamic_reflection'],
    'dynamic_policy':'Methods, paths, parameter names/types, struct child offsets and runtime sizes are discovered by reflection; they are not mandatory native constant offsets. No original menu/controller/R3/spawn success is established.',
    'carried_startup_seed_pins':seedpins,
    'version_queries':inv['version_queries'],'assertion_throw_audit':inv['assertion_throw_audit'],
    'census_policy':'144 version source queries and 719 assertion/throw source sites are reachability-aware source audits, not counts of executed operations or mandatory target constants.',
    'historical_static_unresolved':[{**u,'source':str(path),'superseded_by_positive_static':u['key'].removesuffix('.vtable_byte_offset') in static_index}
                                    for doc,path in zip(sources,source_paths) for u in doc.get('unresolved',[])],
    'profile_evidence':profile['evidence'],'local_verification_receipt':receipt,
    'actual_target_accessor_width_receipt':target_width_receipt,
    'source_selection':{'path':str(PROFILE_DIR/'reports/SOURCE-SELECTION.json'),
                        'sha256':sha(PROFILE_DIR/'reports/SOURCE-SELECTION.json')},
    'input_sha256':input_hashes,'verification':{'json_header_pin_equality':header_checks,
          'unknown_width_list_header_equality':True,'all_static_keys_represented':True,
          'all_inventory_rows_preserved':True,'profile_source_hash_checks':profile_source_checks,
          'all_67_selected_source_hash_checks':selection_source_checks,
          'all_23_selected_evidence_hash_checks':selection_evidence_checks,
          'profile_header_consumer_source_selection_hashes_match':True,'inputs_unchanged_after_synthesis':False},
    'historical_inventory_limitations':inv['limitations'],
    'limitations':[
         'Actual selected legacy target provider Func/ClassCastFlags/CastFlags returnsnull; MinAlignment returns4. Historical source-type models Func16/cast8 are not actual provider availability. R2 records native, selected public getter, and legacy widths separately.',
         'Class totals are known only where named registration, registered constructor allocation, or qualified target containment establishes them. Remaining totals stay unknown; compiler templates and largest member extents are not substitutes.',
         'Recorded profile host checks/target TU compile are inherited receipts, not rerun by this report task; parent full pipeline result is outside this synthesis.']}

def fmt(value):
    if value is None:return 'UNKNOWN'
    if isinstance(value,bool):return 'true' if value else 'false'
    if isinstance(value,dict):return '; '.join(str(k)+'='+str(v) for k,v in value.items())
    return str(value)

def escape(text):
    return str(text).replace('|','\\|').replace('\n',' ')

def link(path,label=None,line=None):
    dest=str(path)+(':'+str(line) if line else '')
    return '['+(label or Path(path).name)+']('+dest+')'

def source_cell(row):
    m=row['measured_target']; skey=m.get('static_fact_key') or m.get('native_operand_fact_key')
    if skey:
        e=static_index[skey][-1]
        addrs=', '.join(i['elf_image_vaddr'] for i in e['instructions'][:4])
        return link(e['source'],Path(e['source']).name+':'+skey)+'; '+addrs+('…' if len(e['instructions'])>4 else '')+'; high static confidence'
    if m['status']=='retained-target-pin-not-remeasured-in-l19-static-slice':
        refs=m['evidence']; ref=next((r for r in refs if r.get('path','').endswith('REPORT.md')),refs[0] if refs else None)
        return (link(ref['path'],line=ref.get('line'))+'; ' if ref else '')+m['confidence']
    ev=m.get('evidence',[])
    ref=ev[-1] if ev else (row.get('consumer') or [{}])[0]
    return (link(ref['path'],line=ref.get('line'))+'; ' if ref.get('path') else '')+m.get('confidence','unresolved')

def table_row(row):
    d,m,p=row['saved_staged_declaration'],row['measured_target'],row['profile_supplied']
    declaration=fmt(d['value']) if d['present'] else ('absent' if 'candidate' in d['status'] else '—')
    if row['id'].startswith(('member:','vtable:')) and d['present']:declaration=hex(d['value'])+' (saved candidate)'
    measurement=fmt(m['value'])
    if row['id'].startswith(('member:','vtable:')) and isinstance(m['value'],int):measurement=hex(m['value'])
    if m.get('width_bytes') is not None:measurement+=' / '+str(m['width_bytes'])+'B target'
    if m['status']=='retained-target-pin-not-remeasured-in-l19-static-slice':measurement+=' (retained proof)'
    if m['status']=='selected-port-contract-only':measurement='target unknown; port contract '+fmt(m['selected_contract_value'])
    if p['status']=='known':
        pv=hex(p['value']) if row['id'].startswith(('member:','vtable:')) else fmt(p['value'])
        if p.get('width_bytes') is not None:pv+=' / '+str(p['width_bytes'])+'B pin'
    elif p['status']=='evidence-backed-static-fact':pv=fmt(p['value'])+' (evidence)'
    elif p['status']=='explicit-unknown':pv='UNKNOWN (explicit)'
    else:pv='not supplied'
    width=row.get('binding_validator_width_bytes')
    if row.get('r2_selected_accessor_width_bytes') is not None:pv+='; R2 active '+str(row['r2_selected_accessor_width_bytes'])+'B'
    elif width is not None:pv+='; inventory width '+str(width)+'B'
    req=row['requirement']+'; '+row['stage']
    return '| '+' | '.join(escape(v) for v in [row['id'],declaration,measurement,pv,req,source_cell(row)])+' |'

lines=['# l19 R2 startup/menu/spawn fact table','',
       'LOCAL ONLY. This joins saved declaration candidates, authenticated saved-image evidence, and the final frozen R2 generated profile. Required unresolved target facts still refuse startup before seeded engine calls. No original menu, controller, R3 carrier, or spawn success is claimed.','',
       f'**{len(members)} required member pins: {known_members} known / {len(unknown_members)} unknown. {len(vtables)} required vtable pins: {known_vtables} known / {len(unknown_vtables)} unknown. {len(profile["unknown_required_static_facts"])} cast accessor widths remain unknown.**','',
       f'The joined evidence has **{len(static_index)} unique positive static facts** in {len(static_records)} records from {len(sources)} frozen documents. V1 contained 37 unique facts in 39 records. This R2 join checks {static_window_checks} recorded instruction windows ({len(unique_static_windows)} distinct address/byte windows); these counts are separate from required pin counts.','',
       'Saved local INIs contain 17 member declarations and one vtable declaration. Saved l18 loadlist sizes 588B/216B corroborate those candidates; their exact current installed byte hashes remain unproved. Static proof and supplied profile pins do not prove that staged declarations are complete or current.','',
       'Shipping and STATS Off are independently resolved: profile/detector enums1/1 correspond to INI NonShipping0/Stats0. The old inventory first-source-order FUObjectItem gap is superseded; R2 collects all required facts before seeded consumers.','',
       '## Exact remaining mandatory gaps','',
       '| Kind | Exact unresolved value |','|---|---|']
for name in unknown_members:lines.append('| member offset / exact total | `'+name+'` |')
for name in unknown_vtables:lines.append('| vtable byte slot | `'+name+'` |')
for name in profile['unknown_required_static_facts']:lines.append('| cast accessor width | `'+name+'` |')
lines += ['',f'The current gap count is {len(unknown_members)} member values + {len(unknown_vtables)} vtable slots + {len(profile["unknown_required_static_facts"])} separate width facts. Historical freeze77 and V1 gap counts are not final R2 counts. Dynamic reflected sizes and dormant editor-only policy are separate below.','',
          '## Joined fact census','',
          'Every original inventory row is preserved. Additional measured facts, image metadata, format checks, and both required cast width facts are explicit. Dormant means outside the selected mandatory accessor graph; dynamic means reflection must provide the current value. Port contract is a selected compiled assumption. Exact instruction bytes, names, relocations, and inference chains are preserved in FACTS.json.','',
          '| Fact | Saved staged declaration | Measured target | Profile supplied / selected width | Requirement / stage | Evidence / confidence |',
          '|---|---|---|---|---|---|']
lines += [table_row(r) for r in rows]
lines += ['', '## Width, total, and dormant-policy distinctions','',
          'UFunction.Func is target +0xD8/8B; +0xC8 is EventGraphFunction. The selected GetFunc419/GetFunc pointer reference is 8B. The actual legacy target width provider returns null for Func and both cast fields; source-type models 16/8 did not prove provider availability. UStruct.MinAlignment is target +0x5C/2B while the actual legacy provider returns 4. R2 derives widths 8/8/8/2 from the four public getter types. Frozen target compilation receipts and adapter source are hashed in the input manifest.','',
          'Exact class totals promoted in this slice have named native registration or registered ConstructFn allocation proof. UObjectBase28 is a separately qualified two-sided bound with target allocation-to-base-constructor this-flow. FField and FFieldClass candidate extents/guard spacing remain unproved exact totals unless later frozen evidence explicitly resolves them. Width=null is no independent pin-width comparison, not a zero-byte accessor.','',
          'WITH_EDITORONLY_DATA is unknown and dormant on the actual runtime Version path; Shipping does not establish it. The global UE_ENABLE_FUOBJECT_ITEM_PACKING macro remains unknown, while the effective native chunked65536 Object0/Flags8/stride18 path is measured. FProperty.InitializeValueInternal remains conditional outside the seven mandatory slots.','',
          'Six measured member fields remain outside the mandatory census: FProperty.DestructorLinkNext/PostConstructLinkNext, UFunction.EventGraphCallOffset/EventGraphFunction/FirstPropertyToInit, and UStruct.DestructorLink. UStruct.Script60 is separate partial diagnostic offset evidence with no exact accessor/storage width. UClass.CreateDefaultObject400 is a measured supplemental slot and does not enlarge the required seven-slot census.','',
          '## Dynamic reflected inputs','',
          f'{len(inv["dynamic_reflection"]["methods"])} unique colon-method names, {len(inv["dynamic_reflection"]["object_class_paths"])} object/class paths, and {len(inv["dynamic_reflection"]["structs"])} struct shapes are runtime discoveries. Parameter layouts, flags, return/property offsets and nested child types are discovered through reflection. FText/Vector/Quat/Transform/FAssetData sizes stay dynamic. No constant transform or hit-result matrix is substituted.','',
          '| Dynamic method name | Original Lua line(s) |','|---|---|']
for name,lns in inv['dynamic_reflection']['methods'].items():lines.append('| `'+name+'` | '+', '.join(map(str,lns))+' |')
lines += ['', '| Dynamic object/class path | Original Lua line(s) |','|---|---|']
for name,lns in inv['dynamic_reflection']['object_class_paths'].items():lines.append('| `'+name+'` | '+', '.join(map(str,lns))+' |')
lines += ['', '| Dynamic struct shape / queried children |','|---|']
for name in inv['dynamic_reflection']['structs']:lines.append('| '+name+' |')
helper=inv['dynamic_reflection']['helper_get_player_controller']
lines += ['', 'Reached GetPlayerController helper also calls globals '+', '.join('`'+n+'`' for n in helper['called_globals'])+' and methods '+', '.join('`'+n+'`' for n in helper['called_methods'])+'. Dynamic source: '+link(inv['dynamic_reflection']['mod_source'])+'. Helper source: '+link(helper['source']['path'],line=helper['source']['line'])+'.','',
          '## Carried startup address prerequisites','',
          'These four SeedTable address pins carry prior derivation receipts. They are separate from native layout facts and required layout pins. Runtime startup authenticates its current image/cache and applicable binding conditions before use.','',
          '| Pin | ELF image virtual address | Slot / kind | Evidence |','|---|---|---|']
for seed in seedpins:lines.append('| '+seed['label']+' | '+seed['image_vaddr']+' | '+str(seed['slot'])+' / '+seed['kind']+' | '+link(seed['source'],line=seed['source_line'])+' |')
lines += ['', '## Audit and frozen provenance','',
          f'FACTS.json preserves {len(inv["version_queries"])} Version source queries and {len(inv["assertion_throw_audit"])} assertion/throw source sites. They are source audit counts, not executed calls or a mandatory global layout matrix. Superseded unresolved records remain marked as historical.','',
          'Final R2 profile digest: `'+digest+'`. Profile SHA256: `'+sha(PROFILE_PATH)+'`. Generated header equality is checked for all69 member pins, all7 vtable pins, and the unknown-width list. All input SHA256 values are preserved and rechecked after synthesis. V1 artifacts, source, build scripts, frozen evidence, and checkpoints are unchanged by this report task.','',
          'Local verification receipts are inherited from the frozen R2 profile; this synthesis performs static joins and hash/header checks only. The parent full pipeline receipt is separate.','',
          'Authoritative inputs: '+link(INV)+'; '+ '; '.join(link(p) for p in source_paths)+'; '+link(PROFILE_PATH)+'; '+link(HEADER)+'; '+link(PROFILE_DIR/'reports/FINAL-RECEIPT.json')+'.','']

assert input_hashes=={rel(p):sha(p) for p in INPUTS}
summary['verification']['inputs_unchanged_after_synthesis']=True
OUT.mkdir(exist_ok=True)
(OUT/'FACTS.json').write_text(json.dumps(summary,indent=2)+'\n')
(OUT/'FACT-TABLE.md').write_text('\n'.join(lines))
(OUT/'SOURCE-MANIFEST.json').write_text(json.dumps({'scope':'report input byte hashes; no input modified',
    'input_sha256':input_hashes,'output_sha256':{name:sha(OUT/name) for name in ['FACTS.json','FACT-TABLE.md']},
    'generator_sha256':sha(Path(__file__))},indent=2)+'\n')
print(json.dumps({'out':str(OUT),'counts':summary['counts'],'header_equality':header_checks,
                  'inputs_unchanged':True},indent=2))
