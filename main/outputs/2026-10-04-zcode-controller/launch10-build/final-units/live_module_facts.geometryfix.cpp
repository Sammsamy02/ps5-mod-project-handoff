// live_module_facts.cpp
//
// Generic core of the retained live-module facts supplier. This TU makes NO
// OS calls and contains NO firmware or title constants; everything OS- or
// firmware-specific enters through LiveModuleOsOps / LiveFirmwareProfile.
//
// IN-GAME UPDATE (checkpoint zcode_ingame_v1_results, 2026-10-04):
//   * capture() is now VMMAP-FIRST: KERN_PROC_VMMAP (the only proven
//     mapping source in-game, g5) is queried before anything identity-related
//     and feeds both the correspondence math and the base scan;
//   * a refused/failed link-map walk no longer aborts the capture — the
//     observed in-game state is exactly that (DT_DEBUG absent/unfilled, g2);
//   * when no name anchor exists, identity is derived from the VMMAP spans
//     plus the in-memory ELF scan op (IdentitySource::VmScan); on that route
//     there is no loader name, so retention is honestly degraded (kind None)
//     rather than fatal — the dl-degraded composition stays as-is;
//   * the rtld NAME route keeps its original strictness: a failed retain
//     still yields zero facts (RetainFailed).
//
// Invariants enforced here:
//   * no reported address is ever dereferenced by THIS TU (integers only;
//     span-validated reads happen only inside the scan_image_base op);
//   * a failed retain on the name route yields NO facts (never a fake
//     "enabled" state); the vm-scan route yields facts with retention
//     explicitly absent (kind None, capability Failed);
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

