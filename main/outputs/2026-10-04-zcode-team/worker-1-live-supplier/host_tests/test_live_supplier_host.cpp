// test_live_supplier_host.cpp
//
// Host tests for the generic live-module-facts supplier core using injected
// fake OS ops. These tests verify CONTRACT BEHAVIOR ONLY (validation,
// retention accounting, honest-absence rules). No SDK symbol, no target
// behavior and no PS5 firmware fact is claimed by a passing test.
//
// Every fake address below (0x1000, 0x2000, ...) is deliberately
// non-dereferenceable host memory: the core treats addresses as integers,
// and a passing test proves it never read through them.
#include "../src/live_module_facts.hpp"
#include "../src/live_module_os_ops.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

using namespace lmf;

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                              \
        }                                                              \
    } while (0)

struct FakeOps {
    // link map
    LinkMapEntry entries[8];
    size_t entry_count = 0;
    int link_map_rc = 0;
    // loader
    void *retain_result = reinterpret_cast<void *>(0xAAA1);
    const char *last_retain_name = nullptr;
    int retains = 0;
    int releases = 0;
    void *last_released = nullptr;
    // module info
    int info_rc = 0;
    bool echo_size = true;
    sce::ModuleInfo info_to_return{};
    bool info_called = false;
    // vmentry
    int vmentry_rc = 0;
    OsVmMapping mappings[16];
    size_t mapping_count = 0;
    // exec path / pid
    const char *exec_path = "/system/exe_pkg/APP0001/eboot.bin";
    int pid = 4242;
    // optional exec handle probe
    bool bind_exec_handle = false;
    int exec_handle_rc = 0;
    sce::ModuleHandle exec_handle_value = 77;
    bool exec_handle_called = false;
};

static FakeOps *g_fake = nullptr;

static int fake_link_map(void *, LinkMapEntry *out, size_t cap, size_t *count) {
    *count = g_fake->entry_count;
    for (size_t i = 0; i < g_fake->entry_count && i < cap; ++i) out[i] = g_fake->entries[i];
    return g_fake->link_map_rc;
}
static void *fake_retain(void *, const char *name) {
    ++g_fake->retains;
    g_fake->last_retain_name = name;
    return g_fake->retain_result;
}
static int fake_release(void *, void *h) {
    ++g_fake->releases;
    g_fake->last_released = h;
    return 0;
}
static int fake_get_module_info(void *, sce::ModuleHandle h, sce::ModuleInfo *info) {
    g_fake->info_called = true;
    *info = g_fake->info_to_return;
    if (g_fake->echo_size) info->size = sizeof(sce::ModuleInfo);  // kernel echoes
    else info->size = 31337;  // kernel disagreed with the ABI size
    (void)h;
    return g_fake->info_rc;
}
static int fake_vmentry(void *, OsVmMapping *out, size_t cap, size_t *count) {
    *count = g_fake->mapping_count;  // total may exceed what we can supply
    size_t available = g_fake->mapping_count < 16 ? g_fake->mapping_count : 16;
    for (size_t i = 0; i < available && i < cap; ++i) out[i] = g_fake->mappings[i];
    return g_fake->vmentry_rc;
}
static int fake_exec_path(void *, char *out, size_t cap) {
    if (g_fake->exec_path == nullptr) return -1;
    std::snprintf(out, cap, "%s", g_fake->exec_path);
    return 0;
}
static int fake_get_pid(void *) { return g_fake->pid; }
static int fake_exec_handle(void *, sce::ModuleHandle *out) {
    g_fake->exec_handle_called = true;
    *out = g_fake->exec_handle_value;
    return g_fake->exec_handle_rc;
}

static LiveModuleOsOps make_ops() {
    LiveModuleOsOps ops{};
    ops.link_map_snapshot = fake_link_map;
    ops.loader_retain_noload = fake_retain;
    ops.loader_release = fake_release;
    ops.get_module_info = fake_get_module_info;
    ops.query_vmentry = fake_vmentry;
    ops.executable_path = fake_exec_path;
    ops.get_pid = fake_get_pid;
    ops.get_executable_module_handle = nullptr;
    return ops;
}

