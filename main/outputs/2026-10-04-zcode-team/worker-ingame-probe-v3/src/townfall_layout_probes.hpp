// VENDORED-ORIGIN: outputs/2026-10-04-zcode-team/worker-5-layout-validator/src/townfall_layout_probes.hpp
// Team 2026-10-04 worker-5-layout-validator (ZCode). Byte-logic preserved except this
// provenance header. Original file untouched. License/scope: internal team artifacts.

// townfall_layout_probes.hpp
//
// Read-only IN-PROCESS LAYOUT VALIDATION PREDICATES (layout-DATA gate).
// Team: 2026-10-04-zcode-team / worker-5-layout-validator (ZCode).
//
// Purpose: elevate worker-2 profile layout CANDIDATES toward VALIDATED facts,
// using worker-1 live-module facts {image_base, readable mappings}. Pure
// functions ONLY: no OS calls, no allocation, no exceptions, no threads, no
// console/network. All memory access flows through caller-supplied MemSpan
// views so the same code runs in host tests (synthetic images) and later
// inside the game module (real mapped spans).
//
// PROVENANCE OF EVERY CONSTANT (nothing guessed):
//   - GUObjectArray vaddr 0x93ba110, UGameEngine::Tick vaddr 0x2fac8c0,
//     StaticConstructObject vaddr 0x490aa0:
//       outputs/2026-10-04-zcode-team/worker-2-layout-facts/profiles/
//       townfall-ps5-ue56.profile.json (facts GUObjectArray.guobject_original,
//       GameEngineTick.gameengine_tick, UObject.static_construct_object),
//       corroborated by survey stdout lines in
//       .superpowers/native-dependency-spike/ue4ss-native-file-core-survey-1/
//       run-release-1/stdout.txt lines 2, 8, 9. The predicates take vaddrs as
//       ARGUMENTS from the profile; they hardcode no address.
//   - FChunkedFixedUObjectArray member offsets (both layout hypotheses):
//       .superpowers/native-dependency-spike/native-coherent-runtime-1/
//       source/port/unreal/generated_include/FunctionBodies/
//         5_00_MemberVariableLayout_DefaultSetter_TUObjectArray.cpp
//         5_06_MemberVariableLayout_DefaultSetter_TUObjectArray.cpp  (identical)
//         5_00_MemberVariableLayout_DefaultSetter_FUObjectArray.cpp
//         5_06_MemberVariableLayout_DefaultSetter_FUObjectArray.cpp
//       (TUObjectArray: Objects+0x0, PreAllocatedObjects+0x8, MaxElements+0x10,
//        NumElements+0x14, MaxChunks+0x18, NumChunks+0x1C, UEP_TotalSize 0x20;
//        FUObjectArray outer: ObjFirstGCIndex+0x0, ObjLastNonGCIndex+0x4,
//        MaxObjectsNotConsideredByGC+0x8, OpenForDisregardForGC+0xC,
//        ObjObjects+0x10 embedded TUObjectArray.)
//       Engine is 5.6 per profile identity.engine_major/minor; TUObjectArray
//       offsets are identical across all retained 5_00..5_08 setters, and
//       FUObjectItem::UEP_TotalSize is 0x18 across 5.x (5_06 adds only the
//       RefCount accessor at +0x14 inside the same 0x18 size).
//   - NumElementsPerChunk 64*1024 and the [512] chunk-pointer bound:
//       .../source/port/unreal/include/Unreal/UObjectArray.hpp lines 88
//       (NumElementsPerChunk = 64 * 1024) and the generated header wrapper
//       Members "Chunks_Type = UObjectBase**[512]".
//   - The 0x93ba110 label is the FUObjectArray-shaped singleton per the port
//     (GUObjectArray is `FUObjectArray*`), but the profile's unresolved_list
//     says member offsets were never resolved FOR THE BUILD. The probe
//     therefore tests BOTH retained 5.x hypotheses honestly:
//       Layout A: label -> FUObjectArray outer (GC indices at +0x0/+0x4,
//                 embedded TUObjectArray at +0x10)
//       Layout B: label -> TUObjectArray directly (Objects at +0x0)
//     and reports per-layout verdicts plus which one matched.
//
// VERDICT DISCIPLINE (honest evidence):
//   Validated    = every predicate in the chain passed under exactly one
//                  layout hypothesis (this elevates candidate -> validated
//                  FOR THE MAPPED SNAPSHOT ONLY; lifetime/identity remain
//                  separate worker-1 facts).
//   Refuted      = a hard contradiction: bytes encode something the cited
//                  layout forbids (non-canonical/dangling pointer, insane
//                  counts, GC-order violation, null/duplicate/overlapping/
//                  unreadable needed chunk, capacity mismatch).
//   Inconclusive = evidence unavailable (unreadable header, unverifiable
//                  chunk extent) or ambiguity (both layouts pass). NEVER
//                  treated as failure or success.
//
//   Code-shape probes (UGameEngine::Tick, StaticConstructObject) are
//   INCONCLUSIVE-CLASSIFIER ONLY: readable first bytes are matched against a
//   conservative x86-64 frame-setup byte grammar. They can NEVER return
//   Validated (no callable-ABI claim is in scope) and never Refuted (a
//   shape mismatch is recorded as negative evidence for the controller, not
//   a refutation; the retained iced_x86 CFG decoder in
//   .superpowers/native-dependency-spike/ue4ss-native-tick-relocation-
//   resolver-1/crate/src/cfg_disassemble.rs is the deeper technique and is
//   NOT reimplemented here).
//
//   FName pool: NO PREDICATE, by rule. The profile's FName family is
//   candidate-only/absent and the P2 rule (see worker-2 profile
//   plans_of_record + unresolved_list: "FName storage width and
//   FNamePool/FNameEntry layout: ... entirely unresolved") forbids name-pool
//   heuristics. kFNamePoolPredicateExcluded documents this as data.

