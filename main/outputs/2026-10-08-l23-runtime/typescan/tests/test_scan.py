from pathlib import Path
import re,subprocess,json,sys
p=Path(__file__).resolve().parent.parent
s=(p/'candidate/src/TypeChecker.cpp').read_text();a=s.index('            auto find_all_property_types');b=s.index('            // TODO: Maybe replace',a)
body=s[a:b]
if '--original' in sys.argv:
 baseline=(p/'original/src/TypeChecker.cpp').read_text();body=baseline[baseline.index('            auto find_all_property_types'):baseline.index('            // TODO: Maybe replace')]
props=sorted(set(re.findall(r'(F\w+Property)::Static',body)))
header=r'''
#include <algorithm>
#include <bit>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "NativeTypeScanPolicy.hpp"
using namespace RC::Unreal;
#define STR(x) x
#define __PROSPERO__ 1
#define STATIC_CLASS_INVALID reinterpret_cast<void*>(-1)
using StringType=std::string;using uint64=uint64_t;constexpr int FNAME_Find=0;
struct FName {std::string name; FName()=default; FName(const char*p,int):name(p){} bool operator==(const FName&)const=default;inline static int StaticSize_Private=0,StaticAlignment_Private=0;};
FName GPropertyName("Property",0);
struct FFieldClass;
struct Variant{FFieldClass* c=nullptr;Variant()=default;Variant(FFieldClass*v):c(v){}bool IsValid()const{return c!=nullptr;}bool operator!=(void*p)const{return reinterpret_cast<void*>(c)!=p;}explicit operator bool()const{return c!=nullptr;}size_t HashObject(){return reinterpret_cast<size_t>(c);}std::string GetName();};
template<class T>struct Store {inline static Variant StaticClassStorage;inline static int StaticValueSizeStorage=0;};
struct ClassMap{std::map<std::string,FFieldClass*> m;bool Contains(FName n){return m.contains(n.name);}void Add(FName n,FFieldClass*c){m.emplace(n.name,c);}};
struct AllClasses{void Add(FFieldClass*){}};
struct FFieldClass {FName name;FFieldClass* parent=nullptr;uint64_t flags=0;FName GetFName(){return name;}uint64_t GetCastFlags(){return flags;}FFieldClass* GetSuperClass(){return parent;}static ClassMap& GetNameToFieldClassMap(){static ClassMap x;return x;}static AllClasses& GetAllFieldClasses(){static AllClasses x;return x;}};
std::string Variant::GetName(){return c->name.name;}
struct FField:Store<FField>{FFieldClass* cls=nullptr;FField* next=nullptr;FFieldClass* GetFFieldClassUnsafe(){return cls;}FField* GetNextFFieldUnsafe(){return next;}};
struct FProperty:FField,Store<FProperty>{using Store<FProperty>::StaticClassStorage;using Store<FProperty>::StaticValueSizeStorage;int size=8;bool unrelated=false;int GetSize(){if(unrelated)throw std::runtime_error("unrelated size");return size;}int GetMinAlignment(){return 8;}Variant GetClass(){return cls;}virtual FProperty* GetInner(){throw std::runtime_error("Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version.");}virtual FProperty* GetElementProp(){throw std::runtime_error("Tried getting member variable 'FSetProperty::ElementProp' that doesn't exist in this engine version.");}virtual FProperty* GetKeyProp(){throw std::runtime_error("Tried getting member variable 'FMapProperty::KeyProp' that doesn't exist in this engine version.");}virtual FProperty* GetValueProp(){throw std::runtime_error("Tried getting member variable 'FMapProperty::ValueProp' that doesn't exist in this engine version.");}void* GetSignatureFunction(){return nullptr;}};
struct FText {inline static int StaticSize_Private=0;inline static FProperty* ValueOpsProperty=nullptr;};
namespace Version {bool IsAtLeast(int,int){return true;}}
namespace NativePropertyGuard{FFieldClass* ReadClass(FField*f){return f->cls;}}
enum class EClassCastFlags:uint64_t{CASTCLASS_FObjectPtrProperty=1};
namespace EFieldIterationFlags {constexpr int IncludeDeprecated=0;}
template<class T>struct TFieldRange:std::vector<T*>{TFieldRange(void*,int){}};
struct UStruct{FField* child=nullptr;FField* GetChildProperties(){return child;}};using UClass=UStruct;
UStruct obj;
namespace UObjectGlobals{UStruct* StaticFindObject_InternalSlow(void*,void*,const char*){return &obj;}}
'''
for t in props+['FObjectPropertyBase']:header+=f'struct {t}:FProperty,Store<{t}>{{using Store<{t}>::StaticClassStorage;using Store<{t}>::StaticValueSizeStorage;}};\n'
core=s[s.index('        bool core_ready = true;'):s.index('        if (!core_ready) return false;')+len('        if (!core_ready) return false;')]
coretypes=re.findall(r'require_type\("(\w+)"',core)
for t in coretypes:
 if t.startswith('U') or t in ['AActor']:
  if t not in ['UStruct','UClass']:header+=f'struct {t}:Store<{t}> {{}};\n'