static LiveFirmwareProfile default_profile() {
    LiveFirmwareProfile p{};
    p.module_info_abi_size = 352;  // pinned prosperodev ABI (compile-asserted)
    p.module_segment_capacity = 4;
    p.fw_version = 0;              // unknown: never guessed
    p.allow_loader_handle_as_module_handle = false;
    std::strncpy(p.exe_name_suffix, "eboot.bin", sizeof(p.exe_name_suffix) - 1);
    return p;
}

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
    f.mappings[0] = {0x400000, 0x500000, 5};  // covers segment 0 exactly
    f.mappings[1] = {0x500000, 0x700008, 3};  // covers segments 1 and 2 partly
    f.mapping_count = 2;
    return f;
}

static void test_full_success_via_exec_handle_probe() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops();
    ops.get_executable_module_handle = fake_exec_handle;
    f.bind_exec_handle = true;
    f.exec_handle_rc = 0;
    f.exec_handle_value = 77;
    RetainedLiveModuleFactsSupplier s(ops, default_profile());
    CHECK(s.capture() == Status::Ok);
    const LiveModuleFactsSnapshot &fs = s.facts();
    CHECK(fs.pid == 4242);
    CHECK(fs.capabilities.pid == ProbeResult::Ok);
    CHECK(std::strcmp(fs.executable_path, "/system/exe_pkg/APP0001/eboot.bin") == 0);
    CHECK(std::strcmp(fs.image_name, "/app0/eboot.bin") == 0);
    CHECK(fs.image_name_from_link_map);
    CHECK(fs.observed_base == reinterpret_cast<void *>(0x400000));
    CHECK(fs.retention.kind == RetainKind::LoaderReference);
    CHECK(fs.retention.loader_handle == reinterpret_cast<void *>(0xAAA1));
    CHECK(std::strcmp(fs.retention.retained_name, "/app0/eboot.bin") == 0);
    CHECK(fs.module_info.obtained);
    CHECK(fs.module_info.handle_source == ModuleInfoFacts::kSourceExecHandle);
    CHECK(fs.module_info.handle == 77);
    CHECK(fs.module_info.segment_count == 3);
    CHECK(fs.module_info.image_span_begin == 0x400000);
    CHECK(fs.module_info.image_span_end == 0x780000);
    CHECK(fs.segment_correspondence_count == 3);
    CHECK(fs.segment_correspondence[0].covering_mappings == 1);
    CHECK(fs.segment_correspondence[0].fully_covered);
    CHECK(fs.segment_correspondence[1].covering_mappings == 1);
    CHECK(fs.segment_correspondence[1].fully_covered);  // 0x500000..0x700008 covers seg1
    CHECK(!fs.segment_correspondence[2].fully_covered); // 0x700000+0x80000 > 0x700008
    CHECK(fs.anchor_mapping_count == 1);  // observed_base 0x400000 inside map 0
    CHECK(fs.capabilities.module_info == ProbeResult::Ok);
    CHECK(fs.capabilities.exec_handle == ProbeResult::Ok);
    CHECK(f.exec_handle_called && f.info_called);
    // addresses were never dereferenced (test would have crashed otherwise)
    CHECK(s.release() == Status::Ok);
    CHECK(f.retains == 1 && f.releases == 1);
    CHECK(s.release() == Status::Ok);  // idempotent no-op, never a second dlclose
    CHECK(f.retains == 1 && f.releases == 1);
}

static void test_degraded_no_exec_handle_no_cast() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops();
    RetainedLiveModuleFactsSupplier s(ops, default_profile());
    CHECK(s.capture() == Status::ModuleInfoNotObtained);
    const LiveModuleFactsSnapshot &fs = s.facts();
    CHECK(!fs.module_info.obtained);
    CHECK(fs.module_info.handle_source == ModuleInfoFacts::kSourceNone);
    CHECK(fs.capabilities.module_info == ProbeResult::NotBound);
    CHECK(fs.capabilities.module_info_degraded);
    CHECK(fs.capabilities.exec_handle == ProbeResult::NotBound);
    CHECK(!f.info_called);  // no handle bridge: info API never invoked
    // retention and identity are still real
    CHECK(fs.retention.kind == RetainKind::LoaderReference);
    CHECK(fs.observed_base == reinterpret_cast<void *>(0x400000));
    CHECK(fs.anchor_mapping_count == 1);  // vmentry still corroborates base
    s.release();
}

static void test_loader_handle_cast_allowed() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops();
    auto profile = default_profile();
    profile.allow_loader_handle_as_module_handle = true;
    RetainedLiveModuleFactsSupplier s(ops, profile);
    CHECK(s.capture() == Status::Ok);
    CHECK(s.facts().module_info.obtained);
    CHECK(s.facts().module_info.handle_source == ModuleInfoFacts::kSourceLoaderHandleCast);
    s.release();
}