#ifndef TOWNFALL_LAYOUT_PROBES_HPP
#define TOWNFALL_LAYOUT_PROBES_HPP

#include <cstddef>
#include <cstdint>

namespace tlp {

// ---------------------------------------------------------------------------
// Memory views
// ---------------------------------------------------------------------------

// One contiguous readable region backed by bytes. Later in-process this is a
// live mapping (start = mapping address, data = direct pointer into it); in
// host tests it is a synthetic image. Spans may be supplied unsorted and may
// not overlap (overlaps are never created by worker-1 vmmap snapshots).
struct MemSpan {
    uint64_t start;        // inclusive
    uint64_t size;         // bytes; end = start + size (exclusive)
    const uint8_t* data;   // host pointer to the bytes; never dereferenced
                           // beyond [data, data+size)
};

// ---------------------------------------------------------------------------
// Verdicts and rationale codes (stable, machine-readable)
// ---------------------------------------------------------------------------

enum class Verdict : uint8_t {
    NotProbed = 0,
    Validated = 1,
    Inconclusive = 2,
    Refuted = 3,
};

enum class RCode : uint16_t {
    OK = 0,

    // GUObjectArray family — Inconclusive-class (0x01xx)
    GOA_E_ADDR_UNREADABLE          = 0x1001, // header bytes not in any span
    GOA_E_CHUNK_EXTENT_UNVERIFIABLE= 0x1002, // needed chunk bytes only partially covered
    GOA_E_LAYOUT_AMBIGUOUS         = 0x1003, // both layouts pass all hard checks

    // GUObjectArray family — Refute-class (0x11xx)
    GOA_R_PTR_NONCANONICAL         = 0x1101, // pointer misaligned or non-canonical x86-64
    GOA_R_PTR_OUTSIDE_READABLE     = 0x1102, // non-null pointer not inside any span
    GOA_R_PTRS_BOTH_NULL           = 0x1103, // Objects and PreAllocatedObjects both null
    GOA_R_COUNTS_INSANE            = 0x1104, // 0<Num<=Max, MaxChunks in [1,512],
                                             // NumChunks<=MaxChunks,
                                             // Max<=MaxChunks*PerChunk violated
    GOA_R_GC_ORDER                 = 0x1105, // layout A: !(0<=FirstGC<=LastNonGC<=Max)
    GOA_R_CHUNK_NULL_WHILE_NEEDED  = 0x1106, // chunk[i]==null for i<ceil(Num/PerChunk)
    GOA_R_CHUNK_UNREADABLE         = 0x1107, // needed chunk pointer outside all spans
    GOA_R_CHUNK_DUPLICATE          = 0x1108, // two needed chunks share one address
    GOA_R_CHUNK_OVERLAP            = 0x1109, // two needed chunk windows overlap
    GOA_R_CAPACITY_MISMATCH        = 0x110A, // NumElements > NumChunks*PerChunk

