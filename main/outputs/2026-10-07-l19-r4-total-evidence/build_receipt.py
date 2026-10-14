#!/usr/bin/env python3
"""Local-only source census and native-prefix evidence; no runtime/source edits."""
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
PORT = ROOT / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port'
R3 = ROOT / 'outputs/2026-10-07-l19-local-profile-r3'
IMAGE = ROOT / '.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
IMAGE_SHA = '3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc'
SUFFIXES = {'.cpp', '.c', '.cc', '.cxx', '.hpp', '.h', '.hh', '.hxx', '.inl'}

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''): h.update(chunk)
    return h.hexdigest()

def save(name, obj):
    (OUT / name).write_text(json.dumps(obj, indent=2) + '\n')

def source_record(path):
    return {'path': str(path), 'sha256': sha(path), 'bytes': path.stat().st_size}

selection = json.loads((R3 / 'reports/SOURCE-SELECTION.json').read_text())
selected = [R3 / p for p in selection['source_files']]
for p in selected:
    assert sha(p) == selection['source_files'][str(p.relative_to(R3))]
files = set(selected)
for scope in ('unreal', 'ue4ss', 'first'):
    files.update(p for p in (PORT / scope).rglob('*') if p.is_file() and p.suffix.lower() in SUFFIXES)
sources = [source_record(p) for p in sorted(files)]
patterns = {
    'total_identifier': re.compile(r'\bUEP_TotalSize\b'),
    'member_map': re.compile(r'\bMemberOffsets\b'),
    'field_value_size': re.compile(r'\bStaticValueSize(?:Storage)?\b'),
    'field_class_macro': re.compile(r'\b(?:DECLARE_FIELD_CLASS(?:_WITH_CAST)?|IMPLEMENT_FIELD_CLASS|FFieldClassRegistrar(?:Template)?)\b'),
    'virtual_wrapper_macro': re.compile(r'\b(?:DECLARE_VIRTUAL_TYPE_BASE|IMPLEMENT_VIRTUAL_TYPE|VirtualFunctionRegistrar)\b'),
}
hits = []
for p in sorted(files):
    lines = p.read_text(errors='replace').splitlines()
    for number, line in enumerate(lines, 1):
        match = [name for name, pattern in patterns.items() if pattern.search(line)]
        if match:
            hits.append({'path': str(p), 'line': number, 'patterns': match, 'text': line})
wrapper_defs = []
for p in sorted(files):
    if p.name.startswith('MemberVariableLayout_SrcWrapper_'):
        text = p.read_text(errors='replace')
        for m in re.finditer(r'int32_t&\s+([\w:]+)::UEP_TotalSize\(\)\s*\{', text):
            start = text.count('\n', 0, m.start()) + 1
            tail = text[m.end():]
            # Existing generated bodies have nested lambda but no nested function.
            end = tail.find('\n}\n')
            body = text[m.start():m.end()+end+3]
            wrapper_defs.append({'path': str(p), 'class': m.group(1), 'line': start,
                                 'returns_cached_reference': 'static int32_t cached' in body,
                                 'missing_key_returns_negative_one': ': -1;' in body,
                                 'checked_throw_in_body': 'throw ' in body, 'body': body})
