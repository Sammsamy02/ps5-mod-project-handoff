#!/usr/bin/env python3
"""Read-only source correlation; writes only this new report directory.

This is not an engine execution, marshalling simulation, or menu-success test.
"""
import hashlib
import json
from pathlib import Path

BASE = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = Path(__file__).resolve().parent
FINAL = BASE / 'outputs/2026-10-08-r5-thread-final-integration-v2'

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def pin(p, expected=None):
    p = p.resolve()
    sha = digest(p)
    if expected is not None:
        assert sha == expected, (str(p), sha, expected)
    return dict(path=str(p), sha256=sha, size=p.stat().st_size)

def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n')

artifacts = [
    pin(FINAL / 'SOURCE-SELECTION.json', '5fb5f8d55ed8b7f42ce4734ca6d4850ca3f68b658c363ec984f16f5d3b2f1b43'),
    pin(FINAL / 'pass1/COMPOSITION-RECEIPT.json', '82bad5f6016cf8a1407a1385d231b852683f553bfba9de0c37fd0127da6b720c'),
    pin(FINAL / 'pass2/COMPOSITION-RECEIPT.json', '3a1eb72f50079adf77957bf34e17764729569bfbf72d3b1ecfee2578b5c0a657'),
    pin(FINAL / 'pass1/ACTUAL-DEPENDENCIES.json', 'e0d4403e2890768d9cd5a73b05c291b49ee0003f5064e7262eae631bcb5bd760'),
    pin(BASE / 'outputs/2026-10-08-original-menu-graph/REPORT.md'),
]
p1 = json.loads(Path(artifacts[1]['path']).read_text())
p2 = json.loads(Path(artifacts[2]['path']).read_text())
deps = json.loads(Path(artifacts[3]['path']).read_text())
assert len(p1['selected_units']) == len(p2['selected_units']) == 374
identity = lambda u: (u['name'], u['source'], u['source_sha256'], u['object_sha256'])
assert list(map(identity, p1['selected_units'])) == list(map(identity, p2['selected_units']))
units = {u['name']: u for u in p1['selected_units']}
names = [
    'program', 'lua-luauobject', 'lua-luaufunction', 'lua-luaftext', 'luamod',
    'unreal-uobject', 'unreal-uobjectglobals', 'unreal-ustruct', 'unreal-fproperty',
    'unreal-unrealinitializer', 'unreal-flat-ftext', 'unreal-flat-typechecker',
    'unreal-flat-ukismettextlibrary', 'unreal-flat-uscriptstruct',
    'unreal-src-property-ftextproperty', 'unreal-ufunction',
]
sources = {}
for name in names:
    u = units[name]
    sources[name] = dict(pin(Path(u['source']), u['source_sha256']), authority='actual_selected_TU')
for name, suffix in [('function-header', '/LuaType/LuaUFunction.hpp'),
                     ('object-header', '/LuaType/LuaUObject.hpp'),
                     ('text-header', '/Unreal/FText.hpp'),
                     ('bp-macros', '/Unreal/BPMacros.hpp')]:
    matches = [x for x in deps['files'] if x['path'].endswith(suffix)]
    assert len(matches) == 1, (suffix, matches)
    x = matches[0]
    sources[name] = dict(pin(Path(x['path']), x['sha256']), authority='actual_emitted_dependency')
script = BASE / 'outputs/2026-10-08-menu-path-audit/source-cache/main.lua'
sources['original-mod'] = dict(pin(script, 'f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658'),
                              authority='untouched_original_mod')

