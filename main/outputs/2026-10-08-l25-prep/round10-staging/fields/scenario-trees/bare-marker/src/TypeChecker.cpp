#include <algorithm>
#include <string>
#include "NativeTypeScanPolicy.hpp"
#ifdef __PROSPERO__
#include "NativePropertyPointerGuard.hpp"
#include <atomic>
#endif

#include <DynamicOutput/Output.hpp>
#include <Helpers/Casting.hpp>
// l26 fix (agent-ES): narrow-first refusal lines need the explicit
// locale-free widen (broken short-wchar codec doctrine, Utf16Utf8.hpp).
#include <Helpers/Utf16Utf8.hpp>
#include <Unreal/TypeChecker.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/FField.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/FAssetData.hpp>
#include <Unreal/UInterface.hpp>
#include <Unreal/UActorComponent.hpp>
#include <Unreal/UGameViewportClient.hpp>
#include <Unreal/Engine/UDataTable.hpp>
#include <Unreal/UPackage.hpp>
#include <Unreal/CanvasPanel.hpp>
#include <Unreal/World.hpp>
#include <Unreal/Property/FEnumProperty.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Property/FFieldPathProperty.hpp>
#include <Unreal/Property/FOptionalProperty.hpp>
#include <Unreal/CoreUObject/UObject/FUtf8StrProperty.hpp>
#include <Unreal/CoreUObject/UObject/FAnsiStrProperty.hpp>
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/FSoftObjectPath.hpp>
#include <String/StringType.hpp>

