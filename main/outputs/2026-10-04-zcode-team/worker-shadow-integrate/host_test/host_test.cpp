// host_test.cpp — macOS host verification of the staged shadow integration
// (worker-shadow-integrate).
//
// Verifies the four admission questions against the REAL contracts and the
// REAL pinned artifacts:
//
//  [A] the compact SHA-256 (identity gate engine) passes FIPS test vectors
//  [I] the scanner initializes from the retained decrypted eboot: the
//      identity gate passes against SeedTable::kImageSha256 (generated header
//      included — not transcribed constants) and the shadow ModuleSource is
//      created through the REAL PlatformModuleSnapshot constructor
//  [E] the ModuleSnapshot extent contract is satisfied (and independently
//      recomputed), plus the exact SetupUnrealModules seam lines consume it
//  [P] patterns found in the shadow map to the correct live addresses —
//      both via scan_pattern AND via the REAL UE4SS owned-range
//      SinglePassScanner::start_scan (OwnedRangeScanner.inl compiled in a
//      mocked environment) over snapshot->ReadableRanges(), with the
//      ShadowSigAdapter bifurcation (live() store address vs bytes() decode)
//  [C] the four REQUIRED seeds cross-check green against the pinned
//      SeedTable values; tampered pins / non-unique sigs refuse loudly
//  [R] refusal honesty: no geometry, slide mismatch, identity mismatch,
//      seed/shadow disagreement — each refusal names both values
//  [O] optional slots: no pinned sigs -> "fallback retained" (existing
//      degradation paths stay); the resolution machinery itself is proven
//  [L] lifetime: releasing the snapshot runs shadow_shutdown (no leak)
//
// No console contact, no game claims. Exit 0 = all checks passed.

#include "../shadow_arm.hpp"
#include "../pinned/SeedTable.h" // the REAL generated seed table (launch11-combined units)

#include <SigScanner/SinglePassSigScanner.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace SeedTableNS = RC::Unreal::UnrealInitializer::SeedTable;

namespace
{

    int g_failures = 0;
    int g_checks = 0;

    void check(bool ok, const std::string &label, const std::string &detail = "")
    {
        ++g_checks;
        std::printf("[%s] %s%s%s\n", ok ? "PASS" : "FAIL", label.c_str(),
                    detail.empty() ? "" : " — ", detail.c_str());
        if (!ok)
        {
            ++g_failures;
        }
    }

