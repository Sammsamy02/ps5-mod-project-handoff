// test_lmf_adapter_host.cpp
//
// Host tests for the S1 conversion adapter. These drive the REAL
// lmf::RetainedLiveModuleFactsSupplier core (worker-1 sources, injected fake
// OS ops) and the REAL immutable loader-context chain (spike
// NativeLoaderContext/OwnedMappedScanSource headers+sources,
// MakeNativeProgramContext, PLH owned-memory host implementation).
//
// IN-GAME UPDATE (worker-supplier-ingame-update, checkpoint
// zcode_ingame_v1_results): the original 102 checks are kept verbatim and
// re-run against the PATCHED vendored supplier/adapter; new checks (N1..N5)
// prove: VMMAP-first capture order, both observed-dead routes refused with
// DISTINCT codes citing the observed facts, the vendored V2 base-scan helper
// (real ELF fixture pages), and a scan-fed successful conversion.
//
// Everything here is contract behavior only. No SDK symbol, no PS5 target
// fact, no console contact. Addresses in the fake-ops tests are deliberately
// non-dereferenceable integers; the factory-probe tests use genuinely mapped
// host pages (mmap) plus a registered owned-memory ScopedRegion so the real
// ReadLease path runs.
#include "lmf_to_native_loader_facts.hpp"
#include "live_module_os_ops.hpp"
#include "LiveLoaderObservedFacts.hpp"
#include "live_module_base_scan.hpp"

#include <NativeLoaderContext.hpp>
#include <polyhook2/OwnedMemory.hpp>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

#ifndef ADAPTER_FIXTURE_EBOOT
#define ADAPTER_FIXTURE_EBOOT "/nonexistent/eboot.bin"
#endif
#ifndef ADAPTER_FIXTURE_MODULE
#define ADAPTER_FIXTURE_MODULE "/nonexistent/ue4ss.so"
#endif

using namespace lmf;
using RC::Platform::LmfConversionError;
using RC::Platform::LmfImageBaseProvenance;
using RC::Platform::LmfConversionOptions;
using RC::Platform::NativeLoaderFacts;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        ++g_checks;                                                     \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                               \
        }                                                               \
    } while (0)

// ---------------------------------------------------------------------------
// Fake OS ops (same pattern as worker-1's host tests; rewritten here).
// ---------------------------------------------------------------------------
struct FakeOps {
    LinkMapEntry entries[8];
    size_t entry_count = 0;
    int link_map_rc = 0;
    void *retain_result = reinterpret_cast<void *>(0xAAA1);
    int info_rc = 0;
    bool echo_size = true;
    bool info_segments_from_region = false;  // T16: use real mmap region
    void *region_base = nullptr;
    size_t region_size = 0;
    sce::ModuleInfo info_to_return{};
    int vmentry_rc = 0;
    OsVmMapping mappings[16];
    size_t mapping_count = 0;
    const char *exec_path = ADAPTER_FIXTURE_EBOOT;
    int pid = 4242;
    bool bind_exec_handle = false;
    int exec_handle_rc = 0;
    sce::ModuleHandle exec_handle_value = 77;
    bool bind_scan = false;  // N1..N5: route the REAL vendored base scan
};

static FakeOps *g_fake = nullptr;

// N1: capture-order log. Tags: 1 = query_vmentry, 2 = link_map_snapshot,
// 3 = scan_image_base, 4 = get_executable_module_handle.
static int g_order[8];
static int g_order_n = 0;
static void order_mark(int tag) {
    if (g_order_n < 8) g_order[g_order_n++] = tag;
}
static int order_find(int tag) {
    for (int i = 0; i < g_order_n; ++i)
        if (g_order[i] == tag) return i;
    return -1;
}

static int fake_link_map(void *, LinkMapEntry *out, size_t cap, size_t *count) {
    order_mark(2);
    *count = g_fake->entry_count;
    for (size_t i = 0; i < g_fake->entry_count && i < cap; ++i) out[i] = g_fake->entries[i];
    return g_fake->link_map_rc;
}
static void *fake_retain(void *, const char *) { return g_fake->retain_result; }
static int fake_release(void *, void *) { return 0; }
static int fake_get_module_info(void *, sce::ModuleHandle, sce::ModuleInfo *info) {
    *info = g_fake->info_to_return;
    info->size = g_fake->echo_size ? sizeof(sce::ModuleInfo) : 31337;
    if (g_fake->info_segments_from_region && info->segmentCount >= 1) {
        info->segmentInfo[0].address = g_fake->region_base;
        info->segmentInfo[0].size = g_fake->region_size;
    }
    return g_fake->info_rc;
}
static int fake_vmentry(void *, OsVmMapping *out, size_t cap, size_t *count) {
    order_mark(1);  // N1: VMMAP must be FIRST
    *count = g_fake->mapping_count;
    for (size_t i = 0; i < g_fake->mapping_count && i < cap; ++i) out[i] = g_fake->mappings[i];
    return g_fake->vmentry_rc;
}
static int fake_exec_path(void *, char *out, size_t cap) {
    if (g_fake->exec_path == nullptr) return -1;
    std::snprintf(out, cap, "%s", g_fake->exec_path);
    return 0;
}
static int fake_get_pid(void *) { return g_fake->pid; }
static int fake_exec_handle(void *, sce::ModuleHandle *out) {
    order_mark(4);
    *out = g_fake->exec_handle_value;
    return g_fake->exec_handle_rc;
}

