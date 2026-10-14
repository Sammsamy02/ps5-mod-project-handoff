// test_geometry_fix_host.cpp
//
// worker-facts-geometry-fix host fixture (controller-updated shapes).
// Replays the launch-9 (pid 273) conversion-boundary shapes through the REAL
// lmf::RetainedLiveModuleFactsSupplier core (fake OS ops, exactly the
// worker-supplier-ingame-update host-test pattern) and the REAL canonical
// comp6 adapter (outputs/2026-10-04-zcode-controller/comp6-integration/
// lmf_to_native_loader_facts.cpp, UNMODIFIED). The launch-9 defect is
// composition-confirmed: both PRXs' chains refused here with code=1 while the
// SAME attempt's bridge-side lease registration counted 6 readable spans
// intersecting the extent (bridge_lease_reg_spans=6/reg=6, entry-log delta).
//
// Shapes are derived from LIVE VMMAP input semantics per the controller's
// phdr cross-check (retained eboot sha 3b6cfad4, LOAD0 --X / LOAD1 R-- /
// LOAD3 RW- / LOAD7 RW- fragment / LOAD8 --- ; runtime = file+0x400000 for
// LOAD0/LOAD1 ONLY; later spans taken from observed live records, never phdr
// arithmetic). Only span READABILITY, ORDER and EXTENT MEMBERSHIP drive the
// predicates under test; placeholder spans are labeled synthetic.
//
// The harness is compiled TWICE against the SAME adapter and headers:
//   BEFORE (-DGEOMETRY_FIX_BEFORE=1, canonical supplier core):
//       must reproduce the EXACT launch-9 refusal on every launch-9 shape.
//   AFTER  (-DGEOMETRY_FIX_BEFORE=0, staging/live_module_facts.geometryfix.cpp):
//       must convert with the readable spans of the anchored image, matching
//       bridge-side geometry (intervals == the spans the r13 registration
//       counted), and surface the diagnostic inputs the controller-required
//       events report (extent base/end, anchor count, intervals found).
// No target execution, no SDK symbols, no MakeNativeProgramContext/lease
// downstream — launch-9 died at this boundary.
#include "lmf_to_native_loader_facts.hpp"
#include "live_module_os_ops.hpp"
#include "LiveLoaderObservedFacts.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace lmf;
using RC::Platform::LmfConversionError;
using RC::Platform::LmfConversionOptions;
using RC::Platform::LmfConversionResult;

#ifndef GEOMETRY_FIX_BEFORE
#define GEOMETRY_FIX_BEFORE 1
#endif

static int g_failures = 0;
static int g_checks = 0;
static void record(bool ok, const char *name, const char *detail = "") {
    ++g_checks;
    std::printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name,
                detail[0] ? " — " : "", detail);
    if (!ok) ++g_failures;
}
#define CHECK(cond) record((cond), #cond)

// ---------------------------------------------------------------------------
// Fake OS ops: the launch-9 conversion-side observation shape (pid 273).
// ---------------------------------------------------------------------------
struct FakeOps {
    std::vector<OsVmMapping> mappings;
    int link_map_rc = LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame;  // g2 (-4002), DEMOTED in-game
    int exec_handle_rc = LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame;  // g1 (-4001) == observed lmf_provider_code 0xfffff05f
    int pid = 273;                        // launch-9 pid
    // Window-anchor outcome of the r13 bridge scan wrapper at launch-9:
    // header scan found no candidate (needs-base), data-window anchor
    // verified B&&D&&E&&(C||C2) -> chosen 0x400000 + kImageExtentVaddr.
    uint64_t chosen_base = 0x400000;
    uint64_t chosen_extent = 0xbc602c8;
    // Composition contrast case: the scan CHOSE a readable-anchored image.
    bool scan_chooses_readable_anchor = false;
    uint64_t readable_anchor_base = 0x7f000000000ull;  // high mapping, readable (outside the game extent)
    uint64_t readable_anchor_extent = 0x100000;
    // Case 4: a walked link map whose eboot.bin entry anchors at the lib base.
    bool link_map_names_eboot = false;
};