    std::string hex(uint64_t v)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(v));
        return buf;
    }

    bool contains(const std::string &haystack, const char *needle)
    {
        return haystack.find(needle) != std::string::npos;
    }

    // ---- pinned SeedTable values (from the generated header, not retyped) ----
    constexpr uint64_t kPinnedBase = SeedTableNS::kBaseObserved; // 0x400000
    constexpr uint64_t kPinnedExtent = 0xbc602c8ull;             // game profile whole-image extent

    // Log sink: collect arm lines (also proves the injectable Output::send seam).
    std::vector<std::string> g_log_lines;
    void test_log_sink(void *, const char *msg)
    {
        g_log_lines.emplace_back(msg);
    }

    // Build the default pins from the REAL generated SeedTable.
    rc::shadowarm::SeedPins pins_from_seed_table()
    {
        namespace sa = rc::shadowarm;
        sa::SeedPins pins{};
        pins.base_observed = SeedTableNS::kBaseObserved;
        pins.image_sha256 = SeedTableNS::kImageSha256.data();
        pins.image_extent = kPinnedExtent;
        pins.required[0] = {&sa::kGuaXref, SeedTableNS::kGUObjectArray.label};
        pins.required[1] = {&sa::kFNameCtor, SeedTableNS::kFNameCtorWchar.label};
        pins.required[2] = {&sa::kGMallocStore, SeedTableNS::kGMalloc.label};
        pins.required[3] = {&sa::kScoPrologue, SeedTableNS::kStaticConstructObject.label};
        return pins;
    }
} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: %s <decrypted-eboot.bin>\n", argv[0]);
        return 2;
    }

    // ---- load the whole file (the PRX reads the same bytes via its file core)
    FILE *fh = std::fopen(argv[1], "rb");
    if (fh == nullptr)
    {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::fseek(fh, 0, SEEK_END);
    const long fsize = std::ftell(fh);
    std::fseek(fh, 0, SEEK_SET);
    uint8_t *file_bytes = static_cast<uint8_t *>(std::malloc(static_cast<size_t>(fsize)));
    if (file_bytes == nullptr || std::fread(file_bytes, 1, static_cast<size_t>(fsize), fh) != static_cast<size_t>(fsize))
    {
        std::fprintf(stderr, "read failed (%ld bytes)\n", fsize);
        return 2;
    }
    std::fclose(fh);
    std::printf("loaded %s: %ld bytes\n", argv[1], fsize);

    // =====================================================================
    // [A] SHA-256 self-test (FIPS 180-4 vectors) — the gate's engine
    // =====================================================================
    {
        char d[65];
        rc::shadowsource::sha256_hex(nullptr, 0, d); // null with 0 length is not dereferenced
        check(std::string(d) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
              "sha256(\"\") FIPS empty-vector digest", d);
        const uint8_t abc[] = {'a', 'b', 'c'};
        rc::shadowsource::sha256_hex(abc, sizeof(abc), d);
        check(std::string(d) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              "sha256(\"abc\") FIPS vector", d);
        // multi-block boundary: 64 bytes of 0x61 ("a"*64)
        uint8_t block64[64];
        std::memset(block64, 0x61, sizeof(block64));
        rc::shadowsource::sha256_hex(block64, sizeof(block64), d);
        check(std::string(d) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb",
              "sha256(64 bytes) block-boundary vector", d);
    }

    // =====================================================================
    // [I] identity gate against the pinned generated SeedTable
    // =====================================================================
    char fixture_hash[65] = {};
    {
        rc::shadowsource::sha256_hex(file_bytes, static_cast<size_t>(fsize), fixture_hash);
        check(std::string(fixture_hash) == std::string(SeedTableNS::kImageSha256),
              "decrypted eboot sha256 == SeedTable::kImageSha256 (generated header pin)",
              fixture_hash);
        check(rc::shadowsource::sha256_hex_equal(fixture_hash, "3B6CFAD47DD17FEA17D429BCE80AE374D1FD2EF8894672CCE63F5A26853622FC"),
              "pin comparison is case-insensitive");
    }

    // =====================================================================
    // [I]+[E] shadow ModuleSource over the REAL PlatformModuleSnapshot
    // =====================================================================
    std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> source_snapshot;
    [[maybe_unused]] const rc::shadowcopy::ShadowImage *source_img = nullptr;
    {
        auto r = rc::shadowsource::Create(file_bytes, static_cast<size_t>(fsize),
                                          kPinnedBase, SeedTableNS::kImageSha256.data());
        check(r.err == rc::shadowsource::SourceError::None && r.snapshot && r.image,
              "ShadowModuleSource::Create passes the REAL PlatformModuleSnapshot contract",
              "err=" + std::string(rc::shadowsource::source_error_name(r.err)) + " detail: " + r.detail);
        source_snapshot = r.snapshot;
        source_img = r.image;

        const auto &views = source_snapshot->Views();
        const auto &main = views.array[static_cast<size_t>(RC::ScanTarget::MainExe)];
        check(main.image_base == r.image->base && main.image_size == r.image->backing_size,
              "MainExe view: base == shadow base, size == backing_size",
              "base=" + hex(reinterpret_cast<uint64_t>(main.image_base)) + " size=" + hex(main.image_size));
        check(source_snapshot->IsMonolithic() && main.image_size == 0x9633418ull,
              "monolithic MainExe, pinned backing size 0x9633418", hex(main.image_size));

        // ReadableRanges() == per-PT_LOAD file-backed windows, in order.
        const auto &ranges = source_snapshot->ReadableRanges();
        const auto it = ranges.find(RC::ScanTarget::MainExe);
        bool ranges_ok = it != ranges.end() && it->second.size() == r.image->segment_count;
        for (size_t s = 0; ranges_ok && s < r.image->segment_count; ++s)
        {
            ranges_ok = it->second[s].data == r.image->segments[s].backing &&
                        it->second[s].size == r.image->segments[s].filesz;
        }
        check(ranges_ok, "ReadableRanges(): per-segment {backing, filesz} file-backed windows");

        // Independent recomputation of the extent contract (the constructor
        // already enforces it; assert it from first principles too).
        const uintptr_t base = reinterpret_cast<uintptr_t>(r.image->base);
        const uintptr_t end = base + r.image->backing_size;
        uintptr_t prev_end = base;
        bool sorted_ok = true, inside_ok = true;
        for (size_t s = 0; s < r.image->segment_count; ++s)
        {
            const uintptr_t b = reinterpret_cast<uintptr_t>(it->second[s].data);
            const uintptr_t e = b + it->second[s].size;
            if (b < prev_end || e > end)
                inside_ok = false;
            if (s > 0 && b <= reinterpret_cast<uintptr_t>(it->second[s - 1].data))
                sorted_ok = false;
            prev_end = e;
        }
        check(sorted_ok && inside_ok, "extent contract: sorted, non-overlapping, inside [base, base+size)");

        // find_export seam: configured, returns nullptr (no dynsym on this image).
        check(source_snapshot->FindExport("anything") == nullptr,
              "FindExport configured and returns nullptr (no dynsym; bridge source keeps that duty)");

        // The EXACT SetupUnrealModules seam lines (UnrealInitializer.cpp:317-318)
        // consume the snapshot unchanged.
        RC::SigScannerStaticData::m_modules_info = source_snapshot->Views();
        RC::SigScannerStaticData::m_is_modular = !source_snapshot->IsMonolithic();
        check(!RC::SigScannerStaticData::m_is_modular &&
                  RC::SigScannerStaticData::m_modules_info.array[static_cast<size_t>(RC::ScanTarget::MainExe)].image_base == r.image->base &&
                  RC::SigScannerStaticData::m_modules_info.array[static_cast<size_t>(RC::ScanTarget::Core)].image_base == r.image->base,
              "SetupUnrealModules seam: Views()/IsMonolithic() consumed unchanged (monolithic publish to every target)");
    }

    // =====================================================================
    // [C] shadow_arm happy path: required seeds cross-check against SeedTable
    // =====================================================================
    rc::shadowarm::ShadowArmReport report{};
    {
        namespace sa = rc::shadowarm;
        sa::ShadowArmInput in{};
        in.eboot = file_bytes;
        in.eboot_size = static_cast<size_t>(fsize);
        in.bridge_observed_base = SeedTableNS::kBaseObserved; // validated uniform slide
        in.pins = pins_from_seed_table();
        in.optional[0] = {"fname_tostring", nullptr};
        in.optional[1] = {"gnatives", nullptr};
        in.optional[2] = {"fuobject_hash_tables_get", nullptr};
        in.optional[3] = {"console_manager_singleton", nullptr};
        // Machinery demonstration ONLY (labeled as such): proves the optional
        // resolution path end-to-end using an already-audited unique sig. NOT
        // a claim about fname_tostring-class slots — those need their own
        // derivation + uniqueness audit before pinning.
        in.optional[4] = {"demo.optional_machinery", &sa::kScoPrologue};
        in.log = &test_log_sink;
        in.log_ctx = nullptr;
        g_log_lines.clear();

        const auto status = sa::shadow_arm(in, &report);
        check(status == sa::ShadowArmStatus::Armed,
              "shadow_arm ARMED (identity + geometry + image-shape pins + 4 cross-checks)",
              "status=" + std::string(sa::shadow_arm_status_name(status)) +
                  (status == sa::ShadowArmStatus::Armed ? "" : " detail: " + std::string(report.detail)));
        check(report.slide == SeedTableNS::kBaseObserved, "slide == bridge observed_base == SeedTable base_observed",
              hex(report.slide));

        // The four REQUIRED slots: seed-authoritative, cross-check green, live
        // values exactly base + pinned label (asserted from the header pins).
        struct Exp
        {
            const char *name;
            uint64_t label;
        };
        const Exp expected[4] = {
            {"g_uobject_array", SeedTableNS::kGUObjectArray.label},
            {"fname_ctor_wchar", SeedTableNS::kFNameCtorWchar.label},
            {"gmalloc", SeedTableNS::kGMalloc.label},
            {"static_construct_object_internal", SeedTableNS::kStaticConstructObject.label}};
        for (size_t i = 0; i < 4; ++i)
        {
            const auto &res = report.required[i];
            const bool ok = res.ran && res.agree && res.shadow_label == expected[i].label &&
                            res.live == SeedTableNS::kBaseObserved + expected[i].label &&
                            std::string(res.name) == expected[i].name;
            check(ok, std::string("required slot cross-check GREEN: ") + expected[i].name,
                  "seed=" + hex(res.seed_label) + " shadow=" + hex(res.shadow_label) +
                      " live=" + hex(res.live) + " hits=" + std::to_string(res.hits));
        }
        // Spot-assert the canonical live addresses from the seed derivations.
        check(report.required[0].live == 0x97ba110ull && report.required[1].live == 0x63d590ull &&
                  report.required[2].live == 0x970add8ull && report.required[3].live == 0x890aa0ull,
              "live addresses match the proven seed derivations (0x97ba110/0x63d590/0x970add8/0x890aa0)");

        // GUA corroboration (sig A, 2 sites) ran inside the arm without refusal.
        bool saw_green = false;
        for (const auto &l : g_log_lines)
            if (contains(l, "g_uobject_array") && contains(l, "GREEN"))
                saw_green = true;
        check(saw_green && g_log_lines.size() >= 7, "arm log seam delivered the cross-check lines",
              std::to_string(g_log_lines.size()) + " lines");

        // [O] optional slots: fallbacks retained; machinery resolves.
        bool fallback_ok = true, demo_ok = false;
        for (size_t i = 0; i < report.optional_count; ++i)
        {
            const auto &o = report.optional[i];
            if (std::string(o.name) == "demo.optional_machinery")
            {
                demo_ok = o.resolved && o.live == SeedTableNS::kBaseObserved + SeedTableNS::kStaticConstructObject.label;
            }
            else if (o.resolved || !contains(o.note ? o.note : "", "fallback retained"))
            {
                fallback_ok = false;
            }
        }
        check(fallback_ok, "optional slots without pinned sigs: 'fallback retained' (degradation paths stay)");
        check(demo_ok, "optional-slot resolution machinery proven (demo entry -> live 0x890aa0)",
              demo_ok ? hex(report.optional[4].live) : "unresolved");
    }

    // =====================================================================
    // [P] the REAL UE4SS owned-range start_scan over the shadow ranges
    //     (the exact Signatures.cpp seam) + the match-address bifurcation
    // =====================================================================
    {
        using RC::SignatureContainer;
        using RC::SignatureData;
        using RC::SinglePassScanner;

        uint64_t fname_live = 0, gua_live = 0, gua_linked = 0;
        int fname_hits = 0, gua_hits = 0, absent_hits = 0;
        const uint8_t *fname_bytes = nullptr;

        std::vector<SignatureContainer> containers;

        // Direct-function class (FName ctor): handler stores live(c), reads bytes(c).
        containers.emplace_back(
            std::vector<SignatureData>{{"55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74", 0, ""}},
            [&](SignatureContainer &c) -> bool {
                ++fname_hits;
                fname_live = reinterpret_cast<uint64_t>(rc::shadowsource::ShadowSigAdapter::live(report.image, c));
                fname_bytes = rc::shadowsource::ShadowSigAdapter::bytes(c);
                return true; // unique sig: stop after first
            },
            [](SignatureContainer &) {});
        // Data class (GUA): handler rip-decodes on shadow bytes, stores live.
        containers.emplace_back(
            std::vector<SignatureData>{{"48 8d 1d ?? ?? ?? ?? 4c 89 e7 4c 89 fe ff 50 18", 0, ""}},
            [&](SignatureContainer &c) -> bool {
                ++gua_hits;
                const bool ok = rc::shadowsource::ShadowSigAdapter::live_via_rip(report.image, c, &gua_live, &gua_linked);
                return ok; // unique sig: stop after first successful resolve
            },
            [](SignatureContainer &) {});
        // Absent needle: handler must never fire.
        containers.emplace_back(
            std::vector<SignatureData>{{"de ad be ef de ad be ef de ad be ef de ad be ef de ad be ef", 0, ""}},
            [&](SignatureContainer &) -> bool {
                ++absent_hits;
                return true;
            },
            [](SignatureContainer &) {});

        SinglePassScanner::SignatureContainerMap map;
        map.emplace(RC::ScanTarget::Core, std::move(containers));

        // The exact non-Windows Signatures.cpp call:
        SinglePassScanner::start_scan(map, report.snapshot->ReadableRanges());

        check(fname_hits == 1 && fname_live == 0x63d590ull,
              "REAL start_scan: fname sig matched; live(c) == 0x63d590 (seed-consistent)",
              "hits=" + std::to_string(fname_hits) + " live=" + hex(fname_live));
        check(fname_bytes != nullptr && fname_bytes[0] == 0x55 && fname_bytes[1] == 0x48 &&
                  fname_bytes[2] == 0x89 && fname_bytes[3] == 0xe5,
              "bytes(c) reads the pinned prologue from the shadow (never the live exec page)");
        check(gua_hits == 1 && gua_linked == 0x93ba110ull && gua_live == 0x97ba110ull,
              "REAL start_scan: GUA xref matched; rip decode on shadow bytes -> live 0x97ba110",
              "hits=" + std::to_string(gua_hits) + " linked=" + hex(gua_linked) + " live=" + hex(gua_live));
        check(absent_hits == 0, "absent pattern: handler never fires (0 fabricated matches)");

        // The match pointer lies inside a supplied readable range (no match
        // outside the ranges, none crossing a boundary).
        const auto &ranges = report.snapshot->ReadableRanges().find(RC::ScanTarget::Core)->second;
        bool inside = false;
        for (const auto &range : ranges)
        {
            const uintptr_t p = reinterpret_cast<uintptr_t>(fname_bytes);
            if (p >= reinterpret_cast<uintptr_t>(range.data) &&
                p < reinterpret_cast<uintptr_t>(range.data) + range.size)
                inside = true;
        }
        check(inside, "match pointer inside a supplied readable range (owned-range contract)");
    }

    // =====================================================================
    // [R] refusal honesty — every refusal is loud and names both values
    // =====================================================================
    {
        namespace sa = rc::shadowarm;

        // 1. No bridge geometry: refused before anything is scanned.
        sa::ShadowArmInput in{};
        in.eboot = file_bytes;
        in.eboot_size = static_cast<size_t>(fsize);
        in.bridge_observed_base = 0;
        in.pins = pins_from_seed_table();
        sa::ShadowArmReport rep{};
        check(sa::shadow_arm(in, &rep) == sa::ShadowArmStatus::RefusedNoGeometry &&
                  contains(rep.detail, "never a cached"),
              "refusal: no bridge geometry (observed_base == 0)", rep.detail);

        // 2. Slide mismatch: bridge base != seed base_observed (both named).
        in.bridge_observed_base = 0x500000ull;
        check(sa::shadow_arm(in, &rep) == sa::ShadowArmStatus::RefusedSlideMismatch &&
                  contains(rep.detail, "0x500000") && contains(rep.detail, "0x400000"),
              "refusal: slide mismatch names both bases", rep.detail);

        // 3. Identity mismatch: both hashes named.
        in.bridge_observed_base = SeedTableNS::kBaseObserved;
        in.pins.image_sha256 = "0000000000000000000000000000000000000000000000000000000000000000";
        check(sa::shadow_arm(in, &rep) == sa::ShadowArmStatus::RefusedIdentity &&
                  contains(rep.detail, fixture_hash) &&
                  contains(rep.detail, "0000000000000000000000000000000000000000000000000000000000000000"),
              "refusal: identity mismatch names both hashes (actual first)", rep.detail);

        // 4. Malformed pin.
        in.pins.image_sha256 = "abc";
        check(sa::shadow_arm(in, &rep) == sa::ShadowArmStatus::RefusedSource &&
                  contains(rep.detail, "64 hex"),
              "refusal: malformed sha pin", rep.detail);

        // 5. Seed/shadow disagreement: tampered gmalloc seed label (seed
        //    0x930ade0 vs shadow-resolved 0x930add8, both named, never overridden).
        in.pins = pins_from_seed_table();
        in.pins.required[2].seed_label = SeedTableNS::kGMalloc.label + 8;
        check(sa::shadow_arm(in, &rep) == sa::ShadowArmStatus::RefusedCrossCheck &&
                  contains(rep.detail, "gmalloc") && contains(rep.detail, "930ade0") && contains(rep.detail, "930add8"),
              "refusal: required-slot mismatch names seed and shadow values (no silent override)", rep.detail);

        // 6. Uniqueness discipline: the documented NON-unique SCO 16-byte
        //    prologue as a slot sig must refuse on hit count.
        sa::SlotSig non_unique{"static_construct_object_internal",
                               "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec", "xxxxxxxxxxxxxxxx",
                               false, 1, nullptr, nullptr, 0};
        in.pins = pins_from_seed_table();
        in.pins.required[3].sig = &non_unique;
        check(sa::shadow_arm(in, &rep) == sa::ShadowArmStatus::RefusedCrossCheck &&
                  contains(rep.detail, "hit count"),
              "refusal: non-unique sig violates the audited hit-count discipline", rep.detail);

        // 7. Image-shape pin: tampered extent refuses.
        in.pins = pins_from_seed_table();
        in.pins.image_extent = kPinnedExtent + 0x1000;
        check(sa::shadow_arm(in, &rep) == sa::ShadowArmStatus::RefusedImageShape &&
                  contains(rep.detail, "extent"),
              "refusal: image extent drifted from the pin", rep.detail);
    }

    // =====================================================================
    // [D2] determinism: a second arm run reproduces identical live values
    // =====================================================================
    std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> d2_snapshot;
    const rc::shadowcopy::ShadowImage *d2_image = nullptr;
    {
        namespace sa = rc::shadowarm;
        sa::ShadowArmInput in{};
        in.eboot = file_bytes;
        in.eboot_size = static_cast<size_t>(fsize);
        in.bridge_observed_base = SeedTableNS::kBaseObserved;
        in.pins = pins_from_seed_table();
        sa::ShadowArmReport rep{};
        const auto status = sa::shadow_arm(in, &rep);
        bool same = status == sa::ShadowArmStatus::Armed;
        for (size_t i = 0; same && i < 4; ++i)
        {
            same = rep.required[i].agree && rep.required[i].live == report.required[i].live &&
                   rep.required[i].shadow_label == report.required[i].shadow_label;
        }
        check(same, "second arm run: identical cross-check results (deterministic)");
        d2_snapshot = rep.snapshot; // keep for the lifetime check below
        d2_image = rep.image;
    }

    // =====================================================================
    // [L] lifetime: releasing the snapshot tears the shadow down
    // =====================================================================
    {
        // The most recent successful arm (D2) owns the module-active slot.
        check(rc::shadowcopy::shadow_active() == d2_image,
              "module-active shadow installed while armed");
        report.snapshot.reset(); // earlier image: released, active unchanged
        source_snapshot.reset();
        check(rc::shadowcopy::shadow_active() == d2_image,
              "superseded images release without disturbing the active one");
        d2_snapshot.reset(); // the active image's last owner drops it
        check(rc::shadowcopy::shadow_active() == nullptr,
              "lifetime owner: last snapshot release ran shadow_shutdown (no leaked active image)");
    }

    std::free(file_bytes);
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