// N-series: the REAL vendored base scan driven through the ops slot, with a
// host raw-read callback over genuinely mapped fixture pages. The scan does
// the span validation itself (no dereference beyond span-validated reads).
static size_t host_raw_read(void *, uint64_t addr, void *dst, size_t len) {
    std::memcpy(dst, reinterpret_cast<const void *>(static_cast<uintptr_t>(addr)), len);
    return len;
}
// Simulates an unreadable image: every raw read refuses (returns 0).
static size_t refusing_read(void *, uint64_t, void *, size_t) { return 0; }
static VmScanCandidate g_scan_cands[kVmScanCandidateCapacity];
static int fake_scan_image_base(void *, const OsVmMapping *m, size_t count,
                                VmScanResult *out) {
    order_mark(3);
    VmScanSpan spans[16];
    if (count > 16) count = 16;
    for (size_t i = 0; i < count; ++i) {
        spans[i].start = m[i].start;
        spans[i].end = m[i].end;
        spans[i].protection = static_cast<uint32_t>(m[i].protection);
    }
    *out = lmf_scan_image_bases(spans, count, host_raw_read, nullptr,
                                g_scan_cands, kVmScanCandidateCapacity);
    return 0;
}

static LiveModuleOsOps make_ops(bool bind_exec_handle, bool bind_scan) {
    LiveModuleOsOps ops{};
    ops.link_map_snapshot = fake_link_map;
    ops.loader_retain_noload = fake_retain;
    ops.loader_release = fake_release;
    ops.get_module_info = fake_get_module_info;
    ops.query_vmentry = fake_vmentry;
    ops.executable_path = fake_exec_path;
    ops.get_pid = fake_get_pid;
    ops.get_executable_module_handle = bind_exec_handle ? fake_exec_handle : nullptr;
    ops.scan_image_base = bind_scan ? fake_scan_image_base : nullptr;
    return ops;
}
static LiveModuleOsOps make_ops(bool bind_exec_handle) {
    return make_ops(bind_exec_handle, false);
}

static LiveFirmwareProfile default_profile() {
    LiveFirmwareProfile p{};
    p.module_info_abi_size = 352;
    p.module_segment_capacity = 4;
    p.fw_version = 0;
    p.allow_loader_handle_as_module_handle = false;
    std::strncpy(p.exe_name_suffix, "eboot.bin", sizeof(p.exe_name_suffix) - 1);
    return p;
}

// Default geometry (integers; deliberately non-dereferenceable):
// image span [0x400000, 0x780000), mappings cover [0x400000, 0x700008).
static void fill_valid_info(sce::ModuleInfo &info) {
    info = {};
    std::strncpy(info.name, "eboot.bin", sizeof(info.name) - 1);
    info.segmentCount = 3;
    info.segmentInfo[0] = {reinterpret_cast<void *>(0x400000), 0x100000, 5};
    info.segmentInfo[1] = {reinterpret_cast<void *>(0x500000), 0x200000, 3};
    info.segmentInfo[2] = {reinterpret_cast<void *>(0x700000), 0x80000, 4};
    for (int i = 0; i < 20; ++i) info.fingerprint[i] = static_cast<uint8_t>(0xA0 + i);
}

static FakeOps default_fake() {
    FakeOps f{};
    f.entries[0] = {"libkernel.so", reinterpret_cast<void *>(0x1000)};
    f.entries[1] = {"/app0/eboot.bin", reinterpret_cast<void *>(0x400000)};
    f.entries[2] = {"libScePad.sprx", reinterpret_cast<void *>(0x9000)};
    f.entry_count = 3;
    fill_valid_info(f.info_to_return);
    f.mappings[0] = {0x400000, 0x500000, 5};
    f.mappings[1] = {0x500000, 0x700008, 3};
    f.mapping_count = 2;
    f.bind_exec_handle = true;
    return f;
}

static const std::filesystem::path kModulePath = ADAPTER_FIXTURE_MODULE;

