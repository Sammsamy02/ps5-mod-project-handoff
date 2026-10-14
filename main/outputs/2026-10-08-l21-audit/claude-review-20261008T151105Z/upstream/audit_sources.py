"""Read-only audit of selected sources; writes reports only beside this file."""
from pathlib import Path
import csv
import hashlib
import json

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
PORT = ROOT / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port'
UP = ROOT / '.superpowers/native-dependency-spike/ue4ss-unreal-source-access-1/upstream'
PUBLIC = ROOT / '.superpowers/native-dependency-spike/ue4ss-binding-research-1/upstream/RE-UE4SS'
MANIFEST = ROOT / 'outputs/2026-10-08-menu-complete-build/SOURCE-SELECTION.json'
COMPOSITION = ROOT / 'outputs/2026-10-08-menu-complete-build/pass1/COMPOSITION-RECEIPT.json'
manifest = json.loads(MANIFEST.read_text())
composition = json.loads(COMPOSITION.read_text())
units = {r['name']: r for r in composition['selected_units']}
pins = {r['path']: r for r in manifest['source_pins']}
private = json.loads((UP.parent / 'source-receipt.json').read_text())
CORE = 'https://github.com/Re-UE4SS/UEPseudo/blob/' + private['commit'] + '/'
BASE = 'e3ba1016562d6c0868c410d0a71e88bfcdbf691b'
PUBLIC_URL = 'https://github.com/UE4SS-RE/RE-UE4SS/blob/' + BASE + '/'

def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n')

def unit(name):
    return Path(units[name]['source'])

def ev(path, needle, original=None):
    path = Path(path)
    line = next((i for i, text in enumerate(path.read_text().splitlines(), 1) if needle in text), None)
    assert line, (path, needle)
    sha = digest(path)
    data = dict(path=str(path), line=line, needle=needle, sha256=sha,
                selected_pin_matches=pins.get(str(path), {}).get('sha256') == sha,
                selected_tus=[r['name'] for r in units.values() if r['source'] == str(path)])
    if original:
        canonical = UP / original
        data.update(retained_upstream_path=str(canonical), retained_upstream_sha256=digest(canonical),
                    whole_file_matches_retained_upstream=sha == digest(canonical),
                    retained_upstream_commit=private['commit'])
    return data

def p(path):
    return PORT / path

rows = []
def add(ident, version, status, change, evidence, links, limit, origin=None):
    rows.append(dict(id=ident, engine_version=version, status=status, change=change,
                     evidence=evidence, upstream_links=links, limits=limit,
                     origin_evidence=origin or 'Retained UEPseudo snapshot at pinned revision; exact introduction commit unknown because local archive has no Git history.',
                     native_execution_proven=False))

add('54-text', '5.4+', 'present',
    'FText supports the 0x10-byte newer layout with dynamic active size; copy/destruction use engine FTextProperty operations instead of assuming the older shared-reference representation.',
    [ev(p('unreal/include/Unreal/FText.hpp'), 'before 5.4', 'include/Unreal/FText.hpp'),
     ev(unit('unreal-flat-ftext'), 'ValueOpsProperty->CopyCompleteValue', 'src/FText.cpp'),
     ev(unit('unreal-flat-typechecker'), 'FText::ValueOpsProperty', 'src/TypeChecker.cpp')],
    [CORE+'include/Unreal/FText.hpp', CORE+'src/FText.cpp'],
    'Engine value operations, reflected size and a target FText roundtrip remain unproved by this source-only audit.')
add('54-optional', '5.4+', 'present',
    'OptionalProperty discovery scans field classes and WorldPartitionRuntimeCellData. Its layout operations distinguish intrusive unset state from a separate is-set flag and initialize/destroy values accordingly.',
    [ev(unit('unreal-flat-typechecker'), 'type_name == FName(STR("OptionalProperty")', 'src/TypeChecker.cpp'),
     ev(unit('unreal-flat-typechecker'), 'WorldPartitionRuntimeCellData', 'src/TypeChecker.cpp'),
     ev(p('unreal/include/Unreal/Property/FOptionalProperty.hpp'), 'HasIntrusiveUnsetOptionalState', 'include/Unreal/Property/FOptionalProperty.hpp')],
    ['https://github.com/UE4SS-RE/RE-UE4SS/pull/708', CORE+'include/Unreal/Property/FOptionalProperty.hpp'],
    'ValueProperty is located after RepNotifyFunc + FName::StaticSize; correct native metadata is still required. Discovery is not a complete Lua optional-value API.',
    'PR #708 explicitly adds TOptional/FOptionalProperty and moves the Unreal gitlink to 0a6f51de59e967199d64cc54c1c95c7b6af33b6b; exact private implementation diff unavailable locally.')
