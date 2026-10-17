"""Run actual binding/parser for all required keys and ten public-width boundaries.
Host replaces engine-map storage; genuine public signatures are separately target compiled.
"""
from pathlib import Path
import json, subprocess, hashlib, sys

root = Path(__file__).resolve().parents[1]
w = root.parents[1]
red = len(sys.argv) > 1 and sys.argv[1] == 'red'
out = root / 'tests/required-binding-host'
out.mkdir(exist_ok=True)
parser = w / '.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1'
widths = w / '.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1'
schema = w / '.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc'
profile = json.loads((root / 'profiles/townfall-build-facts.json').read_text())
census_path = w / 'outputs/2026-10-07-l19-width-census/CENSUS.json'
census = json.loads(census_path.read_text())
legacy_values = {row['name']: row['active_width'] for row in census['rows']}
legacy_values.update({'UFunction::Func': 0, 'UStruct::MinAlignment': 4, 'UClass::ClassCastFlags': 0, 'FFieldClass::CastFlags': 0})
expected_widths = dict(legacy_values)
expected_widths.update({'UFunction::Func': 8, 'UStruct::MinAlignment': 2, 'UClass::ClassCastFlags': 8,
                       'FFieldClass::CastFlags': 8, 'UStruct::SuperStruct': 8, 'FField::Owner': 16,
                       'FField::Next': 8, 'FField::NamePrivate': 8, 'FProperty::PropertyFlags': 8, 'UFunction::FunctionFlags': 4})
assert len(expected_widths) == 54 and all(expected_widths.values())
names = [row['name'].split('::') for row in profile['required_members']] + [['UStruct', 'Script']]
selected = []
for cls, key in names:
    rows = [line for line in schema.read_text().splitlines() if line.strip().startswith('{"' + cls + '", "' + key + '",')]
    assert len(rows) == 1, (cls, key)
    selected += rows
classes = sorted({cls for cls, key in names})
maps = 'inline static std::unordered_map<std::string,int32_t> MemberOffsets; inline static std::unordered_map<std::string,BitfieldInfo> BitfieldInfos;'
public = {'UFunction': 'std::function<void(UObject*,FFrame&,void* const)>*& GetFunc(); uint32_t& GetFunctionFlags();',
          'UStruct': 'int16_t& GetMinAlignment(); UStruct*& GetSuperStruct();',
          'UClass': 'uint64_t& GetClassCastFlags();', 'FFieldClass': 'uint64_t GetCastFlags();',
          'FField': 'FFieldVariant GetOwnerVariant(); FProperty* GetNextFieldAsProperty(); FName GetFName();',
          'FProperty': 'EPropertyFlags& GetPropertyFlags(); inline static std::unordered_map<std::string,uint32_t> VTableLayoutMap;'}
engine = ['#pragma once', '#include <cstdint>', '#include <unordered_map>', '#include <string>', '#include <functional>',
          '#define STR(s) s', 'namespace RC::File {using StringType=std::string;}', 'namespace RC::Unreal {',
          'struct BitfieldInfo {uint8_t position,length,storage;};', 'struct UObject; struct FFrame; struct FProperty;',
          'struct FName {uint64_t value;}; struct FFieldVariant {void* container; bool legacyIsObject;};',
          'enum class EPropertyFlags:uint64_t {};']
engine += ['struct ' + cls + ' {' + public.get(cls, '') + maps + '};' for cls in classes]
engine += ['static_assert(sizeof(FFieldVariant)==16);', '}']
(out / 'NativeLayoutEngineIncludes.hpp').write_text('\n'.join(engine) + '\n')
targets = ','.join('{"' + cls + '",&Unreal::' + cls + '::MemberOffsets,&Unreal::' + cls + '::BitfieldInfos}' for cls in classes)
virtual_row = next(line for line in schema.read_text().splitlines() if line.strip() == '{"FProperty", &Unreal::FProperty::VTableLayoutMap},')
(out / 'NativeLayoutSchema.inc').write_text('static const std::vector<MemberSchema> MemberSchemaEntries{\n' + '\n'.join(selected) + '\n};\nstatic auto MemberTargets(){return std::array<MemberTarget,' + str(len(classes)) + '>{{' + targets + '}};}\nstatic auto VirtualTargets(){return std::array<VirtualTarget,1>{{\n' + virtual_row + '\n}};}\n')

