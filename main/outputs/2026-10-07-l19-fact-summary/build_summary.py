#!/usr/bin/env python3
"""Join frozen, local-only l19 fact artifacts; never modify inputs."""
import configparser
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = ROOT / 'outputs/2026-10-07-l19-fact-summary'
INV = ROOT / 'outputs/2026-10-07-l19-fact-inventory/FACTS.json'
STATIC_DIR = ROOT / 'outputs/2026-10-07-l19-static-layout-check'
PROFILE_DIR = ROOT / 'outputs/2026-10-07-l19-local-profile'
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

inv = read(INV)
profile = read(PROFILE_PATH)
receipt = read(PROFILE_DIR/'reports/FINAL-RECEIPT.json')
sources = [read(STATIC_DIR/'STATIC-FACTS.json'), read(STATIC_DIR/'REFLECTION-FACTS.json')]
source_names = ['STATIC-FACTS.json', 'REFLECTION-FACTS.json']
member_path = Path(inv['actual_layout_provenance']['matching_local_member_candidate'])
vtable_path = Path(inv['actual_layout_provenance']['matching_local_vtable_candidate'])
INPUTS += [member_path, vtable_path]
input_hashes = {rel(p):sha(p) for p in INPUTS}
profile_source_checks = []
for path,expected in profile['evidence']['sources'].items():
    actual=sha(ROOT/path)
    assert actual==expected, path
    profile_source_checks.append({'path':str(ROOT/path),'sha256':actual,'matches_profile':True})
source_selection=read(PROFILE_DIR/'reports/SOURCE-SELECTION.json')
for path in ['units/RuntimeBuildProfile.hpp','units/BuildFactsChecks.hpp','profiles/townfall-build-facts.json']:
    assert sha(PROFILE_DIR/path)==source_selection['source_files'][path],path

def ini(path):
    c = configparser.ConfigParser(interpolation=None)
    c.optionxform = str
    c.read(path)
    return c

member_ini, vtable_ini = ini(member_path), ini(vtable_path)
members = {p['name'].replace('::','.'):p for p in profile['required_members']}
vtables = {p['name'].replace('::','.'):p for p in profile['required_vtables']}
static_records, static_index = [], {}
for document, filename in zip(sources,source_names):
    for f in document['facts']:
        entry = dict(f)
        entry['source'] = str(STATIC_DIR/filename)
        entry['instructions'] = [{**document['evidence'][k], 'evidence_key':k,
                                  'disassembly_artifact':str(STATIC_DIR/document['evidence'][k]['disassembly_artifact'])}
                                 for k in f['evidence_keys']]
        static_records.append(entry)
        static_index.setdefault(f['key'],[]).append(entry)
assert len(static_records)==39 and len(static_index)==37
duplicates = {k:len(v) for k,v in static_index.items() if len(v)>1}
assert duplicates == {'UFunction.ParmsSize':2,'UFunction.ReturnValueOffset':2}

header_text = HEADER.read_text()
digest = re.search(r'kDigest="([0-9a-f]+)"',header_text).group(1)
assert digest==receipt['profile_digest']
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
assert (len(members),known_members,len(vtables),known_vtables)==(69,44,7,1)

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
                'width_bytes':f['width_bytes'],'confidence':'high','static_fact_key':skey,
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
        row['resolution_note']='Func is target +0xD8 with an 8-byte native function pointer. Selected GetFunc419/GetFunc reads/writes a pointer reference (8B). GetAccessorWidth retains conservative 16B authority from GetFuncBase; it is not evidence of a 16B active access. Historical C8 identifies EventGraphFunction.'
    if fid=='member:UStruct.MinAlignment':
        row['resolution_note']='Target field is word +0x5C (2B). Retained binding width authority is 4B; do not confuse it with the measured target operand.'
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
for fid in profile['unknown_required_static_facts']:
    rows.append({'id':fid,'origin':'l19-required-static-width','requirement':'required',
                 'requirement_condition':'selected cast-mask width proof, separately required from member offset',
                 'stage':'l19 preflight before seeded consumers','consumer':[{'path':str(HEADER)}],
                 'saved_staged_declaration':saved_declaration(fid),
                 'measured_target':{'status':'unknown','value':None,'confidence':'unresolved','fresh_live_validation':False},
                 'profile_supplied':{'status':'explicit-unknown','value':None,'mandatory_static_width':True,'path':str(PROFILE_PATH)},
                 'binding_validator_width_bytes':None})
