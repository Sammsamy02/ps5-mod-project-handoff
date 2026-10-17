#include <cstring>
#include <type_traits>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <UnrealCustom/CustomProperty.hpp>

namespace RC::Unreal
{
#ifdef __PROSPERO__
    // TU-local construction seam: only a freshly allocated runtime-owned
    // unique_ptr reaches this writer, never an engine/game array property.
    template<class OwnedProperty>
    static void SetOwnedArrayInner(const std::unique_ptr<OwnedProperty>& owner,FProperty* inner)
    {
        static_assert(std::is_base_of_v<CustomProperty,OwnedProperty>);
        const auto found=FArrayProperty::MemberOffsets.find(STR("Inner"));
        if(found==FArrayProperty::MemberOffsets.end())
            throw std::runtime_error("Tried getting member variable 'FArrayProperty::Inner' that doesn't exist in this engine version.");
        const int offset=found->second;
        if(!owner || offset<0 || static_cast<size_t>(offset)>sizeof(OwnedProperty) ||
           sizeof(inner)>sizeof(OwnedProperty)-static_cast<size_t>(offset))
            throw std::runtime_error("[PropertyPointer] custom array Inner exceeds owned allocation");
        std::memcpy(reinterpret_cast<std::byte*>(owner.get())+offset,&inner,sizeof(inner));
    }
#endif
    CustomProperty::CustomProperty(int32_t offset_internal, int32_t element_size)
    {
        // set_member_variable(MemberOffsets::XProperty_Offset_Internal, this, offset_internal);
        // set_member_variable(MemberOffsets::Property_PropertyFlags, -4, this, element_size);
        GetOffset_Internal() = offset_internal;
        GetElementSize() = element_size;
    }

    auto CustomProperty::construct(int32_t offset_internal, UClass* belongs_to_class, UClass* inner_class, int32_t element_size) -> std::unique_ptr<CustomProperty>
    {
        std::unique_ptr<CustomProperty> custom_property = std::make_unique<CustomProperty>(offset_internal, element_size);

        // set_member_variable(MemberOffsets::FField_Class, custom_property.get(), inner_class);

        if (Version::IsAtLeast(4, 25))
        {
            custom_property->SetClass(inner_class);
            // set_member_variable(MemberOffsets::FField_Owner, custom_property.get(), belongs_to_class);
            custom_property->SetOwnerVariant(belongs_to_class);
        }
        else
        {
            std::bit_cast<UObject*>(custom_property.get())->GetClassPrivate() = inner_class;
            // set_member_variable(MemberOffsets::UObject_OuterPrivate, custom_property.get(), belongs_to_class);
            std::bit_cast<UObject*>(custom_property.get())->GetOuterPrivate() = belongs_to_class;
        }

        return custom_property;
    }

    CustomArrayProperty::CustomArrayProperty(int32_t offset_internal, int32_t element_size) : CustomProperty(offset_internal, element_size)
    {
    }

    auto CustomArrayProperty::construct(int32_t offset_internal, FProperty* array_inner, int32_t element_size) -> std::unique_ptr<CustomProperty>
    {
        std::unique_ptr<CustomArrayProperty> custom_array_property = std::make_unique<CustomArrayProperty>(offset_internal, element_size);
        // set_member_variable(MemberOffsets::ArrayProperty_Inner, custom_array_property.get(), array_inner);
#ifdef __PROSPERO__
        SetOwnedArrayInner(custom_array_property,array_inner);
#else
        std::bit_cast<FArrayProperty*>(custom_array_property.get())->GetInner() = array_inner;
#endif

        return custom_array_property;
    }

    auto CustomArrayProperty::construct(int32_t offset_internal, UClass* belongs_to_class, UClass* inner_class, FProperty* array_inner, int32_t element_size)
            -> std::unique_ptr<CustomProperty>
    {
        // Create dummy property to act as the 'Inner' member variable for the ArrayProperty
        // Then we set the inner type for the array property as the 'FFieldClass' of this dummy property
        std::unique_ptr<CustomProperty> array_inner_owner = std::make_unique<CustomProperty>(0x0, element_size);
        CustomProperty* array_inner_property = array_inner_owner.get();
        // set_member_variable(MemberOffsets::FField_Class, array_inner_property, array_inner);
        array_inner_property->SetClass(array_inner);

        std::unique_ptr<CustomProperty> custom_array_property = CustomProperty::construct(offset_internal, belongs_to_class, inner_class, element_size);
        // set_member_variable(MemberOffsets::ArrayProperty_Inner, custom_array_property.get(), array_inner_property);
#ifdef __PROSPERO__
        SetOwnedArrayInner(custom_array_property,array_inner_property);
#else
        std::bit_cast<FArrayProperty*>(custom_array_property.get())->GetInner() = array_inner_property;
#endif
        // Preserve existing custom-inner lifetime only after publication; a
        // refused layout/bounds check leaves the allocation under RAII.
        array_inner_owner.release();

        return custom_array_property;
    }

    CustomStructProperty::CustomStructProperty(int32_t offset_internal, int32_t element_size) : CustomProperty(offset_internal, element_size)
    {
    }

    auto CustomStructProperty::construct(int32_t offset_internal, UScriptStruct* script_struct, int32_t element_size) -> std::unique_ptr<CustomProperty>
    {
        std::unique_ptr<CustomStructProperty> custom_struct_property = std::make_unique<CustomStructProperty>(offset_internal, element_size);
        // set_member_variable(MemberOffsets::StructProperty_Struct, custom_struct_property.get(), script_struct);
        std::bit_cast<FStructProperty*>(custom_struct_property.get())->GetStruct() = script_struct;

        return custom_struct_property;
    }
} // namespace RC::Unreal