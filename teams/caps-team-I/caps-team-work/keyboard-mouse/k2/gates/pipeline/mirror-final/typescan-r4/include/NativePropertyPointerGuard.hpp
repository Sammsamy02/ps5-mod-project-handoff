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
    // l25-fix (review F1/F3): appended WITHOUT renumbering the values above.
    LayoutMissing = 4,  // layout member missing/negative at scan time (caught inside Checked wrappers)
    Unknown = 5,        // [PropertyPointer] message without a parseable detail trailer (legacy marker)
};
inline constexpr const char* reason_text(RefusalReason r) noexcept {
    switch (r) {
    case RefusalReason::SlotRead:        return "slot read";
    case RefusalReason::ClassNull:       return "class null";
    case RefusalReason::ClassMisaligned: return "class misaligned";
    case RefusalReason::ClassIdentity:   return "class identity";
    case RefusalReason::LayoutMissing:   return "layout missing";
    case RefusalReason::Unknown:         return "unknown";
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

// ---- l25: RAII scan-mode scope ----
// Begins collection on construction and ends it on destruction, so an
// exception escaping the scan body (cyclic-chain refusal, an uncaught
// [PropertyPointer] refusal) cannot leave scan mode active with a dangling
// scan_log pointer. Not copyable, not movable; one scope per scan.
class ScanCollectionScope {
public:
    explicit ScanCollectionScope(RefusalLog* log) noexcept { BeginScanCollection(log); }
    ~ScanCollectionScope() { EndScanCollection(); }
    ScanCollectionScope(const ScanCollectionScope&) = delete;
    ScanCollectionScope& operator=(const ScanCollectionScope&) = delete;
};

// ---- l25: verified FName pool extent ----
// SeedArming publishes the pool address once, after the admission round that
// verified the pool shape (CurrentBlock/Blocks[0]/'None' anchor). The guard
// uses it only to bound-check a FName comparison index (current block /
// cursor words are re-read live through the installed reader) BEFORE any
// ToString suffix test, so an out-of-pool index can never reach the pool
// dereference inside ToString. Single-shot: the first publication wins,
// exactly like Install. While unpublished (host fixtures, pre-arming), the
// suffix test keeps its l24 behavior.
void PublishVerifiedPoolAddress(uintptr_t pool_address) noexcept;

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
// l25-fix (review F1): the layout_offset lookups these wrappers make
// (directly and through the classification/identity helpers) can throw on
// a missing or negative layout member; each Checked wrapper catches that,
// records LayoutMissing and returns nullptr, so nothing can escape the
// noexcept boundary and terminate the scan.
FProperty* ReadPointerChecked(const void* owner, int offset, const char* key, RefusalLog* log) noexcept;
FFieldClass* ReadClassChecked(const FField* field, RefusalLog* log) noexcept;
FField* ReadNextChecked(const FField* field, RefusalLog* log) noexcept;

}
}