// ---------------------------------------------------------------------------
// T1: successful conversion of a synthetic Ok snapshot (full facts).
// ---------------------------------------------------------------------------
static void test_conversion_success(RetainedLiveModuleFactsSupplier &supplier) {
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(result.converted);
    CHECK(result.error == LmfConversionError::None);
    if (!result.converted) return;
    const auto &facts = result.facts;

    CHECK(facts.module_file_path == kModulePath);
    CHECK(facts.game_executable_path == std::string(ADAPTER_FIXTURE_EBOOT));

    CHECK(facts.mapped.format == PS_MAPPED_IMAGE_ELF64);
    CHECK(facts.mapped.text_base == 0);      // never supplied: unresolved
    CHECK(facts.mapped.image_base == 0);     // ABI: ELF64 image_base is zero
    CHECK(facts.mapped.eh_frame.address == 0 && facts.mapped.eh_frame.size == 0);
    CHECK(facts.mapped.eh_frame_hdr.address == 0 && facts.mapped.eh_frame_hdr.size == 0);
    CHECK(facts.mapped.pe_exception_directory.address == 0 && facts.mapped.pe_exception_directory.size == 0);

    CHECK(result.image_base_provenance == LmfImageBaseProvenance::ModuleInfoSpan);
    CHECK(result.resolved_image_base == 0x400000);
    CHECK(result.resolved_image_end == 0x780000);

    CHECK(facts.modules.lifetime.operator bool());
    CHECK(facts.modules.lifetime.get() == static_cast<const void *>(&supplier));  // non-owning alias
    CHECK(facts.modules.monolithic);
    CHECK(facts.modules.images.size() == 1);
    CHECK(facts.modules.images.count(RC::ScanTarget::MainExe) == 1);
    const auto &image = facts.modules.images.at(RC::ScanTarget::MainExe);
    CHECK(image.base == reinterpret_cast<const uint8_t *>(0x400000));
    CHECK(image.size == 0x380000);
    // Only mappings CONTAINING observed_base (0x400000) become anchor
    // mappings, so mappings[1] ([0x500000,0x700008)) is not retained by the
    // supplier; the single clamped anchor range is [0x400000,0x500000).
    CHECK(image.readable_ranges.size() == 1);
    CHECK(image.readable_ranges[0].data == reinterpret_cast<const uint8_t *>(0x400000));
    CHECK(image.readable_ranges[0].size == 0x100000);

    CHECK(!facts.modules.find_export);  // no export resolver: documented FindExport throw path
    CHECK(!facts.input_source);
    CHECK(facts.scan_policy == RC::Unreal::Platform::CandidateSelectionPolicy::OriginalOnly);

    // Real host-runnable factory validation: geometry must survive
    // ModuleSnapshot::Create (the exact check MakeNativeProgramContext runs).
    auto snapshot = RC::Unreal::Platform::ModuleSnapshot::Create(facts.modules);
    CHECK(snapshot->IsMonolithic());
    CHECK(snapshot->ReadableRanges().at(RC::ScanTarget::MainExe).size() == 1);
}

// T1b: defensive merge of genuinely overlapping anchor mappings.
static void test_overlapping_mappings_merged() {
    FakeOps f = default_fake();
    f.mappings[0] = {0x3F0000, 0x480000, 5};  // both contain observed_base
    f.mappings[1] = {0x400000, 0x500000, 5};
    f.mapping_count = 2;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::Ok);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(result.converted);
    const auto &image = result.facts.modules.images.at(RC::ScanTarget::MainExe);
    // Clamped to the module span [0x400000,0x780000) and overlap-merged.
    CHECK(image.readable_ranges.size() == 1);
    CHECK(image.readable_ranges[0].data == reinterpret_cast<const uint8_t *>(0x400000));
    CHECK(image.readable_ranges[0].size == 0x100000);
    auto snapshot = RC::Unreal::Platform::ModuleSnapshot::Create(result.facts.modules);
    CHECK(snapshot->ReadableRanges().at(RC::ScanTarget::MainExe).size() == 1);
}

// T2: fresh supplier, capture() never called.
static void test_not_captured() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierNotCaptured);
    CHECK(!result.facts.modules.lifetime);  // no partial output
}

// T3: RTLD_NOLOAD retain failed -> zero facts.
static void test_retain_failed() {
    FakeOps f = default_fake();
    f.retain_result = nullptr;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::RetainFailed);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierRetainFailed);
}

// T4: no link-map anchor and no explicit name (never dlopen(NULL)).
static void test_no_anchor() {
    FakeOps f = default_fake();
    f.entries[0] = {"libkernel.so", reinterpret_cast<void *>(0x1000)};
    f.entry_count = 1;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::NoImageAnchor);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierNoImageAnchor);
}

// T5: degraded capture (no exec-handle bridge) is retention+identity only and
// must never convert as full facts — the opt-in flag cannot bypass that gate.
static void test_degraded_module_info_not_obtained() {
    FakeOps f = default_fake();
    f.bind_exec_handle = false;
    g_fake = &f;
    auto ops = make_ops(false);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::ModuleInfoNotObtained);
    auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierModuleInfoNotObtained);

    LmfConversionOptions opt_in{};
    opt_in.allow_observed_base_as_module_image_base = true;
    result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath, opt_in);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierModuleInfoNotObtained);
}

// T6: kernel module info violating the pinned ABI (size echo) -> rejected.
static void test_module_info_rejected() {
    FakeOps f = default_fake();
    f.echo_size = false;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::ModuleInfoRejected);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierModuleInfoRejected);
}

// T7: invalid ops table -> InvalidArgument status.
static void test_invalid_ops() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops(true);
    ops.link_map_snapshot = nullptr;
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::InvalidArgument);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierInvalidArgument);
}

// T8: sysctl executable path absent -> refuse, never fabricate.
static void test_executable_path_missing() {
    FakeOps f = default_fake();
    f.exec_path = nullptr;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::Ok);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::GameExecutablePathMissing);
}

// T9: observed executable path not absolute/normalized.
static void test_executable_path_invalid() {
    for (const char *bad : {"app0/eboot.bin", "/app0/../eboot.bin", "/app0/"}) {
        FakeOps f = default_fake();
        f.exec_path = bad;
        g_fake = &f;
        auto ops = make_ops(true);
        RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
        CHECK(supplier.capture() == Status::Ok);
        const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
        CHECK(!result.converted);
        CHECK(result.error == LmfConversionError::GameExecutablePathInvalid);
    }
}