static FakeOps *g_fake = nullptr;

static int fake_link_map(void *, LinkMapEntry *out, size_t cap, size_t *count) {
    if (g_fake->link_map_rc != 0 || !g_fake->link_map_names_eboot) {
        *count = 0;
        return g_fake->link_map_rc;       // refused in-game: VMMAP scan route takes over
    }
    static char name_storage[] = "eboot.bin";
    if (cap < 1) return -2;
    out[0].name = name_storage;
    out[0].base = reinterpret_cast<void *>(g_fake->readable_anchor_base);
    *count = 1;
    return 0;
}
static void *fake_retain(void *, const char *) { return nullptr; }
static int fake_release(void *, void *) { return 0; }
static int fake_get_module_info(void *, sce::ModuleHandle, sce::ModuleInfo *info) {
    std::memset(info, 0, sizeof(*info));
    return -1;                            // no handle source on the VmScan route
}
static int fake_exec_handle(void *, sce::ModuleHandle *out) {
    *out = 0;
    return g_fake->exec_handle_rc;        // g1 refusal -> provider_code -4001 (observed)
}
static int fake_vmentry(void *, OsVmMapping *out, size_t cap, size_t *count) {
    // The bridge serves the FULL walk; the core truncates to its own capacity
    // exactly like launch-9 (bridge saw 10019 records, core keeps 1024). The
    // copied tail is zero-filled: the real buffer's unused capacity is zero
    // .bss, and zero spans (start==end==0) can never contain a base.
    *count = g_fake->mappings.size() + 10015;
    size_t i = 0;
    for (; i < g_fake->mappings.size() && i < cap; ++i) out[i] = g_fake->mappings[i];
    for (; i < cap; ++i) out[i] = OsVmMapping{0, 0, 0};
    return 0;
}
static int fake_exec_path(void *, char *, size_t) { return -1; }
static int fake_get_pid(void *) { return g_fake->pid; }
static int fake_scan_image_base(void *, const OsVmMapping *, size_t, VmScanResult *out) {
    // Stands for the r13 bridge wrapper's OUTCOME (proven by the launch-9
    // anchor_cov/anchor_match block + lmf_vm_scan_cap=2 diagnostic).
    std::memset(out, 0, sizeof(*out));
    out->candidate_count = 1;
    out->chosen_index = 0;
    if (g_fake->scan_chooses_readable_anchor) {
        out->chosen_base = g_fake->readable_anchor_base;
        out->chosen_extent = g_fake->readable_anchor_extent;
    } else {
        out->chosen_base = g_fake->chosen_base;
        out->chosen_extent = g_fake->chosen_extent;
    }
    out->chosen_dyn_mode = 1;
    std::snprintf(out->provenance, sizeof(out->provenance), "v2_elf_base_scan");
    return 0;
}

