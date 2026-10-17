
#include <cstdint>
#include <bit>
#include <cstring>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#define RC_UE_API
#define UE_FORCEINLINE_HINT inline
namespace RC::Unreal {
using int32 = int32_t;
struct UClass; struct FFieldClass;
struct alignas(8) UObject { uintptr_t reserved{}; UObject* GetOuterPrivate(); };
class FFieldVariant;
class FField {
public:
    unsigned char* owner{};
    auto GetOwner() -> FFieldVariant&;
    auto GetFFieldOwnerUnsafe() -> FFieldVariant;
    auto GetOwnerVariant() -> FFieldVariant;
    auto GetOutermostOwner() -> UObject*;
    void SetOwnerVariant(FField*);
    void SetOwnerVariant(UObject*);
    auto AsUFieldUnsafe() -> UObject* {return nullptr;}
};
namespace Version {
inline int major=5, minor=6;
bool IsAtLeast(int a,int b) {return major>a || (major==a && minor>=b);}
bool IsBelow(int a,int b) {return !IsAtLeast(a,b);}
}
class RC_UE_API FFieldVariant
    {
        static constexpr uintptr_t UObjectMask = 0x1;

        union FFieldObjectUnion
        {
            FField* Field;
            UObject* Object;
        } Container;

        /// <summary>
        /// Pre 5.3 only
        /// </summary>
        bool IsObject;
    public:
        FFieldVariant(FField* Field) : IsObject(false)
        {
            Container.Field = Field;
        }

        FFieldVariant(UObject* Object) : IsObject(true)
        {
            // UE5.3+ stores the owner kind in the pointer's low bit. Keep a
            // null owner null and preserve the legacy bool representation.
            Container.Object = Version::IsAtLeast(5, 3) && Object
                ? reinterpret_cast<UObject*>(reinterpret_cast<uintptr_t>(Object) | UObjectMask)
                : Object;
        }
    public:
        auto IsValid() -> bool
        {
            return Container.Object;
        }

        auto GetOwnerVariant() -> FFieldVariant;

        inline bool IsUObject() const
        {
            if (Version::IsAtLeast(5, 3)) {
                return reinterpret_cast<uintptr_t>(Container.Object) & UObjectMask;
            } else {
                return IsObject;
            }
        }

        auto ToField() -> FField*
        {
            if (!IsUObject())
            {
                return Container.Field;
            }
            else
            {
                return nullptr;
            }
        }

        /** FOR INTERNAL USE ONLY: Function that returns the owner as FField without checking if it's actually an FField */
        UE_FORCEINLINE_HINT FField* ToFieldUnsafe() const
        {
            return Container.Field;
        }
        /** FOR INTERNAL USE ONLY: Function that returns the owner as UObject without checking if it's actually a UObject */
        UE_FORCEINLINE_HINT UObject* ToUObjectUnsafe() const
        {
            return (UObject*)((uintptr_t)Container.Object & ~UObjectMask);
        }

        void* GetRawPointer() const
        {
            return Container.Field;
        }

        bool IsA(const UClass* InClass) const;
        bool IsA(const FFieldClass* InClass) const;
        template <typename T>
        bool IsA() const
        {
            static_assert(sizeof(T) > 0, "T must not be an incomplete type");
            return IsA(T::StaticClass());
        }

        template <typename T>
        T* Get() const
            {
                static_assert(sizeof(T) > 0, "T must not be an incomplete type");
                if (IsA(T::StaticClass()))
                {
                    if constexpr (std::is_base_of_v<UObject, T>)
                    {
                        return static_cast<T*>(ToUObjectUnsafe());
                    }
                    else
                    {
                        return static_cast<T*>(Container.Field);
                    }
                }
                return nullptr;
            }

        auto ToUObject() -> UObject*
        {
            if (IsUObject())
            {
                if (Version::IsAtLeast(5, 3)) {
                    return reinterpret_cast<UObject*>(reinterpret_cast<uintptr_t>(Container.Object) & ~UObjectMask);
                }
                else {
                    return Container.Object;
                }
            }
            else
            {
                return nullptr;
            }
        }

        void Set(FField*);
        void Set(UObject*);

        auto HashObject() -> size_t;