// T10: caller-supplied module path argument validation.
static void test_module_path_invalid() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::Ok);
    for (const char *bad : {"", "relative/ue4ss.so", "/sp/../ue4ss.so", "/dir/"}) {
        const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, bad);
        CHECK(!result.converted);
        CHECK(result.error == LmfConversionError::ModuleFilePathInvalid);
    }
}

// T11: Ok snapshot without any observed mapping -> refuse (ModuleSnapshot
// requires non-empty readable ranges).
static void test_no_mappings() {
    FakeOps f = default_fake();
    f.mapping_count = 0;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::Ok);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::NoReadableMappings);
}

// T12: mappings exist but none intersect the module image span.
static void test_mappings_disjoint_from_span() {
    FakeOps f = default_fake();
    // Anchor base stays 0x400000 (mappings contain it) but the authoritative
    // module span is moved to [0xB00000, 0xC00000).
    f.info_to_return.segmentInfo[0] = {reinterpret_cast<void *>(0xB00000), 0x100000, 5};
    f.info_to_return.segmentInfo[1] = {reinterpret_cast<void *>(0x000000), 0, 0};
    f.info_to_return.segmentInfo[2] = {reinterpret_cast<void *>(0x000000), 0, 0};
    f.info_to_return.segmentCount = 1;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::Ok);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::InvalidReadableMappings);
}

// T14: throwing form.
static void test_throwing_form(RetainedLiveModuleFactsSupplier &ok_supplier,
                               RetainedLiveModuleFactsSupplier &bad_supplier) {
    auto facts = RC::Platform::MakeNativeLoaderFactsFromSupplier(ok_supplier, kModulePath);
    CHECK(facts.mapped.format == PS_MAPPED_IMAGE_ELF64);
    bool threw = false;
    try {
        (void)RC::Platform::MakeNativeLoaderFactsFromSupplier(bad_supplier, kModulePath);
    } catch (const std::invalid_argument &e) {
        threw = true;
        CHECK(std::strstr(e.what(), "retain failed") != nullptr);
    }
    CHECK(threw);
}

// T15: owning shared_ptr overload keeps the supplier genuinely alive.
static void test_owning_overload() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops(true);  // named: the supplier borrows it by reference
    auto supplier = std::make_shared<RetainedLiveModuleFactsSupplier>(ops, default_profile());
    CHECK(supplier->capture() == Status::Ok);
    auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(
        std::shared_ptr<const RetainedLiveModuleFactsSupplier>{supplier}, kModulePath);
    CHECK(result.converted);
    CHECK(result.facts.modules.lifetime.get() == supplier.get());
    auto keep = result.facts.modules.lifetime;
    supplier.reset();
    result.facts = NativeLoaderFacts{};  // drop the facts, keep one alias
    CHECK(keep.operator bool());
    CHECK(keep.get() != nullptr);  // supplier still alive through the alias
    keep.reset();                  // last alias: supplier destroyed here (balanced dtor)
}

// T16: real factory probes with genuinely mapped host pages. Records exactly
// which MakeNativeProgramContext validations pass on host and which need the
// target (see REPORT.md refusal/deviation tables).
static void test_factory_validations() {
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const size_t size = 3 * page;
    void *region = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    CHECK(region != MAP_FAILED);
    if (region == MAP_FAILED) return;
    std::memset(region, 0, size);
    // Register the pages with the real owned-memory service (readable).
    PLH::OwnedMemory::ScopedRegion registration(region, size, PLH::R);

    FakeOps f = default_fake();
    f.info_segments_from_region = true;
    f.region_base = region;
    f.region_size = size;
    f.info_to_return.segmentCount = 1;
    const auto base = reinterpret_cast<uintptr_t>(region);
    f.entries[1] = {"/app0/eboot.bin", region};
    f.mappings[0] = {base, base + size, 5};
    f.mapping_count = 1;
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::Ok);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(result.converted);
    if (!result.converted) return;
    CHECK(result.resolved_image_base == base);
    CHECK(result.resolved_image_end == base + size);

    // Probe A: adapter output as-is (text_base unresolved). Everything up to
    // the mapped-text check must pass on host: ELF64 format gate,
    // ModuleSnapshot::Create geometry, both path validations (the fixture
    // files exist), ReadLease over registered pages, span coverage.
    {
        auto facts = result.facts;
        bool threw = false;
        std::string message;
        try {
            auto context = RC::Platform::MakeNativeProgramContext(facts);
            (void)context;
        } catch (const std::invalid_argument &e) {
            threw = true;
            message = e.what();
        }
        CHECK(threw);
        const bool text_stage = message.find("Mapped text base") != std::string::npos ||
                                message.find("Mapped metadata or spans exceed") != std::string::npos;
        CHECK(text_stage);
        std::printf("probe A (adapter facts, text_base unresolved) threw: %s\n", message.c_str());
    }

    // Probe B (deviation proof): a candidate in-span text_base with a NONZERO
    // ELF64 image_base must be rejected by the immutable ABI rule, proving
    // why the adapter stores 0 in mapped.image_base.
    {
        auto facts = result.facts;
        facts.mapped.text_base = base + 0x10;
        facts.mapped.image_base = result.resolved_image_base;
        bool threw = false;
        std::string message;
        try {
            auto context = RC::Platform::MakeNativeProgramContext(facts);
            (void)context;
        } catch (const std::invalid_argument &e) {
            threw = true;
            message = e.what();
        }
        CHECK(threw);
        CHECK(message == "Mapped metadata does not match format");
        std::printf("probe B (ELF64 nonzero image_base) threw: %s\n", message.c_str());
    }

    // Probe C: candidate in-span text_base, image_base correctly zero. The
    // pages are registered readable-only, so the remaining gap is executable
    // registration — a genuine target-side fact.
    {
        auto facts = result.facts;
        facts.mapped.text_base = base + 0x10;
        bool threw = false;
        std::string message;
        try {
            auto context = RC::Platform::MakeNativeProgramContext(facts);
            (void)context;
        } catch (const std::invalid_argument &e) {
            threw = true;
            message = e.what();
        }
        CHECK(threw);
        CHECK(message == "Mapped text base must be registered readable executable bytes");
        std::printf("probe C (text_base readable-only) threw: %s\n", message.c_str());
    }
}