namespace RC::Unreal
{
#ifdef __PROSPERO__
#include "NativePropertyPointerGuard.inl"
    // l25: parse the guard's refusal-detail trailer (reason/object/field/class)
    // back out of a [PropertyPointer] exception text. The per-object catch in
    // the type scan uses this to record thrown refusals with the exact reason,
    // field address and class pointer the guard reported.
    struct ScanRefusalDetail {
        NativePropertyGuard::RefusalReason reason;
        uintptr_t field_address;
        uintptr_t class_pointer;
        bool parsed;
    };
    auto parse_property_pointer_refusal = [](const std::string& text) -> ScanRefusalDetail {
        ScanRefusalDetail out{NativePropertyGuard::RefusalReason::SlotRead, 0, 0, false};
        const auto reason_pos = text.find(" reason=");
        const auto object_pos = text.find(" object=");
        const auto field_pos = text.find(" field=0x");
        const auto class_pos = text.find(" class=0x");
        if (reason_pos == std::string::npos || object_pos == std::string::npos ||
            field_pos == std::string::npos || class_pos == std::string::npos)
            return out;
        // l25-fix (review F3): the guard always emits the four markers in
        // fixed order with room for each; bounds-check that ordering before
        // extracting so out-of-order/overlapping markers in a hostile or
        // legacy text return unparsed instead of feeding the substr
        // arithmetic a negative (underflowing) count.
        if (!(reason_pos + 8 <= object_pos && object_pos + 8 <= field_pos && field_pos + 9 <= class_pos))
            return out;
        using NativePropertyGuard::RefusalReason;
        const std::string reason = text.substr(reason_pos + 8, object_pos - (reason_pos + 8));
        if      (reason == "slot read")        out.reason = RefusalReason::SlotRead;
        else if (reason == "class null")       out.reason = RefusalReason::ClassNull;
        else if (reason == "class misaligned") out.reason = RefusalReason::ClassMisaligned;
        else if (reason == "class identity")   out.reason = RefusalReason::ClassIdentity;
        else return out;
        const auto parse_hex_digits = [](const std::string& digits) -> uintptr_t {
            uintptr_t value = 0;
            for (const char c : digits) {
                unsigned d;
                if      (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
                else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
                else break; // first non-hex char ends the value (next field or end)
                value = (value << 4) | d;
            }
            return value;
        };
        out.field_address = parse_hex_digits(text.substr(field_pos + 9, class_pos - (field_pos + 9)));
        out.class_pointer = parse_hex_digits(text.substr(class_pos + 9));
        out.parsed = true;
        return out;
    };
#endif
    FName GWorldName{};
    FName GLevelName{};
    FName GPackageName{};
    FName GPropertyName{};
    FName GFunctionName{};

    template <typename T>
    concept HasStaticAlignment_Private = requires(T t) {
        { t.StaticAlignment_Private };
    };

    template <typename T>
    concept HasValueOpsProperty = requires(T t) {
        { t.ValueOpsProperty };
    };

    auto TypeChecker::get_world_name() -> FName
    {
        return GWorldName;
    }

    auto TypeChecker::get_level_name() -> FName
    {
        return GLevelName;
    }

    auto TypeChecker::store_all_object_names() -> void
    {
        //@formatter:off
        // Misc
        GWorldName    =FName(STR("World"), FNAME_Find);
        GLevelName    =FName(STR("Level"), FNAME_Find);
        GPackageName  =FName(STR("Package"), FNAME_Find);
        GPropertyName =FName(STR("Property"), FNAME_Find);
        GFunctionName =FName(STR("Function"), FNAME_Find);
        //@formatter:on
    }

    auto TypeChecker::store_all_object_types() -> bool
    {
        NativeTypeScan::MissingKeys missing_keys;
#ifdef __PROSPERO__
        // l24: scan-lifetime refusal buffer (function scope so the post-scan
        // print sees it regardless of the engine-version branch taken)
        NativePropertyGuard::RefusalLog store_all_object_types_scan_refusals{};
        // l25: post-fill the scan-object name on refusal records a Checked
        // call appended during one guarded region. The Checked API records
        // before any object name is known (it has no name parameter), so the
        // scan driver stamps the name immediately afterwards.
        auto name_new_refusal_records = [](NativePropertyGuard::RefusalLog& log, int from_count, const char* name) {
            for (int i = from_count; i < log.count && i < NativePropertyGuard::RefusalLog::kCapacity; ++i) {
                auto& record = log.records[i];
                size_t j = 0;
                if (name) {
                    for (; j < sizeof(record.object_name) - 1 && name[j]; ++j) record.object_name[j] = name[j];
                }
                record.object_name[j] = '\0';
            }
        };
#endif
       //StaticFindObject_InternalSlow is used for compatability reasons.
        UObject::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Object")));
        UField::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Field")));
        UStruct::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Struct")));

        AActor::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.Actor")));
        UWorld::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.World")));

        UClass* enum_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Enum")));
        UEnum::StaticClassStorage = enum_ptr;

        UClass* user_defined_enum_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.UserDefinedEnum")));
        UUserDefinedEnum::StaticClassStorage = user_defined_enum_ptr;

        UClass* class_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Class")));
        UClass::StaticClassStorage = class_ptr;

        UClass* bp_generated_class_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.BlueprintGeneratedClass")));
        UBlueprintGeneratedClass::StaticClassStorage = bp_generated_class_ptr;

        UClass* widget_bp_generated_class_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/UMG.WidgetBlueprintGeneratedClass")));
        UWidgetBlueprintGeneratedClass::StaticClassStorage = widget_bp_generated_class_ptr;

        UClass* anim_bp_generated_class_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.AnimBlueprintGeneratedClass")));
        UAnimBlueprintGeneratedClass::StaticClassStorage = anim_bp_generated_class_ptr;

        UClass* canvas_panel_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/UMG.CanvasPanel")));
        UCanvasPanel::StaticClassStorage = canvas_panel_ptr;

        // Not available in 4.12 (I've not checked exactly when it starts being available)
        UClass* asset_data_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/AssetRegistry.AssetData")));
        if (!asset_data_ptr)
        {
            // In 4.26, they moved it from the 'AssetRegistry' package to the 'CoreUObject' package
            asset_data_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.AssetData")));
        }
        FAssetData::StaticClassStorage = asset_data_ptr;

        UPackage::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Package")));
        UInterface::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Interface")));
        UActorComponent::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.ActorComponent")));
        USceneComponent::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.SceneComponent")));
        UGameViewportClient::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.GameViewportClient")));
        UDataTable::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/Engine.UDataTable")));

        UFunction::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.Function")));
        UDelegateFunction::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.DelegateFunction")));

        if (Version::IsAtLeast(4, 23))
        {
            USparseDelegateFunction::StaticClassStorage = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.SparseDelegateFunction")));
        }

        UClass* script_struct_ptr = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, STR("/Script/CoreUObject.ScriptStruct")));
        UScriptStruct::StaticClassStorage = script_struct_ptr;

