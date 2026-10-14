// Included inside RC::Unreal in the actual TypeChecker translation unit.
// l24: bootstrap-safe class check during scan + detailed refusal collection.
namespace NativePropertyGuard {
namespace {
    std::atomic<bool> enabled{false};
    ReadBytes read_bytes = nullptr;
    ImageClassRange image_class_range = nullptr;
    bool installed = false; // serialized initialization, never rewritten

    // l24: scan-mode state (active only between BeginScanCollection/EndScanCollection)
    std::atomic<bool> scan_mode{false};
    RefusalLog* scan_log = nullptr; // owned by the scan caller, not by the guard

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
    bool property_readable(uintptr_t field) {
        const int size=layout_offset(FProperty::MemberOffsets,STR("UEP_TotalSize"),"FProperty::UEP_TotalSize");
        if(size<=0 || size>4096) return false;
        unsigned char bytes[4096]; return read(field,static_cast<size_t>(size),bytes);
    }
    bool read_class_fname(uintptr_t klass, FName& name) {
        return read_member(klass, layout_offset(FFieldClass::MemberOffsets, STR("FName"), "FFieldClass::FName"), name);
    }
    // Does the FName's display text end with "Property"? Resolved through
    // the already-verified pool (FName::ToString), no new interning.
    bool fname_ends_in_property(const FName& name) {
        // Fast rejection: the display string of any FFieldClass ending in
        // "Property" is at least 9 chars ("Property" = 8 + 1 for any prefix).
        // We use ToString to resolve through the verified pool. The string
        // may be ANSI or wide depending on the build; handle both.
        const auto& display = name.ToString();
        const auto* data = display.data();
        const auto len = display.size();
        if (len < 8) return false;
        // Check the last 8 characters for "Property" (case-sensitive, engine convention)
        if constexpr (sizeof(decltype(*data)) == 1) {
            // ANSI string
            return len >= 8 && std::memcmp(data + (len - 8), "Property", 8) == 0;
        } else {
            // Wide string
            static const wchar_t suffix[] = L"Property";
            for (size_t i = 0; i < 8; ++i) {
                if (data[len - 8 + i] != suffix[i]) return false;
            }
            return true;
        }
    }
    // l24: full known-class check (post-scan). Image range + CastFlags + exact name.
    // This is the l23 known_property_class, unchanged in semantics.
    bool KnownPropertyClassFull(uintptr_t klass) {
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
    // l24: bootstrap class check (during scan). Accepts readable heap FFieldClass
    // objects whose FName ends in "Property" or matches a known name. No
    // CastFlags requirement (classes may still be initializing). Uses read_bytes
    // (which admits heap regions), not image_class_range (image-only).
    bool KnownPropertyClassBootstrap(uintptr_t klass) {
        if (!enabled.load(std::memory_order_acquire)) return false;
        // Readability: the class must be readable through the proven backend.
        const int size = layout_offset(FFieldClass::MemberOffsets, STR("UEP_TotalSize"), "FFieldClass::UEP_TotalSize");
        if (!size || size > 4096) return false;
        unsigned char probe[64]; // read the first cache line to confirm readability
        if (!read(klass, size < 64 ? static_cast<size_t>(size) : 64, probe)) return false;
        FName name{};
        if (!read_class_fname(klass, name)) return false;
        // Accept if the name ends in "Property" (suffix heuristic) or is a
        // known FField class name (exact match, no interning side effects).
        if (fname_ends_in_property(name)) return true;
        const CharType* names[] = {STR("Property"),STR("NumericProperty"),STR("ObjectPropertyBase"),
            STR("ObjectProperty"),STR("ObjectPtrProperty"),STR("ClassProperty"),STR("ClassPtrProperty"),
            STR("Int8Property"),STR("Int16Property"),STR("IntProperty"),STR("Int64Property"),
            STR("UInt16Property"),STR("UInt32Property"),STR("UInt64Property"),STR("ByteProperty"),
            STR("BoolProperty"),STR("FloatProperty"),STR("DoubleProperty"),STR("NameProperty"),
            STR("StrProperty"),STR("TextProperty"),STR("AnsiStrProperty"),STR("Utf8StrProperty"),
            STR("ArrayProperty"),STR("SetProperty"),STR("MapProperty"),STR("StructProperty"),STR("EnumProperty"),
            STR("SoftObjectProperty"),STR("SoftClassProperty"),STR("WeakObjectProperty"),STR("LazyObjectProperty"),
            STR("InterfaceProperty"),STR("DelegateProperty"),STR("MulticastDelegateProperty"),
            STR("MulticastInlineDelegateProperty"),STR("MulticastSparseDelegateProperty"),STR("FieldPathProperty"),STR("OptionalProperty")};
        for (const auto* known : names) {
            const FName candidate(known, FNAME_Find);
            if (candidate.GetComparisonIndex().ToUnstableInt() != 0 && name == candidate) return true;
        }
        return false;
    }
    // l24: dispatch to the appropriate check based on scan mode.
    bool known_property_class(uintptr_t klass) {
        return scan_mode.load(std::memory_order_acquire)
            ? KnownPropertyClassBootstrap(klass)
            : KnownPropertyClassFull(klass);
    }
    // l24: detailed refusal classification (which check failed).
    struct ClassRefusalDetail {
        RefusalReason reason;
        uintptr_t class_value;
    };
    ClassRefusalDetail classify_class_refusal(uintptr_t field, int class_offset) {
        uintptr_t klass = 0;
        if (!property_readable(reinterpret_cast<uintptr_t>(field)) ||
            !read_member(reinterpret_cast<uintptr_t>(field), class_offset, klass)) {
            return {RefusalReason::SlotRead, 0};
        }
        if (!klass) return {RefusalReason::ClassNull, 0};
        if (klass % alignof(void*)) return {RefusalReason::ClassMisaligned, klass};
        return {RefusalReason::ClassIdentity, klass};
    }
    void record_refusal(RefusalLog* log, RefusalReason reason, uintptr_t field, uintptr_t klass, const char* name) noexcept {
        if (!log) return;
        if (log->count >= RefusalLog::kCapacity) { ++log->dropped; return; }
        auto& r = log->records[log->count++];
        r.reason = reason;
        r.field_address = field;
        r.class_pointer = klass;
        if (name) {
            size_t i = 0;
            for (; i < sizeof(r.object_name) - 1 && name[i]; ++i) r.object_name[i] = name[i];
            r.object_name[i] = '\0';
        } else {
            r.object_name[0] = '\0';
        }
    }
}
// l24: RefusalLog::record implementation (declared in hpp, defined here for the TU)
void RefusalLog::record(RefusalReason r, uintptr_t field, uintptr_t klass, const char* name) noexcept {
    record_refusal(this, r, field, klass, name);
}
void BeginScanCollection(RefusalLog* log) noexcept {
    scan_log = log;
    scan_mode.store(true, std::memory_order_release);
}
void EndScanCollection() noexcept {
    scan_mode.store(false, std::memory_order_release);
    scan_log = nullptr;
}
bool ScanModeActive() noexcept {
    return scan_mode.load(std::memory_order_acquire);
}
void Install(ReadBytes reader,ImageClassRange image_range) {
    if(installed || !reader || !image_range) throw std::logic_error("[PropertyPointer] invalid/double guard installation");
    read_bytes=reader; image_class_range=image_range; installed=true;
    enabled.store(true,std::memory_order_release);
}
void Revoke() noexcept { enabled.store(false,std::memory_order_release); }

// ---- l24: checked (noexcept) versions ----
FFieldClass* ReadClassChecked(const FField* field, RefusalLog* log) noexcept {
    const int offset = layout_offset(FField::MemberOffsets, STR("ClassPrivate"), "FField::ClassPrivate");
    const auto detail = classify_class_refusal(reinterpret_cast<uintptr_t>(field), offset);
    if (detail.reason != RefusalReason::SlotRead && detail.reason != RefusalReason::ClassNull &&
        detail.reason != RefusalReason::ClassMisaligned) {
        // The slot was readable; the class pointer is valid-shaped; check identity.
        if (!known_property_class(detail.class_value)) {
            if (log) record_refusal(log, RefusalReason::ClassIdentity,
                                    reinterpret_cast<uintptr_t>(field), detail.class_value, nullptr);
            return nullptr;
        }
        return reinterpret_cast<FFieldClass*>(detail.class_value);
    }
    if (log) record_refusal(log, detail.reason, reinterpret_cast<uintptr_t>(field), detail.class_value, nullptr);
    return nullptr;
}
FField* ReadNextChecked(const FField* field, RefusalLog* log) noexcept {
    const int offset = layout_offset(FField::MemberOffsets, STR("Next"), "FField::Next");
    if (!ReadClassChecked(field, log)) return nullptr; // validate this node first
    uintptr_t next = 0;
    if (!read_member(reinterpret_cast<uintptr_t>(field), offset, next)) {
        if (log) record_refusal(log, RefusalReason::SlotRead, reinterpret_cast<uintptr_t>(field), 0, nullptr);
        return nullptr;
    }
    if (!next) return nullptr; // end of chain, not a refusal
    if (next == reinterpret_cast<uintptr_t>(field) || next % alignof(void*)) {
        if (log) record_refusal(log, RefusalReason::ClassMisaligned, reinterpret_cast<uintptr_t>(field), next, nullptr);
        return nullptr;
    }
    if (!ReadClassChecked(reinterpret_cast<FField*>(next), log)) return nullptr;
    return reinterpret_cast<FField*>(next);
}
FProperty* ReadPointerChecked(const void* owner, int offset, const char* key, RefusalLog* log) noexcept {
    const int class_offset = layout_offset(FField::MemberOffsets, STR("ClassPrivate"), "FField::ClassPrivate");
    const auto target = NativeTypeScan::checked_pointer(reinterpret_cast<uintptr_t>(owner), offset, class_offset,
        [](uintptr_t a, size_t n, void* out) { return read(a, n, out); }, known_property_class);
    if (!target || !property_readable(target)) {
        if (log) record_refusal(log, RefusalReason::SlotRead, reinterpret_cast<uintptr_t>(owner), 0, key);
        return nullptr;
    }
    return reinterpret_cast<FProperty*>(target);
}

// ---- throwing wrappers (unchanged behavior for post-scan callers) ----
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