static void test_retain_failure_yields_no_facts() {
    FakeOps f = default_fake();
    f.retain_result = nullptr;
    g_fake = &f;
    auto ops = make_ops();
    RetainedLiveModuleFactsSupplier s(ops, default_profile());
    CHECK(s.capture() == Status::RetainFailed);
    CHECK(s.facts().retention.kind == RetainKind::None);
    CHECK(s.facts().capabilities.loader_retain == ProbeResult::Failed);
    CHECK(!f.info_called);  // no reference owned -> no downstream facts
    CHECK(s.release() == Status::Ok);  // nothing was owned: safe no-op
    CHECK(f.releases == 0);
}

static void test_no_anchor() {
    FakeOps f = default_fake();
    f.entries[1].name = "libother.so";  // reassign pointer, never write literals
    g_fake = &f;
    auto ops = make_ops();
    RetainedLiveModuleFactsSupplier s(ops, default_profile());
    CHECK(s.capture() == Status::NoImageAnchor);
    CHECK(s.facts().image_name[0] == '\0');
    CHECK(s.facts().observed_base == nullptr);
    CHECK(f.retains == 0);  // never falls back to dlopen(NULL)
    s.release();
}

static void test_explicit_name_with_and_without_corroboration() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops();
    RetainedLiveModuleFactsSupplier s(ops, default_profile());
    CHECK(s.capture("libkernel.so") == Status::ModuleInfoNotObtained);
    CHECK(std::strcmp(s.facts().image_name, "libkernel.so") == 0);
    CHECK(s.facts().image_name_from_link_map);  // corroborated by walk
    CHECK(s.facts().observed_base == reinterpret_cast<void *>(0x1000));
    s.release();
    FakeOps f2 = default_fake();
    g_fake = &f2;
    RetainedLiveModuleFactsSupplier s2(make_ops(), default_profile());
    CHECK(s2.capture("/not/in/link/map.bin") == Status::ModuleInfoNotObtained);
    CHECK(!s2.facts().image_name_from_link_map);
    CHECK(s2.facts().observed_base == nullptr);
    s2.release();
}

static void test_info_rejection_rules() {
    FakeOps base = default_fake();
    auto ops0 = make_ops();
    ops0.get_executable_module_handle = fake_exec_handle;
    // size echo mismatch
    {
        FakeOps f = base; g_fake = &f;
        f.echo_size = false;
        RetainedLiveModuleFactsSupplier s(ops0, default_profile());
        CHECK(s.capture() == Status::ModuleInfoRejected);
        CHECK(s.facts().capabilities.module_info == ProbeResult::Failed);
        CHECK(!s.facts().module_info.obtained);
        CHECK(s.facts().retention.kind == RetainKind::LoaderReference); // still owned
        s.release();
    }
    // provider error
    {
        FakeOps f = base; g_fake = &f;
        f.info_rc = 0x80020011;
        RetainedLiveModuleFactsSupplier s(ops0, default_profile());
        CHECK(s.capture() == Status::ModuleInfoRejected);
        CHECK(static_cast<uint32_t>(s.facts().provider_code) == 0x80020011u);
        s.release();
    }
    // unterminated name
    {
        FakeOps f = base; g_fake = &f;
        std::memset(f.info_to_return.name, 'x', sizeof(f.info_to_return.name));
        RetainedLiveModuleFactsSupplier s(ops0, default_profile());
        CHECK(s.capture() == Status::ModuleInfoRejected);
        s.release();
    }
    // zero segment count
    {
        FakeOps f = base; g_fake = &f;
        f.info_to_return.segmentCount = 0;
        RetainedLiveModuleFactsSupplier s(ops0, default_profile());
        CHECK(s.capture() == Status::ModuleInfoRejected);
        s.release();
    }
    // overflow segment end
    {
        FakeOps f = base; g_fake = &f;
        fill_valid_info(f.info_to_return);
        f.info_to_return.segmentInfo[1].address = reinterpret_cast<void *>(0xFFFFFFFFFFFF0000ULL);
        f.info_to_return.segmentInfo[1].size = 0x20000;
        RetainedLiveModuleFactsSupplier s(ops0, default_profile());
        CHECK(s.capture() == Status::ModuleInfoRejected);
        s.release();
    }
}