cases = [('func', 'UFunction', 'Func', 0xE0, 0xD8, 8), ('alignment', 'UStruct', 'MinAlignment', 0xB0, 0xAE, 2),
         ('class_cast', 'UClass', 'ClassCastFlags', 0x200, 0xD8, 8), ('field_cast', 'FFieldClass', 'CastFlags', 0x40, 0x10, 8),
         ('super', 'UStruct', 'SuperStruct', 0xB0, 0x40, 8), ('owner', 'FField', 'Owner', 0x30, 0x10, 16),
         ('next', 'FField', 'Next', 0x30, 0x18, 8), ('name', 'FField', 'NamePrivate', 0x30, 0x20, 8),
         ('property_flags', 'FProperty', 'PropertyFlags', 0x70, 0x38, 8), ('function_flags', 'UFunction', 'FunctionFlags', 0xE0, 0xB0, 4)]
all_lines = []
fixture_notes = []
for cls in classes:
    pins = [row for row in profile['required_members'] if row['name'].startswith(cls + '::')]
    total = next(row['offset'] for row in pins if row['name'].endswith('::UEP_TotalSize'))
    if total is None:
        total = {'FField': 0x30, 'FFieldClass': 0x40}[cls]
        fixture_notes.append(cls + ' total owned host fixture ' + hex(total) + '; not a native sizeof claim unless independently accepted profile supplies it')
    all_lines += ['[' + cls + ']', 'UEP_TotalSize=' + hex(total)]
    for pin in pins:
        key = pin['name'].split('::')[1]
        if key == 'UEP_TotalSize':
            continue
        offset = pin['offset']
        if offset is None:
            assert pin['name'] == 'UClass::Interfaces'
            offset = 0x1D8
            fixture_notes.append('Interfaces1D8/16 from separately accepted interface evidence; profile regeneration may follow this initial RED')
        all_lines.append(key + '=' + hex(offset))
all_text = '\n'.join(all_lines) + '\n'
cpp = ['#include "NativeLayoutBinding.hpp"', '#include <NativeLayoutEngineIncludes.hpp>', '#include <NativeLayoutWidths.hpp>',
       '#include "NativeLayoutActiveWidths.hpp"', '#include <iostream>', '#include <map>', 'namespace RC::Platform::NativeLayouts {',
       'auto GetAccessorWidth(std::string_view cls,std::string_view key) noexcept -> std::optional<uint32_t> {']
for name, value in sorted(legacy_values.items()):
    cls, key = name.split('::')
    cpp.append('if(cls==' + json.dumps(cls) + '&&key==' + json.dumps(key) + ')return ' + (str(value) if value else 'std::nullopt') + ';')
cpp += ['if(cls=="FallbackControl"&&key=="Known")return 4;', 'return std::nullopt;}', '}', 'using namespace RC::Platform::NativeLayouts;',
        'int main(int argc,char**argv){if(argc!=2)return 2;std::string mode=argv[1];',
        'const std::string id="[NativeLayout]\\nFormat=1\\nTarget=prospero-x86_64-sysv\\nPointerSize=8\\nEngineMajor=5\\nEngineMinor=6\\nNonShipping=0\\nStats=0\\nEngineRevision=owned-host-control\\nBuildId=owned-host-control\\n";',
        'std::string m=id,v=id,want;bool expected=true;std::map<std::string,uint32_t> wantWidths;']
for i, (mode, cls, key, total, good, width) in enumerate(cases):
    for suffix, offset in [('good', good), ('boundary', total - width + 1)]:
        text = '[' + cls + ']\nUEP_TotalSize=' + hex(total) + '\n' + key + '=' + hex(offset) + '\n'
        cpp.append(('if' if not i and suffix == 'good' else 'else if') + '(mode==' + json.dumps(mode + '_' + suffix) + '){m+=' + json.dumps(text) + ';')
        if suffix == 'boundary':
            cpp.append('expected=false;want=' + json.dumps('Native member is outside declared class size: ' + cls) + ';')
        else:
            cpp.append('wantWidths.emplace(' + json.dumps(cls + '::' + key) + ',' + str(width) + ');')
        cpp.append('}')
cpp += ['else if(mode=="unknown_key"){m+="[UStruct]\\nUEP_TotalSize=0xB0\\nNoAccessor=0x60\\n";expected=false;want="Unknown native member: UStruct::NoAccessor";}',
        'else if(mode=="unknown_width"){m+="[UStruct]\\nUEP_TotalSize=0xB0\\nScript=0x60\\n";expected=false;want="Native accessor width unavailable: UStruct::Script";}',
        'else if(mode=="virtual_metadata")v+="[FProperty]\\nGetMinAlignment=0x170\\n";',
        'else if(mode=="all_required"){m+=' + json.dumps(all_text) + ';']
