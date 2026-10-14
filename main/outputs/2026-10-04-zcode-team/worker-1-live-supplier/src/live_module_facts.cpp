// live_module_facts.cpp
//
// Generic core of the retained live-module facts supplier. This TU makes NO
// OS calls and contains NO firmware or title constants; everything OS- or
// firmware-specific enters through LiveModuleOsOps / LiveFirmwareProfile.
//
// Invariants enforced here:
//   * no reported address is ever dereferenced (integers only);
//   * a failed retain yields NO facts (never a fake "enabled" state);
//   * missing evidence stays missing (module_info.obtained, empty strings,
//     correspondence counts of zero) instead of defaults;
//   * retention accounting is exactly balanced (probe references taken by
//     validate_retention are immediately released);
//   * every fixed-size copy is bounds-checked and NUL-terminated.
#include "live_module_facts.hpp"

#include "live_module_os_ops.hpp"

namespace lmf {
namespace {

bool copy_bounded(const char *src, char *dst, size_t cap) noexcept {
    // Copies at most cap-1 bytes + NUL. Returns false when src is null or
    // longer than cap-1 (no truncation: oversize input is rejected, because a
    // silently truncated image name would be a fabricated identity).
    if (src == nullptr || cap == 0) return false;
    size_t n = 0;
    while (n < cap && src[n] != '\0') ++n;
    if (n == cap) return false;  // not NUL-terminated within cap
    for (size_t i = 0; i <= n; ++i) dst[i] = src[i];
    return true;
}

// Local strlen with an explicit cap; returns cap when not terminated.
size_t capped_len(const char *s, size_t cap) noexcept {
    size_t n = 0;
    while (n < cap && s[n] != '\0') ++n;
    return n;
}

bool ends_with(const char *s, const char *suffix) noexcept {
    if (s == nullptr || suffix == nullptr) return false;
    const size_t ls = capped_len(s, kMaxPath * 4);
    const size_t lx = capped_len(suffix, 64);
    if (ls == kMaxPath * 4 || lx == 64) return false;  // unterminated input
    if (lx == 0 || ls < lx) return false;
    const char *at = s + (ls - lx);
    for (size_t i = 0; i < lx; ++i)
        if (at[i] != suffix[i]) return false;
    return true;
}

bool checked_add_u64(uint64_t a, uint64_t b, uint64_t *out) noexcept {
    if (b > UINT64_MAX - a) return false;
    *out = a + b;
    return true;
}

bool segments_overlap(uint64_t a_start, uint64_t a_end, uint64_t b_start,
                      uint64_t b_end) noexcept {
    return a_start < b_end && b_start < a_end;
}

// Validates a kernel-returned ModuleInfo against the pinned prosperodev ABI
// acceptance rules (same rules as the project's native-module-metadata
// adapter: echoed size, terminated nonempty name, 1..capacity active
// segments with representable exclusive ends).
bool validate_module_info(const sce::ModuleInfo &info, uint32_t abi_size,
                          uint32_t segment_capacity, int *reason) noexcept {
    *reason = 0;
    if (info.size != static_cast<size_t>(abi_size)) { *reason = 1; return false; }
    if (capped_len(info.name, sizeof(info.name)) == sizeof(info.name) ||
        info.name[0] == '\0') { *reason = 2; return false; }
    if (info.segmentCount == 0 ||
        info.segmentCount > segment_capacity ||
        info.segmentCount > 4) { *reason = 3; return false; }
    for (uint32_t i = 0; i < info.segmentCount; ++i) {
        const sce::ModuleSegment &seg = info.segmentInfo[i];
        uint64_t addr = reinterpret_cast<uintptr_t>(seg.address);
        uint64_t end = 0;
        if (addr == 0 || seg.size == 0) { *reason = 4; return false; }
        if (!checked_add_u64(addr, seg.size, &end)) { *reason = 5; return false; }
    }
    return true;
}

}  // namespace

RetainedLiveModuleFactsSupplier::RetainedLiveModuleFactsSupplier(
    const LiveModuleOsOps &ops, LiveFirmwareProfile profile) noexcept
    : ops_(ops), profile_(profile) {
    facts_ = {};
    facts_.status = Status::NotCaptured;
}

RetainedLiveModuleFactsSupplier::~RetainedLiveModuleFactsSupplier() {
    release();  // no-op when already released or never retained
}

RetainedLiveModuleFactsSupplier::RetainedLiveModuleFactsSupplier(
    RetainedLiveModuleFactsSupplier &&other) noexcept
    : ops_(other.ops_), profile_(other.profile_), facts_(other.facts_),
      captured_(other.captured_), released_(other.released_) {
    // Moved-from object keeps a snapshot for facts() but owns nothing.
    other.reset_retention();
    other.facts_.retention.kind = RetainKind::None;
    other.facts_.retention.loader_handle = nullptr;
    other.released_ = true;
    other.captured_ = true;
}

RetainedLiveModuleFactsSupplier &RetainedLiveModuleFactsSupplier::operator=(
    RetainedLiveModuleFactsSupplier &&other) noexcept {
    if (this != &other) {
        release();
        facts_ = other.facts_;
        captured_ = other.captured_;
        released_ = other.released_;
        other.reset_retention();
        other.facts_.retention.kind = RetainKind::None;
        other.facts_.retention.loader_handle = nullptr;
        other.released_ = true;
        other.captured_ = true;
    }
    return *this;
}

void RetainedLiveModuleFactsSupplier::reset_retention() noexcept {
    facts_.retention.loader_handle = nullptr;
}

const LiveModuleFactsSnapshot &RetainedLiveModuleFactsSupplier::facts()
    const noexcept {
    return facts_;
}

Status RetainedLiveModuleFactsSupplier::capture(
    const char *explicit_image_name) noexcept {
    facts_ = {};
    facts_.status = Status::InvalidArgument;
    captured_ = true;
    released_ = false;
    facts_.capabilities.exec_handle = ProbeResult::NotBound;

    // Required ops slots (optional: get_executable_module_handle).
    if (ops_.link_map_snapshot == nullptr || ops_.loader_retain_noload == nullptr ||
        ops_.loader_release == nullptr || ops_.get_module_info == nullptr ||
        ops_.query_vmentry == nullptr || ops_.executable_path == nullptr ||
        ops_.get_pid == nullptr)
        return facts_.status;
    // Profile sanity: the pinned ABI struct is compile-time asserted to 352
    // bytes in sce_module_abi.hpp; the profile must agree and stay in range.
    if (profile_.module_info_abi_size != sizeof(sce::ModuleInfo) ||
        profile_.module_segment_capacity == 0 ||
        profile_.module_segment_capacity > 4)
        return facts_.status;

    // ---- pid ------------------------------------------------------------
    const int pid = ops_.get_pid(ops_.context);
    if (pid >= 0) {
        facts_.pid = pid;
        facts_.capabilities.pid = ProbeResult::Ok;
    } else {
        facts_.capabilities.pid = ProbeResult::Failed;
    }

    // ---- observed executable path (non-dereferencing sysctl) -------------
    char path_buf[kMaxPath];
    path_buf[0] = '\0';
    if (ops_.executable_path(ops_.context, path_buf, sizeof(path_buf)) == 0 &&
        copy_bounded(path_buf, facts_.executable_path, sizeof(facts_.executable_path))) {
        facts_.capabilities.executable_path = ProbeResult::Ok;
    } else {
        facts_.capabilities.executable_path = ProbeResult::Failed;
        facts_.provider_code = -1;  // path probe failed; provider code unknown
    }

    // ---- link-map walk (rtld object identities) --------------------------
    LinkMapEntry entries[kMaxLinkMapEntries];
    size_t total = 0;
    const int walk_rc =
        ops_.link_map_snapshot(ops_.context, entries, kMaxLinkMapEntries, &total);
    const char *anchor = nullptr;
    if (walk_rc == 0) {
        facts_.capabilities.link_map_walk = ProbeResult::Ok;
        const size_t copied = total < kMaxLinkMapEntries ? total : kMaxLinkMapEntries;
        if (explicit_image_name != nullptr) {
            // Explicit anchor, corroborated against the walk when present.
            for (size_t i = 0; i < copied; ++i) {
                if (entries[i].name != nullptr &&
                    copy_bounded(explicit_image_name, facts_.image_name,
                                 sizeof(facts_.image_name))) {
                    // compare against the walked name
                    size_t a = 0;
                    for (; a < sizeof(facts_.image_name) &&
                           facts_.image_name[a] != '\0' &&
                           entries[i].name[a] != '\0'; ++a) {
                        if (facts_.image_name[a] != entries[i].name[a]) break;
                    }
                    if (facts_.image_name[a] == '\0' && entries[i].name[a] == '\0') {
                        facts_.observed_base = entries[i].base;
                        facts_.image_name_from_link_map = true;
                        break;
                    }
                }
            }
            if (!facts_.image_name_from_link_map) {
                if (!copy_bounded(explicit_image_name, facts_.image_name,
                                  sizeof(facts_.image_name)))
                    return (facts_.status = Status::InvalidArgument);
            }
            anchor = facts_.image_name;
        } else {
            for (size_t i = 0; i < copied; ++i) {
                if (ends_with(entries[i].name, profile_.exe_name_suffix)) {
                    if (!copy_bounded(entries[i].name, facts_.image_name,
                                      sizeof(facts_.image_name)))
                        continue;  // oversized names cannot become identity
                    facts_.observed_base = entries[i].base;
                    facts_.image_name_from_link_map = true;
                    break;
                }
            }
            anchor = facts_.image_name[0] != '\0' ? facts_.image_name : nullptr;
        }
    } else {
        facts_.capabilities.link_map_walk = ProbeResult::Failed;
        facts_.provider_code = walk_rc;
        if (explicit_image_name != nullptr) {
            if (!copy_bounded(explicit_image_name, facts_.image_name,
                              sizeof(facts_.image_name)))
                return (facts_.status = Status::InvalidArgument);
            anchor = facts_.image_name;
        }
    }

    if (anchor == nullptr) {
        facts_.status = Status::NoImageAnchor;
        return facts_.status;  // honest absence; never fall back to dlopen(NULL)
    }

    // ---- retention: genuine loader reference, never a load ---------------
    void *handle = ops_.loader_retain_noload(ops_.context, anchor);
    if (handle == nullptr) {
        facts_.capabilities.loader_retain = ProbeResult::Failed;
        facts_.retention.kind = RetainKind::None;
        facts_.status = Status::RetainFailed;
        return facts_.status;  // no reference owned: no further facts
    }
    facts_.capabilities.loader_retain = ProbeResult::Ok;
    facts_.retention.kind = RetainKind::LoaderReference;
    facts_.retention.loader_handle = handle;
    if (!copy_bounded(anchor, facts_.retention.retained_name,
                      sizeof(facts_.retention.retained_name))) {
        // Cannot happen (anchor was just copied); treat as internal failure.
        (void)ops_.loader_release(ops_.context, handle);
        reset_retention();
        facts_.retention.kind = RetainKind::None;
        return (facts_.status = Status::RetainFailed);
    }

    // ---- module info bridge ----------------------------------------------
    sce::ModuleHandle module_handle = 0;
    bool have_handle = false;
    uint8_t handle_source = ModuleInfoFacts::kSourceNone;
    if (ops_.get_executable_module_handle != nullptr) {
        facts_.capabilities.exec_handle = ProbeResult::Failed;
        sce::ModuleHandle eh = 0;
        if (ops_.get_executable_module_handle(ops_.context, &eh) == 0 && eh != 0) {
            module_handle = eh;
            have_handle = true;
            handle_source = ModuleInfoFacts::kSourceExecHandle;
            facts_.capabilities.exec_handle = ProbeResult::Ok;
        }
    }
    if (!have_handle && profile_.allow_loader_handle_as_module_handle) {
        module_handle = static_cast<sce::ModuleHandle>(
            reinterpret_cast<uintptr_t>(handle));
        have_handle = true;  // attempt allowed by profile; call may still fail
        handle_source = ModuleInfoFacts::kSourceLoaderHandleCast;
    }

    if (!have_handle) {
        facts_.capabilities.module_info = ProbeResult::NotBound;
        facts_.capabilities.module_info_degraded = true;
        facts_.module_info.obtained = false;
        facts_.module_info.handle_source = ModuleInfoFacts::kSourceNone;
        // Retention is owned; mapping info below still records honest facts.
    } else {
        sce::ModuleInfo info{};
        info.size = profile_.module_info_abi_size;
        const int rc = ops_.get_module_info(ops_.context, module_handle, &info);
        int reason = 0;
        if (rc == 0 && validate_module_info(info, profile_.module_info_abi_size,
                                            profile_.module_segment_capacity,
                                            &reason)) {
            facts_.capabilities.module_info = ProbeResult::Ok;
            ModuleInfoFacts &mi = facts_.module_info;
            mi.obtained = true;
            mi.handle_source = handle_source;
            mi.handle = module_handle;
            for (size_t i = 0; i < sizeof(mi.name); ++i) mi.name[i] = info.name[i];
            for (size_t i = 0; i < sizeof(mi.fingerprint); ++i)
                mi.fingerprint[i] = info.fingerprint[i];
            mi.segment_count = info.segmentCount;
            uint64_t lo = UINT64_MAX, hi = 0;
            for (uint32_t i = 0; i < info.segmentCount; ++i) {
                mi.segments[i] = info.segmentInfo[i];
                const uint64_t a = reinterpret_cast<uintptr_t>(info.segmentInfo[i].address);
                uint64_t e = 0;
                // checked in validate; recompute for the span
                (void)checked_add_u64(a, info.segmentInfo[i].size, &e);
                if (a < lo) lo = a;
                if (e > hi) hi = e;
            }
            mi.image_span_begin = lo;
            mi.image_span_end = hi;
        } else {
            facts_.capabilities.module_info = ProbeResult::Failed;
            facts_.provider_code = rc;
            facts_.module_info.obtained = false;
            facts_.module_info.handle_source = ModuleInfoFacts::kSourceNone;
            facts_.status = Status::ModuleInfoRejected;
            // NOTE: retention stays owned; caller may still inspect mappings.
        }
    }

    // ---- non-dereferencing mapping correspondence -------------------------
    OsVmMapping mappings[kMaxVmMappings];
    size_t vm_total = 0;
    if (ops_.query_vmentry(ops_.context, mappings, kMaxVmMappings, &vm_total) == 0) {
        facts_.capabilities.vmentry = ProbeResult::Ok;
        facts_.capabilities.vm_mappings_truncated = vm_total > kMaxVmMappings;
        const size_t copied = vm_total < kMaxVmMappings ? vm_total : kMaxVmMappings;
        if (facts_.module_info.obtained) {
            for (uint32_t s = 0; s < facts_.module_info.segment_count; ++s) {
                const uint64_t a =
                    reinterpret_cast<uintptr_t>(facts_.module_info.segments[s].address);
                uint64_t e = 0;
                (void)checked_add_u64(a, facts_.module_info.segments[s].size, &e);
                SegmentCorrespondence corr{};
                corr.segment_index = s;
                for (size_t m = 0; m < copied; ++m) {
                    if (segments_overlap(a, e, mappings[m].start, mappings[m].end)) {
                        ++corr.covering_mappings;
                        if (mappings[m].start <= a && mappings[m].end >= e)
                            corr.fully_covered = true;
                    }
                }
                facts_.segment_correspondence[facts_.segment_correspondence_count++] =
                    corr;
            }
        }
        if (facts_.observed_base != nullptr) {
            const uint64_t base =
                reinterpret_cast<uintptr_t>(facts_.observed_base);
            for (size_t m = 0; m < copied; ++m) {
                if (mappings[m].start <= base && base < mappings[m].end &&
                    facts_.anchor_mapping_count < kMaxAnchorMappings) {
                    VmMapping vm{};
                    vm.start = mappings[m].start;
                    vm.end = mappings[m].end;
                    vm.protection = mappings[m].protection;
                    facts_.anchor_mappings[facts_.anchor_mapping_count++] = vm;
                }
            }
        }
    } else {
        facts_.capabilities.vmentry = ProbeResult::Failed;
    }

    if (facts_.status == Status::InvalidArgument) {
        // No failure recorded above: choose final status from evidence.
        facts_.status = facts_.capabilities.module_info_degraded
                            ? Status::ModuleInfoNotObtained
                            : Status::Ok;
    }
    return facts_.status;
}

Status RetainedLiveModuleFactsSupplier::validate_retention() noexcept {
    if (!captured_) return Status::NotCaptured;
    if (facts_.retention.kind != RetainKind::LoaderReference)
        return Status::AlreadyReleased;
    void *again = ops_.loader_retain_noload(ops_.context,
                                            facts_.retention.retained_name);
    if (again == nullptr) return Status::RetentionProbeFailed;
    bool consistent = (again == facts_.retention.loader_handle);
    const int rc = ops_.loader_release(ops_.context, again);  // balance probe
    if (rc != 0) {
        facts_.provider_code = rc;
        return Status::RetentionProbeFailed;
    }
    return consistent ? Status::Ok : Status::RetentionProbeFailed;
}

Status RetainedLiveModuleFactsSupplier::release() noexcept {
    // Idempotent: releasing when no reference is owned is a safe no-op (Ok).
    if (facts_.retention.kind != RetainKind::LoaderReference ||
        facts_.retention.loader_handle == nullptr)
        return Status::Ok;
    released_ = true;
    if (facts_.retention.kind == RetainKind::LoaderReference &&
        facts_.retention.loader_handle != nullptr) {
        const int rc =
            ops_.loader_release(ops_.context, facts_.retention.loader_handle);
        reset_retention();
        facts_.retention.kind = RetainKind::None;
        if (rc != 0) {
            facts_.provider_code = rc;
            return Status::InvalidArgument;  // release failed; recorded, not retried
        }
    }
    return Status::Ok;
}

}  // namespace lmf