add('54-dynamicclass', '5.4+', 'present',
    'Initializer stops looking up UDynamicClass for 5.4 and above, where retained upstream says that class was removed.',
    [ev(unit('unreal-unrealinitializer'), 'Version::IsAtLeast(4, 11) && Version::IsBelow(5, 4)')],
    [CORE+'src/UnrealInitializer.cpp'],
    'Selected PS5 initializer preserves this guard and also has native startup/profile/thread gates.')
add('54-hitresult', '5.4+', 'partial',
    'LineTraceMod switches HitObjectHandle.Actor:Get() to HitObjectHandle.ReferenceObject:Get(); LuaUScriptStruct errors name the missing property.',
    [ev(p('ue4ss/src/LuaType/LuaUScriptStruct.cpp'), "Was unable to retrieve property '{}' mapped")],
    ['https://github.com/UE4SS-RE/RE-UE4SS/pull/503/files'],
    'The diagnostic is selected. LineTraceMod/shared Lua asset deployment is outside this C++ selection manifest and is unknown here.',
    'Exact public #503 diff, merged af9c6b66f26f414e281619e298db0da51978434b on 2024-05-17. Core support requires private UEPseudo #90.')
add('55-optional-generator', '5.4/5.5', 'present',
    'SDK/UHT generation recursively emits TOptional<T> and optional default expressions, and skips unsupported optional initialization.',
    [ev(unit('generator-sdkgenerator-common'), 'return fmt::format(STR("TOptional<{}>"'),
     ev(unit('generator-sdkgenerator-ueheadergenerator'), '// TODO: Support optional properties initialization later'),
     ev(unit('generator-sdkgenerator-ueheadergenerator'), 'STR("TOptional<{}>()")')],
    ['https://github.com/UE4SS-RE/RE-UE4SS/pull/708/files'],
    'Skipping optional initialization is deliberate in upstream. Generator support is not complete Lua get/set support.',
    'Exact public #708 diff; merged f4a775ec48466ce2e8d31eb2ab88d5a4f204d3b0 on 2024-12-06.')
add('55-enum-width', '5.5+', 'present',
    'UEnum::ECppForm and EEnumFlags use byte-sized types/accessors. Retained source documents narrowing from four bytes to one in 5.5.',
    [ev(p('unreal/include/Unreal/CoreUObject/UObject/Class.hpp'), 'enum class ECppForm : uint8', 'include/Unreal/CoreUObject/UObject/Class.hpp'),
     ev(p('unreal/include/Unreal/UnrealFlags.hpp'), 'enum class EEnumFlags : uint8_t', 'include/Unreal/UnrealFlags.hpp'),
     ev(p('unreal/generated_include/MemberVariableLayout_SrcWrapper_UEnum.hpp'), 'UEnum::ECppForm& UEnum::GetCppForm()', 'generated_include/MemberVariableLayout_SrcWrapper_UEnum.hpp')],
    [CORE+'include/Unreal/CoreUObject/UObject/Class.hpp', CORE+'include/Unreal/UnrealFlags.hpp'],
    'Typed reads are present; native offsets remain separate evidence. This changes access width, not just an offset table.')
