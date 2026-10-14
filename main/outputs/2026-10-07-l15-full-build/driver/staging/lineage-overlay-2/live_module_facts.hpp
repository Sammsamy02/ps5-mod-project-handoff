// live_module_facts.hpp
//
// Typed RETAINED LIVE-MODULE FACTS SUPPLIER contract for the PS5 UE4SS port.
//
// Goal (handoff NEXT_ACTIONS.txt item 2): establish, inside the live game
// process, image identity, mapped segment correspondence, sizes, lifetime and
// valid API capabilities — from APIs that genuinely exist in the pinned
// ps5-payload-sdk v0.43, without inventing signatures, without guessing
// addresses, and without relabeling dlopen(NULL) (payload root) as the game
// executable.
//
// IN-GAME UPDATE (checkpoint zcode_ingame_v1_results, 2026-10-04):
//   * KERN_PROC_VMMAP is queried FIRST and is the PRIMARY mapping source;
//   * image identity is derived from VMMAP spans + the in-memory ELF
//     header/program-header scan (IdentitySource::VmScan) whenever the
//     link-map walk yields no anchor — the observed in-game case (DT_DEBUG
//     absent/unfilled, exec-handle raw 0, module-info domain mismatch);
//   * on the VmScan route there is no loader name to retain, so retention is
//     structurally degraded (kind None) — never a fake reference;
//   * the rtld name route keeps its original stricter semantics (a failed
//     retain still yields zero facts).
//
// Layering:
//   live_module_facts.hpp/.cpp   generic core: validation, retention state
//                                machine, capability recording. NO OS calls,
//                                NO firmware constants, NO title names.
//   live_module_os_ops.hpp       host-injectable OS boundary (ops table).
//   live_module_os_ops_sce.cpp   the only TU that binds pinned SDK symbols.
//   live_module_base_scan.hpp/.cpp  in-memory ELF base scan (vendored V2
//                                logic; used by the SCE scan_image_base op).
//
// Firmware-specific facts (ABI sizes, allowances, version) arrive exclusively
// through LiveFirmwareProfile data supplied by the caller. Title-specific
// facts (which file name is the game executable) arrive exclusively through
// the profile's matcher or an explicit capture() name.
//
// The generic core NEVER dereferences a reported address: segment addresses
// and mapping ranges are treated as integers (span-validated reads live
// behind the scan_image_base op only). No allocation, no exceptions, no
// threads in capture paths. All output storage is caller-owned fixed-size POD.
#ifndef LMF_LIVE_MODULE_FACTS_HPP
#define LMF_LIVE_MODULE_FACTS_HPP

#include <cstddef>
#include <cstdint>

#include "live_module_base_scan.hpp"
#include "sce_module_abi.hpp"

