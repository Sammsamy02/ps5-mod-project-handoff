#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/NameTypes.hpp>
using P=RC::Unreal::UObjectGlobals::Above55::FStaticConstructObjectParameters;
extern "C" { extern const unsigned long long ConstructionFacts[] = {
sizeof(P),
alignof(P),
__builtin_offsetof(P,Class),
__builtin_offsetof(P,Outer),
__builtin_offsetof(P,Name),
__builtin_offsetof(P,SetFlags),
__builtin_offsetof(P,InternalSetFlags),
__builtin_offsetof(P,bCopyTransientsFromClassDefaults),
__builtin_offsetof(P,bAssumeTemplateIsArchetype),
__builtin_offsetof(P,Template),
__builtin_offsetof(P,InstanceGraph),
__builtin_offsetof(P,ExternalPackage),
__builtin_offsetof(P,PropertyInitCallback),
__builtin_offsetof(P,SerialNumber),
__builtin_offsetof(P,SubobjectOverrides),
sizeof(((P*)nullptr)->Class),
sizeof(((P*)nullptr)->Outer),
sizeof(((P*)nullptr)->Name),
sizeof(((P*)nullptr)->SetFlags),
sizeof(((P*)nullptr)->InternalSetFlags),
sizeof(((P*)nullptr)->bCopyTransientsFromClassDefaults),
sizeof(((P*)nullptr)->bAssumeTemplateIsArchetype),
sizeof(((P*)nullptr)->Template),
sizeof(((P*)nullptr)->InstanceGraph),
sizeof(((P*)nullptr)->ExternalPackage),
sizeof(((P*)nullptr)->PropertyInitCallback),
sizeof(((P*)nullptr)->SerialNumber),
sizeof(((P*)nullptr)->SubobjectOverrides)
}; }
extern "C" RC::Unreal::UObject* CallConstruct(RC::Unreal::UObject*(*fn)(const P&),const P& p) {return fn(p);}
