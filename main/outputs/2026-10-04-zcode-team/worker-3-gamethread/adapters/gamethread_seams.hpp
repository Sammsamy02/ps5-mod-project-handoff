// gamethread_seams.hpp — typed seam adapters for the existing UE4SS integration
// path (see ../INTEGRATION-MAP.md).
//
// Purpose: make the two in-flight worker outputs PLUG IN, not force a rewrite,
// and make every absent-facts case an EXPLICIT REFUSAL with preserved state.
// Nothing here may be used to fake live-module facts or layout facts: a path,
// hash, base address, caller flag or plain boolean can never mint the proof
// tokens below. On target, only the genuine suppliers can mint them; host
// fixture minting is compiled only under MODBRIDGE_SEAM_HOST_TESTS and never
// exists in a target build of these adapters.
//
// Host-only mirror types: field-for-field these mirror the real seam types so
// the controller can mechanically swap them for the real SDK types at
// integration time:
//   LiveModuleFacts            <-> RC::Platform::NativeLoaderFacts (NativeLoaderContext.hpp)
//   ResolvedLoaderInputs       <-> the MakeNativeProgramContext consumption set
//   ResolvedLayoutFacts        <-> PublishStagedNativeLayoutMaps(major, minor, non_shipping, stats) arguments
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef MODBRIDGE_SEAM_HOST_TESTS
#include <atomic>
#include <chrono>
#endif