// The observed launch-9 live-VMMAP spans around the anchored game image
// [0x400000, 0xc0602c8). Spans 1-4 are the controller-parsed phdr/live
// records (LOAD0 --X runtime [0x400000,0x787c000) contains the anchor base;
// LOAD1 R-- [0x787c000,0x82bc000); LOAD3 RW- observed prot=1
// [0x82bc000,0x953c000) == the IGW L3 record; LOAD7 RW fragment
// [0x970c000,0x9a34000) contains GUA 0x97ba110 and does NOT sit at
// file+0x400000). Spans 5-6 are SYNTHETIC readable spans inside the extent,
// present only so the shape reaches the bridge-observed count of six
// readable intersecting spans (bridge_lease_reg_spans=6).
static void fill_launch9_mappings(FakeOps &f, bool bridge_parity_six) {
    // NOTE: readable spans are NON-ADJACENT (page gaps), matching live VMMAP
    // holes: LOAD1 ends at page-round(0x82ba918)=0x82bb000 < LOAD3 0x82bc000.
    // The adapter coalesces overlaps AND adjacency, so adjacent fixture spans
    // would understate the interval count vs the bridge's raw span counters.
    f.mappings = {
        {0x400000ull, 0x787c000ull, 4},   // LOAD0 --X (CONTAINS anchor base 0x400000)
        {0x787c000ull, 0x82bb000ull, 1},  // LOAD1 R--  (readable 1)
        {0x82bc000ull, 0x953c000ull, 1},  // LOAD3 (observed prot=1) (readable 2)
        {0x970c000ull, 0x9a34000ull, 3},  // LOAD7 RW fragment, GUA 0x97ba110 (readable 3)
    };
    if (bridge_parity_six) {
        f.mappings.push_back({0x9b00000ull, 0x9c80000ull, 1});  // synthetic readable 4 (gap kept)
        f.mappings.push_back({0x9d00000ull, 0x9e80000ull, 1});  // synthetic readable 5 (gap kept)
        f.mappings.push_back({0x9f00000ull, 0xa000000ull, 1});  // synthetic readable 6 (gap kept)
    }
    f.mappings.push_back({0xa000000ull, 0xc0602c8ull, 0});    // LOAD8 --- (in extent, NOT readable)
    f.mappings.push_back({0xc0602c8ull, 0xc400000ull, 3});    // past the extent: must stay out
    f.mappings.push_back({0x7f000000000ull, 0x7f000100000ull, 3}); // unrelated high mapping (readable; contrast/linkmap anchor)
}

static LiveFirmwareProfile make_profile() {
    LiveFirmwareProfile p{};
    p.module_info_abi_size = 352;
    p.module_segment_capacity = 4;
    p.fw_version = 0;
    p.allow_loader_handle_as_module_handle = false;
    std::snprintf(p.exe_name_suffix, sizeof(p.exe_name_suffix), "%s", "eboot.bin");
    return p;
}

static LiveModuleOsOps make_ops() {
    LiveModuleOsOps ops{};
    ops.link_map_snapshot = fake_link_map;
    ops.loader_retain_noload = fake_retain;
    ops.loader_release = fake_release;
    ops.get_module_info = fake_get_module_info;
    ops.get_executable_module_handle = fake_exec_handle;
    ops.query_vmentry = fake_vmentry;
    ops.scan_image_base = fake_scan_image_base;
    ops.executable_path = fake_exec_path;
    ops.get_pid = fake_get_pid;
    return ops;
}

// The r13 bridge's exact launch-9 conversion options (r13 lines 634-643).
static LmfConversionOptions bridge_options() {
    LmfConversionOptions o{};
    o.allow_vm_scan_base_as_module_image_base = true;
    o.allow_configured_roots_for_missing_executable_path = true;
    return o;
}

struct RunOutcome {
    LmfConversionResult result;
    size_t anchor_count = 0;
    uint64_t anchor_first_start = 0, anchor_first_end = 0;
    int32_t anchor_first_prot = 0;
};

static RunOutcome run_conversion(FakeOps &fake) {
    g_fake = &fake;
    LiveModuleOsOps ops = make_ops();
    RetainedLiveModuleFactsSupplier supplier(ops, make_profile());
    const Status st = supplier.capture();
    RunOutcome out{};
    out.result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(
        supplier, std::filesystem::path("/data/plugins/modbridge_supplier_composition.prx"),
        bridge_options());
    const auto &snap = supplier.facts();
    out.anchor_count = snap.anchor_mapping_count;
    if (out.anchor_count > 0) {
        out.anchor_first_start = snap.anchor_mappings[0].start;
        out.anchor_first_end = snap.anchor_mappings[0].end;
        out.anchor_first_prot = snap.anchor_mappings[0].protection;
    }
    g_fake = nullptr;
    record(st == Status::ModuleInfoNotObtained,
           "capture lands on ModuleInfoNotObtained (VmScan route)",
           std::to_string(static_cast<int>(st)).c_str());
    return out;
}

