#include "NativeLayoutBinding.hpp"
#include <NativeLayoutEngineIncludes.hpp>
#include <NativeLayoutWidths.hpp>
#include "NativeLayoutActiveWidths.hpp"
#include <array>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace RC::Platform::NativeLayouts {
namespace {
using MemberMap = std::unordered_map<File::StringType, int32_t>;
using VirtualMap = std::unordered_map<File::StringType, uint32_t>;
using BitfieldMap = std::unordered_map<File::StringType, Unreal::BitfieldInfo>;
struct MemberSchema {
    const char* section;
    const char* key;
    const char* target;
    const char* canonical;
    bool bitfield;
};
struct MemberTarget { const char* name; MemberMap* offsets; BitfieldMap* bits; };
struct VirtualTarget { const char* name; VirtualMap* offsets; };
#include <NativeLayoutSchema.inc>
struct MemberStage { MemberTarget target; MemberMap offsets; BitfieldMap bits;
    std::unordered_map<File::StringType, uint32_t> widths; };
struct Prepared {
    Identity identity;
    std::vector<StagedLayoutFact> member_facts,virtual_facts;
    std::map<std::string, MemberStage> members;
    std::map<std::string, VirtualTarget> virtual_targets;
    std::map<VirtualMap*, VirtualMap> virtual_maps;
};
std::mutex BindingMutex;
std::unique_ptr<Prepared> Staged;
std::optional<bool> KnownNonShipping;
std::optional<bool> KnownStats;
bool Published{};

auto Wide(const std::string& ascii) -> File::StringType {
    return File::StringType(ascii.begin(), ascii.end());
}
auto ReadBounded(const std::filesystem::path& path) -> std::string {
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)))
        throw std::runtime_error("Native layout must be an existing regular file: " + path.string());
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open native layout: " + path.string());
    std::string text;
    std::array<char, 8192> buffer;
    while (input.read(buffer.data(), buffer.size()) || input.gcount()) {
        const auto count = static_cast<size_t>(input.gcount());
        if (count > 8u * 1024 * 1024 - text.size())
            throw std::runtime_error("Native layout exceeds 8 MiB: " + path.string());
        text.append(buffer.data(), count);
    }
    if (!input.eof() || input.bad()) throw std::runtime_error("Cannot read native layout: " + path.string());
    return text;
}
auto Prepare(Bundle bundle) -> std::unique_ptr<Prepared> {
    auto result = std::make_unique<Prepared>();
    result->identity = std::move(bundle.identity);
    for (auto target : MemberTargets()) result->members.emplace(target.name, MemberStage{target, {}, {}, {}});
    for (auto target : VirtualTargets()) {
        result->virtual_targets.emplace(target.name, target);
        result->virtual_maps.try_emplace(target.offsets);
    }
    for (const auto& entry : bundle.members) {
        const MemberSchema* schema{};
        for (const auto& candidate : MemberSchemaEntries)
            if (entry.section == candidate.section && entry.key == candidate.key) { schema = &candidate; break; }
        if (!schema) throw std::invalid_argument("Unknown native member: " + entry.section + "::" + entry.key);
        if (schema->bitfield != entry.bitfield.has_value())
            throw std::invalid_argument("Native bitfield tuple required only for bitfield member: " + entry.section + "::" + entry.key);
        // Current genuine BitfieldProxy forms (1ULL << bit_len); a 64-bit
        // tuple would invoke undefined behavior even though it fits storage.
        if (entry.bitfield && entry.bitfield->length == 64)
            throw std::invalid_argument("Native 64-bit bitfield proxy is unavailable");
        const auto fact_name=std::string(schema->target)+"::"+schema->canonical;
        uint32_t fact_width=0;
        auto& stage = result->members.at(schema->target);
        const auto key = Wide(schema->canonical);
        if (!stage.offsets.emplace(key, static_cast<int32_t>(entry.offset)).second)
            throw std::invalid_argument("Duplicate native member or alias: " + entry.section + "::" + entry.key);
        if (entry.bitfield) {
            const auto& bits = *entry.bitfield;
            stage.bits.emplace(key, Unreal::BitfieldInfo{bits.position, bits.length, bits.storage_bytes});
            stage.widths.emplace(key, bits.storage_bytes);
            fact_width=bits.storage_bytes;
        } else if (key != STR("UEP_TotalSize")) {
            const auto width = GetBindingAccessorWidth(schema->target, schema->canonical);
            if (!width || *width == 0)
                throw std::invalid_argument("Native accessor width unavailable: " + entry.section + "::" + entry.key);
            stage.widths.emplace(key, *width);
            fact_width=*width;
        }
        result->member_facts.push_back({fact_name,entry.offset,fact_width});
    }
    for (const auto& entry : bundle.vtables) {
        result->virtual_facts.push_back({entry.section+"::"+entry.key,entry.offset,sizeof(void*)});
        auto found = result->virtual_targets.find(entry.section);
        if (found == result->virtual_targets.end()) throw std::invalid_argument("Unknown native vtable class: " + entry.section);
        // Some wrappers inherit one physical map (for example viewport/UObject).
        // Merge supplied sections into that real map, and reject representations
        // that this inherited map API cannot express without losing a value.
        auto& offsets = result->virtual_maps.at(found->second.offsets);
        auto [previous, inserted] = offsets.emplace(Wide(entry.key), entry.offset);
        if (!inserted && previous->second != entry.offset)
            throw std::invalid_argument("Conflicting native virtual aliases share a map: " + entry.section + "::" + entry.key);
    }
    for (const auto& [name, member] : result->members) {
        if (member.offsets.empty()) continue;
        auto total = member.offsets.find(STR("UEP_TotalSize"));
        if (total == member.offsets.end() || total->second <= 0)
            throw std::invalid_argument("Native member class requires positive UEP_TotalSize: " + name);
        for (const auto& [key, offset] : member.offsets) {
            if (key == STR("UEP_TotalSize")) continue;
            const auto width = member.widths.at(key);
            if (offset < 0 || offset >= total->second || width > static_cast<uint32_t>(total->second - offset))
                throw std::invalid_argument("Native member is outside declared class size: " + name);
        }
    }
    return result;
}
void CheckEmpty(const Prepared& prepared) {
    for (const auto& [name, member] : prepared.members)
        if (!member.target.offsets->empty() || (member.target.bits && !member.target.bits->empty()))
            throw std::logic_error("Native layouts cannot replace existing/cached member maps: " + name);
    for (const auto& [target, table] : prepared.virtual_maps)
        if (!target->empty())
            throw std::logic_error("Native layouts cannot replace existing/cached vtable maps");
}
}