static void test_retention_lifecycle_and_probe() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops();
    {
        RetainedLiveModuleFactsSupplier s(ops, default_profile());
        CHECK(s.capture() == Status::ModuleInfoNotObtained);
        // probe: second retain returns same handle, immediately balanced
        CHECK(s.validate_retention() == Status::Ok);
        CHECK(f.retains == 2 && f.releases == 1);
        // probe with inconsistent handle
        void *other = reinterpret_cast<void *>(0xBBB2);
        f.retain_result = other;
        CHECK(s.validate_retention() == Status::RetentionProbeFailed);
        CHECK(f.retains == 3 && f.releases == 2);
        CHECK(f.last_released == other);  // the PROBE ref was released, not ours
        // probe with lost object
        f.retain_result = nullptr;
        CHECK(s.validate_retention() == Status::RetentionProbeFailed);
        CHECK(f.retains == 4 && f.releases == 2);
        // original reference still owned and releasable exactly once
        CHECK(s.release() == Status::Ok);
        CHECK(f.releases == 3 && f.last_released == reinterpret_cast<void *>(0xAAA1));
        CHECK(s.release() == Status::Ok);  // idempotent; exactly one dlclose happened
    }
    // destructor releases when caller forgot
    FakeOps f2 = default_fake();
    g_fake = &f2;
    {
        RetainedLiveModuleFactsSupplier s(make_ops(), default_profile());
        (void)s.capture();
        CHECK(f2.retains == 1 && f2.releases == 0);
    }
    CHECK(f2.releases == 1);  // destructor balanced the reference
}

static void test_move_semantics() {
    FakeOps f = default_fake();
    g_fake = &f;
    RetainedLiveModuleFactsSupplier a(make_ops(), default_profile());
    CHECK(a.capture() == Status::ModuleInfoNotObtained);  // no exec op, no cast
    RetainedLiveModuleFactsSupplier b(std::move(a));
    CHECK(b.facts().retention.kind == RetainKind::LoaderReference);
    b.release();
    CHECK(f.releases == 1);  // moved-from destructor must not double-release
}

static void test_vmentry_absence_and_truncation() {
    FakeOps f = default_fake();
    f.vmentry_rc = -3;  // sysctl failed on target
    g_fake = &f;
    auto ops0 = make_ops();
    ops0.get_executable_module_handle = fake_exec_handle;
    RetainedLiveModuleFactsSupplier s(ops0, default_profile());
    CHECK(s.capture() == Status::Ok);  // info obtained; mapping info absent
    CHECK(s.facts().capabilities.vmentry == ProbeResult::Failed);
    CHECK(s.facts().segment_correspondence_count == 0);  // absent, not invented
    CHECK(s.facts().anchor_mapping_count == 0);
    s.release();
    // truncation flag: more mappings than core capacity would allow
    FakeOps f2 = default_fake();
    g_fake = &f2;
    f2.mapping_count = 900;  // fake reports total 900 > kMaxVmMappings
    RetainedLiveModuleFactsSupplier s2(make_ops(), default_profile());
    (void)s2.capture();
    CHECK(s2.facts().capabilities.vm_mappings_truncated);
    s2.release();
}

static void test_invalid_arguments() {
    FakeOps f = default_fake();
    g_fake = &f;
    auto ops = make_ops();
    ops.get_pid = nullptr;  // required slot missing
    RetainedLiveModuleFactsSupplier s(ops, default_profile());
    CHECK(s.capture() == Status::InvalidArgument);
    auto bad_profile = default_profile();
    bad_profile.module_info_abi_size = 348;  // disagrees with pinned ABI
    RetainedLiveModuleFactsSupplier s2(make_ops(), bad_profile);
    CHECK(s2.capture() == Status::InvalidArgument);
    // validate before capture
    RetainedLiveModuleFactsSupplier s3(make_ops(), default_profile());
    CHECK(s3.validate_retention() == Status::NotCaptured);
}

int main() {
    test_full_success_via_exec_handle_probe();
    test_degraded_no_exec_handle_no_cast();
    test_loader_handle_cast_allowed();
    test_retain_failure_yields_no_facts();
    test_no_anchor();
    test_explicit_name_with_and_without_corroboration();
    test_info_rejection_rules();
    test_retention_lifecycle_and_probe();
    test_move_semantics();
    test_vmentry_absence_and_truncation();
    test_invalid_arguments();
    if (g_failures == 0) {
        std::printf("ALL LIVE SUPPLIER HOST TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURES\n", g_failures);
    return 1;
}