// The controller-required diagnostic inputs (lmf_extent_base / lmf_extent_end
// / lmf_anchor_count / lmf_intervals_found): extent from the conversion
// result (VmScanBase provenance), anchor count from the snapshot's
// anchor_mappings, intervals from the converted facts' readable_ranges.
static void check_diagnostic_inputs(const RunOutcome &o, size_t want_intervals) {
    if constexpr (!GEOMETRY_FIX_BEFORE) {
        record(o.result.converted && o.result.image_base_provenance ==
                   RC::Platform::LmfImageBaseProvenance::VmScanBase,
               "diag: extent provenance VmScanBase");
        record(o.result.resolved_image_base == 0x400000ull,
               "diag: lmf_extent_base would emit 0x400000");
        record(o.result.resolved_image_end == 0x400000ull + 0xbc602c8ull,
               "diag: lmf_extent_end would emit 0xc0602c8");
        record(o.anchor_count >= 2,
               "diag: lmf_anchor_count would emit >1 (observed spans, not just the header span)",
               std::to_string(o.anchor_count).c_str());
        if (o.result.converted) {
            const auto &ranges = o.result.facts.modules.images.at(
                RC::ScanTarget::MainExe).readable_ranges;
            record(ranges.size() == want_intervals,
                   "diag: lmf_intervals_found matches the bridge-side count",
                   std::to_string(ranges.size()).c_str());
        }
    } else {
        record(o.anchor_count == 1 && o.anchor_first_prot == 4,
               "diag (BEFORE): snapshot carried ONE anchor span, the exec-only LOAD0",
               ("n=" + std::to_string(o.anchor_count) +
                " prot=" + std::to_string(o.anchor_first_prot)).c_str());
        record(o.anchor_first_start == 0x400000ull && o.anchor_first_end == 0x787c000ull,
               "diag (BEFORE): that span is [0x400000,0x787c000) — the whole conversion input");
    }
}