save('SOURCE-CONSUMER-CENSUS.json', {
    'scope': 'All C/C++ source/header/inl files under saved port unreal, ue4ss, first (including every generated wrapper), plus every R3 selected source file. Lexical source audit; external mods, unselected dependencies, generated future code, and arbitrary indirect external calls are outside scope.',
    'selected_R3_source_manifest': source_record(R3 / 'reports/SOURCE-SELECTION.json'),
    'source_files': sources,
    'source_file_count': len(sources),
    'selected_R3_source_pins_verified': len(selected),
    'patterns': {k: v.pattern for k, v in patterns.items()},
    'hits': hits,
    'hit_counts': {k: sum(k in h['patterns'] for h in hits) for k in patterns},
    'generated_total_getter_definitions': wrapper_defs,
    'consumer_decisions': [
        {'consumer':'NativeLayoutBinding::Prepare generic members loop', 'path':str(R3/'units/NativeLayoutBinding.cpp'), 'lines':[113,123],
         'role':'For every supplied nonempty class map, reads arbitrary class offsets[UEP_TotalSize] as positive total and verifies all offset/width spans. This shared consumer reaches FField/FFieldClass without direct class-qualified getter calls. Current exact-total requirement remains enforced.'},
        {'consumer':'Generated total getters', 'role':'All41 generated definitions were enumerated. FField and FFieldClass return a cached int32_t reference; missing lookup becomes-1 and no checked throw exists. Header wrapper declarations expose these getters publicly; arbitrary external callers remain possible.'},
        {'consumer':'UObjectArray.cpp and UObjectGlobals.cpp', 'role':'Qualified FUObjectItem::UEP_TotalSize calls compute object-item strides; TUObjectArray unqualified UEP_TotalSize returns its own total. These require exact size and are excluded from any prefix proposal.'},
        {'consumer':'UnrealInitializer.cpp', 'role':'Only direct total mutations concern FUObjectItem and UObjectBase vptr-size adjustments. Initializer assigns supplied offsets to all class maps; assigning a prefix as UEP_TotalSize would change the exposed exact-size datum and is forbidden.'},
        {'consumer':'UE4SSProgram member-map macro', 'role':'Enumerates supplied maps for logging. CustomProperty.cpp old map operations are comments. FOptionalProperty uses RepNotifyFunc offsets, not UEP_TotalSize; no FField-total-derived inheritance/allocation was found in audited tree.'},
        {'consumer':'UObjectMacros.hpp / VirtualFunctionHelper.hpp', 'role':'DECLARE/IMPLEMENT_FIELD_CLASS expose StaticClass and StaticValueSize storage; registrar stubs ignore arguments. Virtual type aliases/registrars dispatch wrappers. No UEP_TotalSize expansion exists. StaticValueSize is not physical FField/FFieldClass total.'},
        {'consumer':'FFieldClass::Construct', 'role':'Calls stored ConstructFn with FFieldClass/owner/name/flags arguments; no total allocation is performed by the saved wrapper.'},
    ],
})

prior = ROOT / 'outputs/2026-10-07-l19-remaining-total-evidence/REMAINING-TOTAL-FACTS.json'
old = json.loads(prior.read_text())
wanted = ['0xe0', '0xe1', '0xe4', '0xe6', '0xe7',
          '0x3645c4','0x3645cf','0x3645d2','0x3645d6','0x3645db',
          '0x3645f0','0x3645f7','0x364642','0x36464d','0x364651',
          '0x36453b','0x364549','0x364559','0x364560','0x364567','0x36457b','0x36457e',
          '0x3639a2','0x3639b7','0x3639bb','0x3639bf','0x3639c6','0x3639ca','0x3639d2','0x3639d6',
          '0x363a1d','0x363a22','0x363a32','0x363a39',
          '0x364cd2','0x364cd6','0x364cda','0x364cea','0x364cfd','0x364d00',
          '0x3701a0','0x3701a1','0x3701a4','0x3701a9','0x3701aa']
evidence = {k: old['instruction_evidence'][k] for k in wanted}
import capstone
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
image = IMAGE.read_bytes()
assert hashlib.sha256(image).hexdigest() == IMAGE_SHA
phoff = struct.unpack_from('<Q', image, 32)[0]
phsize, phnum = struct.unpack_from('<HH', image, 54)
loads = []
for i in range(phnum):
    p = struct.unpack_from('<IIQQQQQQ', image, phoff+i*phsize)
    if p[0] == 1: loads.append({'file_offset': p[2], 'image_vaddr': p[3], 'file_size': p[5], 'memory_size': p[6], 'flags': p[1]})
def offset(addr):
    candidates = [p['file_offset'] + addr-p['image_vaddr'] for p in loads if p['image_vaddr'] <= addr < p['image_vaddr']+p['file_size']]
    assert len(candidates) == 1
    return candidates[0]
for addr in [0x364710,0x36471d,0x364731,0x36474c,0x364762]:
    f = offset(addr)
    ins = next(md.disasm(image[f:f+15], addr, count=1))
    evidence[hex(addr)] = {'elf_image_vaddr': hex(addr), 'file_byte_offset': hex(f),
                           'instruction_bytes': ins.bytes.hex(), 'instruction': f'{ins.mnemonic} {ins.op_str}'}