assert len({r['id'] for r in rows})==len(rows)
assert {r['measured_target'].get('static_fact_key') for r in rows if r['measured_target'].get('static_fact_key')}==set(static_index)

unknown_members=[p['name'] for p in profile['required_members'] if p['offset'] is None]
unknown_vtables=[p['name'] for p in profile['required_vtables'] if p['offset'] is None]
assert len(unknown_members)==25 and len(unknown_vtables)==6
seedtext=(PROFILE_DIR/'units/SeedTable.h').read_text()
seedpins=[]
for m in re.finditer(r'inline constexpr SlotSeed (\w+)\{ (true|false), (true|false), (0x[0-9a-f]+), "([^"]+)", "((?:\\.|[^"\\])*)" \}; // slot (\d+) \((\w+)\)',seedtext):
    seedpins.append({'constant':m[1],'known':m[2]=='true','is_data':m[3]=='true','image_vaddr':m[4],
                     'label':m[5],'provenance':json.loads('"'+m[6]+'"'),'slot':int(m[7]),'kind':m[8],
                     'source':str(PROFILE_DIR/'units/SeedTable.h'),'source_line':seedtext[:m.start()].count('\n')+1,
                     'scope':'carried prior prerequisite; not one of 37 l19 static layout facts; no l19 live claim'})
assert len(seedpins)==4