namespace modbridge::seams
{

// ============================================================================
// SEAM S1 — worker-1 output: owning live-module facts
// ============================================================================

// Opaque proof that a supplier compared the LIVE retained mapping (image
// identity, mapped segment correspondence, permissions, lifetime). It is not
// constructible from raw data in a target build; only the genuine supplier
// implementation mints it. Default construction = absent.
class LiveMappingProof
{
public:
    enum class Kind : uint8_t { Absent = 0, GenuineRetainedMapping = 1 };
    LiveMappingProof() noexcept = default;
    [[nodiscard]] auto kind() const noexcept -> Kind { return kind_; }
    [[nodiscard]] auto is_genuine() const noexcept -> bool
    {
        return kind_ == Kind::GenuineRetainedMapping;
    }
#ifdef MODBRIDGE_SEAM_HOST_TESTS
    // Host fixtures only. Never compiled on target; a host mint is not
    // evidence of a real Townfall mapping.
    [[nodiscard]] static auto MintForHostFixture() -> LiveMappingProof
    {
        return LiveMappingProof{Kind::GenuineRetainedMapping};
    }
#endif

private:
    explicit LiveMappingProof(Kind kind) noexcept : kind_(kind) {}
    Kind kind_{Kind::Absent};
};

struct MappedExtentFact
{
    bool present{};
    uint64_t offset{};
    uint64_t address{};
    uint64_t size{};
};

// Mirrors RC::Unreal::Platform::OwnedMappedImageFacts (format must equal
// PS_MAPPED_IMAGE_ELF64 on target; mirrored as an explicit boolean).
struct MappedImageFacts
{
    bool elf64_sysv{};
    uint64_t text_base{};
    uint64_t image_base{};
    MappedExtentFact eh_frame{};
    MappedExtentFact eh_frame_hdr{};
};

// Mirrors ModuleInput::images[ScanTarget::MainExe].
struct MainExeImage
{
    const uint8_t* base{};
    uint64_t size{};
    bool has_readable_ranges{};
};

// The complete worker-1 output view. An engaged optional means the supplier
// ran; nullopt means it has not produced facts yet (still in flight).
struct LiveModuleFacts
{
    std::shared_ptr<const void> lifetime_owner;        // ModuleInput::lifetime
    bool monolithic{};
    MainExeImage main_exe;
    std::filesystem::path module_file_path;
    std::filesystem::path game_executable_path;
    MappedImageFacts mapped;
    std::vector<uint8_t> serialized_original_elf;      // optional witness route
    LiveMappingProof correspondence;
};

enum class LoaderRefusal : uint8_t
{
    SupplierAbsent,        // worker-1 has not produced facts (nullopt)
    ProofAbsent,           // facts present but no genuine-mapping proof token
    EvidencePretender,     // paths/base/hash supplied instead of live proof
    LifetimeAbsent,        // no retaining lifetime owner
    MainExeAbsent,         // no MainExe image bytes
    RangesAbsent,          // MainExe has no readable ranges
    FormatNotElf64,        // mapped facts are not ELF64/SysV
    WitnessWithoutEh,      // serialized ELF offered but EH extents missing
    PathInvalid,           // lexical path rules of ValidateNativeProgramContext
};

struct ResolvedLoaderInputs
{
    std::shared_ptr<const void> lifetime_owner;
    bool monolithic{};
    MainExeImage main_exe;
    std::filesystem::path module_file_path;
    std::filesystem::path game_executable_path;
    MappedImageFacts mapped;
    bool has_serialized_witness{};
};

struct LoaderResolution
{
    bool ok{};
    LoaderRefusal refusal{LoaderRefusal::SupplierAbsent};
    std::string message;
    ResolvedLoaderInputs inputs; // valid only when ok
};

// Pure function: never mutates caller state, never invents facts, never
// performs I/O. File EXISTENCE is deliberately not checked here — that stays
// with the real ValidateNativeProgramContext on target (a host file check
// would prove nothing about the PS5 process).
[[nodiscard]] auto ResolveLiveModuleFacts(const std::optional<LiveModuleFacts>& facts)
    -> LoaderResolution;

// ============================================================================
// SEAM S2 — worker-2 output: validated Townfall UE5.6 layout profile
// ============================================================================

// Opaque proof that the profile entries were validated against the actual
// target build (bounded widths, correct origin), not lifted from PDB defaults
// or another game's offsets.
class LayoutValidationProof
{
public:
    enum class Kind : uint8_t { Absent = 0, ValidatedTargetProfile = 1 };
    LayoutValidationProof() noexcept = default;
    [[nodiscard]] auto kind() const noexcept -> Kind { return kind_; }
    [[nodiscard]] auto is_validated() const noexcept -> bool
    {
        return kind_ == Kind::ValidatedTargetProfile;
    }
#ifdef MODBRIDGE_SEAM_HOST_TESTS
    [[nodiscard]] static auto MintForHostFixture() -> LayoutValidationProof
    {
        return LayoutValidationProof{Kind::ValidatedTargetProfile};
    }
#endif

private:
    explicit LayoutValidationProof(Kind kind) noexcept : kind_(kind) {}
    Kind kind_{Kind::Absent};
};

struct LayoutEntryFact
{
    enum class Origin : uint8_t { Unknown, Guessed, ValidatedProfile };
    std::string canonical_key; // e.g. "UObject.Class", "GUObjectArray", "FName.ToString"
    Origin origin{Origin::Unknown};
    std::optional<uint32_t> width; // bounded by GetAccessorWidth on target
    uint8_t confidence{};          // 0..100 from the validator
};

// Minimum required entry set for the milestone query (reflection access).
[[nodiscard]] inline auto RequiredLayoutEntries() -> std::vector<std::string>
{
    return {"UObject", "GUObjectArray", "FName", "FProperty", "UFunction"};
}

struct LayoutProfileFacts
{
    bool pair_staged{}; // StageNativeLayoutFiles succeeded for this profile
    std::optional<uint32_t> engine_major;
    std::optional<uint32_t> engine_minor;
    enum class Axis : uint8_t { Unknown, No, Yes };
    Axis non_shipping{Axis::Unknown};
    Axis stats{Axis::Unknown};
    LayoutValidationProof validation;
    std::vector<LayoutEntryFact> entries;
};

enum class LayoutRefusal : uint8_t
{
    ProfileAbsent,        // worker-2 has not produced facts (nullopt)
    PairNotStaged,        // no staged member/vtable pair to publish
    VersionAbsent,        // engine major/minor unknown
    AxesUnknown,          // non_shipping or stats unresolved (nullopt axes)
    ProofAbsent,          // no validated-profile proof token
    EntryMissing,         // a required reflection entry is absent
    EntryOriginGuessed,   // an entry is guessed/PDB-default — never admitted
    EntryWidthUnbounded,  // width unknown or exceeding accessor bound
    ConfidenceLow,        // below the admission threshold
};

struct ResolvedLayoutFacts
{
    // Exactly the PublishStagedNativeLayoutMaps(...) argument tuple.
    uint32_t major{};
    uint32_t minor{};
    bool non_shipping{};
    bool stats{};
};

struct LayoutResolution
{
    bool ok{};
    LayoutRefusal refusal{LayoutRefusal::ProfileAbsent};
    std::string message;
    ResolvedLayoutFacts facts; // valid only when ok
};

[[nodiscard]] auto ResolveLayoutProfile(const std::optional<LayoutProfileFacts>& profile,
                                        uint8_t min_confidence = 90)
    -> LayoutResolution;

// ============================================================================
// SEAM S3/S4 — the game-thread query gate
// ============================================================================

// Arms the harmless engine/reflection query milestone ONLY when both seams are
// resolved. Refusal preserves all prior state (last armed inputs, last refusal
// detail). Re-arming while armed refuses explicitly; there is no silent
// rebind, mirroring PublishStagedNativeLayoutMaps' same-facts-retry-only rule.
class GameThreadQueryGate
{
public:
    enum class State : uint8_t { Disarmed, Armed, Refused };
    struct ArmResult
    {
        State state{State::Refused};
        std::string detail;
    };

    // Thread-safe. Both resolutions must be ok().
    auto Arm(const LoaderResolution& loader, const LayoutResolution& layout) -> ArmResult;
    [[nodiscard]] auto armed() const noexcept -> bool;
    [[nodiscard]] auto state() const noexcept -> State;
    [[nodiscard]] auto last_refusal_detail() const -> std::string;

private:
    mutable std::mutex mutex_;
    State state_{State::Disarmed};
    std::string last_refusal_;
    ResolvedLoaderInputs armed_loader_;
    ResolvedLayoutFacts armed_layout_;
};

} // namespace modbridge::seams