    // Code-shape family — Inconclusive-class only (0x2xxx), never Validated
    CODE_E_UNREADABLE              = 0x2001, // entry bytes not in any span
    CODE_I_PROLOGUE_LIKE           = 0x2101, // first bytes decode as frame setup
    CODE_I_NOT_PROLOGUE_LIKE       = 0x2102, // first bytes are padding/body shape
    CODE_I_PRE_PADDING             = 0x2103, // supporting signal only: bytes before
                                             // entry are 0xCC/0x90 filler
};

const char* verdict_string(Verdict v);
const char* rcode_string(RCode c);  // stable short name, e.g. "GOA_R_GC_ORDER"

// ---------------------------------------------------------------------------
// Profile vaddr set (worker-2 facts, consumed as ARGUMENTS, never embedded)
// ---------------------------------------------------------------------------

struct TownfallProfileVaddrs {
    uint64_t guobject_array;        // 0x93ba110 per profile
    uint64_t ugameengine_tick;      // 0x2fac8c0 per profile
    uint64_t static_construct_object; // 0x490aa0 per profile
};

// FName-pool exclusion, recorded as data so downstream tooling sees the rule.
inline constexpr bool kFNamePoolPredicateExcluded = true;
inline constexpr const char* kFNamePoolExclusionReason =
    "P2 rule: FName storage width and FNamePool/FNameEntry layout are "
    "entirely unresolved for this build (worker-2 profile unresolved_list); "
    "name-pool heuristics are excluded and the construction gap is its own "
    "fact.";

// ---------------------------------------------------------------------------
// GUObjectArray probe
// ---------------------------------------------------------------------------

enum class GuaLayout : uint8_t {
    None = 0,
    FUObjectArray_Outer = 1,  // label -> FUObjectArray (GC indices + ObjObjects)
    TUObjectArray_Direct = 2, // label -> TUObjectArray directly
};

// What the probe actually observed (fixed-size POD, caller-owned).
struct GuaObserved {
    GuaLayout layout_matched;         // None when not Validated
    int32_t obj_first_gc_index;       // layout A only, else -1
    int32_t obj_last_non_gc_index;    // layout A only, else -1
    int32_t max_elements;
    int32_t num_elements;
    int32_t max_chunks;
    int32_t num_chunks;
    uint64_t objects_ptr;             // TUObjectArray.Objects under matched layout
    uint64_t preallocated_objects_ptr;
    uint64_t chunk0_ptr;              // first scanned chunk pointer (or 0)
    uint64_t chunk1_ptr;              // second scanned chunk pointer (or 0)
    uint32_t chunks_scanned;          // how many needed chunks were examined
};

constexpr uint8_t kMaxRationaleCodes = 8;

struct GuaLayoutVerdict {
    Verdict verdict;
    uint8_t code_count;
    RCode codes[kMaxRationaleCodes];
};

struct GuaProbeResult {
    Verdict verdict;                  // combined verdict (see combine rules)
    uint8_t code_count;               // combined rationale codes
    RCode codes[kMaxRationaleCodes];
    GuaLayoutVerdict layout_a;        // per-hypothesis detail
    GuaLayoutVerdict layout_b;
    GuaObserved observed;             // fields under layout_matched (if any)
};

// Probe the GUObjectArray label. goa_vaddr comes from the profile (0x93ba110);
// image_base and readable spans come from worker-1 live facts.
GuaProbeResult probe_guobject_array(const MemSpan* spans, size_t span_count,
                                    uint64_t image_base, uint64_t goa_vaddr) noexcept;

// ---------------------------------------------------------------------------
// Code-shape probes (UGameEngine::Tick / StaticConstructObject)
// ---------------------------------------------------------------------------

struct CodeProbeResult {
    Verdict verdict;                  // always Inconclusive (by design)
    uint8_t code_count;
    RCode codes[kMaxRationaleCodes];
    uint8_t bytes_read;               // how many entry bytes were readable
    uint8_t first_bytes[16];          // the bytes that were classified
};

// Classify instruction shape at (vaddr + image_base). family_name is only for
// diagnostics in tests/logging; pass nullptr in production.
CodeProbeResult probe_code_shape(const MemSpan* spans, size_t span_count,
                                 uint64_t image_base, uint64_t vaddr,
                                 const char* /*family_name*/) noexcept;

// ---------------------------------------------------------------------------
// Pure helpers exposed for tests
// ---------------------------------------------------------------------------

// True iff [addr, addr+len) lies fully inside one span AND copies len bytes.
bool read_bytes(const MemSpan* spans, size_t span_count, uint64_t addr,
                size_t len, void* out) noexcept;
// True iff [addr, addr+len) lies fully inside one span (content not copied).
bool range_readable(const MemSpan* spans, size_t span_count, uint64_t addr,
                    size_t len) noexcept;
// True iff p is null OR an aligned canonical x86-64 user-mode pointer
// (bits 63..47 sign-extended from bit 47, alignment 8).
bool pointer_canonical_or_null(uint64_t p) noexcept;

// Cited structure constants (see header provenance block).
inline constexpr int32_t kNumElementsPerChunk = 64 * 1024;   // UObjectArray.hpp:88
inline constexpr int32_t kFuObjectItemSize = 0x18;           // FUObjectItem UEP_TotalSize
inline constexpr int32_t kMaxChunksBound = 512;              // Chunks_Type [512]
// Operational scan bound (not an offset): at most this many needed chunks are
// pairwise checked; enough for 64*65536 = 4M elements.
inline constexpr uint32_t kMaxChunkScan = 64;

}  // namespace tlp

#endif  // TOWNFALL_LAYOUT_PROBES_HPP