        static int32 StaticSize()
        {
            if (Version::IsAtLeast(5, 3))
            {
                return sizeof(FFieldVariant::Container);
            }
            else
            {
                return sizeof(FFieldVariant);
            }
        }
    };
auto FField::GetOwner() -> FFieldVariant& {return *reinterpret_cast<FFieldVariant*>(owner);}
inline UObject* expected_self{};
inline UObject* expected_outer{};
inline bool masked_self{};
UObject* UObject::GetOuterPrivate() {
    masked_self=this==expected_self;
    return masked_self ? expected_outer : nullptr;
}
auto FField::GetOwnerVariant() -> FFieldVariant
    {
        if (Version::IsBelow(4, 25))
        {
            return AsUFieldUnsafe()->GetOuterPrivate();
        }
        else
        {
            return GetFFieldOwnerUnsafe();
        }
    }
auto FField::GetFFieldOwnerUnsafe() -> FFieldVariant
    {
        if (Version::IsBelow(4, 25))
        {
            throw std::runtime_error("FField::Owner is not available in UE versions below 4.25");
        }
        // The version-aware wrapper retains its pre5.3 bool and is16 bytes,
        // but the native UE5.3+ member is only8 bytes. Never copy the adjacent
        // Next pointer into that legacy bool/padding.
        FFieldVariant owner{static_cast<FField*>(nullptr)};
        std::memcpy(std::addressof(owner), std::addressof(GetOwner()), FFieldVariant::StaticSize());
        return owner;
    }
UObject* FField::GetOutermostOwner()
    {
        auto CurrentVariant = GetOwnerVariant();

        while (CurrentVariant.IsValid() && !CurrentVariant.IsUObject())
        {
            CurrentVariant = CurrentVariant.GetOwnerVariant();
        }

        return CurrentVariant.ToUObject();
    }
void FField::SetOwnerVariant(FField* field)
    {
        auto& OwnerVariant = GetOwner();
        if (Version::IsBelow(4, 25))
        {
            // Safe because in <4.25 all fields inherit from UObject
            OwnerVariant.Set(std::bit_cast<UObject*>(field));
        }
        else
        {
            OwnerVariant.Set(field);
        }
    }
void FField::SetOwnerVariant(UObject* object)
    {
        auto& OwnerVariant = GetOwner();
        if (Version::IsBelow(4, 25))
        {
            // Safe because in <4.25 all fields inherit from UObject
            OwnerVariant.Set(std::bit_cast<FField*>(object));
        }
        else
        {
            OwnerVariant.Set(object);
        }
    }
auto FFieldVariant::GetOwnerVariant() -> FFieldVariant
    {
        if (!IsValid())
        {
            return FFieldVariant{static_cast<FField*>(nullptr)};
        }
        if (IsUObject())
        {
            return ToUObjectUnsafe()->GetOuterPrivate();
        }
        else
        {
            return Container.Field->GetOwnerVariant();
        }
    }
void FFieldVariant::Set(FField* Field)
    {
        if (Version::IsBelow(5, 3))
        {
            IsObject = false;
        }
        Container.Field = Field;
    }
void FFieldVariant::Set(UObject* Object)
    {
        if (Version::IsBelow(5, 3))
        {
            IsObject = true;
        }
        Container.Object = Version::IsAtLeast(5, 3) && Object
            ? reinterpret_cast<UObject*>(reinterpret_cast<uintptr_t>(Object) | UObjectMask)
            : Object;
    }
}
using namespace RC::Unreal;
static_assert(sizeof(FFieldVariant)==16); // retained legacy wrapper, not native size
bool check(bool ok,const char* detail) {
    if(!ok)std::cerr<<"FAIL "<<detail<<"\n";
    return ok;
}
int main(int argc,char**argv) {
    if(argc!=2)return 2;
    std::string mode=argv[1];
    const bool legacy=mode.starts_with("legacy_");
    Version::major=mode.starts_with("legacy24_")?4:5;
    Version::minor=mode.starts_with("legacy24_")?24:(legacy?2:6);
    UObject object{},outer{};
    FField field{};
    bool ok=true;
    if(mode.ends_with("construct_object")) {
        FFieldVariant v(&object);
        ok=check(v.IsUObject()&&v.ToUObject()==&object&&v.ToField()==nullptr,
                 "object constructor must preserve owner kind and pointer");
    } else if(mode.ends_with("construct_field")) {
        FFieldVariant v(&field);
        ok=check(!v.IsUObject()&&v.ToField()==&field&&v.ToUObject()==nullptr,
                 "field constructor must preserve owner kind and pointer");
    } else if(mode.ends_with("null_object")) {
        FFieldVariant v(static_cast<UObject*>(nullptr));
        ok=check(!v.IsValid()&&v.ToUObject()==nullptr,"null remains invalid");
    } else if(mode.ends_with("set_object_local")) {
        FFieldVariant v(&field);v.Set(&object);
        ok=check(v.IsUObject()&&v.ToUObject()==&object,"Set object retains kind");
    } else if(mode.ends_with("set_field_local")) {
        FFieldVariant v(&object);v.Set(&field);
        ok=check(!v.IsUObject()&&v.ToField()==&field,"Set field retains kind");
    } else if(mode=="native_set_object_adjacent"||mode=="native_set_field_adjacent"||
              mode=="native_wrapper_set_object"||mode=="native_wrapper_set_field") {
        struct alignas(8) NativeOwnerAndNext {uintptr_t owner,next;} native{0,0x1122334455667788ULL};
        auto& view=*reinterpret_cast<FFieldVariant*>(&native.owner);
        field.owner=reinterpret_cast<unsigned char*>(&native.owner);
        const bool object_kind=mode=="native_set_object_adjacent"||mode=="native_wrapper_set_object";
        if(mode=="native_wrapper_set_object")field.SetOwnerVariant(&object);
        else if(mode=="native_wrapper_set_field")field.SetOwnerVariant(&field);
        else if(object_kind)view.Set(&object);else view.Set(&field);
        auto wanted=object_kind ? reinterpret_cast<uintptr_t>(&object)|1 : reinterpret_cast<uintptr_t>(&field);
        ok=check(native.owner==wanted&&native.next==0x1122334455667788ULL,
                 "native Set must change only8 owner bytes, preserve Next sentinel");
    } else if(mode=="native_owner_read_exact8") {
        auto native=std::make_unique<uintptr_t>(reinterpret_cast<uintptr_t>(&object)|1);
        field.owner=reinterpret_cast<unsigned char*>(native.get());
        auto v=field.GetFFieldOwnerUnsafe();
        ok=check(v.IsUObject()&&v.ToUObject()==&object,"native exact8 read");
    } else if(mode=="legacy_owner_read16") {
        Version::minor=2;FFieldVariant stored(&object);
        field.owner=reinterpret_cast<unsigned char*>(&stored);
        auto v=field.GetFFieldOwnerUnsafe();
        ok=check(v.IsUObject()&&v.ToUObject()==&object,"legacy16 read retains bool");
    } else if(mode=="native_tagged_traversal") {
        auto tagged=reinterpret_cast<uintptr_t>(&object)|1;
        FFieldVariant v(reinterpret_cast<FField*>(tagged));
        expected_self=&object;expected_outer=&outer;
        auto parent=v.GetOwnerVariant();
        ok=check(masked_self&&parent.IsUObject()&&parent.ToUObject()==&outer,
                 "traversal must mask UObject tag before method call");
    } else if(mode=="legacy_traversal") {
        FFieldVariant v(&object);expected_self=&object;expected_outer=&outer;
        auto parent=v.GetOwnerVariant();
        ok=check(masked_self&&parent.IsUObject()&&parent.ToUObject()==&outer,
                 "legacy traversal uses untagged pointer");
    } else if(mode=="native_null_traversal") {
        FFieldVariant v(static_cast<FField*>(nullptr));
        auto parent=v.GetOwnerVariant();
        ok=check(!parent.IsValid(),"null owner traversal must not dereference");
    } else if(mode=="native_null_outermost") {
        auto native=std::make_unique<uintptr_t>(0);
        field.owner=reinterpret_cast<unsigned char*>(native.get());
        ok=check(field.GetOutermostOwner()==nullptr,"null field has no outermost object");
    } else if(mode=="transition_53") {
        Version::minor=3;FFieldVariant v(&object);
        ok=check(v.IsUObject()&&v.ToUObject()==&object&&FFieldVariant::StaticSize()==8,
                 "version5.3 tag transition");
    } else return 2;
    if(ok)std::cout<<"PASS "<<mode<<"\n";
    return ok?0:1;
}