for addr in [0x364530,0x364537,0x364539,0x364550,0x364555,0x364557,0x364583,0x364591,0x364594,0x364599,0x3645a0,0x3645ab]:
    f = offset(addr)
    ins = next(md.disasm(image[f:f+15], addr, count=1))
    evidence[hex(addr)] = {'elf_image_vaddr':hex(addr),'file_byte_offset':hex(f),
                          'instruction_bytes':ins.bytes.hex(),'instruction':f'{ins.mnemonic} {ins.op_str}'}

profile = R3 / 'profiles/townfall-build-facts.json'
public = ROOT / 'outputs/2026-10-07-l19-fact-summary-r3/PUBLIC-WIDTHS.json'
widths = {r['name']: r['selected_public_width_bytes'] for r in json.loads(public.read_text())['rows']}
required = json.loads(profile.read_text())['required_members']
spans = []
for r in required:
    cls, key = r['name'].split('::')
    if cls not in ('FField', 'FFieldClass') or key == 'UEP_TotalSize': continue
    w = widths[r['name']]
    bound = {'FField': 0x2c, 'FFieldClass': 0x3c}[cls]
    spans.append({'name': r['name'], 'offset': r['offset'], 'native_field_width_bytes': r['width'],
                  'R3_selected_public_accessor_width_bytes': w, 'access_end_exclusive': r['offset']+w,
                  'constructor_occupied_prefix_minimum_bytes': bound,
                  'arithmetic_span_within_prefix': r['offset'] >= 0 and 0 < w <= bound-r['offset'],
                  'runtime_semantics_approved': False if key == 'Owner' else None})
sources_extra = [prior, ROOT/'outputs/2026-10-07-l19-remaining-total-evidence/SIZE-QUERY-FAMILY.json',
                 ROOT/'outputs/2026-10-07-l19-unresolved-evidence/FIELDC-FACTS.json',
                 ROOT/'outputs/2026-10-07-l19-static-layout-check/REFLECTION-FACTS.json', profile, public,
                 OUT/'NATIVE-SYMBOL-SIZE-CHECK.json', OUT/'REGISTER-SLOT0-SEARCH.json']
