
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
struct UStruct:Store<UStruct>{FField* child=nullptr;FField* GetChildProperties(){return child;}};using UClass=UStruct;
UStruct obj;
namespace UObjectGlobals{UStruct* StaticFindObject_InternalSlow(void*,void*,const char*){return &obj;}}
struct FAnsiStrProperty:FProperty,Store<FAnsiStrProperty>{using Store<FAnsiStrProperty>::StaticClassStorage;using Store<FAnsiStrProperty>::StaticValueSizeStorage;};
struct FArrayProperty:FProperty,Store<FArrayProperty>{using Store<FArrayProperty>::StaticClassStorage;using Store<FArrayProperty>::StaticValueSizeStorage;};
struct FBoolProperty:FProperty,Store<FBoolProperty>{using Store<FBoolProperty>::StaticClassStorage;using Store<FBoolProperty>::StaticValueSizeStorage;};
struct FByteProperty:FProperty,Store<FByteProperty>{using Store<FByteProperty>::StaticClassStorage;using Store<FByteProperty>::StaticValueSizeStorage;};
struct FClassProperty:FProperty,Store<FClassProperty>{using Store<FClassProperty>::StaticClassStorage;using Store<FClassProperty>::StaticValueSizeStorage;};
struct FClassPtrProperty:FProperty,Store<FClassPtrProperty>{using Store<FClassPtrProperty>::StaticClassStorage;using Store<FClassPtrProperty>::StaticValueSizeStorage;};
struct FDelegateProperty:FProperty,Store<FDelegateProperty>{using Store<FDelegateProperty>::StaticClassStorage;using Store<FDelegateProperty>::StaticValueSizeStorage;};
struct FDoubleProperty:FProperty,Store<FDoubleProperty>{using Store<FDoubleProperty>::StaticClassStorage;using Store<FDoubleProperty>::StaticValueSizeStorage;};
struct FEnumProperty:FProperty,Store<FEnumProperty>{using Store<FEnumProperty>::StaticClassStorage;using Store<FEnumProperty>::StaticValueSizeStorage;};
struct FFieldPathProperty:FProperty,Store<FFieldPathProperty>{using Store<FFieldPathProperty>::StaticClassStorage;using Store<FFieldPathProperty>::StaticValueSizeStorage;};
struct FFloatProperty:FProperty,Store<FFloatProperty>{using Store<FFloatProperty>::StaticClassStorage;using Store<FFloatProperty>::StaticValueSizeStorage;};
struct FInt16Property:FProperty,Store<FInt16Property>{using Store<FInt16Property>::StaticClassStorage;using Store<FInt16Property>::StaticValueSizeStorage;};
struct FInt64Property:FProperty,Store<FInt64Property>{using Store<FInt64Property>::StaticClassStorage;using Store<FInt64Property>::StaticValueSizeStorage;};
struct FInt8Property:FProperty,Store<FInt8Property>{using Store<FInt8Property>::StaticClassStorage;using Store<FInt8Property>::StaticValueSizeStorage;};
struct FIntProperty:FProperty,Store<FIntProperty>{using Store<FIntProperty>::StaticClassStorage;using Store<FIntProperty>::StaticValueSizeStorage;};
struct FInterfaceProperty:FProperty,Store<FInterfaceProperty>{using Store<FInterfaceProperty>::StaticClassStorage;using Store<FInterfaceProperty>::StaticValueSizeStorage;};
struct FLazyObjectProperty:FProperty,Store<FLazyObjectProperty>{using Store<FLazyObjectProperty>::StaticClassStorage;using Store<FLazyObjectProperty>::StaticValueSizeStorage;};
struct FMapProperty:FProperty,Store<FMapProperty>{using Store<FMapProperty>::StaticClassStorage;using Store<FMapProperty>::StaticValueSizeStorage;};
struct FMulticastDelegateProperty:FProperty,Store<FMulticastDelegateProperty>{using Store<FMulticastDelegateProperty>::StaticClassStorage;using Store<FMulticastDelegateProperty>::StaticValueSizeStorage;};
struct FMulticastInlineDelegateProperty:FProperty,Store<FMulticastInlineDelegateProperty>{using Store<FMulticastInlineDelegateProperty>::StaticClassStorage;using Store<FMulticastInlineDelegateProperty>::StaticValueSizeStorage;};
struct FMulticastSparseDelegateProperty:FProperty,Store<FMulticastSparseDelegateProperty>{using Store<FMulticastSparseDelegateProperty>::StaticClassStorage;using Store<FMulticastSparseDelegateProperty>::StaticValueSizeStorage;};
struct FNameProperty:FProperty,Store<FNameProperty>{using Store<FNameProperty>::StaticClassStorage;using Store<FNameProperty>::StaticValueSizeStorage;};
struct FNumericProperty:FProperty,Store<FNumericProperty>{using Store<FNumericProperty>::StaticClassStorage;using Store<FNumericProperty>::StaticValueSizeStorage;};
struct FObjectProperty:FProperty,Store<FObjectProperty>{using Store<FObjectProperty>::StaticClassStorage;using Store<FObjectProperty>::StaticValueSizeStorage;};
struct FObjectPtrProperty:FProperty,Store<FObjectPtrProperty>{using Store<FObjectPtrProperty>::StaticClassStorage;using Store<FObjectPtrProperty>::StaticValueSizeStorage;};
struct FOptionalProperty:FProperty,Store<FOptionalProperty>{using Store<FOptionalProperty>::StaticClassStorage;using Store<FOptionalProperty>::StaticValueSizeStorage;};
struct FSetProperty:FProperty,Store<FSetProperty>{using Store<FSetProperty>::StaticClassStorage;using Store<FSetProperty>::StaticValueSizeStorage;};
struct FSoftClassProperty:FProperty,Store<FSoftClassProperty>{using Store<FSoftClassProperty>::StaticClassStorage;using Store<FSoftClassProperty>::StaticValueSizeStorage;};
struct FSoftObjectProperty:FProperty,Store<FSoftObjectProperty>{using Store<FSoftObjectProperty>::StaticClassStorage;using Store<FSoftObjectProperty>::StaticValueSizeStorage;};
struct FStrProperty:FProperty,Store<FStrProperty>{using Store<FStrProperty>::StaticClassStorage;using Store<FStrProperty>::StaticValueSizeStorage;};
struct FStructProperty:FProperty,Store<FStructProperty>{using Store<FStructProperty>::StaticClassStorage;using Store<FStructProperty>::StaticValueSizeStorage;};
struct FTextProperty:FProperty,Store<FTextProperty>{using Store<FTextProperty>::StaticClassStorage;using Store<FTextProperty>::StaticValueSizeStorage;};
struct FUInt16Property:FProperty,Store<FUInt16Property>{using Store<FUInt16Property>::StaticClassStorage;using Store<FUInt16Property>::StaticValueSizeStorage;};
struct FUInt32Property:FProperty,Store<FUInt32Property>{using Store<FUInt32Property>::StaticClassStorage;using Store<FUInt32Property>::StaticValueSizeStorage;};
struct FUInt64Property:FProperty,Store<FUInt64Property>{using Store<FUInt64Property>::StaticClassStorage;using Store<FUInt64Property>::StaticValueSizeStorage;};
struct FUtf8StrProperty:FProperty,Store<FUtf8StrProperty>{using Store<FUtf8StrProperty>::StaticClassStorage;using Store<FUtf8StrProperty>::StaticValueSizeStorage;};
struct FWeakObjectProperty:FProperty,Store<FWeakObjectProperty>{using Store<FWeakObjectProperty>::StaticClassStorage;using Store<FWeakObjectProperty>::StaticValueSizeStorage;};
struct FObjectPropertyBase:FProperty,Store<FObjectPropertyBase>{using Store<FObjectPropertyBase>::StaticClassStorage;using Store<FObjectPropertyBase>::StaticValueSizeStorage;};
struct UObject:Store<UObject> {};
struct UField:Store<UField> {};
struct UEnum:Store<UEnum> {};
struct UFunction:Store<UFunction> {};
struct UScriptStruct:Store<UScriptStruct> {};
struct AActor:Store<AActor> {};
struct UWorld:Store<UWorld> {};
struct UPackage:Store<UPackage> {};
enum class LogLevel {Warning,Error}; struct Output {template<LogLevel L=LogLevel::Warning,class...A>static void send(A&&...){}};
bool core_gate(const NativeTypeScan::MissingKeys& missing_keys){        bool core_ready = true;
        auto require_type = [&](const char* name, bool found) {
            if (!found) { core_ready = false;
                Output::send<LogLevel::Error>(STR("[TypeScan] missing-core-type={}\n"), StringType(name,name+std::char_traits<char>::length(name)));
            }
        };
        require_type("UObject", UObject::StaticClassStorage && UObject::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UField", UField::StaticClassStorage && UField::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UStruct", UStruct::StaticClassStorage && UStruct::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UClass", UClass::StaticClassStorage && UClass::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UEnum", UEnum::StaticClassStorage && UEnum::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UFunction", UFunction::StaticClassStorage && UFunction::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UScriptStruct", UScriptStruct::StaticClassStorage && UScriptStruct::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("AActor", AActor::StaticClassStorage && AActor::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UWorld", UWorld::StaticClassStorage && UWorld::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("UPackage", UPackage::StaticClassStorage && UPackage::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("FField", FField::StaticClassStorage.IsValid() && FField::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("FProperty", FProperty::StaticClassStorage.IsValid() && FProperty::StaticClassStorage != STATIC_CLASS_INVALID);
        const auto object_property_ready = [](const auto& storage, int size) {
            return storage.IsValid() && storage != STATIC_CLASS_INVALID && size > 0;
        };
        require_type("FObjectProperty|FObjectPtrProperty",
            object_property_ready(FObjectProperty::StaticClassStorage,FObjectProperty::StaticValueSizeStorage) ||
            object_property_ready(FObjectPtrProperty::StaticClassStorage,FObjectPtrProperty::StaticValueSizeStorage));
        require_type("FObjectPropertyBase", FObjectPropertyBase::StaticClassStorage.IsValid() && FObjectPropertyBase::StaticClassStorage != STATIC_CLASS_INVALID);
        require_type("FClassProperty", FClassProperty::StaticClassStorage.IsValid() && FClassProperty::StaticClassStorage != STATIC_CLASS_INVALID && FClassProperty::StaticValueSizeStorage > 0);
        require_type("FStructProperty", FStructProperty::StaticClassStorage.IsValid() && FStructProperty::StaticClassStorage != STATIC_CLASS_INVALID && FStructProperty::StaticValueSizeStorage > 0);
        require_type("FArrayProperty", FArrayProperty::StaticClassStorage.IsValid() && FArrayProperty::StaticClassStorage != STATIC_CLASS_INVALID && FArrayProperty::StaticValueSizeStorage > 0);
        require_type("FBoolProperty", FBoolProperty::StaticClassStorage.IsValid() && FBoolProperty::StaticClassStorage != STATIC_CLASS_INVALID && FBoolProperty::StaticValueSizeStorage > 0);
        require_type("FIntProperty", FIntProperty::StaticClassStorage.IsValid() && FIntProperty::StaticClassStorage != STATIC_CLASS_INVALID && FIntProperty::StaticValueSizeStorage > 0);
        require_type("FByteProperty", FByteProperty::StaticClassStorage.IsValid() && FByteProperty::StaticClassStorage != STATIC_CLASS_INVALID && FByteProperty::StaticValueSizeStorage > 0);
        require_type("FFloatProperty", FFloatProperty::StaticClassStorage.IsValid() && FFloatProperty::StaticClassStorage != STATIC_CLASS_INVALID && FFloatProperty::StaticValueSizeStorage > 0);
        require_type("FDoubleProperty", FDoubleProperty::StaticClassStorage.IsValid() && FDoubleProperty::StaticClassStorage != STATIC_CLASS_INVALID && FDoubleProperty::StaticValueSizeStorage > 0);
        require_type("FNameProperty", FNameProperty::StaticClassStorage.IsValid() && FNameProperty::StaticClassStorage != STATIC_CLASS_INVALID && FNameProperty::StaticValueSizeStorage > 0);
        require_type("FStrProperty", FStrProperty::StaticClassStorage.IsValid() && FStrProperty::StaticClassStorage != STATIC_CLASS_INVALID && FStrProperty::StaticValueSizeStorage > 0);
        require_type("FTextProperty", FTextProperty::StaticClassStorage.IsValid() && FTextProperty::StaticClassStorage != STATIC_CLASS_INVALID && FTextProperty::StaticValueSizeStorage > 0);
        require_type("FEnumProperty", FEnumProperty::StaticClassStorage.IsValid() && FEnumProperty::StaticClassStorage != STATIC_CLASS_INVALID && FEnumProperty::StaticValueSizeStorage > 0);
        Output::send(STR("[TypeScan] scan-complete missing-keys={} core-ready={}\n"), missing_keys.values.size(),core_ready);
        if (!core_ready) return false;return true;}
struct UCanvasPanel:Store<UCanvasPanel>{};

int main(){
 NativeTypeScan::MissingKeys missing_keys;
             auto find_all_property_types = [&](const StringType& obj_string) -> void {
#ifdef __PROSPERO__
                // TypeChecker is a friend of FField; keep bootstrap access in this context.
                // The existing accessor uses the published map and checks the version.
                auto NativeBootstrapFieldClass = [](FField* field) -> FFieldClass* {
                    return NativePropertyGuard::ReadClass(field);
                };
#endif
                UStruct* actor_obj = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, obj_string.c_str()));
                if (!actor_obj) { return; }

                // Manually iterating fields here because 'ForEachProperty' isn't ready until after this function is done
                FField* field = actor_obj->GetChildProperties();
                std::vector<FField*> visited;
                while (field)
                {
                    if (std::find(visited.begin(), visited.end(), field) != visited.end())
                        throw std::runtime_error("[TypeScan] cyclic field chain refused");
                    visited.push_back(field);
                    // Obtain next independently; a missing traversal key ends this
                    // object's chain, but the other object scans still proceed.
                    FField* next = nullptr;
                    missing_keys.attempt([&] { next = field->GetNextFFieldUnsafe(); });
                    missing_keys.attempt([&] {
                    // The native bootstrap uses the already published member map; the legacy branch
                    // retains its original offset because FField::GetClass is not ready here.
#ifdef __PROSPERO__
                    FFieldClass* ffield_class = NativeBootstrapFieldClass(field);
#else
                    FFieldClass* ffield_class = Helper::Casting::ptr_cast_deref<FFieldClass*>(field, FFieldClassOffset);
#endif
                    if (!ffield_class)
                    {
                        return;
                    }

                    FName type_name = ffield_class->GetFName();

                    // Populate the global FFieldClass maps for dynamic type lookup
                    if (!FFieldClass::GetNameToFieldClassMap().Contains(type_name))
                    {
                        FFieldClass::GetNameToFieldClassMap().Add(type_name, ffield_class);
                        FFieldClass::GetAllFieldClasses().Add(ffield_class);
                    }

                    // TODO: Look at the dumped objects and put every single FField type in here
                    // At the moment there are probably some missing types

                    // Fully supported
                    if (type_name == FName(STR("ObjectProperty"), FNAME_Find))
                    {
                        if ((ffield_class->GetCastFlags() & static_cast<uint64>(EClassCastFlags::CASTCLASS_FObjectPtrProperty)) != 0)
                        {
                            FObjectPtrProperty::StaticClassStorage = ffield_class;
                            FObjectPtrProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        }
                        else
                        {
                            FObjectProperty::StaticClassStorage = ffield_class;
                            FObjectProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        }
                        FFieldClass* object_property_base_class = ffield_class->GetSuperClass();
                        FObjectPropertyBase::StaticClassStorage = object_property_base_class;
                        FObjectPropertyBase::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();

                        // Add FObjectPropertyBase to the map
                        if (object_property_base_class)
                        {
                            FName base_type_name = object_property_base_class->GetFName();
                            if (!FFieldClass::GetNameToFieldClassMap().Contains(base_type_name))
                            {
                                FFieldClass::GetNameToFieldClassMap().Add(base_type_name, object_property_base_class);
                                FFieldClass::GetAllFieldClasses().Add(object_property_base_class);
                            }
                        }
                    }
                    if (type_name == FName(STR("ObjectPtrProperty"), FNAME_Find))
                    {
                        // I don't think this is ever hit anymore, but I'm not entirely sure.
                        // It seems that FObjectPtrProperty uses the 'ObjectProperty FName in the final UE5.0 release.
                        FObjectPtrProperty::StaticClassStorage = ffield_class;
                        FObjectPtrProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("Int8Property"), FNAME_Find))
                    {
                        FInt8Property::StaticClassStorage = ffield_class;
                        FInt8Property::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();

                        // This doesn't need to be in 'Int8Property', it can be in any property
                        FFieldClass* ffield_super_class = ffield_class->GetSuperClass();
                        if (ffield_super_class)
                        {
                            do
                            {
                                FName super_type_name = ffield_super_class->GetFName();

                                // Add superclasses to the map as well
                                if (!FFieldClass::GetNameToFieldClassMap().Contains(super_type_name))
                                {
                                    FFieldClass::GetNameToFieldClassMap().Add(super_type_name, ffield_super_class);
                                    FFieldClass::GetAllFieldClasses().Add(ffield_super_class);
                                }

                                if (super_type_name == GPropertyName)
                                {
                                    FProperty::StaticClassStorage = ffield_super_class;
                                    FProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                                    FFieldClass* ffield_base_class = ffield_super_class->GetSuperClass();
                                    FField::StaticClassStorage = ffield_base_class;
                                    FField::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();

                                    // Add FField base class to map
                                    if (ffield_base_class)
                                    {
                                        FName base_type_name = ffield_base_class->GetFName();
                                        if (!FFieldClass::GetNameToFieldClassMap().Contains(base_type_name))
                                        {
                                            FFieldClass::GetNameToFieldClassMap().Add(base_type_name, ffield_base_class);
                                            FFieldClass::GetAllFieldClasses().Add(ffield_base_class);
                                        }
                                    }
                                    break;
                                }

                                ffield_super_class = ffield_super_class->GetSuperClass();
                            } while (ffield_super_class);
                        }
                    }
                    if (type_name == FName(STR("Int16Property"), FNAME_Find))
                    {
                        FInt16Property::StaticClassStorage = ffield_class;
                        FInt16Property::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("IntProperty"), FNAME_Find))
                    {
                        FIntProperty::StaticClassStorage = ffield_class;
                        FIntProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("Int64Property"), FNAME_Find))
                    {
                        FInt64Property::StaticClassStorage = ffield_class;
                        FInt64Property::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("NameProperty"), FNAME_Find))
                    {
                        FNameProperty::StaticClassStorage = ffield_class;
                        FNameProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        FName::StaticSize_Private = static_cast<FNameProperty*>(field)->GetSize();
                        FName::StaticAlignment_Private = static_cast<FNameProperty*>(field)->GetMinAlignment();
                    }
                    if (type_name == FName(STR("FloatProperty"), FNAME_Find))
                    {
                        FFloatProperty::StaticClassStorage = static_cast<FFieldClass*>(ffield_class);
                        FFloatProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();

                        // 'NumericProperty' doesn't appear as the direct type for any properties
                        // Because of this, we need to use a property that we know has 'NumericProperty' in its super chain
                        // We're using 'FFloatProperty' here for no particular reason other than that it has 'NumericProperty' as it's super
                        FFieldClass* numeric_property_class = ffield_class->GetSuperClass();
                        FNumericProperty::StaticClassStorage = numeric_property_class;

                        // Add NumericProperty to the map
                        if (numeric_property_class)
                        {
                            FName numeric_type_name = numeric_property_class->GetFName();
                            if (!FFieldClass::GetNameToFieldClassMap().Contains(numeric_type_name))
                            {
                                FFieldClass::GetNameToFieldClassMap().Add(numeric_type_name, numeric_property_class);
                                FFieldClass::GetAllFieldClasses().Add(numeric_property_class);
                            }
                        }
                    }
                    if (type_name == FName(STR("DoubleProperty"), FNAME_Find))
                    {
                        FDoubleProperty::StaticClassStorage = ffield_class;
                        FDoubleProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("ByteProperty"), FNAME_Find))
                    {
                        FByteProperty::StaticClassStorage = ffield_class;
                        FByteProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("UInt16Property"), FNAME_Find))
                    {
                        FUInt16Property::StaticClassStorage = ffield_class;
                        FUInt16Property::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("UInt32Property"), FNAME_Find))
                    {
                        FUInt32Property::StaticClassStorage = ffield_class;
                        FUInt32Property::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("UInt64Property"), FNAME_Find))
                    {
                        FUInt64Property::StaticClassStorage = ffield_class;
                        FUInt64Property::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("BoolProperty"), FNAME_Find))
                    {
                        FBoolProperty::StaticClassStorage = ffield_class;
                        FBoolProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("ArrayProperty"), FNAME_Find))
                    {
                        FArrayProperty::StaticClassStorage = ffield_class;
                        FArrayProperty::StaticValueSizeStorage = static_cast<FArrayProperty*>(field)->GetSize();

                        // Check inner element property for its type (e.g., AnsiStrProperty in arrays)
                        if (Version::IsAtLeast(5, 6))
                        {
                            FArrayProperty* array_prop = static_cast<FArrayProperty*>(field);
                            if (array_prop)
                            {
                                missing_keys.attempt([&] {
                                FProperty* inner_prop = array_prop->GetInner();
                                if (inner_prop)
                                {
#ifdef __PROSPERO__
                                    FFieldClass* inner_class = NativeBootstrapFieldClass(inner_prop);
#else
                                    FFieldClass* inner_class = Helper::Casting::ptr_cast_deref<FFieldClass*>(inner_prop, FFieldClassOffset);
#endif
                                    if (inner_class)
                                    {
                                        FName inner_type_name = inner_class->GetFName();
                                        if (inner_type_name == FName(STR("AnsiStrProperty"), FNAME_Find))
                                        {
                                            FAnsiStrProperty::StaticClassStorage = inner_class;
                                            FAnsiStrProperty::StaticValueSizeStorage = static_cast<FProperty*>(inner_prop)->GetSize();
                                        }
                                        else if (inner_type_name == FName(STR("Utf8StrProperty"), FNAME_Find))
                                        {
                                            FUtf8StrProperty::StaticClassStorage = inner_class;
                                            FUtf8StrProperty::StaticValueSizeStorage = static_cast<FProperty*>(inner_prop)->GetSize();
                                        }
                                    }
                                }
                                });
                            }
                        }
                    }
                    if (type_name == FName(STR("SetProperty"), FNAME_Find))
                    {
                        FSetProperty::StaticClassStorage = ffield_class;
                        FSetProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();

                        // Check inner element property for its type (e.g., AnsiStrProperty in sets)
                        if (Version::IsAtLeast(5, 6))
                        {
                            FSetProperty* set_prop = static_cast<FSetProperty*>(field);
                            if (set_prop)
                            {
                                missing_keys.attempt([&] {
                                FProperty* element_prop = set_prop->GetElementProp();
                                if (element_prop)
                                {
#ifdef __PROSPERO__
                                    FFieldClass* element_class = NativeBootstrapFieldClass(element_prop);
#else
                                    FFieldClass* element_class = Helper::Casting::ptr_cast_deref<FFieldClass*>(element_prop, FFieldClassOffset);
#endif
                                    if (element_class)
                                    {
                                        FName element_type_name = element_class->GetFName();
                                        if (element_type_name == FName(STR("AnsiStrProperty"), FNAME_Find))
                                        {
                                            FAnsiStrProperty::StaticClassStorage = element_class;
                                            FAnsiStrProperty::StaticValueSizeStorage = static_cast<FProperty*>(element_prop)->GetSize();
                                        }
                                        else if (element_type_name == FName(STR("Utf8StrProperty"), FNAME_Find))
                                        {
                                            FUtf8StrProperty::StaticClassStorage = element_class;
                                            FUtf8StrProperty::StaticValueSizeStorage = static_cast<FProperty*>(element_prop)->GetSize();
                                        }
                                    }
                                }
                                });
                            }
                        }
                    }
                    if (type_name == FName(STR("MapProperty"), FNAME_Find))
                    {
                        FMapProperty::StaticClassStorage = ffield_class;
                        FMapProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();

                        // Check inner key/value properties for their types (e.g., AnsiStrProperty in map keys)
                        if (Version::IsAtLeast(5, 6))
                        {
                            FMapProperty* map_prop = static_cast<FMapProperty*>(field);
                            if (map_prop)
                            {
                                // Check key property type
                                missing_keys.attempt([&] {
                                FProperty* key_prop = map_prop->GetKeyProp();
                                if (key_prop)
                                {
#ifdef __PROSPERO__
                                    FFieldClass* key_class = NativeBootstrapFieldClass(key_prop);
#else
                                    FFieldClass* key_class = Helper::Casting::ptr_cast_deref<FFieldClass*>(key_prop, FFieldClassOffset);
#endif
                                    if (key_class)
                                    {
                                        FName key_type_name = key_class->GetFName();
                                        if (key_type_name == FName(STR("AnsiStrProperty"), FNAME_Find))
                                        {
                                            FAnsiStrProperty::StaticClassStorage = key_class;
                                            FAnsiStrProperty::StaticValueSizeStorage = static_cast<FProperty*>(key_prop)->GetSize();
                                        }
                                        else if (key_type_name == FName(STR("Utf8StrProperty"), FNAME_Find))
                                        {
                                            FUtf8StrProperty::StaticClassStorage = key_class;
                                            FUtf8StrProperty::StaticValueSizeStorage = static_cast<FProperty*>(key_prop)->GetSize();
                                        }
                                    }
                                }
                                });

                                // Check value property type
                                missing_keys.attempt([&] {
                                FProperty* value_prop = map_prop->GetValueProp();
                                if (value_prop)
                                {
#ifdef __PROSPERO__
                                    FFieldClass* value_class = NativeBootstrapFieldClass(value_prop);
#else
                                    FFieldClass* value_class = Helper::Casting::ptr_cast_deref<FFieldClass*>(value_prop, FFieldClassOffset);
#endif
                                    if (value_class)
                                    {
                                        FName value_type_name = value_class->GetFName();
                                        if (value_type_name == FName(STR("AnsiStrProperty"), FNAME_Find))
                                        {
                                            FAnsiStrProperty::StaticClassStorage = value_class;
                                            FAnsiStrProperty::StaticValueSizeStorage = static_cast<FProperty*>(value_prop)->GetSize();
                                        }
                                        else if (value_type_name == FName(STR("Utf8StrProperty"), FNAME_Find))
                                        {
                                            FUtf8StrProperty::StaticClassStorage = value_class;
                                            FUtf8StrProperty::StaticValueSizeStorage = static_cast<FProperty*>(value_prop)->GetSize();
                                        }
                                    }
                                }
                                });
                            }
                        }
                    }
                    if (type_name == FName(STR("StructProperty"), FNAME_Find))
                    {
                        FStructProperty::StaticClassStorage = ffield_class;
                        FStructProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("ClassProperty"), FNAME_Find))
                    {
                        FClassProperty::StaticClassStorage = ffield_class;
                        FClassProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("ClassPtrProperty"), FNAME_Find))
                    {
                        FClassPtrProperty::StaticClassStorage = ffield_class;
                        FClassPtrProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("SoftClassProperty"), FNAME_Find))
                    {
                        FSoftClassProperty::StaticClassStorage = ffield_class;
                        FSoftClassProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("WeakObjectProperty"), FNAME_Find))
                    {
                        FWeakObjectProperty::StaticClassStorage = ffield_class;
                        FWeakObjectProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("LazyObjectProperty"), FNAME_Find))
                    {
                        FLazyObjectProperty::StaticClassStorage = ffield_class;
                        FLazyObjectProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("SoftObjectProperty"), FNAME_Find))
                    {
                        FSoftObjectProperty::StaticClassStorage = ffield_class;
                        FSoftObjectProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("EnumProperty"), FNAME_Find))
                    {
                        FEnumProperty::StaticClassStorage = ffield_class;
                        FEnumProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("TextProperty"), FNAME_Find))
                    {
                        FTextProperty::StaticClassStorage = ffield_class;
                        FTextProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        FText::StaticSize_Private = static_cast<FTextProperty*>(field)->GetSize();
                        FText::ValueOpsProperty = static_cast<FProperty*>(field);
                    }
                    if (type_name == FName(STR("StrProperty"), FNAME_Find))
                    {
                        FStrProperty::StaticClassStorage = ffield_class;
                        FStrProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (Version::IsAtLeast(5, 6))
                    {
                        if (type_name == FName(STR("Utf8StrProperty"), FNAME_Find))
                        {
                            FUtf8StrProperty::StaticClassStorage = ffield_class;
                            FUtf8StrProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        }
                        if (type_name == FName(STR("AnsiStrProperty"), FNAME_Find))
                        {
                            FAnsiStrProperty::StaticClassStorage = ffield_class;
                            FAnsiStrProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        }
                    }
                    if (type_name == FName(STR("DelegateProperty"), FNAME_Find))
                    {
                        FDelegateProperty::StaticClassStorage = ffield_class;
                        FDelegateProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("MulticastInlineDelegateProperty"), FNAME_Find))
                    {
                        FMulticastInlineDelegateProperty::StaticClassStorage = ffield_class;
                        FMulticastInlineDelegateProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        FFieldClass* multicast_delegate_class = ffield_class->GetSuperClass();
                        FMulticastDelegateProperty::StaticClassStorage = multicast_delegate_class;

                        // Add MulticastDelegateProperty to the map
                        if (multicast_delegate_class)
                        {
                            FName multicast_type_name = multicast_delegate_class->GetFName();
                            if (!FFieldClass::GetNameToFieldClassMap().Contains(multicast_type_name))
                            {
                                FFieldClass::GetNameToFieldClassMap().Add(multicast_type_name, multicast_delegate_class);
                                FFieldClass::GetAllFieldClasses().Add(multicast_delegate_class);
                            }
                        }

                        // Attempt to get the pointer to the FObjectProperty FFieldClass.
                        // It seems that all UObject parameters are of type FObjectProperty and not FObjectPtrProperty.
                        // This doesn't have to go in the delegate property section, it can also go in the UFunction section.
                        // The 'HashObject' function simply returns the address in a size_t, but keep this in mind if the 'HashObject' function ever changes.
                        if (Version::IsAtLeast(5, 0) && std::bit_cast<void*>(FObjectProperty::StaticClassStorage.HashObject()) == STATIC_CLASS_INVALID)
                        {
                            FMulticastInlineDelegateProperty* AsMulticastInlineDelegateProperty = static_cast<FMulticastInlineDelegateProperty*>(field);
                            for (FProperty* Param : TFieldRange<FProperty>(AsMulticastInlineDelegateProperty->GetSignatureFunction(), EFieldIterationFlags::IncludeDeprecated)) 
                            {
                                if (Param->GetClass().GetName() == STR("ObjectProperty"))
                                {
                                    FObjectProperty::StaticClassStorage = Param->GetClass();
                                    FObjectProperty::StaticValueSizeStorage = static_cast<FProperty*>(Param)->GetSize();
                                    break;
                                }
                            }
                        }
                    }
                    if (type_name == FName(STR("MulticastSparseDelegateProperty"), FNAME_Find))
                    {
                        FMulticastSparseDelegateProperty::StaticClassStorage = ffield_class;
                        FMulticastSparseDelegateProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("InterfaceProperty"), FNAME_Find))
                    {
                        FInterfaceProperty::StaticClassStorage = ffield_class;
                        FInterfaceProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (type_name == FName(STR("FieldPathProperty"), FNAME_Find))
                    {
                        FFieldPathProperty::StaticClassStorage = ffield_class;
                        FFieldPathProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                    }
                    if (Version::IsAtLeast(5, 4)) {
                        if (type_name == FName(STR("OptionalProperty"), FNAME_Find))
                        {
                            FOptionalProperty::StaticClassStorage = ffield_class;
                            FOptionalProperty::StaticValueSizeStorage = static_cast<FProperty*>(field)->GetSize();
                        }
                    }

                    });
                    field = next;
                }
            };


 FFieldClass mapClass{FName("MapProperty",0)},intClass{FName("IntProperty",0)},setClass{FName("SetProperty",0)},arrayClass{FName("ArrayProperty",0)};
 FMapProperty map;map.cls=&mapClass;FIntProperty integer;integer.cls=&intClass;FSetProperty set;set.cls=&setClass;FArrayProperty array;array.cls=&arrayClass;
 map.next=&integer;integer.next=&set;set.next=&array;obj.child=&map;
 find_all_property_types("fixture");
 assert(missing_keys.values.size()==4);
 assert(FIntProperty::StaticValueSizeStorage==8);
 find_all_property_types("fixture");assert(missing_keys.values.size()==4);
 integer.unrelated=true;bool preserved=false;try{find_all_property_types("fixture");}catch(const std::runtime_error&e){preserved=std::string(e.what())=="unrelated size";}assert(preserved);
 integer.unrelated=false;array.next=&map;bool cycle=false;try{find_all_property_types("fixture");}catch(const std::runtime_error&e){cycle=std::string(e.what()).find("cyclic")!=std::string::npos;}assert(cycle);
 UObject::StaticClassStorage=&intClass; UObject::StaticValueSizeStorage=8;