void copy_scan_facts(const VmScanResult &scan, VmScanFacts &out) noexcept {
    out = {};
    out.candidate_count = scan.candidate_count;
    out.provider_code = 0;
    out.chosen = scan.chosen_index >= 0;
    out.chosen_base = scan.chosen_base;
    out.chosen_extent = scan.chosen_extent;
    out.chosen_dyn_addr = scan.chosen_dyn_addr;
    out.chosen_dyn_mode = scan.chosen_dyn_mode;
    out.chosen_e_type = scan.chosen_e_type;
    size_t i = 0;
    for (; i + 1 < sizeof out.provenance && scan.provenance[i] != '\0'; ++i)
        out.provenance[i] = scan.provenance[i];
    out.provenance[i] = '\0';
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

    // Required ops slots (optional: get_executable_module_handle,
    // scan_image_base).
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

    // ---- VMMAP FIRST (primary mapping source; observed in-game g5) -------
    // Queried before any identity route: it is the only proven mapping
    // source in the game process and the span basis for the base scan.
    // launch10-final (pool fix core hunk): process-lifetime static — 16,384
    // records x 24B = 384KiB; a stack buffer of that size would risk the
    // game-hijacked thread. Taken once per capture; same-cnapshot rule intact.
    static OsVmMapping mappings[kMaxVmMappings];
    size_t vm_total = 0;
    size_t vm_copied = 0;
    const bool vm_ok =
        ops_.query_vmentry(ops_.context, mappings, kMaxVmMappings, &vm_total) == 0;
    if (vm_ok) {
        facts_.capabilities.vmentry = ProbeResult::Ok;
        facts_.capabilities.vm_mappings_truncated = vm_total > kMaxVmMappings;
        vm_copied = vm_total < kMaxVmMappings ? vm_total : kMaxVmMappings;
    } else {
        facts_.capabilities.vmentry = ProbeResult::Failed;
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

    // ---- link-map walk (rtld object identities; DEMOTED, g2) -------------
    // A refused walk is the OBSERVED in-game state (DT_DEBUG absent/unfilled)
    // and no longer aborts the capture; the distinct refusal code is recorded
    // verbatim and the VMMAP scan route takes over below.
    LinkMapEntry entries[kMaxLinkMapEntries];
    size_t total = 0;
    const int walk_rc =
        ops_.link_map_snapshot(ops_.context, entries, kMaxLinkMapEntries, &total);
    const char *anchor = nullptr;
    const bool walk_refused = walk_rc != 0;
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
        facts_.provider_code = walk_rc;  // distinct in-game refusal code (g2)
        // Legacy fallback, kept ONLY when no scan route exists: an explicit
        // caller name may still anchor the capture (it will have to survive
        // the retain gate). When a scan route exists, the OBSERVED span
        // identity wins over an uncorroborated name.
        if (walk_refused && explicit_image_name != nullptr &&
            (ops_.scan_image_base == nullptr || !vm_ok)) {
            if (!copy_bounded(explicit_image_name, facts_.image_name,
                              sizeof(facts_.image_name)))
                return (facts_.status = Status::InvalidArgument);
            anchor = facts_.image_name;
        }
    }

    // ---- identity: VMMAP spans + in-memory ELF scan (primary in-game) ----
    if (anchor == nullptr && facts_.observed_base == nullptr &&
        ops_.scan_image_base != nullptr && vm_ok) {
        VmScanResult scan{};
        const int scan_rc =
            ops_.scan_image_base(ops_.context, mappings, vm_copied, &scan);
        copy_scan_facts(scan, facts_.vm_scan);
        facts_.vm_scan.provider_code = static_cast<int32_t>(scan_rc);
        if (scan_rc == 0 && scan.chosen_index >= 0 && scan.chosen_base != 0) {
            facts_.capabilities.vm_scan = ProbeResult::Ok;
            facts_.identity_source = IdentitySource::VmScan;
            facts_.observed_base =
                reinterpret_cast<void *>(static_cast<uintptr_t>(scan.chosen_base));
        } else {
            facts_.capabilities.vm_scan = ProbeResult::Failed;
        }
    }

    if (anchor == nullptr && facts_.identity_source != IdentitySource::VmScan) {
        facts_.status = Status::NoImageAnchor;
        return facts_.status;  // honest absence; never fall back to dlopen(NULL)
    }
    if (anchor != nullptr) facts_.identity_source = IdentitySource::LinkMap;

    // ---- retention: genuine loader reference, never a load ---------------
    // Name route only. The VmScan route has no loader name (and the composed
    // dl retention is degraded by design), so retention is recorded as
    // honestly absent instead of being faked or fatal.
    if (anchor != nullptr) {
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
    } else {
        // VmScan identity: structurally degraded retention (observed g3:
        // no path source, dl route none in-game; LMF_DISABLE_DL_RETENTION
        // composition returns null unconditionally — kept as-is).
        facts_.capabilities.loader_retain = ProbeResult::Failed;
        facts_.retention.kind = RetainKind::None;
    }

    // ---- module info bridge ----------------------------------------------
    sce::ModuleHandle module_handle = 0;
    bool have_handle = false;
    uint8_t handle_source = ModuleInfoFacts::kSourceNone;
    if (ops_.get_executable_module_handle != nullptr) {
        facts_.capabilities.exec_handle = ProbeResult::Failed;
        sce::ModuleHandle eh = 0;
        const int eh_rc = ops_.get_executable_module_handle(ops_.context, &eh);
        if (eh_rc != 0)
            facts_.provider_code = eh_rc;  // distinct refusal code (observed g1)
        if (eh_rc == 0 && eh != 0) {
            module_handle = eh;
            have_handle = true;
            handle_source = ModuleInfoFacts::kSourceExecHandle;
            facts_.capabilities.exec_handle = ProbeResult::Ok;
        }
    }
    if (!have_handle && anchor != nullptr &&
        profile_.allow_loader_handle_as_module_handle) {
        module_handle = static_cast<sce::ModuleHandle>(
            reinterpret_cast<uintptr_t>(facts_.retention.loader_handle));
        have_handle = true;  // attempt allowed by profile; call may still fail
        handle_source = ModuleInfoFacts::kSourceLoaderHandleCast;
    }

    if (!have_handle) {
        facts_.capabilities.module_info = ProbeResult::NotBound;
        facts_.capabilities.module_info_degraded = true;
        facts_.module_info.obtained = false;
        facts_.module_info.handle_source = ModuleInfoFacts::kSourceNone;
        // Retention (if any) is owned; mapping info below still records
        // honest facts.
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

    // ---- non-dereferencing mapping correspondence (from the FIRST-pass
    // VMMAP capture; never re-queried) --------------------------------------
    if (vm_ok) {
        if (facts_.module_info.obtained) {
            for (uint32_t s = 0; s < facts_.module_info.segment_count; ++s) {
                const uint64_t a =
                    reinterpret_cast<uintptr_t>(facts_.module_info.segments[s].address);
                uint64_t e = 0;
                (void)checked_add_u64(a, facts_.module_info.segments[s].size, &e);
                SegmentCorrespondence corr{};
                corr.segment_index = s;
                for (size_t m = 0; m < vm_copied; ++m) {
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
            // Works for both routes: rtld-reported base or the span-validated
            // VmScan chosen base (the adapter plants images[MainExe].base
            // from the scan result via identity_source/vm_scan).
            //
            // worker-facts-geometry-fix (launch-9, 2026-10-05): on the VmScan
            // route the conversion adapter derives readable_ranges by
            // clamping THESE anchor mappings into
            // [chosen_base, chosen_base+chosen_extent) and requires the
            // kernel readable bit (comp6 D2 Diff A). The containing-base-only
            // rule collected exactly ONE mapping — the span holding the ELF
            // header — which on the launch-9 target is the EXEC-ONLY text
            // span ([0x400000,0x787c000) prot=4, trusted in-game record).
            // The filter then correctly dropped it and conversion refused
            // with "no observed mapping interval intersects the module image
            // extent with valid geometry" while the READABLE data spans of
            // the SAME anchored image sat one entry away in the same
            // capture. On the VmScan route the anchor mappings are therefore
            // the mappings INTERSECTING the chosen extent — the honest
            // anchor geometry the scan itself established; every other route
            // keeps the exact containing rule. Integers only; never
            // dereferenced; still capped by kMaxAnchorMappings (VMMAP order,
            // lowest addresses first). On checked-overflow of the extent end
            // (never observed) the containing rule applies unchanged.
            const uint64_t base =
                reinterpret_cast<uintptr_t>(facts_.observed_base);
            uint64_t scan_end = 0;
            bool intersect_mode = false;
            if (facts_.identity_source == IdentitySource::VmScan &&
                facts_.vm_scan.chosen && facts_.vm_scan.chosen_extent != 0) {
                intersect_mode =
                    checked_add_u64(base, facts_.vm_scan.chosen_extent,
                                    &scan_end);
            }
            for (size_t m = 0; m < vm_copied; ++m) {
                const bool hits = intersect_mode
                    ? (mappings[m].start < scan_end &&
                       base < mappings[m].end)
                    : (mappings[m].start <= base && base < mappings[m].end);
                if (hits &&
                    facts_.anchor_mapping_count < kMaxAnchorMappings) {
                    VmMapping vm{};
                    vm.start = mappings[m].start;
                    vm.end = mappings[m].end;
                    vm.protection = mappings[m].protection;
                    facts_.anchor_mappings[facts_.anchor_mapping_count++] = vm;
                }
            }
        }
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
