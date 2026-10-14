
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

}

using namespace RC::Unreal;
FProperty* inner(const std::unique_ptr<CustomProperty>&p){FProperty*v=nullptr;std::memcpy(&v,reinterpret_cast<const char*>(p.get())+120,8);return v;}
int main(){FArrayProperty::MemberOffsets={{"Inner",120}};CustomProperty element(0,8);auto a=CustomArrayProperty::construct(4,&element,8);assert(inner(a)==&element&&a->GetOffset_Internal()==4&&a->GetElementSize()==8);
 UClass owner,klass;auto b=CustomArrayProperty::construct(4,&owner,&klass,&element,8);auto*dummy=inner(b);assert(dummy&&dummy!=&element&&dummy->GetElementSize()==8);delete static_cast<CustomProperty*>(dummy);
 FArrayProperty::MemberOffsets.clear();bool rejected=false;try{CustomArrayProperty::construct(4,&element,8);}catch(const std::runtime_error&){rejected=true;}assert(rejected&&FArrayProperty::MemberOffsets.empty());
 FArrayProperty::MemberOffsets={{"Inner",int(sizeof(CustomProperty)-4)}};rejected=false;try{CustomArrayProperty::construct(4,&owner,&klass,&element,8);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 FArrayProperty::MemberOffsets={{"Inner",-1}};rejected=false;try{CustomArrayProperty::construct(4,&element,8);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 std::cout<<"actual custom array constructors: 5 assertions passed\n";
}