add('56-construct', '5.6+', 'present_with_limit',
    'StaticConstructObject builds a distinct Above55 parameter structure with a 0x30-byte callback slot, SerialNumber, conditional remote-object fields and moved SubobjectOverrides.',
    [ev(unit('unreal-uobjectglobals'), 'else if (Version::IsBelow(5, 6))', 'src/UObjectGlobals.cpp'),
     ev(p('unreal/include/Unreal/UObjectGlobals.hpp'), 'uint8 PropertyInitCallback[0x30]', 'include/Unreal/UObjectGlobals.hpp'),
     ev(p('unreal/include/Unreal/Hooks/Internal/DetourSubclasses.hpp'), 'v506    // 5.6+')],
    [CORE+'src/UObjectGlobals.cpp', CORE+'include/Unreal/UObjectGlobals.hpp', 'https://github.com/UE4SS-RE/RE-UE4SS/pull/977'],
    'The upstream comment explicitly calls the new TFunction unsupported: its slot is zeroed padding, not callable support. UE_WITH_REMOTE_OBJECT_HANDLE defaults to 0. Native ABI/remote-handle mode are unproved.',
    '#977 requires private UEPseudo #148. The retained core has this concrete version branch; exact historical private introduction commit unknown.')
add('56-alignment', '5.6+', 'present',
    'UStruct::GetMinAlignment reads int16 through GetMinAlignment506 instead of the older int32 accessor width.',
    [ev(unit('unreal-ustruct'), 'const int16& UStruct::GetMinAlignment() const', 'src/UStruct.cpp'),
     ev(p('unreal/generated_include/MemberVariableLayout_SrcWrapper_UStruct.hpp'), 'int16& UStruct::GetMinAlignment506()', 'generated_include/MemberVariableLayout_SrcWrapper_UStruct.hpp')],
    [CORE+'src/UStruct.cpp'],
    'The common accessor always takes the narrow path, rather than dynamically choosing int32 for older versions. Its width matches the retained 5.6 model, but its native offset is separate.')
add('56-string-discovery', '5.6+', 'present',
    'TypeChecker discovers UTF-8/ANSI string-property classes directly and within array/set/map members; VerseEnum supplies a UTF-8 metadata probe.',
    [ev(unit('unreal-flat-typechecker'), '// Check inner element property for its type (e.g., AnsiStrProperty in arrays)', 'src/TypeChecker.cpp'),
     ev(unit('unreal-flat-typechecker'), 'if (type_name == FName(STR("Utf8StrProperty")', 'src/TypeChecker.cpp'),
     ev(unit('unreal-flat-typechecker'), 'CoreUObject.VerseEnum', 'src/TypeChecker.cpp')],
    [CORE+'src/TypeChecker.cpp'],
    'The separate VerseClass ANSI fallback is gated at 5.7. Finding a property class does not connect a Lua value pusher or prove a target invocation.')
add('56-string-lua', '5.6 follow-up', 'partial_missing_registration',
    'FUtf8String/FAnsiString constructors, property-pusher function bodies and SDK/UHT type handling exist, but both property-pusher registration entries from #1015 are missing.',
    [ev(unit('luamod'), 'lua.register_function("FUtf8String"'),
     ev(unit('luamod'), 'lua.register_function("FAnsiString"'),
     ev(unit('lua-luauobject'), 'auto push_utf8strproperty'),
     ev(unit('lua-luauobject'), 'auto push_ansistrproperty'),
     ev(unit('program'), 'LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("StrProperty")'),
     ev(unit('generator-sdkgenerator-common'), 'property->IsA<FUtf8StrProperty>()')],
    ['https://github.com/UE4SS-RE/RE-UE4SS/pull/1015/files', PUBLIC_URL+'UE4SS/src/UE4SSProgram.cpp'],
    'Selected Program and retained canonical e3ba1016 Program both omit the two entries. Bodies alone are insufficient for ordinary reflected property routing. This is inherited at the selected upstream revision, not shown to be PS5-only. No current menu-causality claim.',
    'Exact public #1015 diff adds both registrations; merged 800347c544458fdc98fd4ff7a0379875032e0434 on 2025-11-06. Initial #977 explicitly excluded UTF-8 string support.')
add('gap-optional-lua', '5.4+', 'absent_in_selected_surface',
    'A dedicated Lua OptionalProperty value pusher and registration are absent from the selected LuaUObject/Program surface.',
    [ev(unit('program'), 'LuaType::StaticState::m_property_value_pushers.emplace(FName(STR("ObjectProperty")'),
     ev(unit('lua-luauobject'), 'if (StaticState::m_property_value_pushers.contains(name_comparison_index))')],
    ['https://github.com/UE4SS-RE/RE-UE4SS/pull/708/files'],
    'Scoped negative search; the unsupported-property route exists. This is a broader API limit and is not evidence of the current menu failure or a port regression.',
    '#708 implements core/generator support, not a dedicated Lua optional pusher; selected-file absence checked directly.')