// ---------------------------------------------------------------------------
// N-series: in-game update checks (checkpoint zcode_ingame_v1_results).
// Real mmap'd fixture pages carry a synthetic ELF64 image so the VENDORED
// base scan parses real bytes through its span-validation path.
// ---------------------------------------------------------------------------
struct ScanFixture {
    void *main_region = nullptr;   // 4 pages: header r--, code r-x
    size_t main_size = 0;
    void *decoy_region = nullptr;  // 2 pages: smaller valid ELF (rx)
    size_t decoy_size = 0;
    void *junk_region = nullptr;   // 1 page: rx, no ELF magic
    size_t junk_size = 0;
    uint64_t main_base = 0;
    uint64_t decoy_base = 0;
    uint64_t junk_base = 0;
};

static void poke64(unsigned char *p, size_t off, uint64_t v) {
    for (int i = 0; i < 8; ++i)
        p[off + static_cast<size_t>(i)] =
            static_cast<unsigned char>((v >> (8 * i)) & 0xff);
}
static void poke32(unsigned char *p, size_t off, uint32_t v) {
    poke64(p, off, v);
}
static void poke16(unsigned char *p, size_t off, uint16_t v) {
    poke64(p, off, v);
}

// Builds an ET_DYN ELF64 with 3 phdrs at e_phoff=64 (LOAD r--, LOAD r+x,
// DYNAMIC) and a 2-entry dynamic (DT_DEBUG, DT_NULL) at dyn_vaddr.
// Mode-1 layout: vaddrs relative to the region start.
static void build_fixture_elf(unsigned char *p, uint64_t load0_memsz,
                              uint64_t load1_vaddr, uint64_t load1_memsz,
                              uint64_t dyn_vaddr, size_t dyn_filesz) {
    p[0] = 0x7f; p[1] = 'E'; p[2] = 'L'; p[3] = 'F';
    p[4] = 2;   // ELFCLASS64
    p[5] = 1;   // ELFDATA2LSB
    p[6] = 1;   // EV_CURRENT
    poke16(p, 16, 3);    // e_type = ET_DYN
    poke16(p, 18, 0x3e); // e_machine (arbitrary fixture value)
    poke32(p, 20, 1);    // e_version
    poke64(p, 32, 64);   // e_phoff
    poke16(p, 52, 64);   // e_ehsize
    poke16(p, 54, 56);   // e_phentsize
    poke16(p, 56, 3);    // e_phnum
    // ph0: PT_LOAD, PF_R (4), vaddr 0, memsz load0_memsz
    poke32(p, 64 + 0, 1);   poke32(p, 64 + 4, 4);
    poke64(p, 64 + 8, 0);   poke64(p, 64 + 16, 0);
    poke64(p, 64 + 24, 0);  poke64(p, 64 + 32, load0_memsz);
    poke64(p, 64 + 40, load0_memsz); poke64(p, 64 + 48, 0x1000);
    // ph1: PT_LOAD, PF_R|PF_X (5), vaddr load1_vaddr, memsz load1_memsz
    poke32(p, 120 + 0, 1);  poke32(p, 120 + 4, 5);
    poke64(p, 120 + 8, load1_vaddr);  poke64(p, 120 + 16, load1_vaddr);
    poke64(p, 120 + 24, load1_vaddr); poke64(p, 120 + 32, load1_memsz);
    poke64(p, 120 + 40, load1_memsz); poke64(p, 120 + 48, 0x1000);
    // ph2: PT_DYNAMIC at dyn_vaddr, filesz dyn_filesz
    poke32(p, 176 + 0, 2);  poke32(p, 176 + 4, 0);
    poke64(p, 176 + 8, dyn_vaddr);    poke64(p, 176 + 16, dyn_vaddr);
    poke64(p, 176 + 24, dyn_vaddr);   poke64(p, 176 + 32, dyn_filesz);
    poke64(p, 176 + 40, dyn_filesz);  poke64(p, 176 + 48, 8);
    // Dynamic entries: DT_DEBUG (21) value 0, then DT_NULL.
    const size_t d = static_cast<size_t>(dyn_vaddr);
    poke64(p, d + 0, 21);  poke64(p, d + 8, 0);
    poke64(p, d + 16, 0);  poke64(p, d + 24, 0);
}

