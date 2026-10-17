#pragma once
#include <cstddef>
#include <cstdint>
namespace RC::Unreal { class FField; class FFieldClass; class FProperty;
namespace NativePropertyGuard {

using ReadBytes = bool (*)(uintptr_t, size_t, void*);
using ImageClassRange = bool (*)(uintptr_t, size_t);

// ---- l24: refusal taxonomy (exact strings for harvest) ----
enum class RefusalReason : int {
    SlotRead = 0,       // field slot or property span unreadable
    ClassNull = 1,      // class pointer is null
    ClassMisaligned = 2,// class pointer not pointer-aligned
    ClassIdentity = 3,  // class readable but not a recognized property class
};
inline constexpr const char* reason_text(RefusalReason r) noexcept {
    switch (r) {
    case RefusalReason::SlotRead:        return "slot read";
    case RefusalReason::ClassNull:       return "class null";
    case RefusalReason::ClassMisaligned: return "class misaligned";
    case RefusalReason::ClassIdentity:   return "class identity";
    }
    return "?";
}

// ---- l24: refusal collection (bounded, scan-lifetime) ----
struct RefusalRecord {
    RefusalReason reason;
    uintptr_t field_address;
    uintptr_t class_pointer;
    char object_name[64]; // truncated scan-object name
};
struct RefusalLog {
    static constexpr int kCapacity = 50;
    RefusalRecord records[kCapacity];
    int count = 0;
    int dropped = 0; // refusals past capacity
    void record(RefusalReason r, uintptr_t field, uintptr_t klass, const char* name) noexcept;
};

// ---- l24: scan-mode control ----
// During the type scan, the bootstrap class check is used (readable heap
// + FName suffix "Property" or known FField class name). After the scan,
// the full check applies (image range + CastFlags + exact name match).
void BeginScanCollection(RefusalLog* log) noexcept;
void EndScanCollection() noexcept;
bool ScanModeActive() noexcept;

// Install only after image identity and layout publication. Callbacks have
// process lifetime; no owner captured. Revoke before teardown. No re-install.
void Install(ReadBytes, ImageClassRange);
void Revoke() noexcept;

// Throwing versions (post-scan or non-scan callers — unchanged from l23).
FProperty* ReadPointer(const void* owner, int offset, const char* key);
FFieldClass* ReadClass(const FField* field);
FField* ReadNext(const FField* field);

// ---- l24: checked (noexcept) versions for scan-time collect-and-continue ----
// Return nullptr on refusal and record the reason; never throw.
FProperty* ReadPointerChecked(const void* owner, int offset, const char* key, RefusalLog* log) noexcept;
FFieldClass* ReadClassChecked(const FField* field, RefusalLog* log) noexcept;
FField* ReadNextChecked(const FField* field, RefusalLog* log) noexcept;

}
}