add('gap-native-provider', '5.4/5.5/5.6', 'unknown_native_coverage',
    'PS5 chooses NativeProvidedVirtualVC rather than upstream UnrealVirtual504/505/506; native offsets and dispatch require independent proof.',
    [ev(p('unreal/src/VersionedContainer/Container.cpp'), 'else if (Version::IsEqual(5, 4))', 'src/VersionedContainer/Container.cpp'),
     ev(p('unreal/src/VersionedContainer/Container.cpp'), 'else if (Version::IsEqual(5, 6))', 'src/VersionedContainer/Container.cpp')],
    [CORE+'src/VersionedContainer/Container.cpp'],
    'This is the intentional port route. Merely retaining upstream generated tables does not establish native compatibility.',
    'Selected __PROSPERO__ branches compared with retained canonical source.')

verification = []
for r in units.values():
    actual = digest(r['source'])
    verification.append(dict(unit=r['name'], path=r['source'], expected=r['source_sha256'], actual=actual,
                             match=actual == r['source_sha256'], object=r['object'], object_sha256=r['object_sha256']))
write('selected-tu-hash-verification.json', verification)
assert all(r['match'] for r in verification)
assert all(e['selected_pin_matches'] for r in rows for e in r['evidence'])

# Exact negative evidence scans source TUs without reaching outside the selected graph.
negative = []
for name, terms in [('utf8-ansi-registration', ['&LuaType::push_utf8strproperty', '&LuaType::push_ansistrproperty']),
                    ('optional-pusher', ['push_optionalproperty'])]:
    matches = []
    for u in units.values():
        for i, line in enumerate(Path(u['source']).read_text(errors='replace').splitlines(), 1):
            if any(term in line for term in terms):
                matches.append(dict(unit=u['name'], path=u['source'], line=i, text=line.strip()))
    negative.append(dict(name=name, terms=terms, selected_tus_scanned=len(units), matches=matches))
assert not negative[0]['matches']
assert not negative[1]['matches']
canonical_program = PUBLIC / 'UE4SS/src/UE4SSProgram.cpp'
negative.append(dict(name='canonical-base-program', path=str(canonical_program), sha256=digest(canonical_program),
                     utf8_registration_present='&LuaType::push_utf8strproperty' in canonical_program.read_text(),
                     ansi_registration_present='&LuaType::push_ansistrproperty' in canonical_program.read_text()))
write('negative-evidence.json', negative)
write('version-sensitive-changes.json', rows)

columns = ['id', 'engine_version', 'status', 'change', 'origin_evidence', 'limits', 'evidence', 'upstream_links']
with (OUT / 'version-sensitive-changes.tsv').open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=columns, delimiter='\t')
    writer.writeheader()
    for r in rows:
        entry = {k: r[k] for k in columns}
        entry['evidence'] = '; '.join(e['path']+':'+str(e['line']) for e in r['evidence'])
        entry['upstream_links'] = '; '.join(r['upstream_links'])
        writer.writerow(entry)

metadata = dict(scope='Local source/provenance audit plus read-only official public GitHub research. No console contact, builds, auth changes or private-repository network requests.',
                manifest=str(MANIFEST), manifest_sha256=digest(MANIFEST), composition=str(COMPOSITION), composition_sha256=digest(COMPOSITION),
                ue4ss_base=BASE, uepseudo_base=private['commit'], uepseudo_tree=private['tree'],
                archive_sha256=private['archive_sha256'], selected_tus_verified=len(verification), catalog_rows=len(rows),
                banner_flags=sorted({a for u in units.values() for a in u.get('argv', []) if a.startswith(('-DUE4SS_LIB_VERSION_', '-DUE4SS_LIB_BUILD_GITSHA='))}),
                private_history_limit='Saved source archive and public gitlink, not private commit history; exact introduction commits and exhaustive engine compatibility remain unknown.',
                console_contact=False, new_build=False, runtime_modified=False)