header=header.replace('struct UStruct{','struct UStruct:Store<UStruct>{')
header+='enum class LogLevel {Warning,Error}; struct Output {template<LogLevel L=LogLevel::Warning,class...A>static void send(A&&...){}};\n'
header+='bool core_gate(const NativeTypeScan::MissingKeys& missing_keys){'+core+'return true;}\n'
header+='struct UCanvasPanel:Store<UCanvasPanel>{};\n'
init='\n'.join(f'{t}::StaticClassStorage=&intClass; {t}::StaticValueSizeStorage=8;' for t in coretypes)
main=r'''
int main(){
 NativeTypeScan::MissingKeys missing_keys;
 BODY
 FFieldClass mapClass{FName("MapProperty",0)},intClass{FName("IntProperty",0)},setClass{FName("SetProperty",0)},arrayClass{FName("ArrayProperty",0)};
 FMapProperty map;map.cls=&mapClass;FIntProperty integer;integer.cls=&intClass;FSetProperty set;set.cls=&setClass;FArrayProperty array;array.cls=&arrayClass;
 map.next=&integer;integer.next=&set;set.next=&array;obj.child=&map;
 find_all_property_types("fixture");
 assert(missing_keys.values.size()==4);
 assert(FIntProperty::StaticValueSizeStorage==8);
 find_all_property_types("fixture");assert(missing_keys.values.size()==4);
 integer.unrelated=true;bool preserved=false;try{find_all_property_types("fixture");}catch(const std::runtime_error&e){preserved=std::string(e.what())=="unrelated size";}assert(preserved);
 integer.unrelated=false;array.next=&map;bool cycle=false;try{find_all_property_types("fixture");}catch(const std::runtime_error&e){cycle=std::string(e.what()).find("cyclic")!=std::string::npos;}assert(cycle);
 CORETEST
 std::cout<<"actual TypeChecker field-scan: 9 assertions passed\n";
}
'''.replace('BODY',body).replace('CORETEST',init+'\nFObjectProperty::StaticClassStorage=&intClass;FObjectProperty::StaticValueSizeStorage=8;FObjectPtrProperty::StaticClassStorage=nullptr;UCanvasPanel::StaticClassStorage=nullptr;assert(core_gate(missing_keys));FObjectProperty::StaticClassStorage=nullptr;FObjectPtrProperty::StaticClassStorage=&intClass;FObjectPtrProperty::StaticValueSizeStorage=8;assert(core_gate(missing_keys));FObjectPtrProperty::StaticClassStorage=nullptr;assert(!core_gate(missing_keys));FObjectProperty::StaticClassStorage=&intClass;FArrayProperty::StaticClassStorage=nullptr;assert(!core_gate(missing_keys));')
(p/'tests/actual_scan.cpp').write_text(header+main)
argv=['clang++','-std=c++20','-I',str(p/'include'),str(p/'tests/actual_scan.cpp'),'-o',str(p/'tests/scan')]
x=subprocess.run(argv,capture_output=True,text=True);print(x.stderr);assert x.returncode==0
x=subprocess.run([str(p/'tests/scan')],capture_output=True,text=True);print(x.stdout+x.stderr);assert (x.returncode!=0 if '--original' in sys.argv else x.returncode==0)