specs = [
    ('M1', 'original-mod', 41, 56, ['StaticConstructObject']),
    ('M2', 'original-mod', 426, 485, ['widget:SetText(FText', 'parent:AddChild', 'pcall']),
    ('M3', 'original-mod', 488, 644, ['widgetLib:Create(pc, userWidgetClass, pc)', 'ui.root.WidgetTree', 'ui.tree.RootWidget', 'AddToViewport(9999)', 'SetVisibility(1)']),
    ('M4', 'original-mod', 647, 696, ['SetVisibility(0)', 'menuOpen = true', 'ExecuteInGameThread(toggleMenu)']),
    ('R1', 'program', 1783, 1811, ['ObjectProperty', 'ClassProperty', 'StructProperty', 'IntProperty', 'ByteProperty', 'EnumProperty', 'FloatProperty', 'DoubleProperty', 'TextProperty']),
    ('C1', 'luamod', 2231, 2339, ['FStaticConstructObjectParameters params{param_class, param_outer}', 'StaticConstructObject(params)']),
    ('C2', 'unreal-uobjectglobals', 19, 135, ['StaticConstructObject', '5, 6']),
    ('C3', 'lua-luaufunction', 60, 76, ['call_ufunction_from_lua']),
    ('C4', 'lua-luauobject', 154, 327, ['GetNumParms', 'GetReturnValueOffset', 'CPF_Parm', 'ProcessEvent']),
    ('C5', 'lua-luauobject', 330, 405, ['return_value_property', 'GetNonTrivialLocal']),
    ('C6', 'unreal-uobject', 448, 451, ['ProcessEvent', 'IMPLEMENT_UNREAL_VIRTUAL_WRAPPER']),
    ('P1', 'lua-luauobject', 440, 576, ['auto_construct_object', 'push_objectproperty', 'push_classproperty']),
    ('P2', 'lua-luauobject', 634, 717, ['convert_lua_table_to_struct', 'adjusted_index = table_index - 1', 'leave default value', 'Skipping field']),
    ('P3', 'lua-luauobject', 816, 898, ['CopyCompleteValue', 'convert_lua_table_to_struct']),
    ('P4', 'lua-luauobject', 1513, 1699, ['push_floatproperty', 'push_doubleproperty', 'push_enumproperty', 'GetElementSize']),
    ('P5', 'lua-luauobject', 1793, 1821, ['text->SetString', 'ToFString']),
    ('P6', 'lua-luauobject', 2303, 2370, ['GetFunctionByNameInChain', 'property_value_pushers', 'GetOffset_Internal']),
    ('B1', 'function-header', 14, 19, ['uint8_t data[0x200]{}']),
    ('T1', 'lua-luaftext', 52, 92, ['FText', 'is_string']),
    ('T2', 'unreal-flat-typechecker', 528, 541, ['FText::StaticSize_Private', 'ValueOpsProperty']),
    ('T3', 'unreal-flat-typechecker', 626, 671, ['/Script/Engine.GameModeBase', '/Script/UMG.Widget']),
    ('T4', 'unreal-flat-typechecker', 790, 798, ['FText::StaticSize() == 0']),
    ('T5', 'unreal-flat-ftext', 12, 165, ['StaticSize_Private{-1}', 'CanUseValueOps', 'CopyCompleteValue', 'DestroyValue', 'StealFromBuffer']),
    ('T6', 'text-header', 16, 85, ['MaxStorageSize = 0x30', 'alignas(8)', 'StaticSize_Private']),
    ('T7', 'unreal-flat-ukismettextlibrary', 7, 37, ['Conv_TextToString', 'Conv_StringToText', 'StealFromBuffer']),
    ('T8', 'bp-macros', 16, 127, ['MakePayloadParameterFrame', 'Function->GetParmsSize()', 'StaticSelf->ProcessEvent']),
    ('V1', 'unreal-fproperty', 245, 267, ['CopyValuesInternal', 'GetArrayDim']),
    ('V2', 'unreal-fproperty', 342, 382, ['DestroyValue', 'InitializeValue']),
]
windows = []
text = []
for key, source, start, end, tokens in specs:
    p = Path(sources[source]['path'])
    lines = p.read_text().splitlines(keepends=True)
    assert 0 < start <= end <= len(lines), (key, len(lines))
    body = ''.join(lines[start - 1:end])
    for token in tokens:
        assert token in body, (key, token)
    windows.append(dict(id=key, source=source, path=str(p), start=start, end=end,
                        source_sha256=sources[source]['sha256'],
                        window_sha256=hashlib.sha256(body.encode()).hexdigest()))
    text.append(f'### {key}\n\n[{p.name}:{start}]({p}:{start})\n\n```text\n' +
                ''.join(f'{i}: {line}' for i, line in enumerate(lines[start-1:end], start)) + '```\n')

call = Path(sources['lua-luauobject']['path']).read_text().splitlines()
call_body = '\n'.join(call[153:405])
for absent in ['GetParmsSize', 'InitializeValue(', 'DestroyValue(']:
    assert absent not in call_body, absent
registration = Path(sources['program']['path']).read_text()
handlers = dict(ObjectProperty='push_objectproperty', ClassProperty='push_classproperty',
                IntProperty='push_intproperty', ByteProperty='push_byteproperty',
                EnumProperty='push_enumproperty', BoolProperty='push_boolproperty',
                StructProperty='push_structproperty', FloatProperty='push_floatproperty',
                DoubleProperty='push_doubleproperty', TextProperty='push_textproperty')
for typename, fn in handlers.items():
    assert any(f'"{typename}"' in line and fn in line and 'emplace' in line for line in registration.splitlines())