write('provenance.json', metadata)

def link(e):
    relative = str(Path(e['path']).relative_to(ROOT))
    return '['+relative+':'+str(e['line'])+']('+e['path']+':'+str(e['line'])+')'

report = [
    '# l21 upstream engine-version audit',
    '',
    'The banner does not identify the source age. The selected compile receipts explicitly define version **3.0.1** while setting the Git SHA to **'+BASE+'**. Official GitHub compares that September 29, 2026 revision as **1,152 commits ahead** of stable v3.0.1 (`d935b5b23bac03b65c14ae38382b02007204cc2e`, released February 14, 2024). [Pinned comparison](https://github.com/UE4SS-RE/RE-UE4SS/compare/v3.0.1...'+BASE+').',
    '',
    'The local public superproject gitlink and previously retained, authenticated UEPseudo source receipt agree on **'+private['commit']+'**. This audit used that existing private snapshot; it made no private repository requests or authentication changes. Public PR diffs and metadata are saved beside this report. The initial engine-support PRs were [#503, UE 5.4](https://github.com/UE4SS-RE/RE-UE4SS/pull/503), [#708, UE 5.5/optionals](https://github.com/UE4SS-RE/RE-UE4SS/pull/708), and [#977, UE 5.6](https://github.com/UE4SS-RE/RE-UE4SS/pull/977). The private core introduction history is unavailable locally, so current code presence is distinguished from a proven exact historical introduction commit.',
    '',
    '**Concrete gap:** later [#1015](https://github.com/UE4SS-RE/RE-UE4SS/pull/1015/files) added UTF-8 and ANSI property registration as well as implementations. The selected build retains their implementations and constructors but omits both entries from the property-pusher map. The canonical retained public source also omits them. All 374 selected translation units were searched for those registrations with no match. That prevents calling these functions through the ordinary registered reflected-property path; it is not proof that the original menu exercises such a property.',
    '',
    'All **374 selected translation-unit source hashes match** the build receipt. Every catalog line citation also matches a selected-source manifest pin. Source presence does not prove PS5 execution or the correct native ABI. No build or console action was performed.',
    '',
    '## Actual code/API adaptations',
    '',
]
for r in rows:
    report += ['### '+r['id']+' — '+r['engine_version']+' — '+r['status'], '', r['change'], '',
               'Selected evidence: '+ '; '.join(link(e) for e in r['evidence'])+'.', '',
               'Origin: '+r['origin_evidence']+' '+' '.join('[source '+str(i+1)+']('+u+')' for i,u in enumerate(r['upstream_links']))+'.', '',
               'Limit: '+r['limits'], '']
report += [
    '## Actionable interpretation', '',
    '1. Do not replace this port merely because it prints 3.0.1; substantive later engine adaptations are selected.',
    '2. Review the missing UTF-8/ANSI pusher registrations against the actual mod property types before proposing a change. Their absence is concrete, but no current failure is attributed to it.',
    '3. Keep construction callback support and remote-object-handle mode explicit: upstream padding does not implement the new callback, and the default mode is zero.',
    '4. Continue the native layout/ABI audit. `__PROSPERO__` uses NativeProvidedVirtualVC, so Windows-generated layout tables cannot settle it.',
    '5. Treat missing Lua optional-value routing and untracked shared Lua helper assets as separate API/asset coverage limits.',
    '',
    'The catalog is a focused inventory of checked version-sensitive algorithms and APIs, not an exhaustive claim that every engine source change or game customization is covered. Historical private PRs #90/#148 are referenced by public PRs; this report does not claim their exact diffs were read.',
    '',
    'Machine-readable evidence: [catalog](version-sensitive-changes.json), [TSV](version-sensitive-changes.tsv), [provenance](provenance.json), [selected TU hashes](selected-tu-hash-verification.json), [negative registration evidence](negative-evidence.json).',
]
(OUT / 'REPORT.md').write_text('\n'.join(report)+'\n')
print(json.dumps(dict(catalog=len(rows), selected_tus=len(verification), all_hashes_match=True,
                      registration_matches=len(negative[0]['matches']), report=str(OUT/'REPORT.md'))))