        /*
        FProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.Property"));
        FNumericProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.NumericProperty"));
        FInt8Property::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.Int8Property"));
        FInt16Property::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.Int16Property"));
        FIntProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.IntProperty"));
        FInt64Property::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.Int64Property"));
        FFloatProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.FloatProperty"));
        FDoubleProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.DoubleProperty"));
        FByteProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.ByteProperty"));
        FUInt16Property::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.UInt16Property"));
        FUInt32Property::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.UInt32Property"));
        FUInt64Property::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.UInt64Property"));
        FObjectProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.ObjectProperty"));
        FClassProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.ClassProperty"));
        FSoftClassProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.SoftClassProperty"));
        FEnumProperty<uint8_t>::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.EnumProperty"));
        FArrayProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.ArrayProperty"));
        FStructProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.StructProperty"));
        FNameProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.NameProperty"));
        XBoolProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.BoolProperty"));
        FMapProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.MapProperty"));
        FWeakObjectProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.WeakObjectProperty"));
        FLazyObjectProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.LazyObjectProperty"));
        FSoftObjectProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.SoftObjectProperty"));
        FTextProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.TextProperty"));
        FStrProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.StrProperty"));
        FInterfaceProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.InterfaceProperty"));
        FFieldPathProperty::m_static_obj = UObjectGlobals::static_find_object<UClass*>(nullptr, nullptr, STR("/Script/CoreUObject.FieldPathProperty"));
        //*/

