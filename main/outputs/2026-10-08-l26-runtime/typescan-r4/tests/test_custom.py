from pathlib import Path
import subprocess,sys
p=Path(__file__).resolve().parent.parent
source=p/'candidate/src/CustomProperty.cpp'
if '--original' in sys.argv:source=p/'original/src/CustomProperty.cpp'
s=source.read_text();s=s[s.index('namespace RC::Unreal'):];s=s[:s.index('    CustomStructProperty::')]+ '}\n'
header=r'''
#include <bit>
#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <iostream>
#define __PROSPERO__ 1
#define STR(x) x
namespace RC::Unreal {
struct UClass{};struct UObject {UClass*& GetClassPrivate(){return *reinterpret_cast<UClass**>(reinterpret_cast<char*>(this)+8);}void*& GetOuterPrivate(){return *reinterpret_cast<void**>(reinterpret_cast<char*>(this)+16);}};
namespace Version {bool IsAtLeast(int,int){return true;}}
struct FProperty {int32_t& GetOffset_Internal(){return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(this)+44);}int32_t& GetElementSize(){return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(this)+48);}template<class T>void SetClass(T*p){std::memcpy(reinterpret_cast<char*>(this)+8,&p,8);}template<class T>void SetOwnerVariant(T*p){std::memcpy(reinterpret_cast<char*>(this)+16,&p,8);}};
struct FArrayProperty {inline static std::unordered_map<std::string,int> MemberOffsets;FProperty*GetInner(){return nullptr;}};
struct CustomProperty:FProperty {std::byte data[0x128]{};CustomProperty(int32_t,int32_t);static std::unique_ptr<CustomProperty> construct(int32_t,UClass*,UClass*,int32_t);};
struct CustomArrayProperty:CustomProperty {CustomArrayProperty(int32_t,int32_t);static std::unique_ptr<CustomProperty> construct(int32_t,FProperty*,int32_t);static std::unique_ptr<CustomProperty> construct(int32_t,UClass*,UClass*,FProperty*,int32_t);};
}
'''
main=r'''
using namespace RC::Unreal;
FProperty* inner(const std::unique_ptr<CustomProperty>&p){FProperty*v=nullptr;std::memcpy(&v,reinterpret_cast<const char*>(p.get())+120,8);return v;}
int main(){FArrayProperty::MemberOffsets={{"Inner",120}};CustomProperty element(0,8);auto a=CustomArrayProperty::construct(4,&element,8);assert(inner(a)==&element&&a->GetOffset_Internal()==4&&a->GetElementSize()==8);
 UClass owner,klass;auto b=CustomArrayProperty::construct(4,&owner,&klass,&element,8);auto*dummy=inner(b);assert(dummy&&dummy!=&element&&dummy->GetElementSize()==8);delete static_cast<CustomProperty*>(dummy);
 FArrayProperty::MemberOffsets.clear();bool rejected=false;try{CustomArrayProperty::construct(4,&element,8);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&FArrayProperty::MemberOffsets.empty());
 FArrayProperty::MemberOffsets={{"Inner",int(sizeof(CustomProperty)-4)}};rejected=false;try{CustomArrayProperty::construct(4,&owner,&klass,&element,8);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 FArrayProperty::MemberOffsets={{"Inner",-1}};rejected=false;try{CustomArrayProperty::construct(4,&element,8);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 std::cout<<"actual custom array constructors: 5 assertions passed\n";
}
'''
(p/'tests/actual_custom.cpp').write_text(header+s+main)
a=['clang++','-std=c++20',str(p/'tests/actual_custom.cpp'),'-o',str(p/'tests/custom')]
x=subprocess.run(a,capture_output=True,text=True)
if '--original' in sys.argv:
 print(x.stderr);assert x.returncode!=0;sys.exit(0)
assert x.returncode==0,x.stderr
x=subprocess.run([str(p/'tests/custom')],capture_output=True,text=True);print(x.stdout+x.stderr);assert x.returncode==0