namespace lmf {

enum class Status : uint32_t {
    Ok = 0,
    InvalidArgument,        // null ops/callbacks/output, bad profile
    NoImageAnchor,          // no link-map match, no explicit image name, no
                            // VMMAP scan identity (VMMAP absent or needs-base)
    RetainFailed,           // RTLD_NOLOAD probe returned null (no fake facts)
    ModuleInfoNotObtained,  // degraded: no bridged handle (see capabilities);
                            // VmScan-identity captures land here too
    ModuleInfoRejected,     // kernel returned data violating pinned ABI rules
    NotCaptured,            // accessors called before successful capture
    AlreadyReleased,
    RetentionProbeFailed,
};

// Retention model. LoaderReference is a genuine dynamic-loader reference
// (dlopen(name, RTLD_NOLOAD) held; dlclose on release) — a real refcount on
// an already-loaded object, never a load of new bytes. HandleValidated is not
// implemented by the pinned SCE ops because sceKernelIsModuleLoaded has no
// pinned signature; the enum value documents the future verified variant.
enum class RetainKind : uint8_t { None = 0, LoaderReference, HandleValidated };

// How image identity was established for this capture.
enum class IdentitySource : uint8_t {
    None = 0,    // not yet captured / no identity
    LinkMap = 1, // rtld link-map name (or caller-supplied name anchor)
    VmScan = 2,  // VMMAP spans + in-memory ELF base scan (primary in-game)
};

// Bounded capacities for fixed-size snapshots (data, not firmware facts).
static constexpr size_t kMaxLinkMapEntries = 128;
// Raised 512 -> 1024 by the in-game update: the observed in-game VMMAP run
// reports 1252 entries (g5); the snapshot keeps 1024 and records honest
// truncation, exactly like the in-game probe's retained-span bound.
// launch10-final (pool fix, worker-fname-heap-runtime-bounds core hunk):
// 1024 -> 16384. Observed live walks ~10,019 records; heap mappings sit at
// record ~9,500 — at 1024 the capture truncates BEFORE the heap and the
// pool validator must refuse ('snapshot truncated'), failing a valid pool.
// 16384 covers observed walks with margin; the core query buffer becomes
// process-lifetime static (384KiB BSS, not stack). Consumers all fresh units.
static constexpr size_t kMaxVmMappings = 32768;
static constexpr size_t kMaxName = 256;
static constexpr size_t kMaxPath = 1024;
// launch10-c (2026-10-05, worker-anchor-cap-analysis): 8 -> 16. Best-known
// igf3 fragmentation needs 7 pre-filter records; worst plausible (LOAD8
// materialized + RELRO splits + 4-way LOAD7 fragmentation) reaches 11-14.
// At 8, overflow keeps the LOWEST records and silently drops the GUA-hosting
// tail span [0x970c000,0x9a34000) — the exact degradation launch-10 tests.
static constexpr size_t kMaxAnchorMappings = 16;

// One observed kernel-reported mapping. Integers only; never dereferenced.
struct VmMapping {
    uint64_t start;
    uint64_t end;  // exclusive, checked for representability
    int32_t protection;  // opaque reported value; not interpreted
};

// Observed outcome per API probe. Values are facts about this capture only.
enum class ProbeResult : uint8_t { NotBound = 0, Failed, Ok };

struct CapabilitySet {
    ProbeResult pid;                 // getpid
    ProbeResult executable_path;     // sysctl KERN_PROC_PATHNAME
    ProbeResult link_map_walk;       // DT_DEBUG route (refused in-game: g2)
    ProbeResult loader_retain;       // dlopen(name, RTLD_NOLOAD)
    ProbeResult module_info;         // sceKernelGetModuleInfo (pinned ABI)
    ProbeResult vmentry;             // sysctl KERN_PROC_VMMAP (primary: g5)
    ProbeResult exec_handle;         // sceKernelGetExecutableModuleHandle:
                                     // explicitly-disabled (in-game g1 raw 0);
                                     // NotBound only when the slot is null
    ProbeResult vm_scan;             // VMMAP + in-memory ELF base scan
    bool module_info_degraded;       // true when ModuleInfoNotObtained
    bool vm_mappings_truncated;      // more mappings existed than capacity
};

// Authoritative module info per pinned prosperodev ABI (352-byte struct).
// Obtained==false means NOT observed — never fabricated.
struct ModuleInfoFacts {
    static constexpr uint8_t kSourceNone = 0;
    static constexpr uint8_t kSourceExecHandle = 1;
    static constexpr uint8_t kSourceLoaderHandleCast = 2;
    bool obtained;
    uint8_t handle_source;  // which bridge produced the handle (audit trail)
    sce::ModuleHandle handle;
    char name[kMaxName];        // verified nonempty NUL-terminated
    uint8_t fingerprint[20];    // opaque reported bytes, not identity proof
    uint32_t segment_count;     // 1..4 when obtained
    sce::ModuleSegment segments[4];  // addresses are integers, never read
    uint64_t image_span_begin;  // min segment address (computed, checked)
    uint64_t image_span_end;    // max exclusive end (computed, checked)
};

// Per-segment correspondence between reported module segments and observed
// process mappings (sysctl KERN_PROC_VMMAP). Coverage counts mappings whose
// range intersects the segment; full coverage additionally requires one
// mapping to contain the whole segment.
struct SegmentCorrespondence {
    uint32_t segment_index;
    uint32_t covering_mappings;   // 0 == uncorroborated (recorded honestly)
    bool fully_covered;
};

struct RetentionFacts {
    RetainKind kind;
    void *loader_handle;          // owned reference when LoaderReference
    char retained_name[kMaxName]; // exact name passed to the retain op
};

// VMMAP + in-memory ELF scan identity evidence (copied from the provider's
// VmScanResult by the core; every field is observation, nothing guessed).
struct VmScanFacts {
    uint32_t candidate_count;     // candidates enumerated from the spans
    int32_t provider_code;        // scan op return code (0 == completed)
    bool chosen;                  // a main-image candidate was chosen
    uint64_t chosen_base;         // span-validated ELF header address
    uint64_t chosen_extent;       // max(p_vaddr+p_memsz) over PT_LOADs
    uint64_t chosen_dyn_addr;     // resolved dynamic address under the mode
    int32_t chosen_dyn_mode;      // 1 dyn@base+vaddr, 2 dyn@absolute vaddr
    uint16_t chosen_e_type;       // raw e_type of the chosen image
    char provenance[24];          // "v2_elf_base_scan" | "needs-base"
};

// Final typed snapshot for the controller to convert into
// RC::Platform::NativeLoaderFacts (see REPORT.md contract section).
struct LiveModuleFactsSnapshot {
    Status status;
    int32_t provider_code;        // last nonzero OS return, else 0
    int32_t pid;                  // 0 when probe failed
    char executable_path[kMaxPath];  // observed sysctl path; empty if absent
    char image_name[kMaxName];       // anchor identity actually used (name route)
    bool image_name_from_link_map;
    void *observed_base;             // rtld-reported base OR VmScan chosen base
    IdentitySource identity_source;  // which route produced the identity
    RetentionFacts retention;
    ModuleInfoFacts module_info;
    VmScanFacts vm_scan;             // VMMAP+scan evidence (primary in-game)
    size_t segment_correspondence_count;
    SegmentCorrespondence segment_correspondence[4];
    size_t anchor_mapping_count;     // mappings containing observed_base
    VmMapping anchor_mappings[kMaxAnchorMappings];
    CapabilitySet capabilities;
};

// Firmware profile: the ONLY place firmware-specific facts may enter. Built
// by profile code (or tests), never hardcoded inside the generic core.
struct LiveFirmwareProfile {
    uint32_t module_info_abi_size;  // 352 for the pinned prosperodev ABI
    uint32_t module_segment_capacity;  // 4 for the pinned ABI
    uint32_t fw_version;            // 0 == unknown (never guessed here)
    // Whether the dynamic-loader handle may be reinterpreted as a
    // SceKernelModule handle for sceKernelGetModuleInfo. FALSE until a real
    // target experiment proves the two handle domains coincide (in-game g4
    // observed 0x7FFFFFFF on list handles: still unresolved, stays false).
    bool allow_loader_handle_as_module_handle;
    // Suffix matcher for identifying the game executable image among rtld
    // link-map entries, e.g. "eboot.bin". Empty suffix matches nothing; an
    // explicit capture(name) bypasses the matcher entirely.
    char exe_name_suffix[64];
};

// OS boundary (see live_module_os_ops.hpp). Generic core never touches the
// SDK directly.
struct LiveModuleOsOps;

class RetainedLiveModuleFactsSupplier final {
public:
    // ops must outlive the supplier (borrowed, never stored beyond calls).
    RetainedLiveModuleFactsSupplier(const LiveModuleOsOps &ops,
                                    LiveFirmwareProfile profile) noexcept;
    ~RetainedLiveModuleFactsSupplier();  // releases any held reference

