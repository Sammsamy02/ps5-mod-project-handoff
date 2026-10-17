// Included inside RC::Unreal in the actual TypeChecker translation unit.
// l24: bootstrap-safe class check during scan + detailed refusal collection.
// l25: known-name index compare first in the bootstrap check, verified-pool
// index bound before any ToString, refusal detail in every thrown message.
namespace NativePropertyGuard {
namespace {
    std::atomic<bool> enabled{false};
    ReadBytes read_bytes = nullptr;
    ImageClassRange image_class_range = nullptr;
    bool installed = false; // serialized initialization, never rewritten

    // l24: scan-mode state (active only between BeginScanCollection/EndScanCollection)
    std::atomic<bool> scan_mode{false};
    RefusalLog* scan_log = nullptr; // owned by the scan caller, not by the guard

    // l25: FName pool address verified by the SeedArming admission round.
    // Single-shot publication (first wins, like Install). The layout words
    // below mirror rc_seed::kPoolLayout (SeedValidation.hpp), frozen engine
    // facts already pinned for this title: CurrentBlock u32 at +0x8,
    // CurrentByteOffset u32 at +0xC, id >> 16 = block index (16 offset bits,
    // 8192 max blocks), entry stride 2 bytes.
    std::atomic<uintptr_t> verified_pool_address{0};
    constexpr uint32_t kPoolCurrentBlockOffset = 0x8;
    constexpr uint32_t kPoolCurrentByteOffsetOffset = 0xC;
    constexpr uint32_t kPoolBlockOffsetBits = 16;
    constexpr uint32_t kPoolMaxBlocks = 8192;
    constexpr uint32_t kPoolEntryStride = 2;

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
    // l25: uppercase hex append (dependency-free; used by refusal_detail).
    char* append_hex(char* out, uintptr_t value) {
        char digits[2 * sizeof(uintptr_t)];
        int n = 0;
        do {
            const unsigned d = static_cast<unsigned>(value & 0xFu);
            digits[n++] = static_cast<char>(d < 10 ? '0' + d : 'A' + d - 10);
            value >>= 4;
        } while (value);
        while (n) *out++ = digits[--n];
        return out;
    }
    // l25: stable, parseable detail trailer appended to every throwing
    // refusal. reason uses the reason_text() harvest vocabulary; object is
    // the layout key when the wrapper knows one, "-" otherwise. The scan's
    // per-object catch parses reason/field/class back out of this trailer
    // and records them with the scan-object name it owns.
    std::string refusal_detail(RefusalReason reason, uintptr_t field, uintptr_t klass, const char* object) {
        char field_hex[2 * sizeof(uintptr_t) + 1];
        char class_hex[2 * sizeof(uintptr_t) + 1];
        char* f = append_hex(field_hex, field); *f = '\0';
        char* c = append_hex(class_hex, klass); *c = '\0';
        std::string detail(" reason=");
        detail += reason_text(reason);
        detail += " object=";
        detail += (object && object[0]) ? object : "-";
        detail += " field=0x";
        detail += field_hex;
        detail += " class=0x";
        detail += class_hex;
        return detail;
    }
    // l25: pre-computed comparison indices for the 39 known FField class
    // names. Resolved once via FNAME_Find (lookup only, no interning side
    // effects); index 0 = never interned and never matches a real name.
    // Comparison afterwards is a pure integer compare per name.
    struct KnownClassIndices { uint32_t values[39]; };
    const KnownClassIndices& known_property_class_indices() {
        static const KnownClassIndices cached = [] {
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
            static_assert(sizeof(names) / sizeof(names[0]) == 39);
            KnownClassIndices out{};
            for (size_t i = 0; i < 39; ++i)
                out.values[i] = FName(names[i], FNAME_Find).GetComparisonIndex().ToUnstableInt();
            return out;
        }();
        return cached;
    }
    bool known_property_class_index_match(const FName& name) {
        const uint32_t id = name.GetComparisonIndex().ToUnstableInt();
        if (id == 0) return false;
        for (const uint32_t known : known_property_class_indices().values)
            if (known != 0 && known == id) return true;
        return false;
    }
    // l25: bound the FName comparison index by the verified pool before any
    // ToString (which dereferences a pool entry). The pool address is the
    // SeedArming-verified one; the current block / cursor words are re-read
    // live through the proven reader so entries interned after admission are
    // still bounded correctly. Unpublished address (host fixtures, pre-arming)
    // keeps the l24 suffix behavior; unreadable words fail closed (refuse the
    // suffix path) rather than risk an out-of-pool ToString.
    bool fname_index_within_verified_pool(const FName& name) {
        const uintptr_t pool = verified_pool_address.load(std::memory_order_acquire);
        if (!pool) return true; // unpublished: l24 behavior
        uint32_t current_block = 0, current_byte_offset = 0;
        if (!read(pool + kPoolCurrentBlockOffset, sizeof(current_block), &current_block) ||
            !read(pool + kPoolCurrentByteOffsetOffset, sizeof(current_byte_offset), &current_byte_offset))
            return false;
        if (current_block >= kPoolMaxBlocks) return false; // not initialized / corrupt
        const uint32_t id = name.GetComparisonIndex().ToUnstableInt();
        const uint32_t block = id >> kPoolBlockOffsetBits;
        const uint32_t offset = (id & ((1u << kPoolBlockOffsetBits) - 1u)) * kPoolEntryStride;
        if (block > current_block) return false; // beyond the open block
        // Suffix test needs the entry header (2 bytes) + 8 suffix characters.
        // l25-fix note (review F2): this margin assumes an ANSI suffix —
        // 2 (entry header) + 8 ("Property" ANSI bytes). A wide-character
        // entry whose display name ends in L"Property" needs 2 + 16 bytes
        // and is NOT covered by this bound; the bound is therefore
        // necessary-not-sufficient, consistent with the admission doctrine
        // in SeedValidation's check_entry_bounds.
        if (block == current_block && offset + 2u + 8u > current_byte_offset) return false;
        return true;
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
    // l25 order: (a) exact known-name comparison first — pure index compares
    // against pre-computed indices, no FName lookup and no ToString for the
    // common case; (b) only then the suffix heuristic, and only after the
    // FName index is bounded by the verified pool, so an out-of-pool index
    // can never reach the pool dereference inside ToString.
    bool KnownPropertyClassBootstrap(uintptr_t klass) {
        if (!enabled.load(std::memory_order_acquire)) return false;
        // Readability: the class must be readable through the proven backend.
        const int size = layout_offset(FFieldClass::MemberOffsets, STR("UEP_TotalSize"), "FFieldClass::UEP_TotalSize");
        if (!size || size > 4096) return false;
        unsigned char probe[64]; // read the first cache line to confirm readability
        if (!read(klass, size < 64 ? static_cast<size_t>(size) : 64, probe)) return false;
        FName name{};
        if (!read_class_fname(klass, name)) return false;
        // (a) known-name index match: no lookup, no ToString.
        if (known_property_class_index_match(name)) return true;
        // (b) suffix heuristic, gated by the verified-pool index bound.
        if (!fname_index_within_verified_pool(name)) return false;
        return fname_ends_in_property(name);
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
// l25: single-shot verified-pool-address publication (SeedArming, after the
// pool admission round). First call wins; later calls are ignored so a stale
// or foreign address can never replace the verified one.
void PublishVerifiedPoolAddress(uintptr_t pool_address) noexcept {
    uintptr_t expected = 0;
    verified_pool_address.compare_exchange_strong(expected, pool_address,
        std::memory_order_release, std::memory_order_relaxed);
}
void Install(ReadBytes reader,ImageClassRange image_range) {
    if(installed || !reader || !image_range) throw std::logic_error("[PropertyPointer] invalid/double guard installation");
    read_bytes=reader; image_class_range=image_range; installed=true;
    enabled.store(true,std::memory_order_release);
}
void Revoke() noexcept { enabled.store(false,std::memory_order_release); }

// ---- l24: checked (noexcept) versions ----
// l25-fix (review F1): layout_offset throws on a missing or negative layout
// member — directly in these wrappers and transitively through
// classify_class_refusal (FProperty::UEP_TotalSize) and known_property_class
// (FFieldClass::UEP_TotalSize / FFieldClass::FName). Escaping a noexcept
// wrapper would std::terminate the scan. Each wrapper therefore wraps its
// whole body (covering every layout_offset call it can reach) in a catch
// that records LayoutMissing and returns the failure sentinel, restoring
// the l24 collect-and-continue degradation.
FFieldClass* ReadClassChecked(const FField* field, RefusalLog* log) noexcept {
    try {
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
    } catch (const std::runtime_error&) {
        if (log) record_refusal(log, RefusalReason::LayoutMissing,
                                reinterpret_cast<uintptr_t>(field), 0, nullptr);
        return nullptr;
    }
}
FField* ReadNextChecked(const FField* field, RefusalLog* log) noexcept {
    try {
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
    } catch (const std::runtime_error&) {
        if (log) record_refusal(log, RefusalReason::LayoutMissing,
                                reinterpret_cast<uintptr_t>(field), 0, nullptr);
        return nullptr;
    }
}
FProperty* ReadPointerChecked(const void* owner, int offset, const char* key, RefusalLog* log) noexcept {
    try {
        const int class_offset = layout_offset(FField::MemberOffsets, STR("ClassPrivate"), "FField::ClassPrivate");
        const auto target = NativeTypeScan::checked_pointer(reinterpret_cast<uintptr_t>(owner), offset, class_offset,
            [](uintptr_t a, size_t n, void* out) { return read(a, n, out); }, known_property_class);
        if (!target || !property_readable(target)) {
            if (log) record_refusal(log, RefusalReason::SlotRead, reinterpret_cast<uintptr_t>(owner), 0, key);
            return nullptr;
        }
        return reinterpret_cast<FProperty*>(target);
    } catch (const std::runtime_error&) {
        if (log) record_refusal(log, RefusalReason::LayoutMissing,
                                reinterpret_cast<uintptr_t>(owner), 0, key);
        return nullptr;
    }
}

// ---- throwing wrappers ----
// l25: behavior is unchanged (same refusal conditions as l23/l24), but every
// thrown message now ends with the parseable reason/object/field/class
// trailer from refusal_detail() so a fatal terminal line and the scan's
// per-object catch both see exactly which check refused what.
FFieldClass* ReadClass(const FField* field) {
    const int offset=layout_offset(FField::MemberOffsets,STR("ClassPrivate"),"FField::ClassPrivate");
    const auto detail=classify_class_refusal(reinterpret_cast<uintptr_t>(field),offset);
    if(detail.reason==RefusalReason::ClassIdentity && known_property_class(detail.class_value))
        return reinterpret_cast<FFieldClass*>(detail.class_value);
    throw std::runtime_error(std::string("[PropertyPointer] unreadable field or unknown property FField class refused")+
        refusal_detail(detail.reason,reinterpret_cast<uintptr_t>(field),detail.class_value,nullptr));
}
FField* ReadNext(const FField* field) {
    const int offset=layout_offset(FField::MemberOffsets,STR("Next"),"FField::Next");
    ReadClass(field);
    uintptr_t next=0;
    if(!read_member(reinterpret_cast<uintptr_t>(field),offset,next))
        throw std::runtime_error(std::string("[PropertyPointer] unreadable next-field slot refused")+
            refusal_detail(RefusalReason::SlotRead,reinterpret_cast<uintptr_t>(field),0,nullptr));
    if(!next) return nullptr;
    if(next==reinterpret_cast<uintptr_t>(field) || next%alignof(void*))
        throw std::runtime_error(std::string("[PropertyPointer] invalid next-field target refused")+
            refusal_detail(RefusalReason::ClassMisaligned,reinterpret_cast<uintptr_t>(field),next,nullptr));
    auto* target=reinterpret_cast<FField*>(next);
    ReadClass(target);
    return target;
}
FProperty* ReadPointer(const void* owner,int offset,const char* key) {
    const int class_offset=layout_offset(FField::MemberOffsets,STR("ClassPrivate"),"FField::ClassPrivate");
    const auto target=NativeTypeScan::checked_pointer(reinterpret_cast<uintptr_t>(owner),offset,class_offset,
        [](uintptr_t a,size_t n,void* out) {return read(a,n,out);},known_property_class);
    if(!target || !property_readable(target)) throw std::runtime_error(std::string("[PropertyPointer] unreadable/null/foreign target refused")+
        refusal_detail(RefusalReason::SlotRead,reinterpret_cast<uintptr_t>(owner),0,key));
    return reinterpret_cast<FProperty*>(target);
}
}
