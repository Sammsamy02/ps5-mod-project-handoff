// test_lmf_adapter_host.cpp
//
// Host tests for the S1 conversion adapter. These drive the REAL
// lmf::RetainedLiveModuleFactsSupplier core (worker-1 sources, injected fake
// OS ops) and the REAL immutable loader-context chain (spike
// NativeLoaderContext/OwnedMappedScanSource headers+sources,
// MakeNativeProgramContext, PLH owned-memory host implementation).
//
// Everything here is contract behavior only. No SDK symbol, no PS5 target
// fact, no console contact. Addresses in the fake-ops tests are deliberately
// non-dereferenceable integers; the factory-probe tests use genuinely mapped
// host pages (mmap) plus a registered owned-memory ScopedRegion so the real
// ReadLease path runs.
#include "lmf_to_native_loader_facts.hpp"
#include "live_module_os_ops.hpp"

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
};

static FakeOps *g_fake = nullptr;

static int fake_link_map(void *, LinkMapEntry *out, size_t cap, size_t *count) {
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
    *out = g_fake->exec_handle_value;
    return g_fake->exec_handle_rc;
}

static LiveModuleOsOps make_ops(bool bind_exec_handle) {
    LiveModuleOsOps ops{};
    ops.link_map_snapshot = fake_link_map;
    ops.loader_retain_noload = fake_retain;
    ops.loader_release = fake_release;
    ops.get_module_info = fake_get_module_info;
    ops.query_vmentry = fake_vmentry;
    ops.executable_path = fake_exec_path;
    ops.get_pid = fake_get_pid;
    ops.get_executable_module_handle = bind_exec_handle ? fake_exec_handle : nullptr;
    return ops;
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

    if (g_failures == 0) {
        std::printf("ALL S1 ADAPTER HOST TESTS PASSED (%d checks)\n", g_checks);
        return 0;
    }
    std::printf("S1 ADAPTER HOST TESTS FAILED: %d of %d checks failed\n", g_failures, g_checks);
    return 1;
}