int main() {
    std::printf("== worker-facts-geometry-fix host fixture (GEOMETRY_FIX_BEFORE=%d) ==\n",
                GEOMETRY_FIX_BEFORE ? 1 : 0);

    // ---- Case 1: the OBSERVED launch-9 shape (documented spans only) -------
    {
        std::printf("-- case observed-launch9 --\n");
        FakeOps fake;
        fill_launch9_mappings(fake, /*bridge_parity_six=*/false);
        const RunOutcome o = run_conversion(fake);
        if (GEOMETRY_FIX_BEFORE) {
            record(!o.result.converted, "BEFORE: conversion refused (the launch-9 outcome, both PRXs)");
            record(o.result.error == LmfConversionError::InvalidReadableMappings,
                   "BEFORE: error code InvalidReadableMappings",
                   std::to_string(static_cast<int>(o.result.error)).c_str());
            record(std::strcmp(o.result.error_message,
                               "no observed mapping interval intersects the module image extent with valid geometry") == 0,
                   "BEFORE: exact launch-9 detail text (the per-refusal predicate)", o.result.error_message);
        } else {
            record(o.result.converted, "AFTER: conversion succeeds on the observed shape",
                   o.result.converted ? "" : o.result.error_message);
            if (o.result.converted) {
                const auto &ranges = o.result.facts.modules.images.at(
                    RC::ScanTarget::MainExe).readable_ranges;
                record(ranges.size() == 3, "AFTER: the three documented readable spans derive",
                       std::to_string(ranges.size()).c_str());
                record(o.result.facts.game_executable_path.empty(),
                       "AFTER: game_executable_path stays EMPTY (never fabricated)");
                record(!ranges.empty() &&
                           reinterpret_cast<uintptr_t>(ranges[0].data) == 0x787c000ull,
                       "AFTER: first interval starts at the LOAD1 readable span");
                bool exec_only_excluded = true, past_extent_excluded = true;
                for (const auto &rg : ranges) {
                    const uintptr_t b = reinterpret_cast<uintptr_t>(rg.data);
                    if (b < 0x787c000ull) exec_only_excluded = false;      // LOAD0 X-only kept out
                    if (b + rg.size > 0x400000ull + 0xbc602c8ull) past_extent_excluded = false;
                }
                record(exec_only_excluded, "AFTER: exec-only LOAD0 span stays excluded (D2 filter intact)");
                record(past_extent_excluded, "AFTER: past-extent span stays excluded (clamped)");
            }
        }
        check_diagnostic_inputs(o, 3);
    }

    // ---- Case 2: BRIDGE-PARITY shape (six readable spans, == reg counters) --
    {
        std::printf("-- case bridge-parity-six --\n");
        FakeOps fake;
        fill_launch9_mappings(fake, /*bridge_parity_six=*/true);
        const RunOutcome o = run_conversion(fake);
        if (GEOMETRY_FIX_BEFORE) {
            record(!o.result.converted,
                   "BEFORE: refused even when SIX readable spans intersect the extent (bridge-side count)",
                   o.result.error_message);
        } else {
            record(o.result.converted, "AFTER: bridge-style input PASSES (conversion matches bridge side)");
            if (o.result.converted) {
                const auto &ranges = o.result.facts.modules.images.at(
                    RC::ScanTarget::MainExe).readable_ranges;
                record(ranges.size() == 6, "AFTER: six intervals == bridge_lease_reg_spans=6",
                       std::to_string(ranges.size()).c_str());
            }
        }
        check_diagnostic_inputs(o, 6);
    }

    // ---- Case 3: composition contrast (readable containing anchor) ----------
    // Documents the input difference class: when the scan's chosen base lies
    // in a READABLE containing span, the containing-only rule already
    // converts — on both cores. The launch-9 shape (anchor base inside an
    // exec-only span) is the only failing input.
    {
        std::printf("-- case readable-anchor-contrast --\n");
        FakeOps fake;
        fill_launch9_mappings(fake, false);
        fake.scan_chooses_readable_anchor = true;
        const RunOutcome o = run_conversion(fake);
        record(o.result.converted, "CONTRAST: readable containing anchor converts on BOTH cores",
               o.result.converted ? "" : o.result.error_message);
        if (o.result.converted)
            record(o.result.resolved_image_base == 0x7f000000000ull,
                   "CONTRAST: extent follows the chosen readable anchor");
    }

    // ---- Case 4: LinkMap-route regression guard -----------------------------
    // A name-anchored capture must keep the containing-base rule byte-identical.
    {
        std::printf("-- case linkmap-regression --\n");
        FakeOps fake;
        fill_launch9_mappings(fake, false);
        fake.link_map_rc = 0;             // walk succeeds
        fake.link_map_names_eboot = true; // eboot.bin entry anchors at the lib base
        g_fake = &fake;
        LiveModuleOsOps ops = make_ops();
        struct KeepRetain {
            static void *retain(void *, const char *) { return reinterpret_cast<void *>(0xAAA1); }
        };
        ops.loader_retain_noload = KeepRetain::retain;
        RetainedLiveModuleFactsSupplier supplier(ops, make_profile());
        const Status st = supplier.capture();
        const auto &snap = supplier.facts();
        record(st == Status::ModuleInfoNotObtained, "name route captures (degraded module info)");
        record(snap.identity_source == IdentitySource::LinkMap,
               "identity source is LinkMap",
               std::to_string(static_cast<int>(snap.identity_source)).c_str());
        record(snap.anchor_mapping_count == 1 &&
               snap.anchor_mappings[0].start == 0x7f000000000ull,
               "containing rule unchanged on the name route (1 anchor span)");
        (void)supplier.release();
        g_fake = nullptr;
    }

    std::printf("== %d checks, %d failures ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