    RetainedLiveModuleFactsSupplier(const RetainedLiveModuleFactsSupplier &) = delete;
    RetainedLiveModuleFactsSupplier &operator=(const RetainedLiveModuleFactsSupplier &) = delete;
    RetainedLiveModuleFactsSupplier(RetainedLiveModuleFactsSupplier &&) noexcept;
    RetainedLiveModuleFactsSupplier &operator=(RetainedLiveModuleFactsSupplier &&) noexcept;

    // One bounded capture pass, VMMAP-first. explicit_image_name (optional)
    // overrides the profile matcher; it must name an ALREADY LOADED object —
    // RTLD_NOLOAD never loads new bytes. When the link-map walk yields no
    // anchor (in-game: refused, g2) and a scan_image_base op is bound, the
    // identity comes from the VMMAP span + in-memory ELF scan instead.
    Status capture(const char *explicit_image_name = nullptr) noexcept;

    // Accessors are valid after capture(); facts().status records the outcome.
    const LiveModuleFactsSnapshot &facts() const noexcept;

    // Re-probe the retained reference: a second RTLD_NOLOAD must succeed and
    // return a consistent handle (immediately balanced with dlclose).
    // VmScan-identity captures never own a reference: AlreadyReleased.
    Status validate_retention() noexcept;

    // Release the owned loader reference exactly once. Idempotent.
    Status release() noexcept;

    const LiveFirmwareProfile &profile() const noexcept { return profile_; }

private:
    void reset_retention() noexcept;
    const LiveModuleOsOps &ops_;
    LiveFirmwareProfile profile_{};
    LiveModuleFactsSnapshot facts_{};
    bool captured_{false};
    bool released_{false};
};

}  // namespace lmf

#endif  // LMF_LIVE_MODULE_FACTS_HPP
