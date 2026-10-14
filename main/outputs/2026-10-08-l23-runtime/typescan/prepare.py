# Historical initial-overlay generator only; final candidates include later reviewed changes.
# Do not use to regenerate final CHANGES.json or overwrite reviewed candidates.
from pathlib import Path
import re,json,shutil
root=Path.cwd(); out=root/'outputs/2026-10-08-l23-runtime/typescan'; base=root/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal'
changes=[]
def save(src,rel,txt):
 dest=out/'candidate'/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(txt)
 orig=out/'original'/rel;orig.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,orig)
 changes.append(dict(original=str(src),copied_original=str(orig),candidate=str(dest)))
for typ,methods in [('FMapProperty',['KeyProp','ValueProp']),('FSetProperty',['ElementProp']),('FArrayProperty',['Inner'])]:
 for kind in ['Header','Src']:
  src=base/'generated_include'/f'MemberVariableLayout_{kind}Wrapper_{typ}.hpp';s=src.read_text()
  for method in methods:
   if kind=='Header':
    for const in ['', 'const ']:
     old=f'{const}FProperty*& Get{method}()'+(' const' if const else '')+';'
     s=s.replace(old,'#ifdef __PROSPERO__\n    '+old.replace('*&','*')+'\n#else\n    '+old+'\n#endif')
   else:
    for const in ['', 'const ']:
     pat=re.escape(f'{const}FProperty*& {typ}::Get{method}()'+(' const' if const else ''))+r'\n\{.*?\n\}'
     old=re.search(pat,s,re.S).group(0)
     new=f'''{const}FProperty* {typ}::Get{method}(){" const" if const else ""}
{{
    const auto found = MemberOffsets.find(STR("{method}"));
    if (found == MemberOffsets.end()) throw std::runtime_error{{"Tried getting member variable '{typ}::{method}' that doesn't exist in this engine version."}};
    return NativePropertyGuard::ReadPointer(this, found->second, "{typ}::{method}");
}}'''
     s=s.replace(old,'#ifdef __PROSPERO__\n'+new+'\n#else\n'+old+'\n#endif')
  save(src,Path('generated_include')/src.name,s)
 src=base/'src/Property'/f'{typ}.cpp';s=src.read_text();s='#ifdef __PROSPERO__\n#include "NativePropertyPointerGuard.hpp"\n#endif\n'+s
 save(src,Path('src/Property')/src.name,s)
src=base/'src/TypeChecker.cpp';s=src.read_text();s=s.replace('#include <algorithm>','#include <algorithm>\n#include "NativeTypeScanPolicy.hpp"\n#ifdef __PROSPERO__\n#include "NativePropertyPointerGuard.hpp"\n#include <atomic>\n#endif')
s=s.replace('       //StaticFindObject_InternalSlow', '        NativeTypeScan::MissingKeys missing_keys;\n       //StaticFindObject_InternalSlow')
s=s.replace('auto find_all_property_types = [](const StringType& obj_string)', 'auto find_all_property_types = [&](const StringType& obj_string)')
s=s.replace('return field->GetFFieldClassUnsafe();', 'return NativePropertyGuard::ReadClass(field);')
s=s.replace('                while (field)\n                {', '''                std::vector<FField*> visited;
                while (field)
                {
                    if (std::find(visited.begin(), visited.end(), field) != visited.end())
                        throw std::runtime_error("[TypeScan] cyclic field chain refused");
                    visited.push_back(field);
                    // Obtain next independently; a missing traversal key ends this
                    // object's chain, but the other object scans still proceed.
                    FField* next = nullptr;
                    missing_keys.attempt([&] { next = field->GetNextFFieldUnsafe(); });
                    missing_keys.attempt([&] {''')
s=s.replace('                        field = field->GetNextFFieldUnsafe();\n                        continue;', '                        return;')
s=s.replace('                    field = field->GetNextFFieldUnsafe();\n                }', '                    });\n                    field = next;\n                }')
# Independently isolate each nested probe, including both map slots.
for v in ['inner','element','key','value']:
 start=s.index(f'                                FProperty* {v}_prop =')
 brace=s.index('{',s.index(f'if ({v}_prop)',start));depth=1;end=brace+1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 block=s[start:end]
 s=s[:start]+'                                missing_keys.attempt([&] {\n'+block+'\n                                });'+s[end:]
# Isolate absent top-level child-key failures too; no broad exception absorption.
s=re.sub(r'^(\s*)find_all_property_types\((STR\([^\n]+)\);',r'\1missing_keys.attempt([&] { find_all_property_types(\2); });',s,flags=re.M)
# Emit and gate before later reflection consumers.
marker='        if (FText::StaticSize() == 0)'
core=['UObject','UField','UStruct','UClass','UEnum','UFunction','UScriptStruct','AActor','UWorld','UPackage','UCanvasPanel']
props=['FField','FProperty','FObjectPropertyBase','FObjectProperty','FObjectPtrProperty','FClassProperty','FStructProperty','FArrayProperty','FBoolProperty','FIntProperty','FByteProperty','FFloatProperty','FDoubleProperty','FNameProperty','FStrProperty','FTextProperty','FEnumProperty']
# Sentinel -1 denotes undiscovered; null also is not a type.
block='''        for (const auto& key : missing_keys.values)
            Output::send<LogLevel::Warning>(STR("[TypeScan] missing-layout-key={}\\n"), StringType(key.begin(),key.end()));
#ifdef __PROSPERO__
        // Core list is justified in REPORT.md against selected runtime and mod.
        bool core_ready = true;
        auto require_type = [&](const char* name, bool found) {
            if (!found) { core_ready = false;
                Output::send<LogLevel::Error>(STR("[TypeScan] missing-core-type={}\\n"), StringType(name,name+std::char_traits<char>::length(name)));
            }
        };
'''
for c in core:block+=f'        require_type("{c}", {c}::StaticClassStorage && {c}::StaticClassStorage != STATIC_CLASS_INVALID);\n'
for c in props:block+=f'        require_type("{c}", {c}::StaticClassStorage.IsValid() && {c}::StaticClassStorage != STATIC_CLASS_INVALID'+(f' && {c}::StaticValueSizeStorage > 0' if c not in ['FField','FProperty','FObjectPropertyBase'] else '')+');\n'
block+='''        Output::send(STR("[TypeScan] scan-complete missing-keys={} core-ready={}\\n"), missing_keys.values.size(),core_ready);
        if (!core_ready) return false;
#endif

'''
s=s.replace(marker,block+marker)
s=s.replace('namespace RC::Unreal\n{', 'namespace RC::Unreal\n{\n#ifdef __PROSPERO__\n#include "NativePropertyPointerGuard.inl"\n#endif',1)
save(src,Path('src/TypeChecker.cpp'),s)
(out/'CHANGES.json').write_text(json.dumps({'changes':changes,'include_dirs':[str(out/'include'),str(out/'candidate/generated_include')]},indent=2))