        // FField / FProperty Types
        if (Version::IsAtLeast(4, 25))
        {
            auto find_all_property_types = [&](const StringType& obj_string) -> void {
#ifdef __PROSPERO__
                // l25: ASCII fold of the scan-object path, stamped onto every
                // refusal record this object produces.
                std::string object_name_ascii;
                for (const CharType c : obj_string)
                    if (c) object_name_ascii.push_back(static_cast<char>(c));
                // TypeChecker is a friend of FField; keep bootstrap access in this context.
                // The existing accessor uses the published map and checks the version.
                // l25: checked (recording) class read — a refused inner/key/value/
                // element class records and returns null instead of throwing, so
                // the population logic just skips that inner type and the scan
                // continues (the same containment the scan loop itself now has).
                auto NativeBootstrapFieldClass = [&](FField* field) -> FFieldClass* {
                    const int mark = store_all_object_types_scan_refusals.count;
                    FFieldClass* klass = NativePropertyGuard::ReadClassChecked(field, &store_all_object_types_scan_refusals);
                    name_new_refusal_records(store_all_object_types_scan_refusals, mark, object_name_ascii.c_str());
                    return klass;
                };
#endif
                UStruct* actor_obj = static_cast<UClass*>(UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, obj_string.c_str()));
                if (!actor_obj) { return; }

#ifdef __PROSPERO__
                // l25: per-object refusal containment. Any [PropertyPointer]
                // refusal thrown while scanning this object (e.g. a container
                // inner/key/value read that still goes through a throwing
                // wrapper) ends only this object's chain: it is recorded with
                // the object name and the reason/field/class parsed from the
                // guard's detail trailer, then this lambda returns and the
                // remaining objects still proceed — the same collect-and-
                // continue treatment missing keys get. Anything else (the
                // cyclic-chain refusal, non-guard errors) rethrows exactly as
                // in l24. The core-type gate at the end decides readiness.
                try {
#endif
                // Manually iterating fields here because 'ForEachProperty' isn't ready until after this function is done
                FField* field = actor_obj->GetChildProperties();
                std::vector<FField*> visited;
                while (field)
                {
                    if (std::find(visited.begin(), visited.end(), field) != visited.end())
                        throw std::runtime_error("[TypeScan] cyclic field chain refused");
                    visited.push_back(field);
                    // l25: the class read and the next-field traversal use the
                    // checked (noexcept, recording) guard API, so missing_keys
                    // .attempt no longer wraps the guard calls — a refusal is
                    // recorded instead of thrown. A refused class or next
                    // target ends this object's chain (next stays null); the
                    // other object scans still proceed. The legacy branch
                    // keeps the l24 collectable missing-key behavior.
                    FField* next = nullptr;
#ifdef __PROSPERO__
                    FFieldClass* ffield_class = nullptr;
                    {
                        const int refusal_mark = store_all_object_types_scan_refusals.count;
                        ffield_class = NativePropertyGuard::ReadClassChecked(field, &store_all_object_types_scan_refusals);
                        if (ffield_class)
                            next = NativePropertyGuard::ReadNextChecked(field, &store_all_object_types_scan_refusals);
                        name_new_refusal_records(store_all_object_types_scan_refusals, refusal_mark, object_name_ascii.c_str());
                    }
#else
                    missing_keys.attempt([&] {
                        next = field->GetNextFFieldUnsafe();
                    });
#endif
                    missing_keys.attempt([&] {
                    // The native bootstrap uses the already published member map; the legacy branch
                    // retains its original offset because FField::GetClass is not ready here.
#ifndef __PROSPERO__
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
                } // end try (per-object refusal containment; catch below)
#ifdef __PROSPERO__
                catch (const std::runtime_error& error)
                {
                    // l25: [PropertyPointer] refusals are recorded per object
                    // (with this object's name) and the chain ends here; the
                    // scan driver proceeds with the next object. Any other
                    // error rethrows (l24 semantics).
                    const std::string refusal_text(error.what());
                    if (refusal_text.rfind("[PropertyPointer]", 0) != 0) throw;
                    const auto detail = parse_property_pointer_refusal(refusal_text);
                    // l25-fix (review F3): a prefixed message without the
                    // parseable trailer (e.g. a legacy or unannotated guard
                    // refusal) is recorded with the explicit Unknown marker,
                    // not a fabricated SlotRead with invented addresses.
                    store_all_object_types_scan_refusals.record(
                        detail.parsed ? detail.reason : NativePropertyGuard::RefusalReason::Unknown,
                        detail.field_address, detail.class_pointer, object_name_ascii.c_str());
                }
#endif
            };

            // TODO: Maybe replace all of the calls to 'find_all_property_types' with a GUObjectArray iteration with an early exit once everything's been found ?

#ifdef __PROSPERO__
            // l25: RAII scan-mode — collection ends when the scan block exits
            // or unwinds, so an escaping exception (cyclic-chain refusal, an
            // uncaught [PropertyPointer] refusal) can no longer leave scan
            // mode active with a stale scan_log pointer.
            NativePropertyGuard::ScanCollectionScope scan_collection_scope(&store_all_object_types_scan_refusals);
#endif

            // Bool, Enum, Byte, Struct, Float, Object, Name, Int, Array
            // WeakObject, MulticastSparseDelegate
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.Actor")); });
            // Class
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.Pawn")); });
            // Str, Map
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.NetDriver")); });
            // Double
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.NetConnection")); });
            // Text
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.GameModeBase")); });
            // Int8, LazyObject
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Landscape.LandscapeComponent")); });
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/VariantManagerContent.VariantObjectBinding")); });
            // UInt16, UInt32, UInt64, Int8, Int16, Int64
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.IntSerialization")); });
            // Set, MulticastInlineDelegate, Delegate
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.AudioComponent")); });
            // MulticastInlineDelegate, Interface
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/MovieScene.MovieSceneSequencePlayer")); });
            // Delegate
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/UMG.Widget")); });
            // SoftObject
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.LevelStreaming")); });
            // SoftClass
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.PrimaryAssetTypeInfo")); });
            // FieldPath
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.InterpTrackInstProperty")); });
            missing_keys.attempt([&] { find_all_property_types(STR("/Script/MovieSceneTracks.MovieSceneEventPtrs")); });
            // TOptional
            if (Version::IsAtLeast(5, 4))
            {
                missing_keys.attempt([&] { find_all_property_types(STR("/Script/Engine.WorldPartitionRuntimeCellData")); });
            }
            // FUtf8StrProperty
            if (Version::IsAtLeast(5, 6))
            {
                missing_keys.attempt([&] { find_all_property_types(STR("/Script/CoreUObject.VerseEnum")); });
            }
            // FAnsiStrProperty
            if (Version::IsAtLeast(5, 7))
            {
                missing_keys.attempt([&] { find_all_property_types(STR("/Script/CoreUObject.VerseClass")); });
            }
        }
        else
        {
            // 4.24 and earlier, just use StaticFindObject to find all of the property type pointers

            auto add_property = []<typename PropertyType>(const CharType* full_property_name, [[maybe_unused]]PropertyType property_type_object) {
                UObject* property = UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, full_property_name);
                if (!property)
                {
                    // TODO: Put this error back when you've confirmed when every property type was introduced and you're only trying to find it in those versions and later
                    /*
                    StringType error = fmt::format(STR("Was unable to find pointer to '{}'"), full_property_name);
                    std::string error_ansi = std::string(error.begin(), error.end());
                    throw std::runtime_error{error_ansi};
                    //*/
                    return;
                }

                if constexpr (!std::is_same_v<PropertyType, int32_t>)
                {
                    PropertyType::StaticClassStorage = static_cast<UClass*>(property);
                    const auto cdo_as_prop = std::bit_cast<FProperty*>(static_cast<UClass*>(property)->GetClassDefaultObject().Get());
                    PropertyType::StaticValueSizeStorage = cdo_as_prop->GetSize();
                }
                auto attempt_to_set_size = []<typename T>(const CharType* property_name) {
                    if (T::StaticSize_Private > 0)
                    {
                        return;
                    }
                    // Casting UObject* to FProperty* is safe in <4.25
                    UObject* property_instance = UObjectGlobals::StaticFindObject_InternalSlow(nullptr, nullptr, property_name);
                    if (property_instance)
                    {
                        if (const auto Size = std::bit_cast<PropertyType*>(property_instance)->GetSize(); Size > 0)
                        {
                            T::StaticSize_Private = Size;
                        }
                        if constexpr (HasValueOpsProperty<T>)
                        {
                            if (!T::ValueOpsProperty)
                            {
                                T::ValueOpsProperty = std::bit_cast<FProperty*>(property_instance);
                            }
                        }
                        if constexpr (HasStaticAlignment_Private<T>)
                        {
                            try
                            {
                                if (const auto Alignment = std::bit_cast<PropertyType*>(property_instance)->GetMinAlignment(); Alignment > 0)
                                {
                                    T::StaticAlignment_Private = Alignment;
                                }
                            }
                            catch (...)
                            {
                                // We fallback to 'alignof(T)' later.
                            }
                        }
                    }
                };
                if constexpr (std::is_same_v<PropertyType, FTextProperty>)
                {
                    attempt_to_set_size.template operator()<FText>(STR("/Script/Engine.GameMode:DefaultPlayerName"));
                    attempt_to_set_size.template operator()<FText>(STR("/Script/Engine.TextRenderComponent:Text"));
                    attempt_to_set_size.template operator()<FText>(STR("/Script/UMG.EditableText:Text"));
                    attempt_to_set_size.template operator()<FText>(STR("/Script/Engine.SubtitleCue:Text"));
                    attempt_to_set_size.template operator()<FText>(STR("/Script/Engine.BPVariableDescription:Category"));
                    attempt_to_set_size.template operator()<FText>(STR("/Script/Engine.SpriteCategoryInfo:DisplayName"));
                }
                else if constexpr (std::is_same_v<PropertyType, FNameProperty>)
                {
                    // Actor:ActorHasTag.Tag is confirmed to exist in 4.13 all the way up to 5.6.
                    attempt_to_set_size.template operator()<FName>(STR("/Script/Engine.Actor:ActorHasTag.Tag"));
                }
            };

            add_property(STR("/Script/CoreUObject.ObjectProperty"), FObjectProperty{});
            // AssetObjectProperty & AssetClassProperty only exists in <4.18.
            add_property(STR("/Script/CoreUObject.AssetObjectProperty"), FAssetObjectProperty{});
            add_property(STR("/Script/CoreUObject.AssetClassProperty"), FAssetClassProperty{});
            add_property(STR("/Script/CoreUObject.NumericProperty"), FNumericProperty{});
            add_property(STR("/Script/CoreUObject.Int8Property"), FInt8Property{});
            add_property(STR("/Script/CoreUObject.Int16Property"), FInt16Property{});
            add_property(STR("/Script/CoreUObject.IntProperty"), FIntProperty{});
            add_property(STR("/Script/CoreUObject.Int64Property"), FInt64Property{});
            add_property(STR("/Script/CoreUObject.NameProperty"), FNameProperty{});
            add_property(STR("/Script/CoreUObject.FloatProperty"), FFloatProperty{});
            add_property(STR("/Script/CoreUObject.DoubleProperty"), FDoubleProperty{});
            add_property(STR("/Script/CoreUObject.ByteProperty"), FByteProperty{});
            add_property(STR("/Script/CoreUObject.UInt16Property"), FUInt16Property{});
            add_property(STR("/Script/CoreUObject.UInt32Property"), FUInt32Property{});
            add_property(STR("/Script/CoreUObject.UInt64Property"), FUInt64Property{});
            add_property(STR("/Script/CoreUObject.BoolProperty"), FBoolProperty{});
            add_property(STR("/Script/CoreUObject.ArrayProperty"), FArrayProperty{});
            add_property(STR("/Script/CoreUObject.MapProperty"), FMapProperty{});
            add_property(STR("/Script/CoreUObject.StructProperty"), FStructProperty{});
            add_property(STR("/Script/CoreUObject.ClassProperty"), FClassProperty{});
            add_property(STR("/Script/CoreUObject.SoftClassProperty"), FSoftClassProperty{});
            add_property(STR("/Script/CoreUObject.WeakObjectProperty"), FWeakObjectProperty{});
            add_property(STR("/Script/CoreUObject.LazyObjectProperty"), FLazyObjectProperty{});
            add_property(STR("/Script/CoreUObject.SoftObjectProperty"), FSoftObjectProperty{});
            if (Version::IsAtLeast(4, 15)) { add_property(STR("/Script/CoreUObject.EnumProperty"), FEnumProperty{}); }
            add_property(STR("/Script/CoreUObject.TextProperty"), FTextProperty{});
            add_property(STR("/Script/CoreUObject.StrProperty"), FStrProperty{});
            add_property(STR("/Script/CoreUObject.DelegateProperty"), FDelegateProperty{});
            add_property(STR("/Script/CoreUObject.MulticastDelegateProperty"), FMulticastDelegateProperty{});
            if (Version::IsAtLeast(4, 23)) { add_property(STR("/Script/CoreUObject.MulticastInlineDelegateProperty"), FMulticastInlineDelegateProperty{}); }
            if (Version::IsAtLeast(4, 23)) { add_property(STR("/Script/CoreUObject.MulticastSparseDelegateProperty"), FMulticastSparseDelegateProperty{}); }
            add_property(STR("/Script/CoreUObject.InterfaceProperty"), FInterfaceProperty{});

            // Not yet supported, only here for completion and to prevent crashes
            // Not all of these will be found right now, need to call this function with more UObjects that actually have these properties
            add_property(STR("/Script/CoreUObject.SetProperty"), FSetProperty{});

            // Find the root 'Property' FField. This may not be necessary but I'm leaving it here for now, might be useful later.
            add_property(STR("/Script/CoreUObject.Property"), FProperty{});
        }

#ifdef __PROSPERO__
        // l25: scan mode was already closed by the ScanCollectionScope
        // destructor at the end of the 4.25+ scan block (or during unwinding);
        // the pre-4.25 branch never opens it. Print all collected refusals,
        // then continue.
        if (store_all_object_types_scan_refusals.count > 0 || store_all_object_types_scan_refusals.dropped > 0) {
            Output::send(STR("[TypeScan] refusals collected={} dropped={}\n"), store_all_object_types_scan_refusals.count, store_all_object_types_scan_refusals.dropped);
            for (int i = 0; i < store_all_object_types_scan_refusals.count; ++i) {
                const auto& r = store_all_object_types_scan_refusals.records[i];
                // l26 fix (agent-ES, decoder-truncation assignment 2026-10-09):
                // the previous line passed ensure_str(reason_text) and
                // ensure_str(object_name) as wide args, and on PS5 ensure_str
                // widens through the broken short-wchar codec that consumes
                // every OTHER character — console evidence /tmp/dec1.log
                // 15:26 rendered 'slot read' as 'slot' and every scan-object
                // name as exactly HALF its length ('/Script/Engine.Actor' ->
                // '/Script/En'), which read like a decoder truncation but was
                // the render. Narrow-first doctrine (UnrealInitializer
                // send_narrow_diag, agent-EM 2026-10-09): format the line
                // NARROW (reason/object are narrow already; addresses are
                // numeric), widen ONCE through the explicit locale-free
                // codec, fall back to the old wide line on any failure.
                bool sent_narrow = false;
                try
                {
                    const std::string narrow_line = fmt::format(
                        "[TypeScan] refusal {}/{} reason={} object={} field=0x{:X} class=0x{:X}\n",
                        i + 1, store_all_object_types_scan_refusals.count + store_all_object_types_scan_refusals.dropped,
                        NativePropertyGuard::reason_text(r.reason),
                        r.object_name,
                        r.field_address, r.class_pointer);
                    Output::send<LogLevel::Warning>(Unicode::DecodeUtf8(narrow_line));
                    sent_narrow = true;
                }
                catch (...)
                {
                }
                if (!sent_narrow)
                {
                    Output::send<LogLevel::Warning>(STR("[TypeScan] refusal {}/{} reason={} object={} field=0x{:X} class=0x{:X}\n"),
                        i + 1, store_all_object_types_scan_refusals.count + store_all_object_types_scan_refusals.dropped,
                        ensure_str(NativePropertyGuard::reason_text(r.reason)),
                        ensure_str(r.object_name),
                        r.field_address, r.class_pointer);
                }
            }
            if (store_all_object_types_scan_refusals.dropped > 0)
                Output::send<LogLevel::Warning>(STR("[TypeScan] refusals-truncated stored={} total={}\n"),
                    store_all_object_types_scan_refusals.count, store_all_object_types_scan_refusals.count + store_all_object_types_scan_refusals.dropped);
        }
#endif
        for (const auto& key : missing_keys.values)
            Output::send<LogLevel::Warning>(STR("[TypeScan] missing-layout-key={}\n"), StringType(key.begin(),key.end()));
#ifdef __PROSPERO__
        // Core list is justified in REPORT.md against selected runtime and mod.
        bool core_ready = true;
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
        if (!core_ready) return false;
#endif

        if (FText::StaticSize() == 0)
        {
            FText::StaticSize_Private = sizeof(FText);
            Output::send<LogLevel::Warning>(STR("Was unable to detect FText size, using default: 0x{:X} bytes.\n"), FText::StaticSize());
        }
        else
        {
            Output::send(STR("FText size detected as 0x{:X} bytes.\n"), FText::StaticSize());
        }

        if (FName::StaticSize_Private == -1)
        {
            Output::send<LogLevel::Warning>(STR("Could not dynamically establish size of FName, defaulting to compile-time size: 0x{:X}\n"), sizeof(FName));
            FName::StaticSize_Private = sizeof(FName);
        }

        // Resolve the FSoftObjectPath layout now, while reflection is available, so the
        // reflection-based layout wins over the version-derived fallback for all later users
        {
            const auto& SoftObjectPathLayout = FSoftObjectPath::GetLayout();
            Output::send(STR("FSoftObjectPath layout: size 0x{:X}, SubPathString at 0x{:X}, {} shape, from {}\n"),
                         SoftObjectPathLayout.TotalSize,
                         SoftObjectPathLayout.SubPathOffset,
                         SoftObjectPathLayout.bUsesTopLevelAssetPath ? STR("FTopLevelAssetPath") : STR("FName"),
                         SoftObjectPathLayout.bResolvedFromReflection ? STR("reflection") : STR("version fallback"));
        }

        if (FName::StaticAlignment_Private == -1)
        {
            Output::send<LogLevel::Warning>(STR("Could not dynamically establish alignment of FName, defaulting to compile-time alignment: 0x{:X}\n"), alignof(FName));
            FName::StaticAlignment_Private = alignof(FName);
        }

        return true;
    }

    auto TypeChecker::is_property(FField* field) -> bool
    {
        UObject* object = std::bit_cast<UObject*>(field);
        return is_property(object);
    }

    auto TypeChecker::is_property(UObject* object) -> bool
    {
        UClass* obj_class = object->GetClassPrivate();
        if (!obj_class) { return false; }

        if (obj_class->GetNamePrivate() == GPropertyName) { return true; }

        UStruct* super_struct = obj_class->GetSuperStruct();
        while (super_struct)
        {
            if (super_struct->GetNamePrivate() == GPropertyName)
            {
                return true;
            }

            super_struct = super_struct->GetSuperStruct();
        }

        return false;
    }
}