from pathlib import Path
import re,json,hashlib
w=Path(__file__).resolve().parent;s=w.parent;p=s/'ue4ss-unreal-native-build-1/port/unreal/generated_include';schema=s/'ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc';sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest();facts={};pins={}
for f in sorted(p.glob('MemberVariableLayout_SrcWrapper*.hpp')):
 t=f.read_text();m=re.search(r'([\w:]+)::MemberOffsets',t)
 if not m:continue
 cls=m[1];pins[str(f)]=sha(f)
 rx=r'(?:^|\n)[^\n]*\b'+re.escape(cls)+r'::(Get\w+)\([^\n]*\)\s*(?:const)?\s*\{\s*(?:(?!\n\}).)*?MemberOffsets\.find\(STR\("([^"\n]+)"\)\)\s*(?:(?!\n\}).)*?return \*Helper::Casting::ptr_cast<(.*?)>\(this, offset\);'
 for m in re.finditer(rx,t,re.S):
  method,key,cast=m.groups();v=facts.setdefault((cls,key),{'class':cls,'canonical_key':key,'alternatives':{}})
  a=v['alternatives'].setdefault(method,{'accessor':method,'casts':[],'file':str(f),'line':t.count('\n',0,m.start())+1})
  if cast not in a['casts']:a['casts'].append(cast)
keys=sorted({(c,k)for _,_,c,k,bit in re.findall(r'\{"([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)", (true|false)\}',schema.read_text())if bit=='false' and k!='UEP_TotalSize'})
# Getter declarations provide genuine nested aliases and currently compiled
# platform declarations without inventing names for opaque source types.
code='''#include <NativeLayoutWidths.hpp>
#include <NativeLayoutEngineIncludes.hpp>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
namespace RC::Platform::NativeLayouts {
namespace {
using namespace RC::Unreal;
constexpr auto ConservativeWidth(std::initializer_list<std::optional<uint32_t>> alternatives) noexcept -> std::optional<uint32_t> {
 if(alternatives.size()==0) return std::nullopt;
 uint32_t width{};for(auto value:alternatives){if(!value)return std::nullopt;width=std::max(width,*value);}return width;
}
'''
methods={};rows=[];provenance=[]
for cls,key in keys:
 f=facts.get((cls,key));entries=[]
 if f:
  for method in sorted(f['alternatives']):
   pair=(cls,method)
   if pair not in methods:
    name='Accessor'+str(len(methods));methods[pair]=name
    source_types=sorted({re.sub(r'^const\s+','',cast.strip()[:-1].strip())for cast in f['alternatives'][method]['casts']})
    # Visibility must not hide known pointer/scalar storage widths. This
    # fallback is copied from genuine ptr_cast value types, never numeric.
    simple=lambda ty: bool(re.fullmatch(r'(?:bool|char|float|double|u?int(?:8|16|32|64)|(?:FFieldClass|UClass|UObject|UObjectBase|FUObjectItem|void)\s*\*+)',ty))
    fallback='ConservativeWidth({'+','.join('WidthDetail::ValueWidth<'+ty+'>()'for ty in source_types)+'})' if all(simple(ty)for ty in source_types) else 'std::nullopt'
    code+='template<class T> constexpr auto '+name+'() noexcept -> std::optional<uint32_t> {\n if constexpr(requires(T& value){value.'+method+'();}) return WidthDetail::ValueWidth<decltype(std::declval<T&>().'+method+'())>();\n else return '+fallback+';\n}\n'
   entries.append(methods[pair]+'<'+cls+'>()')
  provenance.append({**f,'alternatives':list(f['alternatives'].values())})
 else:provenance.append({'class':cls,'canonical_key':key,'unsupported_reason':'No matching canonical ptr_cast accessor body in frozen generated source','alternatives':[]})
 rows.append('{"'+cls+'", "'+key+'", ConservativeWidth({'+','.join(entries)+'})}')
code+='struct WidthRecord {std::string_view class_name,key;std::optional<uint32_t> width;};\nconstexpr WidthRecord Records[]{\n'+',\n'.join(rows)+'\n};\nconstexpr auto Find(std::string_view cls,std::string_view key) noexcept -> std::optional<uint32_t> {for(const auto& record:Records)if(record.class_name==cls&&record.key==key)return record.width;return std::nullopt;}\n'
code+='}\nauto GetAccessorWidth(std::string_view cls,std::string_view key) noexcept -> std::optional<uint32_t> {return Find(cls,key); }\n}\n'
control='#include "../src/NativeLayoutWidths.cpp"\nnamespace RC::Platform::NativeLayouts {\nstatic_assert(Find("FField","ClassPrivate")==sizeof(Unreal::FFieldClass*));\nstatic_assert(Find("UObjectBase","ClassPrivate")==sizeof(Unreal::UClass*));\nstatic_assert(Find("UObjectBase","InternalIndex_Private")==sizeof(Unreal::int32));\nstatic_assert(Find("FUObjectItem","Object")==sizeof(Unreal::UObjectBase*));\nstatic_assert(Find("TUObjectArray","NumElements")==sizeof(Unreal::int32));\nstatic_assert(Find("TUObjectArray","Objects")==sizeof(Unreal::FUObjectItem*));\nstatic_assert(Find("TUObjectArray","Chunks")==sizeof(Unreal::TUObjectArray::Chunks_Type));\nstatic_assert(!Find("UFunction","Func")); // Private alias/getters: explicitly unsupported.\nstatic_assert(!Find("UStruct","SuperStruct")); // Legacy value type is forward-only.\nstatic_assert(Find("UStruct","PropertiesSize")==sizeof(Unreal::int32));\nstatic_assert(!Find("Unknown","NoAccessor"));\nstruct IncompleteOwnControl;\nstatic_assert(!WidthDetail::ValueWidth<IncompleteOwnControl>());\nstatic_assert(!WidthDetail::ValueWidth<void>());\nextern "C" [[gnu::section(".width_facts"),gnu::used]] const uint32_t NativeWidthControlFacts[]{\n'
control+=',\n'.join('Find("'+cls+'","'+key+'").value_or(0)'for cls,key in keys)+'\n};\n}\n'
(w/'tests/native_width_controls.cpp').write_text(control)
(w/'src/NativeLayoutWidths.cpp').write_text(code);(w/'provenance.json').write_text(json.dumps({'schema':str(schema),'schema_sha256':sha(schema),'source_pins':pins,'source_canonical_count':len(facts),'schema_canonical_non_bitfield_count':len(keys),'width_policy':'Maximum sizeof all genuine accessor return value types; unavailable if any accessor is missing or value type incomplete. No game offsets, native game ABI not validated.','records':provenance},indent=2)+'\n');print('source',len(facts),'schema',len(keys),'matching',sum(k in facts for k in keys),'missing',sum(k not in facts for k in keys));print('missing records',[k for k in keys if k not in facts])