static bool scan_fixture_init(ScanFixture &fx) {
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    fx.main_size = 4 * page;
    fx.decoy_size = 2 * page;
    fx.junk_size = page;
    fx.main_region = ::mmap(nullptr, fx.main_size, PROT_READ | PROT_WRITE,
                            MAP_ANON | MAP_PRIVATE, -1, 0);
    fx.decoy_region = ::mmap(nullptr, fx.decoy_size, PROT_READ | PROT_WRITE,
                             MAP_ANON | MAP_PRIVATE, -1, 0);
    fx.junk_region = ::mmap(nullptr, fx.junk_size, PROT_READ | PROT_WRITE,
                            MAP_ANON | MAP_PRIVATE, -1, 0);
    if (fx.main_region == MAP_FAILED || fx.decoy_region == MAP_FAILED ||
        fx.junk_region == MAP_FAILED)
        return false;
    std::memset(fx.main_region, 0, fx.main_size);
    std::memset(fx.decoy_region, 0, fx.decoy_size);
    std::memset(fx.junk_region, 0xCC, fx.junk_size);
    fx.main_base = reinterpret_cast<uintptr_t>(fx.main_region);
    fx.decoy_base = reinterpret_cast<uintptr_t>(fx.decoy_region);
    fx.junk_base = reinterpret_cast<uintptr_t>(fx.junk_region);
    // Main image: header r-- [0,2pages), code r+x [2pages,4pages);
    // extent = 3 pages; PT_DYNAMIC at vaddr page (inside the r-- span).
    build_fixture_elf(static_cast<unsigned char *>(fx.main_region),
                      2 * page, 2 * page, page, page, 32);
    // Decoy: single rx span, extent = 1 page (smaller: must NOT be chosen).
    build_fixture_elf(static_cast<unsigned char *>(fx.decoy_region),
                      page, page, 0, page / 2, 16);
    return true;
}

static void scan_fixture_destroy(ScanFixture &fx) {
    if (fx.main_region && fx.main_region != MAP_FAILED) ::munmap(fx.main_region, fx.main_size);
    if (fx.decoy_region && fx.decoy_region != MAP_FAILED) ::munmap(fx.decoy_region, fx.decoy_size);
    if (fx.junk_region && fx.junk_region != MAP_FAILED) ::munmap(fx.junk_region, fx.junk_size);
}

// Fake ops mapping list describing the fixture exactly like an in-game
// KERN_PROC_VMMAP report would (bit0 readable, bit2 executable).
static void fill_fixture_mappings(FakeOps &f, const ScanFixture &fx) {
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    f.mappings[0] = {fx.main_base, fx.main_base + 2 * page, 1};          // r--
    f.mappings[1] = {fx.main_base + 2 * page, fx.main_base + 4 * page, 5};  // r-x
    f.mappings[2] = {fx.decoy_base, fx.decoy_base + 2 * page, 5};        // r-x decoy
    f.mappings[3] = {fx.junk_base, fx.junk_base + page, 5};              // r-x junk
    f.mapping_count = 4;
}

// N1: VMMAP-first capture. query_vmentry is called before link_map_snapshot
// and exactly once per capture.
static void test_vmmap_first_order() {
    g_order_n = 0;
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops(true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::Ok);
    const int vm = order_find(1), lm = order_find(2);
    CHECK(vm >= 0 && lm >= 0);
    CHECK(vm < lm);  // VMMAP FIRST (observed primary source, g5)
    int vm_calls = 0;
    for (int i = 0; i < g_order_n; ++i) vm_calls += g_order[i] == 1;
    CHECK(vm_calls == 1);  // single capture pass; correspondence reuses it
}

// N2: both observed-dead routes refused with DISTINCT codes citing the
// checkpoint facts; capture survives and identity comes from the scan.
static void test_dead_routes_distinct_refusals() {
    ScanFixture fx;
    if (!scan_fixture_init(fx)) { CHECK(false && "fixture mmap failed"); return; }
    // N2a: DT_DEBUG route refused (g2), exec-handle slot unbound.
    {
        FakeOps f = default_fake();
        f.entry_count = 0;
        f.link_map_rc = LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame;
        f.bind_exec_handle = false;
        f.retain_result = nullptr;  // degraded dl retention (composition)
        fill_fixture_mappings(f, fx);
        f.bind_scan = true;
        g_fake = &f;
        auto ops = make_ops(false, true);
        RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
        CHECK(supplier.capture() == Status::ModuleInfoNotObtained);
        const auto &facts = supplier.facts();
        CHECK(facts.capabilities.link_map_walk == ProbeResult::Failed);
        CHECK(facts.provider_code == LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame);
        CHECK(facts.identity_source == IdentitySource::VmScan);
        CHECK(facts.capabilities.vmentry == ProbeResult::Ok);
    }
    // N2b: exec-handle route ALSO refused (g1) with the OTHER distinct code;
    // the later refusal is the one recorded in provider_code.
    {
        FakeOps f = default_fake();
        f.entry_count = 0;
        f.link_map_rc = LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame;
        f.bind_exec_handle = true;
        f.exec_handle_rc = LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame;
        f.retain_result = nullptr;
        fill_fixture_mappings(f, fx);
        f.bind_scan = true;
        g_fake = &f;
        auto ops = make_ops(true, true);
        RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
        CHECK(supplier.capture() == Status::ModuleInfoNotObtained);
        const auto &facts = supplier.facts();
        CHECK(facts.capabilities.exec_handle == ProbeResult::Failed);
        CHECK(facts.provider_code == LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame);
        CHECK(facts.capabilities.link_map_walk == ProbeResult::Failed);
        CHECK(LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame !=
              LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame);
        // Observed-fact citation sanity (values mirror the JSONL records).
        CHECK(LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame == -4001);
        CHECK(LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame == -4002);
    }
    scan_fixture_destroy(fx);
}