public_data = json.loads(public.read_text())
sources_extra.append(Path(public_data['object_path']))
save('NATIVE-PREFIX-CONSUMER-FACTS.json', {
    'format': 'l19-r4-native-prefix-consumer-evidence-v1',
    'scope': 'Saved exact-image static constructor minimum occupied prefixes; proposal-only access contract. No exact total, map publication, runtime test, or permission to relax current preflight.',
    'image': {'path': str(IMAGE), 'sha256': IMAGE_SHA, 'bytes': len(image)}, 'PT_LOAD': loads,
    'exact_total_facts': [],
    'unresolved': [
        {'key':'FField.UEP_TotalSize','status':'unknown','candidate_only':'0x30','missing_proof':'Native slot0 physical-size semantics or independent complete-object allocation/copy/stride proof.'},
        {'key':'FFieldClass.UEP_TotalSize','status':'unknown','candidate_only':'0x40','missing_proof':'Independent complete-object allocation/copy/stride or named registration total; occupied extent and guard placement insufficient.'}],
    'verified_prefixes': [
        {'class':'FField','minimum_occupied_bytes':0x2c,'evidence_keys':['0x3645c4','0x3645cf','0x3645db','0x364651'],
         'derivation':'Target root constructor installs known FField vptr; sixteen-byte store at this+1C occupies through2B; variant constructor independently writes Flags four bytes at+28. Therefore valid instances passed to these constructors occupy at least2C bytes. This is a lower bound, not a physical total or live page-readability guarantee.'},
        {'class':'FFieldClass','minimum_occupied_bytes':0x3c,'evidence_keys':['0x3639a2','0x3639d6','0x364560','0x36457b','0x36457e'],
         'derivation':'Named FField static descriptor passes its storage to common FFieldClass constructor; constructor retains self inRBX and stores counter dword at self+38. Therefore descriptor occupied extent is at least3C. This is a lower bound, not an exact total.'}],
    'instruction_evidence': evidence,
    'data_evidence': [{'elf_image_vaddr':'0x777d7c2','file_byte_offset':hex(offset(0x777d7c2)),
                       'bytes':image[offset(0x777d7c2):offset(0x777d7c2)+14].hex(),'expected_utf16le':'FField\u0000'}],
    'file_data_evidence':[{'file_byte_offset':0,'bytes':image[:64].hex(),'kind':'ELF64 header',
                           'section_header_offset':struct.unpack_from('<Q',image,40)[0],
                           'section_header_entry_size':struct.unpack_from('<H',image,58)[0],
                           'section_header_count':struct.unpack_from('<H',image,60)[0],
                           'interpretation':'The advertised section table lies beyond the saved file; a conventional in-file symbol-size section cannot be recovered through these ELF header coordinates. Parser symtab_count0 is not proof of every possible proprietary symbol table being absent.'}],
    'required_R3_accessor_span_checks': spans,
    'serialization_observation': 'FField::Serialize sends Name at+20 and Flags at+28 with width4 separately to archive methods; no whole-object bytecount is established.',
    'guard_bound_hypothesis': {
        'descriptor_image_vaddr':'0x939ec98','guard_candidate_image_vaddr':'0x939ecd8','gap_bytes':0x40,
        'guarded_flow_evidence':['0x364530','0x364549','0x364550','0x364555','0x364557','0x364559','0x36457b','0x36457e','0x364583','0x364591','0x364594','0x364599','0x3645a0','0x3645ab'],
        'native_observation':'Same function tests candidate guard byte, passes guard address to external thunk, conditionally constructs named descriptor, registers descriptor/dtor addresses through another thunk, passes guard address to final thunk, and returns descriptor. Exact __cxa import names were not established in this receipt.',
        'decision':'No exact total. Even if the candidate is independently established as separately owned guard storage and nonoverlap gives upper40, occupied lower3C leaves interval[3C,40]. Qword members/stores and aligned instance address do not prove required class alignment8; packed3C plus4 padding remains compatible. Unlike contained UObjectBase inside named complete Object allocation28, this has neither a complete-object allocation upper-bound nor equal lower-bound.'},
    'sources': [source_record(p) for p in sources_extra],
    'proposal': {
        'name':'VerifiedNativeMemberPrefix (new metadata capability, not UEP_TotalSize)',
        'values': {'FField':0x2c,'FFieldClass':0x3c},
        'allowed_scope':'Audited individual member access with independently measured offsets and exact selected accessor footprints on authenticated target instances; binder may use this separately named bound only after explicit implementation/review.',
        'forbidden_uses':['Allocation bytecount','by-value array stride','whole-object copying/serialization','inheritance offset derivation','reported physical sizeof','automatic default for unknown classes'],
        'total_getter_guard':'Unknown exact total must be checked before caching or unsigned conversion; throw/refuse a checked exact-total query for FField/FFieldClass. Never publish a prefix into MemberOffsets[UEP_TotalSize]. Existing generated reference getters cache missing as -1 and remain unsafe for unchecked future unsigned allocation. Guard both public total getters or force every reachable exact-total consumer through an enforced checked API; code review alone does not prove external callers cannot bypass it.',
        'reachability_conditions':['Current R3 generic NativeLayoutBinding bounds loop requires exact positive total for every nonempty class map; no prefix path exists today.', 'Prepared fixed required-member/width graph is audited here; arbitrary extension offsets/widths and additional runtime classes require a new audit.', 'No direct FField/FFieldClass total invocation in audited implementation files outside generated definitions; generated public getters and mutable maps remain reachable by unexamined external mods.', 'Prefix arithmetic for Owner16 does not approve the ABI interpretation or store behavior. Parent separately addresses native tagged8 versus legacy wrapper16.'],
        'implementation_performed':False,
    },
    'doc_reference': {'url':'https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/UObject/FField/GetFieldSize?application_version=5.5',
                     'retrieval':'Supplied by parent; this agent performed no network request.', 'supplied_signature':'virtual SIZE_T GetFieldSize() const',
                     'supplied_header':'/Engine/Source/Runtime/CoreUObject/Public/UObject/Field.h',
                     'supplied_return_semantics':None,
                     'decision':'Plausible method-family hypothesis only. Documentation names a concept but supplies no body/return semantics and does not map exact-image slot0. No native total promoted.'},
})
print(json.dumps({'source_files_hashed':len(sources),'R3_selected_pins_verified':len(selected),'census_hits':len(hits),
                  'generated_total_definitions':len(wrapper_defs),'native_instruction_windows':len(evidence),
                  'positive_required_member_span_checks':len(spans),'exact_totals_promoted':0}))