UField::StaticClassStorage=&intClass; UField::StaticValueSizeStorage=8;
UStruct::StaticClassStorage=&intClass; UStruct::StaticValueSizeStorage=8;
UClass::StaticClassStorage=&intClass; UClass::StaticValueSizeStorage=8;
UEnum::StaticClassStorage=&intClass; UEnum::StaticValueSizeStorage=8;
UFunction::StaticClassStorage=&intClass; UFunction::StaticValueSizeStorage=8;
UScriptStruct::StaticClassStorage=&intClass; UScriptStruct::StaticValueSizeStorage=8;
AActor::StaticClassStorage=&intClass; AActor::StaticValueSizeStorage=8;
UWorld::StaticClassStorage=&intClass; UWorld::StaticValueSizeStorage=8;
UPackage::StaticClassStorage=&intClass; UPackage::StaticValueSizeStorage=8;
FField::StaticClassStorage=&intClass; FField::StaticValueSizeStorage=8;
FProperty::StaticClassStorage=&intClass; FProperty::StaticValueSizeStorage=8;
FObjectPropertyBase::StaticClassStorage=&intClass; FObjectPropertyBase::StaticValueSizeStorage=8;
FClassProperty::StaticClassStorage=&intClass; FClassProperty::StaticValueSizeStorage=8;
FStructProperty::StaticClassStorage=&intClass; FStructProperty::StaticValueSizeStorage=8;
FArrayProperty::StaticClassStorage=&intClass; FArrayProperty::StaticValueSizeStorage=8;
FBoolProperty::StaticClassStorage=&intClass; FBoolProperty::StaticValueSizeStorage=8;
FIntProperty::StaticClassStorage=&intClass; FIntProperty::StaticValueSizeStorage=8;
FByteProperty::StaticClassStorage=&intClass; FByteProperty::StaticValueSizeStorage=8;
FFloatProperty::StaticClassStorage=&intClass; FFloatProperty::StaticValueSizeStorage=8;
FDoubleProperty::StaticClassStorage=&intClass; FDoubleProperty::StaticValueSizeStorage=8;
FNameProperty::StaticClassStorage=&intClass; FNameProperty::StaticValueSizeStorage=8;
FStrProperty::StaticClassStorage=&intClass; FStrProperty::StaticValueSizeStorage=8;
FTextProperty::StaticClassStorage=&intClass; FTextProperty::StaticValueSizeStorage=8;
FEnumProperty::StaticClassStorage=&intClass; FEnumProperty::StaticValueSizeStorage=8;
FObjectProperty::StaticClassStorage=&intClass;FObjectProperty::StaticValueSizeStorage=8;FObjectPtrProperty::StaticClassStorage=nullptr;UCanvasPanel::StaticClassStorage=nullptr;assert(core_gate(missing_keys));FObjectProperty::StaticClassStorage=nullptr;FObjectPtrProperty::StaticClassStorage=&intClass;FObjectPtrProperty::StaticValueSizeStorage=8;assert(core_gate(missing_keys));FObjectPtrProperty::StaticClassStorage=nullptr;assert(!core_gate(missing_keys));FObjectProperty::StaticClassStorage=&intClass;FArrayProperty::StaticClassStorage=nullptr;assert(!core_gate(missing_keys));
 std::cout<<"actual TypeChecker field-scan: 9 assertions passed\n";
}