// N3: the vendored V2 base scan directly over the fixture spans: candidate
// classification, span-validated ELF parse, ranking, provenance.
static void test_base_scan_helper() {
    ScanFixture fx;
    if (!scan_fixture_init(fx)) { CHECK(false && "fixture mmap failed"); return; }
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    VmScanSpan spans[4] = {
        {fx.main_base, fx.main_base + 2 * page, 1},
        {fx.main_base + 2 * page, fx.main_base + 4 * page, 5},
        {fx.decoy_base, fx.decoy_base + 2 * page, 5},
        {fx.junk_base, fx.junk_base + page, 5},
    };
    VmScanCandidate cands[kVmScanCandidateCapacity];
    const VmScanResult r = lmf_scan_image_bases(
        spans, 4, host_raw_read, nullptr, cands, kVmScanCandidateCapacity);
    // 4 candidates: main r-- (r_adjacent_exec), main r-x (no magic: zeros),
    // decoy r-x, junk r-x.
    CHECK(r.candidate_count == 4);
    CHECK(r.chosen_index >= 0);
    CHECK(r.chosen_base == fx.main_base);        // largest extent wins
    CHECK(r.chosen_extent == 3 * page);          // 2p r-- + 1p r-x PT_LOADs
    CHECK(r.chosen_dyn_mode == 1);               // dyn at base+vaddr
    CHECK(r.chosen_dyn_addr == fx.main_base + page);
    CHECK(r.chosen_e_type == 3);                 // ET_DYN fixture
    CHECK(std::strcmp(r.provenance, "v2_elf_base_scan") == 0);
    const VmScanCandidate &main_c = cands[0];
    CHECK(std::strcmp(main_c.where, "r_adjacent_exec") == 0);
    CHECK(main_c.magic_ok && main_c.class_ok && main_c.phdr_ok);
    CHECK(main_c.has_exec_segment);
    CHECK(main_c.dyn_parses && main_c.dt_null_seen && main_c.dyn_entries == 2);
    CHECK(main_c.chosen);
    bool decoy_seen = false, junk_seen = false, main_rx_seen = false;
    for (uint32_t i = 0; i < r.candidate_count; ++i) {
        if (cands[i].base == fx.decoy_base) {
            decoy_seen = true;
            CHECK(cands[i].magic_ok && cands[i].dyn_parses && !cands[i].chosen);
        }
        if (cands[i].base == fx.junk_base) {
            junk_seen = true;
            CHECK(!cands[i].magic_ok && !cands[i].chosen);
        }
        if (cands[i].base == fx.main_base + 2 * page) {
            main_rx_seen = true;
            CHECK(std::strcmp(cands[i].where, "rx_span") == 0);
            CHECK(!cands[i].magic_ok && !cands[i].chosen);
        }
    }
    CHECK(decoy_seen && junk_seen && main_rx_seen);
    // Span-validation refusal: when the raw read refuses (unreadable image),
    // the candidate stays magic_ok == 0 and the result is needs-base — the
    // helper never guesses an identity.
    VmScanSpan bad_spans[1] = {{0x10, 0x10 + 2 * page, 5}};
    VmScanCandidate bad_c[4];
    const VmScanResult rb = lmf_scan_image_bases(bad_spans, 1, refusing_read,
                                                 nullptr, bad_c, 4);
    CHECK(rb.candidate_count == 1);
    CHECK(rb.chosen_index == -1);
    CHECK(std::strcmp(rb.provenance, "needs-base") == 0);
    CHECK(!bad_c[0].magic_ok);
    // Readability gating: a span without the readable bit never becomes a
    // candidate (and is therefore never read at all).
    VmScanSpan nr_spans[1] = {{fx.main_base, fx.main_base + 2 * page, 4}};
    const VmScanResult rn = lmf_scan_image_bases(nr_spans, 1, host_raw_read,
                                                 nullptr, bad_c, 4);
    CHECK(rn.candidate_count == 0 && rn.chosen_index == -1);
    scan_fixture_destroy(fx);
}