static void StageNativeLayoutBundle(Bundle bundle);

void StageNativeLayoutFiles(const std::filesystem::path& directory) {
    const auto members = ReadBounded(directory / "MemberVariableLayout.ini");
    const auto virtuals = ReadBounded(directory / "VTableLayout.ini");
    StageNativeLayoutBundle(ParsePair(members, virtuals));
}
void StageNativeLayoutTexts(std::string_view members, std::string_view virtuals) {
    StageNativeLayoutBundle(ParsePair(members, virtuals));
}
static void StageNativeLayoutBundle(Bundle bundle) {
    // A caller cannot supply unchecked values by manually constructing Bundle.
    // This entry is internal to the genuine file loader and focused tests;
    // numeric validation is repeated here before narrowing to actual maps.
    const auto& id = bundle.identity;
    if (id.format != 1 || id.target != "prospero-x86_64-sysv" || id.pointer_size != 8 ||
        id.engine_major < 4 || id.engine_revision.empty() || id.build_id.empty())
        throw std::invalid_argument("Invalid native layout identity");
    for (const auto& entry : bundle.members) {
        if (entry.offset > INT32_MAX) throw std::invalid_argument("Native member offset overflow");
        if (entry.bitfield) {
            const auto& bit = *entry.bitfield;
            if ((bit.storage_bytes != 1 && bit.storage_bytes != 2 && bit.storage_bytes != 4 && bit.storage_bytes != 8) ||
                !bit.length || bit.position >= 8 * bit.storage_bytes || bit.length > 8 * bit.storage_bytes - bit.position)
                throw std::invalid_argument("Invalid native bitfield range");
        }
    }
    for (const auto& entry : bundle.vtables)
        if (entry.offset > INT32_MAX || entry.offset % 8 || entry.bitfield)
            throw std::invalid_argument("Invalid native vtable byte offset");
    auto prepared = Prepare(std::move(bundle));
    std::lock_guard lock{BindingMutex};
    if (Published || Staged) throw std::logic_error("Native layout bundle is already staged for this loaded lifetime");
    CheckEmpty(*prepared);
    Staged = std::move(prepared);
}
auto GetStagedNativeLayoutFacts() -> std::optional<StagedFacts> {
    std::lock_guard lock{BindingMutex};
    if (!Staged) return std::nullopt;
    return StagedFacts{Staged->identity,Staged->member_facts,Staged->virtual_facts};
}
void SetResolvedNativeLayoutAxes(std::optional<bool> non_shipping, std::optional<bool> stats) {
    std::lock_guard lock{BindingMutex};
    if (Published) {
        if (KnownNonShipping != non_shipping || KnownStats != stats)
            throw std::logic_error("Native layout axes cannot change after publication");
        return;
    }
    KnownNonShipping = non_shipping;
    KnownStats = stats;
}
void PublishStagedNativeLayoutMaps(uint32_t major, uint32_t minor, bool non_shipping, bool stats) {
    std::lock_guard lock{BindingMutex};
    if (!Staged) throw std::logic_error("Native layout files have not been staged");
    const auto& id = Staged->identity;
    if (!KnownNonShipping || !KnownStats || *KnownNonShipping != non_shipping || *KnownStats != stats)
        throw std::runtime_error("Native layout build axes require resolved or explicitly supplied facts");
    if (id.engine_major != major || id.engine_minor != minor || id.non_shipping != non_shipping || id.stats != stats)
        throw std::runtime_error("Native layout declarations disagree with resolved engine facts");
    // The genuine early scan can fail in its second pass after map publication.
    // A same-bundle/same-facts retry reuses immutable maps, never rebinds them.
    if (Published) return;
    CheckEmpty(*Staged);
    // Each real map has the same standard allocator/hash. All allocations and
    // validation precede this no-throw publication sequence. Startup is the
    // only publisher; no accessors or callbacks may run concurrently here.
    static_assert(noexcept(std::declval<MemberMap&>().swap(std::declval<MemberMap&>())));
    static_assert(noexcept(std::declval<BitfieldMap&>().swap(std::declval<BitfieldMap&>())));
    static_assert(noexcept(std::declval<VirtualMap&>().swap(std::declval<VirtualMap&>())));
    for (auto& [name, member] : Staged->members) {
        member.target.offsets->swap(member.offsets);
        if (member.target.bits) member.target.bits->swap(member.bits);
    }
    for (auto& [target, table] : Staged->virtual_maps) target->swap(table);
    Published = true;
}
}
