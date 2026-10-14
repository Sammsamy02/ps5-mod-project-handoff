// Included inside RC::Unreal in the actual TypeChecker translation unit.
namespace NativePropertyGuard {
namespace {
    std::atomic<bool> enabled{false};
    ReadBytes read_bytes = nullptr;
    ImageClassRange image_class_range = nullptr;
    bool installed = false; // serialized initialization, never rewritten
    int layout_offset(const auto& map, const CharType* member, const char* full_name) {
        const auto it = map.find(member);
        if (it == map.end()) throw std::runtime_error(std::string("Tried getting member variable '")+full_name+"' that doesn't exist in this engine version.");
        if (it->second < 0) throw std::runtime_error("[PropertyPointer] negative layout offset refused");
        return it->second;
    }
    bool read(uintptr_t address,size_t length,void* bytes) {
        return enabled.load(std::memory_order_acquire) && address && length && length<=UINTPTR_MAX-address &&
            read_bytes(address,length,bytes) && enabled.load(std::memory_order_acquire);
    }
    template<class T> bool read_member(uintptr_t base,int offset,T& value) {
        uintptr_t address=0;
        return NativeTypeScan::add_address(base,offset,address) && read(address,sizeof(value),&value);
    }
    bool known_property_class(uintptr_t klass) {
        const int size=layout_offset(FFieldClass::MemberOffsets,STR("UEP_TotalSize"),"FFieldClass::UEP_TotalSize");
        if (!size || !enabled.load(std::memory_order_acquire) || !image_class_range(klass,size)) return false;
        FName name{}; uint64 flags=0;
        if(!read_member(klass,layout_offset(FFieldClass::MemberOffsets,STR("FName"),"FFieldClass::FName"),name) ||
           !read_member(klass,layout_offset(FFieldClass::MemberOffsets,STR("CastFlags"),"FFieldClass::CastFlags"),flags) ||
           (flags & static_cast<uint64>(EClassCastFlags::CASTCLASS_FProperty)) == 0) return false;
        // Supported engine property identities, independent of discovery order.
        const CharType* names[]={STR("Property"),STR("NumericProperty"),STR("ObjectPropertyBase"),
            STR("ObjectProperty"),STR("ObjectPtrProperty"),STR("ClassProperty"),STR("ClassPtrProperty"),
            STR("Int8Property"),STR("Int16Property"),STR("IntProperty"),STR("Int64Property"),
            STR("UInt16Property"),STR("UInt32Property"),STR("UInt64Property"),STR("ByteProperty"),
            STR("BoolProperty"),STR("FloatProperty"),STR("DoubleProperty"),STR("NameProperty"),
            STR("StrProperty"),STR("TextProperty"),STR("AnsiStrProperty"),STR("Utf8StrProperty"),
            STR("ArrayProperty"),STR("SetProperty"),STR("MapProperty"),STR("StructProperty"),STR("EnumProperty"),
            STR("SoftObjectProperty"),STR("SoftClassProperty"),STR("WeakObjectProperty"),STR("LazyObjectProperty"),
            STR("InterfaceProperty"),STR("DelegateProperty"),STR("MulticastDelegateProperty"),
            STR("MulticastInlineDelegateProperty"),STR("MulticastSparseDelegateProperty"),STR("FieldPathProperty"),STR("OptionalProperty")};
        for(const auto* known:names) { const FName candidate(known,FNAME_Find); if(candidate.GetComparisonIndex().ToUnstableInt()!=0 && name==candidate) return true; }
        return false;
    }
    bool property_readable(uintptr_t field) {
        const int size=layout_offset(FProperty::MemberOffsets,STR("UEP_TotalSize"),"FProperty::UEP_TotalSize");
        // Exact published base-property span; no inferred descendant size.
        if(size<=0 || size>4096) return false;
        unsigned char bytes[4096]; return read(field,static_cast<size_t>(size),bytes);
    }
}
void Install(ReadBytes reader,ImageClassRange image_range) {
    if(installed || !reader || !image_range) throw std::logic_error("[PropertyPointer] invalid/double guard installation");
    read_bytes=reader; image_class_range=image_range; installed=true;
    enabled.store(true,std::memory_order_release);
}
void Revoke() noexcept { enabled.store(false,std::memory_order_release); }
FFieldClass* ReadClass(const FField* field) {
    const int offset=layout_offset(FField::MemberOffsets,STR("ClassPrivate"),"FField::ClassPrivate");
    uintptr_t klass=0;
    if(!property_readable(reinterpret_cast<uintptr_t>(field)) ||
       !read_member(reinterpret_cast<uintptr_t>(field),offset,klass) || !klass || klass%alignof(void*) || !known_property_class(klass))
        throw std::runtime_error("[PropertyPointer] unreadable field or unknown property FField class refused");
    return reinterpret_cast<FFieldClass*>(klass);
}
FField* ReadNext(const FField* field) {
    const int offset=layout_offset(FField::MemberOffsets,STR("Next"),"FField::Next");
    // Never call an engine getter before validating this node. A missing
    // traversal key remains the specific collectable layout exception.
    ReadClass(field);
    uintptr_t next=0;
    if(!read_member(reinterpret_cast<uintptr_t>(field),offset,next))
        throw std::runtime_error("[PropertyPointer] unreadable next-field slot refused");
    if(!next) return nullptr;
    if(next==reinterpret_cast<uintptr_t>(field) || next%alignof(void*))
        throw std::runtime_error("[PropertyPointer] invalid next-field target refused");
    auto* target=reinterpret_cast<FField*>(next);
    ReadClass(target);
    return target;
}
FProperty* ReadPointer(const void* owner,int offset,const char* key) {
    const int class_offset=layout_offset(FField::MemberOffsets,STR("ClassPrivate"),"FField::ClassPrivate");
    const auto target=NativeTypeScan::checked_pointer(reinterpret_cast<uintptr_t>(owner),offset,class_offset,
        [](uintptr_t a,size_t n,void* out) {return read(a,n,out);},known_property_class);
    if(!target || !property_readable(target)) throw std::runtime_error(std::string("[PropertyPointer] unreadable/null/foreign target refused: ")+key);
    return reinterpret_cast<FProperty*>(target);
}
}