// N4: the scan result feeds a SUCCESSFUL conversion (adapter plants
// images[MainExe].base from the scan).
static void test_scan_fed_conversion() {
    ScanFixture fx;
    if (!scan_fixture_init(fx)) { CHECK(false && "fixture mmap failed"); return; }
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    FakeOps f = default_fake();
    f.entry_count = 0;
    f.link_map_rc = LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame;
    f.bind_exec_handle = true;
    f.exec_handle_rc = LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame;
    f.retain_result = nullptr;  // degraded dl retention, observed in-game g3
    fill_fixture_mappings(f, fx);
    f.bind_scan = true;
    g_fake = &f;
    auto ops = make_ops(true, true);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::ModuleInfoNotObtained);
    const auto &snap = supplier.facts();
    CHECK(snap.identity_source == IdentitySource::VmScan);
    CHECK(snap.vm_scan.chosen && snap.vm_scan.chosen_base == fx.main_base);
    CHECK(snap.vm_scan.chosen_extent == 3 * page);
    CHECK(snap.observed_base == reinterpret_cast<void *>(fx.main_base));
    CHECK(snap.retention.kind == RetainKind::None);  // honest degraded retention
    CHECK(snap.anchor_mapping_count == 1);           // r-- span contains base
    CHECK(supplier.validate_retention() == Status::AlreadyReleased);

    // Default options still refuse (degraded status is not full facts).
    auto refused = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!refused.converted);
    CHECK(refused.error == LmfConversionError::SupplierModuleInfoNotObtained);

    // Explicit in-game opt-in: the scan result becomes the image base.
    LmfConversionOptions opt{};
    opt.allow_vm_scan_base_as_module_image_base = true;
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath, opt);
    CHECK(result.converted);
    CHECK(result.error == LmfConversionError::None);
    if (!result.converted) { scan_fixture_destroy(fx); return; }
    CHECK(result.image_base_provenance == LmfImageBaseProvenance::VmScanBase);
    CHECK(result.resolved_image_base == fx.main_base);
    CHECK(result.resolved_image_end == fx.main_base + 3 * page);
    const auto &image = result.facts.modules.images.at(RC::ScanTarget::MainExe);
    CHECK(image.base == reinterpret_cast<const uint8_t *>(fx.main_base));
    CHECK(image.size == 3 * page);
    CHECK(image.readable_ranges.size() == 1);  // r-- span clamped to extent
    CHECK(image.readable_ranges[0].data == reinterpret_cast<const uint8_t *>(fx.main_base));
    CHECK(image.readable_ranges[0].size == 2 * page);
    CHECK(result.facts.mapped.format == PS_MAPPED_IMAGE_ELF64);
    CHECK(result.facts.mapped.image_base == 0);  // ELF64 ABI rule
    CHECK(result.facts.modules.monolithic);
    // Real factory geometry check with the genuinely mapped fixture pages.
    auto snapshot = RC::Unreal::Platform::ModuleSnapshot::Create(result.facts.modules);
    CHECK(snapshot->IsMonolithic());
    scan_fixture_destroy(fx);
}

// N5: no scan route + dead link-map + no explicit name -> honest
// NoImageAnchor (the VMMAP promotion never invents identity).
static void test_no_scan_no_anchor() {
    FakeOps f = default_fake();
    f.entry_count = 0;
    f.link_map_rc = LiveLoaderObservedFacts::kRefusalDtDebugUnfilledIngame;
    f.bind_exec_handle = true;
    f.exec_handle_rc = LiveLoaderObservedFacts::kRefusalExecHandleRawZeroIngame;
    f.bind_scan = false;  // no scan op bound (e.g. older composition)
    g_fake = &f;
    auto ops = make_ops(true, false);
    RetainedLiveModuleFactsSupplier supplier(ops, default_profile());
    CHECK(supplier.capture() == Status::NoImageAnchor);
    CHECK(supplier.facts().identity_source == IdentitySource::None);
    const auto result = RC::Platform::TryMakeNativeLoaderFactsFromSupplier(supplier, kModulePath);
    CHECK(!result.converted);
    CHECK(result.error == LmfConversionError::SupplierNoImageAnchor);
}

int main() {
    {
        FakeOps f = default_fake();
        g_fake = &f;
        auto ops = make_ops(true);
        RetainedLiveModuleFactsSupplier ok_supplier(ops, default_profile());
        CHECK(ok_supplier.capture() == Status::Ok);

        FakeOps bad = default_fake();
        bad.retain_result = nullptr;
        g_fake = &bad;
        auto bad_ops = make_ops(true);
        RetainedLiveModuleFactsSupplier bad_supplier(bad_ops, default_profile());
        CHECK(bad_supplier.capture() == Status::RetainFailed);

        test_conversion_success(ok_supplier);
        test_throwing_form(ok_supplier, bad_supplier);
        // ok_supplier/bad_supplier destroyed here; the aliasing lifetime token
        // from test_conversion_success was non-owning by design and its facts
        // copies went out of scope inside that test without touching it.
    }
    test_overlapping_mappings_merged();
    test_not_captured();
    test_retain_failed();
    test_no_anchor();
    test_degraded_module_info_not_obtained();
    test_module_info_rejected();
    test_invalid_ops();
    test_executable_path_missing();
    test_executable_path_invalid();
    test_module_path_invalid();
    test_no_mappings();
    test_mappings_disjoint_from_span();
    test_owning_overload();
    test_factory_validations();
    // N-series (in-game update).
    test_vmmap_first_order();
    test_dead_routes_distinct_refusals();
    test_base_scan_helper();
    test_scan_fed_conversion();
    test_no_scan_no_anchor();

    if (g_failures == 0) {
        std::printf("ALL S1 ADAPTER HOST TESTS PASSED (%d checks)\n", g_checks);
        return 0;
    }
    std::printf("S1 ADAPTER HOST TESTS FAILED: %d of %d checks failed\n", g_failures, g_checks);
    return 1;
}