gaps = [
 dict(id='G1', path='Create and StaticConstructObject root/child construction', required=True,
      established='Actual SCO global accepts the script UClass, UObject outer, FName triple; typed version-selected native facade exists. Create uses generic reflected UFunction marshalling and object return wrapping.',
      unresolved='Target UMG class/function availability, native creation permissions/flags, initialization and returned object validity; no target UMG schema measured in this slice.',
      observation='Distinguish Create lookup/count/type error, invalid Create result, invalid SCO result, and a valid root lacking native initialization/WidgetTree state.', evidence=['M1','M3','C1','C2','C4','P1']),
 dict(id='G2', path='WidgetTree/RootWidget assignments; AddChild and SetContent', required=True,
      established='Direct properties and method arguments dispatch through reflected field-type handlers. Object/Class handlers write pointer values and returned objects receive typed wrappers.',
      unresolved='Actual target property classes, offsets, storage encoding and native widget/slot ownership. The source trace does not prove an incompatible TObjectPtr encoding.',
      observation='Resolved WidgetTree/RootWidget field schemas; each AddChild/SetContent UFunction NumParms/ParmsSize/return field, valid slot result, native parent/content state.', evidence=['M3','C4','C5','P1','P6']),
 dict(id='G3', path='AddToViewport(9999), hidden(1), visible(0)', required=True,
      established='Int, Byte and Enum handlers are registered; Enum selects native underlying ElementSize. AddToViewport and hidden(1) are mandatory inside build pcall; visible(0) is optional pcall afterward.',
      unresolved='Actual function/enum schemas and effective viewport insertion/visibility. Menu created and menuOpen do not establish a visible widget.',
      observation='Successful required calls plus actual viewport/visibility result; separately retain any visible(0) error rather than interpreting Menu created as presentation success.', evidence=['M3','M4','R1','C4','P4']),
 dict(id='G4', path='TextBlock SetText(FText), dynamic TextProperty and Kismet conversion', required=False,
      established='Text handler and Lua FText constructor are present. Real native Kismet conversions and engine TextProperty copy/destroy operations are used; FText size/value operations are discovered dynamically, notably through GameModeBase.',
      unresolved='Discovered TextProperty size/ArrayDim/value-operation identity, Kismet function/CDO existence and InString/InText/return schema. No target FText extent is inferred from version comments.',
      observation='TypeChecker detected size/value-property identity, Kismet CDO and reflected parameter bounds; capture optional SetText failure and actual label contents.', evidence=['M2','R1','P5','T1','T2','T3','T4','T5','T6','T7','T8']),
 dict(id='G5', path='FMargin, Vector2D, Anchors, colors, font and child-slot setters', required=False,
      established='Struct handler accepts tables and recursively writes registered reflected fields; float/double handlers exist. Missing table fields preserve initial/default bytes and unsupported fields are skipped.',
      unresolved='Exact target struct names/fields and defaults. The nested SpecifiedColor payload passed to SetShadowColorAndOpacity is conditional on its real parameter schema, which is not proven here.',
      observation='Native struct schemas and populated bytes, especially Anchors and colors; optional failures can leave default position/size or illegible labels without stopping Menu created.', evidence=['M2','M3','P2','P3','P4','R1']),
 dict(id='G6', path='Generic reflected parameter frame extent/alignment', required=True,
      established='DynamicUnrealFunctionData contains zeroed uint8_t[512]; caller indexes reflected offsets without GetParmsSize or per-field extent checks. Declaration gives no explicit engine-parameter alignment contract.',
      unresolved='Whether any menu UFunction actually exceeds 512, has a field/return extent outside it, or requires greater alignment. No actual overrun/alignment fault has been established.',
      observation='For each menu UFunction: ParmsSize, every CPF_Parm/return offset+size and required alignment, compared to the actual parameter frame address/extent.', evidence=['B1','C4','C5']),
 dict(id='G7', path='Nontrivial parameter initialization/ownership', required=False,
      established='Generic Lua call frame has no property InitializeValue/DestroyValue sequence; TextProperty writes through FText::SetString into it. FProperty exposes native copy/destroy operations.',
      unresolved='Exact engine caller-buffer ownership/consumption and lifetime contract for these calls. Absence of cleanup alone is not labeled a proven native leak or first-menu failure.',
      observation='Native borrowing/consumption contract and per-slot construction/destruction needs; repeated SetText/refcount behavior can distinguish ownership effects after a single successful menu.', evidence=['C4','C5','P5','T5','V1','V2']),
 dict(id='G8', path='Reflection/property lookup and handler readiness', required=True,
      established='Function/context null, argument-count mismatch and unregistered argument types raise errors. Source registers all handler families used by this graph, but registration uses FNAME_Find comparison identities discovered at startup.',
      unresolved='Actual target names/type discovery success and schemas. Static engine-layout coverage does not prove every UMG method or dynamic TextProperty is available.',
      observation='The first exact UI creation failed error, resolved UFunction/class/property identity and type-handler map lookup; do not replace a missing native fact with a host-model value.', evidence=['M3','R1','C4','P6','T2','T3','T4']),
]
save('SOURCE-PINS.json', dict(scope='bounded selected menu marshalling sources', artifacts=artifacts, sources=sources,
                            selected_unit_count=374, selected_source_object_pairs_match_both_passes=True,
                            source_body_hashes_checked=len(names), header_dependency_pins_checked=4,
                            full_dependency_audit_repeated=False, native_execution=False, console_contact=False))
