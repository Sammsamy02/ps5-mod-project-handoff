// gamethread_seams.cpp — refusal-correct seam adapters. See gamethread_seams.hpp.
#include "gamethread_seams.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace modbridge::seams
{
namespace
{
// Lexical rules mirrored from RC::Platform::ValidateNativeProgramContext
// (NativeProgramContext.hpp). Existence/regular-file checks stay on target.
auto path_lexically_valid(const std::filesystem::path& path, std::string& why) -> bool
{
    const auto& bytes = path.native();
    if (path.empty() || !path.is_absolute() || !path.has_filename() ||
        path.filename() == "." || path.filename() == ".." ||
        path.lexically_normal() != path ||
        bytes.find(static_cast<std::filesystem::path::value_type>('\0')) !=
            std::filesystem::path::string_type::npos)
    {
        why = "path must be absolute, normalized, with a filename and no NUL";
        return false;
    }
    return true;
}

auto make_message(const char* what) -> std::string { return std::string{what}; }

template <typename Resolution, typename Refusal>
auto refused(Refusal code, std::string message) -> Resolution
{
    Resolution resolution{};
    resolution.ok = false;
    resolution.refusal = code;
    resolution.message = std::move(message);
    return resolution;
}
} // namespace

// ---------------------------------------------------------------------------
// SEAM S1
// ---------------------------------------------------------------------------
auto ResolveLiveModuleFacts(const std::optional<LiveModuleFacts>& facts) -> LoaderResolution
{
    if (!facts.has_value())
        return refused<LoaderResolution>(LoaderRefusal::SupplierAbsent, make_message("live-module facts supplier has not produced facts "
                             "(worker-1 in flight); refusing to arm the engine path"));

    // The pretender guard FIRST: raw materials without the live proof are
    // exactly the path/hash/base-address pretender pattern the owner banned.
    const bool has_raw_materials = facts->main_exe.base != nullptr ||
                                   facts->mapped.image_base != 0 ||
                                   !facts->module_file_path.empty() ||
                                   !facts->serialized_original_elf.empty();
    if (!facts->correspondence.is_genuine())
    {
        if (has_raw_materials)
            return refused<LoaderResolution>(LoaderRefusal::EvidencePretender, make_message("paths/base/hash/bytes present but no genuine "
                                 "live-mapping proof; a path, hash or claimed base "
                                 "address is not a supplier"));
        return refused<LoaderResolution>(LoaderRefusal::ProofAbsent, make_message("live-module facts present without a genuine-mapping "
                             "proof token"));
    }
    if (!facts->lifetime_owner)
        return refused<LoaderResolution>(LoaderRefusal::LifetimeAbsent, make_message("no retaining lifetime owner; mapped bytes would be "
                             "unowned for process lifetime"));
    if (facts->main_exe.base == nullptr || facts->main_exe.size == 0)
        return refused<LoaderResolution>(LoaderRefusal::MainExeAbsent, make_message("MainExe image bytes absent"));
    if (!facts->main_exe.has_readable_ranges)
        return refused<LoaderResolution>(LoaderRefusal::RangesAbsent, make_message("MainExe has no supplied readable ranges"));
    if (!facts->mapped.elf64_sysv)
        return refused<LoaderResolution>(LoaderRefusal::FormatNotElf64, make_message("mapped image facts are not ELF64/SysV; native PS5 "
                             "program requires caller-supplied ELF64 facts"));

    const bool wants_witness = !facts->serialized_original_elf.empty();
    if (wants_witness &&
        (!facts->mapped.eh_frame_hdr.present || !facts->mapped.eh_frame.present))
        return refused<LoaderResolution>(LoaderRefusal::WitnessWithoutEh, make_message("serialized ELF witness offered without complete "
                             "EH frame/hdr extents; the witness resolver requires "
                             "bounded complete EH metadata"));

    std::string why;
    if (!path_lexically_valid(facts->module_file_path, why))
        return refused<LoaderResolution>(LoaderRefusal::PathInvalid, "module_file_path invalid: " + why);
    if (!path_lexically_valid(facts->game_executable_path, why))
        return refused<LoaderResolution>(LoaderRefusal::PathInvalid, "game_executable_path invalid: " + why);

    ResolvedLoaderInputs inputs;
    inputs.lifetime_owner = facts->lifetime_owner;
    inputs.monolithic = facts->monolithic;
    inputs.main_exe = facts->main_exe;
    inputs.module_file_path = facts->module_file_path;
    inputs.game_executable_path = facts->game_executable_path;
    inputs.mapped = facts->mapped;
    inputs.has_serialized_witness = wants_witness;
    LoaderResolution resolved{};
    resolved.ok = true;
    resolved.inputs = std::move(inputs);
    return resolved;
}

// ---------------------------------------------------------------------------
// SEAM S2
// ---------------------------------------------------------------------------
auto ResolveLayoutProfile(const std::optional<LayoutProfileFacts>& profile,
                          uint8_t min_confidence) -> LayoutResolution
{
    if (!profile.has_value())
        return refused<LayoutResolution>(LayoutRefusal::ProfileAbsent, make_message("layout profile supplier has not produced facts "
                             "(worker-2 in flight); refusing to publish layouts"));
    if (!profile->pair_staged)
        return refused<LayoutResolution>(LayoutRefusal::PairNotStaged, make_message("no staged member/vtable layout pair; "
                             "PublishStagedNativeLayoutMaps refuses unstaged maps"));
    if (!profile->engine_major.has_value() || !profile->engine_minor.has_value())
        return refused<LayoutResolution>(LayoutRefusal::VersionAbsent, make_message("engine version facts absent; publication requires "
                             "the resolved major/minor identity"));
    if (profile->non_shipping == LayoutProfileFacts::Axis::Unknown ||
        profile->stats == LayoutProfileFacts::Axis::Unknown)
        return refused<LayoutResolution>(LayoutRefusal::AxesUnknown, make_message("non_shipping/stats axes unresolved; defaults do "
                             "not establish target layout facts"));
    if (!profile->validation.is_validated())
        return refused<LayoutResolution>(LayoutRefusal::ProofAbsent, make_message("layout facts present without a validated-profile "
                             "proof token"));

    for (const auto& required : RequiredLayoutEntries())
    {
        const auto it = std::find_if(profile->entries.begin(), profile->entries.end(),
                                     [&](const LayoutEntryFact& entry) {
                                         return entry.canonical_key == required;
                                     });
        if (it == profile->entries.end())
            return refused<LayoutResolution>(LayoutRefusal::EntryMissing, "required layout entry missing: " + required);
        if (it->origin == LayoutEntryFact::Origin::Guessed)
            return refused<LayoutResolution>(LayoutRefusal::EntryOriginGuessed, "layout entry is guessed/PDB-default, never admitted: " +
                        it->canonical_key);
        if (it->origin != LayoutEntryFact::Origin::ValidatedProfile)
            return refused<LayoutResolution>(LayoutRefusal::EntryOriginGuessed, "layout entry origin unknown, never admitted: " +
                        it->canonical_key);
        if (!it->width.has_value())
            return refused<LayoutResolution>(LayoutRefusal::EntryWidthUnbounded, "layout entry width unknown (accessor bound required): " +
                        it->canonical_key);
        if (it->confidence < min_confidence)
        {
            std::ostringstream detail;
            detail << "layout entry confidence " << static_cast<int>(it->confidence)
                   << " below admission threshold " << static_cast<int>(min_confidence)
                   << ": " << it->canonical_key;
            return refused<LayoutResolution>(LayoutRefusal::ConfidenceLow, detail.str());
        }
    }

    ResolvedLayoutFacts resolved;
    resolved.major = *profile->engine_major;
    resolved.minor = *profile->engine_minor;
    resolved.non_shipping = profile->non_shipping == LayoutProfileFacts::Axis::Yes;
    resolved.stats = profile->stats == LayoutProfileFacts::Axis::Yes;
    LayoutResolution outcome{};
    outcome.ok = true;
    outcome.facts = resolved;
    return outcome;
}

// ---------------------------------------------------------------------------
// SEAM S3/S4 gate
// ---------------------------------------------------------------------------
auto GameThreadQueryGate::Arm(const LoaderResolution& loader,
                              const LayoutResolution& layout) -> ArmResult
{
    std::lock_guard lock{mutex_};
    if (state_ == State::Armed)
    {
        const std::string detail = "gate already armed; explicit rebind refused "
                                   "(same-facts retry only, mirroring layout publication)";
        last_refusal_ = detail;
        return {State::Refused, detail};
    }
    if (!loader.ok)
    {
        const std::string detail =
            "seam S1 unresolved (" + loader.message + "); query stays disarmed";
        last_refusal_ = detail;
        return {State::Refused, detail};
    }
    if (!layout.ok)
    {
        const std::string detail =
            "seam S2 unresolved (" + layout.message + "); query stays disarmed";
        last_refusal_ = detail;
        return {State::Refused, detail};
    }
    armed_loader_ = loader.inputs;
    armed_layout_ = layout.facts;
    state_ = State::Armed;
    last_refusal_.clear();
    return {State::Armed, {}};
}

auto GameThreadQueryGate::armed() const noexcept -> bool
{
    std::lock_guard lock{mutex_};
    return state_ == State::Armed;
}

auto GameThreadQueryGate::state() const noexcept -> State
{
    std::lock_guard lock{mutex_};
    return state_;
}

auto GameThreadQueryGate::last_refusal_detail() const -> std::string
{
    std::lock_guard lock{mutex_};
    return last_refusal_;
}

} // namespace modbridge::seams