summary={
    'schema':'l19-joined-facts-v1','scope':'LOCAL ONLY report synthesis; no source/build/frozen input edits; no console/network/contact/upload/game execution',
    'status':'final frozen-source synthesis; exact required target gaps remain',
    'image':profile['image'],'profile_digest':digest,'profile_file_sha256':sha(PROFILE_PATH),
    'counts':{'inventory_fact_rows':len(inv['facts']),'joined_fact_rows':len(rows),
              'required_member_pins':69,'known_member_pins':44,'unknown_member_pins':25,
              'required_vtable_pins':7,'known_vtable_pins':1,'unknown_vtable_pins':6,
              'unknown_required_cast_width_facts':2,'positive_static_records':39,'positive_static_unique_facts':37,
              'static_duplicate_keys':duplicates,'static_instruction_window_checks':141,
              'saved_local_member_declarations':17,'saved_local_vtable_declarations':1,
              'dynamic_method_names':len(inv['dynamic_reflection']['methods']),
              'dynamic_object_class_paths':len(inv['dynamic_reflection']['object_class_paths']),
              'dynamic_struct_shapes':len(inv['dynamic_reflection']['structs']),
              'version_source_queries':len(inv['version_queries']),'assertion_throw_source_sites':len(inv['assertion_throw_audit']),
              'requirements_in_joined_rows':dict(Counter(r['requirement'] for r in rows))},
    'required_unresolved':{'member_values':unknown_members,'vtable_slots':unknown_vtables,
                           'cast_accessor_widths':profile['unknown_required_static_facts'],
                           'counting_policy':'25 offset/total values + 6 vtable slots + 2 separate width facts. The two cast fields also appear in the 25 values; widths do not add two more member offsets. Historical freeze77 is not a final gap count.'},
    'current_staged_layout_provenance':inv['actual_layout_provenance'],
    'freshness_policy':'Saved local INIs are candidates, size-corroborated by the saved l18 loadlist; their exact current installed byte hashes are unproved. Profile known values and static image proof do not establish current game memory or publication. Fresh startup authenticates its own full image/current generation/base/deadline and reads staged bytes through NativeModSource.',
    'preflight_outcome':receipt['current_case_overall_startup'],
    'superseded_inventory_gate':'The old inventory first-source-order FUObjectItem gap is resolved statically. Current l19 preflight collects all required declarations/measured values/widths before seeded engine consumers; no claim that FUObjectItem remains the first gap.',
    'axes_encoding':{'staged_INI':{'NonShipping':0,'Stats':0},'profile_and_detector':{'build_configuration':1,'build_meaning':'Shipping','stats':1,'stats_meaning':'Off'},'independent_axes':True},
    'facts':rows,'positive_static_facts':static_index,
    'dynamic_reflection':inv['dynamic_reflection'],
    'dynamic_policy':'Methods, paths, parameter names/types, struct child offsets and runtime sizes are discovered by reflection; they are not mandatory native constant offsets. No original menu/controller/R3/spawn success is established.',
    'carried_startup_seed_pins':seedpins,
    'version_queries':inv['version_queries'],'assertion_throw_audit':inv['assertion_throw_audit'],
    'census_policy':'144 version source queries and 719 assertion/throw source sites are reachability-aware source audits, not counts of executed operations or mandatory target constants.',
    'historical_static_unresolved':[{**u,'source':str(STATIC_DIR/name),'superseded_by_positive_static':u['key'] in static_index}
                                    for doc,name in zip(sources,source_names) for u in doc['unresolved']],
    'profile_evidence':profile['evidence'],'local_verification_receipt':receipt,
    'source_selection':{'path':str(PROFILE_DIR/'reports/SOURCE-SELECTION.json'),
                        'sha256':sha(PROFILE_DIR/'reports/SOURCE-SELECTION.json')},
    'input_sha256':input_hashes,'verification':{'json_header_pin_equality':header_checks,
          'unknown_width_list_header_equality':True,'all_static_keys_represented':True,
          'all_inventory_rows_preserved':True,'profile_source_hash_checks':profile_source_checks,
          'profile_header_consumer_source_selection_hashes_match':True,'inputs_unchanged_after_synthesis':False},
    'limitations':inv['limitations']+[
         'Target operand width and selected active accessor width differ from retained binding validator authority (notably Func8/16, MinAlignment2/4). No width policy changed.',
         'Exact class totals remain unknown; member extents/lower bounds and compiler templates are not exact sizeof evidence.',
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
    m=row['measured_target']; skey=m.get('static_fact_key')
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
    if width is not None:pv+='; validator '+str(width)+'B'
    req=row['requirement']+'; '+row['stage']
    return '| '+' | '.join(escape(v) for v in [row['id'],declaration,measurement,pv,req,source_cell(row)])+' |'

lines=['# l19 final startup/menu/spawn fact table','',
       'LOCAL ONLY. This joins the saved declarations, target evidence and final generated profile. The current complete startup remains **REFUSE** before seeded engine calls because required exact layout facts remain unknown. No original menu, controller, R3 carrier or spawn success is claimed.','',
       '**69 required member pins: 44 known / 25 unknown. 7 required vtable pins: 1 known / 6 unknown. Two cast accessor widths remain separately unknown.** The static reports contain **37 unique positive facts** (39 records, with ParmsSize and ReturnValueOffset duplicated) and 141 instruction-window checks. These counts describe different sets and must not be added together.','',
       'The saved local map contains **17 member declarations and one vtable declaration**. Saved l18 loadlist sizes (588B / 216B) corroborate the candidates. They do **not** prove the exact current installed map hashes. The 17 array values are retained historical GUA-gated target pins; this slice did not newly measure them. The 27 additional required member values have direct l19 static instruction evidence.','',
       'Profile detector enums **build_configuration=1 (Shipping), stats=1 (Off)** correspond to INI booleans **NonShipping=0, Stats=0**. Both axes are independently resolved. Revision5.6.1-0+++UE5 is reconstructed from image components; build-id04bb4330a7d71dcefe1cc2c4d19769d5 is the significant16-byte ELF GNU link identity.','',
       'The old inventory source-order FUObjectItem first gap is superseded: its stride/Object/Flags are now measured. The l19 preflight collects all required declarations, measured values and width facts before seeded consumers. Missing staged declarations and unknown target facts remain distinct failures.','',
       '## Exact remaining mandatory gaps','',
       '| Kind | Exact unresolved value |','|---|---|']
for name in unknown_members:lines.append('| member offset / exact total | `'+name+'` |')
for name in unknown_vtables:lines.append('| vtable byte slot | `'+name+'` |')
for name in profile['unknown_required_static_facts']:lines.append('| cast accessor width | `'+name+'` |')
lines += ['', 'There are **25 member values + 6 vtable slots + 2 width facts**. The two cast fields also occur among the 25 unknown member values; their widths are a separate requirement. No exact UObjectBase/UField/UStruct/UClass/FField/FFieldClass/FProperty/UFunction/property-subclass total was guessed from a last member or a compiler template. The historical freeze77 unknown tally is not the final gap count.','',
          '## Joined fact census','',
          'Every original inventory row is preserved below; additional l19 cache/format/width checks and supplemental static facts are included explicitly. “Dormant” means outside the selected mandatory accessor graph, including the editor-only compile policy. “Dynamic” means reflection must supply the current value. “Port contract” is a compiled assumption, not independent target proof. All direct instruction bytes, addresses and inference chains are retained in FACTS.json under each measured fact.','',
          '| Fact | Saved staged declaration | Measured target | Profile supplied / validator width | Requirement / stage | Evidence / confidence |',
          '|---|---|---|---|---|---|']
lines += [table_row(r) for r in rows]
lines += ['', '## Width and dormant-policy distinctions','',
          '`UFunction.Func` is **+0xD8 / 8B**. C8 is EventGraphFunction. The selected GetFunc419/GetFunc pointer-reference access is 8B; the retained binding validator reports 16B from its conservative GetFuncBase maximum. UStruct.MinAlignment is target2B at5C while the retained validator authority is4B. Neither numeric mismatch establishes an active access of the validator width. The profile consumer preserves that policy and cannot turn a measured offset into an unsafe engine-call permission.','',
          'Profile width=null is emitted as header width0 and means no independent pin-width comparison; it does not mean a zero-byte accessor. This applies to the 17 retained array pins and class total/slot declarations. Other unavailable binding widths remain unavailable; the table does not infer them from native operands.','',
          '`WITH_EDITORONLY_DATA` remains unknown and dormant on the actual runtime Version path. Shipping does not establish it. The global `UE_ENABLE_FUOBJECT_ITEM_PACKING` spelling/value remains unknown, while the effective 5.6 native Object0/Flags8/stride18 chunked path is independently measured. `FProperty.InitializeValueInternal` remains conditional, outside the seven mandatory slots.','',
          '## Dynamic reflected inputs','',
          'These are runtime discoveries, not additional unresolved native offset pins: 33 unique colon-method names, 16 object/class paths and 11 struct shapes. Method parameter layouts, function flags, return/property offsets and nested child types are discovered through reflection. FText/Vector/Quat/Transform/FAssetData sizes stay in the dynamic rows above. No handwritten constant transform or hit-result matrix is substituted.','',
          '| Dynamic method name | Original Lua line(s) |','|---|---|']
for name,lns in inv['dynamic_reflection']['methods'].items():lines.append('| `'+name+'` | '+', '.join(map(str,lns))+' |')
lines += ['', '| Dynamic object/class path | Original Lua line(s) |','|---|---|']
for name,lns in inv['dynamic_reflection']['object_class_paths'].items():lines.append('| `'+name+'` | '+', '.join(map(str,lns))+' |')
lines += ['', '| Dynamic struct shape / queried children |','|---|']
for name in inv['dynamic_reflection']['structs']:lines.append('| '+name+' |')
helper=inv['dynamic_reflection']['helper_get_player_controller']
lines += ['', 'Reached helper `GetPlayerController` also calls globals '+', '.join('`'+n+'`' for n in helper['called_globals'])+' and methods '+', '.join('`'+n+'`' for n in helper['called_methods'])+'. Other helper exports are omitted. Dynamic source: '+link(inv['dynamic_reflection']['mod_source'])+'. Helper source: '+link(helper['source']['path'],line=helper['source']['line'])+'.','',
          '## Carried startup address prerequisites','',
          'These four selected SeedTable address pins carry prior derivation receipts; they are separate from the 37 new static layout facts and from required layout pins. Runtime startup must authenticate the current image/cache and the applicable binding conditions before using them.','',
          '| Pin | ELF image virtual address | Slot / kind | Evidence |','|---|---|---|']
for seed in seedpins:lines.append('| '+seed['label']+' | '+seed['image_vaddr']+' | '+str(seed['slot'])+' / '+seed['kind']+' | '+link(seed['source'],line=seed['source_line'])+' |')
lines += ['', '## Audit and frozen provenance','',
          'FACTS.json preserves all144 Version source queries, all719 assertion/throw source sites and the inventory reachability notes. They are source audit counts, not executed calls and not a mandatory global layout matrix. It also preserves superseded partial-static records explicitly; positive supplemental evidence takes precedence.','',
          'Final profile digest: `'+digest+'`. Profile file SHA256: `'+sha(PROFILE_PATH)+'`. Generated header equality was checked for all69 member pins, all7 vtable pins and both unknown width names. Input SHA256 values are saved in FACTS.json and SOURCE-MANIFEST.json; all inputs were rehashed after synthesis. No source, build script, frozen evidence, or checkpoint was edited.','',
          'Inherited local profile verification reports18 extracted initializer scenarios,18 strict-schema refusal cases, nine retained host suites and both affected target TUs compiled. Those are prior local receipts, not new game/runtime testing by this synthesis. The parent full pipeline result is separate.','',
          'Authoritative inputs: '+link(INV)+'; '+link(STATIC_DIR/'STATIC-FACTS.json')+'; '+link(STATIC_DIR/'REFLECTION-FACTS.json')+'; '+link(PROFILE_PATH)+'; '+link(HEADER)+'; '+link(PROFILE_DIR/'reports/FINAL-RECEIPT.json')+'.','']

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