save('WINDOWS.json', windows)
(OUT / 'WINDOWS.md').write_text('\n'.join(text))
save('GAPS.json', dict(conclusion='No deterministic first-menu marshalling incompatibility proven by this bounded selected-source trace.',
                       source_fix_written=False, automatic_integration=False, native_success_claim=False,
                       registered_required_handler_families=handlers, gaps=gaps))

def ref(ids):
    return ', '.join(f'[{i}](WINDOWS.md#{i.lower()})' for i in ids)

lines = [
    '# Original menu: selected-source marshalling trace', '',
    'No deterministic missing handler or incompatible first-menu marshalling was proved. The required handler families are registered in the actual selected Program, and the required calls reach real reflection/ProcessEvent or StaticConstructObject code. No runtime source fix was written: the remaining distinctions require native schemas/results, not invented host values.', '',
    'This is a local source audit of the unchanged script against the final 374-unit selection. It does not claim native execution, visible-menu success, or package/runtime admission. The prior original-menu model establishes mandatory/optional Lua control flow only; its mock objects and tagged FText values are not ABI evidence.', '',
    f'Final source selection SHA256 `{artifacts[0]["sha256"]}`. All 374 ordered selected source/object identity records agree across the two composition receipts; the 16 relevant selected C++ bodies, four actual dependency headers and original script hashes were checked. No broad dependency/object audit was repeated.', '',
    'Call chain: original Lua bound method → Lua UFunction __call → reflected CPF_Parm pushers → UObject::ProcessEvent. StaticConstructObject instead uses the actual global Class/Outer/FName parser and version-selected native constructor facade. Direct WidgetTree/RootWidget assignments use reflected property handlers. Text construction/conversion uses real Kismet ProcessEvent calls and dynamically discovered TextProperty operations.', '',
    '| Path | Source-established behavior | Still unknown; distinguishing native observation |',
    '|---|---|---|',
]
for g in gaps:
    lines.append(f'| {g["id"]}: {g["path"]} ({"mandatory" if g["required"] else "optional/conditional"}) | {g["established"]} {ref(g["evidence"])} | {g["unresolved"]} **Observe:** {g["observation"]} |')
lines += ['',
    'The generic 512-byte frame and absent init/destroy sequence are certain source omissions. They are not promoted to an observed menu overrun, alignment fault, or leak. FText ownership analysis additionally depends on the native caller-buffer contract. The private Kismet helper uses reflected ParmsSize allocation; it should not be conflated with the generic fixed frame.', '',
    'The script logs “Menu created” after AddToViewport and SetVisibility(1), but it suppresses text/style errors and later suppresses SetVisibility(0) errors. Therefore that log alone does not prove readable text, valid geometry, or visible presentation. Exact original behavior is preserved.', '',
    'The next discriminating evidence is the first mandatory call result/error plus target reflected schemas for Create/AddChild/SetContent/AddToViewport/SetVisibility and the optional TextBlock/slot calls. Such evidence would support a specific repair. This report adds no new probe, console action, fallback value, or preflight refusal.', '',
    'Reproduce only this source correlation:', '',
    f'```sh\npython3 {OUT / "audit.py"}\n```', '',
    'The validator checks frozen identities, selected-source agreement, exact code-window tokens and the three absent generic-frame operations. It does not invoke the game, compile a fake marshaller, or treat model output as a native test.', '',
]
(OUT / 'REPORT.md').write_text('\n'.join(lines))
receipt = dict(status='PASS_bounded_source_correlation', native_execution=False, console_contact=False,
               source_fix_written=False, automatic_integration=False, runtime_success_claim=False,
               checks=dict(selection_and_composition_hashes=True, selected_374_pairs_match=True,
                           selected_source_bodies=len(names), emitted_header_pins=4, original_script=True,
                           exact_windows=len(windows), required_registered_handler_families=len(handlers),
                           generic_frame_absent_operation_assertions=3),
               artifacts=[pin(OUT / name) for name in ['audit.py','SOURCE-PINS.json','WINDOWS.json','WINDOWS.md','GAPS.json','REPORT.md']])
save('FINAL-RECEIPT.json', receipt)
print(json.dumps(dict(status=receipt['status'], source_bodies=len(names), headers=4,
                      windows=len(windows), gaps=len(gaps), fix=False,
                      receipt_sha256=digest(OUT / 'FINAL-RECEIPT.json')), sort_keys=True))