cpp += ['wantWidths.emplace(' + json.dumps(name) + ',' + str(width) + ');' for name, width in sorted(expected_widths.items())]
cpp += ['}', 'else if(mode=="adapter_policy"){struct UnknownGetter{};']
for function in ['FunctionWidth', 'MinAlignmentWidth', 'ClassCastFlagsWidth', 'FieldCastFlagsWidth', 'SuperStructWidth',
                 'FieldOwnerWidth', 'FieldNextWidth', 'FieldNameWidth', 'PropertyFlagsWidth', 'FunctionFlagsWidth']:
    if not red:
        cpp.append('static_assert(!ActiveWidthDetail::' + function + '<UnknownGetter>());')
cpp += ['if(GetBindingAccessorWidth("FallbackControl","Known")!=GetAccessorWidth("FallbackControl","Known"))return 1;',
        'if(GetBindingAccessorWidth("Unknown","None"))return 1;', 'std::cout<<"PASS missing public getters and unchanged fallback policy\\n";return 0;}', 'else return 2;',
        'bool staged=true;std::string detail;try{StageNativeLayoutTexts(m,v);}catch(const std::exception&e){staged=false;detail=e.what();}',
        'if(staged!=expected){std::cerr<<"FAIL "<<mode<<" expected_stage="<<expected<<" observed_stage="<<staged<<" detail="<<detail<<"\\n";return 1;}',
        'if(!expected&&detail!=want){std::cerr<<"FAIL wrong refusal "<<detail<<" expected "<<want<<"\\n";return 1;}',
        'auto facts=GetStagedNativeLayoutFacts();if(bool(facts)!=staged)return 1;',
        'if(staged){for(auto&[name,width]:wantWidths){bool found=false;for(auto&f:facts->members)if(f.name==name){if(f.width!=width)return 1;found=true;}if(!found)return 1;}',
        'if(mode=="all_required"&&facts->members.size()!=69)return 1;',
        'if(mode=="virtual_metadata"){if(facts->vtables.size()!=1||facts->vtables[0].name!="FProperty::GetMinAlignment"||facts->vtables[0].offset!=0x170||facts->vtables[0].width!=sizeof(void*))return 1;}}']
cpp += ['if(!RC::Unreal::' + cls + '::MemberOffsets.empty()||!RC::Unreal::' + cls + '::BitfieldInfos.empty())return 1;' for cls in classes]
cpp += ['if(!RC::Unreal::FProperty::VTableLayoutMap.empty())return 1;',
        'std::cout<<"PASS "<<mode<<" stage="<<staged<<" checked_widths="<<wantWidths.size()<<" engine_maps_empty=1 "<<detail<<"\\n";', '}']
source = out / ('controls-red.cpp' if red else 'controls.cpp')
source.write_text('\n'.join(cpp) + '\n')
binding = w / 'outputs/2026-10-07-l19-local-profile-r2/units/NativeLayoutBinding.cpp' if red else root / 'units/NativeLayoutBinding.cpp'
exe = out / ('controls-red-san' if red else 'controls-san')
cmd = ['clang++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-O1', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
       '-fno-omit-frame-pointer', '-I', str(out), '-I', str(root / 'units'), '-I', str(parser / 'include'), '-I', str(widths / 'include'),
       str(binding), str(parser / 'src/NativeLayoutFiles.cpp'), str(source), '-o', str(exe)]
r = subprocess.run(cmd, capture_output=True, text=True)
rows = [{'argv': cmd, 'exit': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr}]
if not r.returncode:
    modes = ['super_good'] if red else [mode + '_' + suffix for mode, *unused in cases for suffix in ['good', 'boundary']] + ['unknown_key', 'unknown_width', 'virtual_metadata', 'all_required', 'adapter_policy']
    for mode in modes:
        argv = [str(exe), mode]
        r = subprocess.run(argv, capture_output=True, text=True)
        rows.append({'argv': argv, 'exit': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr})
        print(r.stdout or r.stderr, flush=True)
        if r.returncode:
            break
else:
    print(r.stderr, flush=True)
receipt = {'scope': 'actual full binding/parser; all69 required member declarations/all54 non-total widths tested; ten public-width boundaries; no engine calls/publication',
           'commands': rows, 'geometry_fixture_notes': fixture_notes,
           'host_model': 'engine static storage replaced; legacy44 remaining widths copied from authenticated actual target census; public10 method value types separately actual target compiled',
           'owner': 'native physical width8; whole public variant copy conservative bound16; no variant ABI equivalence claim',
           'sources': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__).resolve(), binding, schema, census_path, parser / 'src/NativeLayoutFiles.cpp', root / 'profiles/townfall-build-facts.json']}}
(root / 'reports' / ('REQUIRED-BINDING-RED.json' if red else 'REQUIRED-BINDING-GREEN.json')).write_text(json.dumps(receipt, indent=2) + '\n')
sys.exit(r.returncode)
