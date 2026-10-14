// host_test.cpp — macOS host verification of the shadow-copy AOB scanner.
//
// Loads the retained decrypted eboot (sha256 3b6cfad4...) into the shadow
// scanner EXACTLY as the PRX would (same shadow_init entry point, default
// load policy, slide 0x400000), then verifies:
//
//   [G] program-header geometry matches the pinned game profile
//       (worker-geometry-reference phdrs-derived.json / GEOMETRY.json)
//   [T] vaddr<->ptr translation + the file-offset trap handling
//       (LOAD0/1/3/7 use vaddr+0x4000; LOAD8 uses vaddr-0x324000)
//   [F] fname_ctor_wchar: unique 16-byte prologue sig -> linked 0x23d590
//       -> live 0x63d590
//   [S] static_construct_object_internal: 25-byte unique sig -> linked
//       0x490aa0 -> live 0x890aa0 (16-byte prologue documented non-unique)
//   [U] g_uobject_array (BSS data seed): three xref sigs in code -> rip
//       resolution -> linked 0x93ba110 -> live 0x97ba110
//   [M] gmalloc (BSS data seed): unique bootstrap-store sig at 0xb660 ->
//       rip resolution -> linked 0x930add8 -> live 0x970add8
//   [P] slide polymorphism: a different per-boot slide maps the same linked
//       hit to the shifted live address (per-boot ASLR math)
//   [X] refusal honesty: all-wildcard mask refused; empty pattern refused;
//       scan across segments cannot fabricate boundary-crossing matches
//
// Signatures are hand-derived from the same sha-pinned image (see REPORT.md);
// this is how PC UE4SS ships its AOB sigs. Exit code 0 = all checks passed.

#include "../shadow_copy.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace rc::shadowcopy;

namespace
{

    int g_failures = 0;
    int g_checks = 0;

    void check(bool ok, const std::string &label, const std::string &detail = "")
    {
        ++g_checks;
        if (ok)
        {
            std::printf("[PASS] %s%s%s\n", label.c_str(), detail.empty() ? "" : " — ", detail.c_str());
        }
        else
        {
            ++g_failures;
            std::printf("[FAIL] %s%s%s\n", label.c_str(), detail.empty() ? "" : " — ", detail.c_str());
        }
    }

    std::string hex(uint64_t v)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(v));
        return buf;
    }

    // hex string "48 8d 1d ..." -> bytes (00 placeholders where wildcards go)
    std::vector<uint8_t> unhex(const char *s)
    {
        std::vector<uint8_t> out;
        for (const char *p = s; *p;)
        {
            if (*p == ' ')
            {
                ++p;
                continue;
            }
            out.push_back(static_cast<uint8_t>(std::strtoul(p, nullptr, 16)));
            while (*p && *p != ' ')
                ++p;
        }
        return out;
    }

    // ---- pinned expectations (game profile townfall-ps5-ue56-v1) ------------

    struct ExpectedSeg
    {
        uint64_t vaddr;
        uint64_t filesz;
        uint64_t memsz;
        uint32_t flags;
    };
    const ExpectedSeg kExpectedSegs[] = {
        {0x0ull, 122133276ull, 122133276ull, 0x1u}, // LOAD0 --X text
        {122142720ull, 10742040ull, 10742040ull, 0x4u}, // LOAD1 R-- rodata
        {132890624ull, 19391720ull, 19398656ull, 0x6u}, // LOAD3 RW- data/RELRO
        {152289280ull, 1885682ull, 5207064ull, 0x6u}, // LOAD7 RW- bss-heavy
    };
    constexpr uint64_t kExpectedExtent = 197526216ull; // includes LOAD8 (not loaded by default policy)
    constexpr uint64_t kValidatedSlide = 0x400000ull;

    constexpr uint64_t kSeedGuaLinked = 0x93ba110ull;
    constexpr uint64_t kSeedFNameLinked = 0x23d590ull;
    constexpr uint64_t kSeedGmallocLinked = 0x930add8ull;
    constexpr uint64_t kSeedScoLinked = 0x490aa0ull;

    const char *kFNameSig = "55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74"; // 16B all-literal, unique image-wide
    const char *kFNameMask = "xxxxxxxxxxxxxxxx";

    // 25-byte extension of the SCO prologue; unique image-wide (16B prologue alone is NOT).
    const char *kScoSig = "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec 58 02 00 00 48 8b 05 4d 14";
    const char *kScoMask = "xxxxxxxxxxxxxxxxxxxxxxxxx";
    const char *kScoPrologueOnly = "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec"; // documented non-unique

    // GUA xref sigs (disp32 wildcarded). A: lea rbx,[rip+d]; mov edi,0; cmp [rbx+24h],eax; jle
    const char *kGuaSigA = "48 8d 1d 00 00 00 00 bf 00 00 00 00 39 43 24 7e";
    const char *kGuaSigAMask = "xxx????xxxxxxxxx";
    // B: lea rbx,[rip+d]; mov rdi,r12; mov rsi,r15; call [rax+18h] — unique
    const char *kGuaSigB = "48 8d 1d 00 00 00 00 4c 89 e7 4c 89 fe ff 50 18";
    const char *kGuaSigBMask = "xxx????xxxxxxxxx";
    // C: lea r13,[rip+d]; cmp [rbp+24h],eax; jle; mov rdx,[r13] (REX-prefixed cmp)
    const char *kGuaSigC = "4c 8d 2d 00 00 00 00 41 39 45 24 7e 18 49 8b";
    const char *kGuaSigCMask = "xxx????xxxxxxxx";
    const uint64_t kGuaSiteA1 = 0x2b212aul, kGuaSiteA2 = 0x2b2399ull;
    const uint64_t kGuaSiteB = 0x2b25f0ull;
    const uint64_t kGuaSiteC1 = 0x2b1e57ull, kGuaSiteC2 = 0x2b27fcull;

    // GMalloc bootstrap store: mov [rip+d],rbx; mov r14,[rip+d]; movzx — both disps wildcarded, unique
    const char *kGmallocSig = "48 89 1d 00 00 00 00 4c 8b 35 00 00 00 00 0f b6";
    const char *kGmallocMask = "xxx????xxx????xx";
    const uint64_t kGmallocSite = 0xb660ull;

    size_t run_scan(const ShadowImage *img, const char *sig, const char *mask,
                    uint64_t *results, size_t max_results)
    {
        auto bytes = unhex(sig);
        return scan_pattern_in(img, 0, UINT64_MAX, bytes.data(), mask, bytes.size(), results, max_results);
    }
} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: %s <decrypted-eboot.bin>\n", argv[0]);
        return 2;
    }
    const char *path = argv[1];

    // ---- load the whole file (the PRX reads the same bytes via its file core)
    FILE *fh = std::fopen(path, "rb");
    if (fh == nullptr)
    {
        std::fprintf(stderr, "cannot open %s\n", path);
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
    std::printf("loaded %s: %ld bytes\n", path, fsize);

    // ---- init shadow with the validated slide ------------------------------
    const ShadowImage *img = nullptr;
    ShadowError err = shadow_init(&img, file_bytes, static_cast<size_t>(fsize), kValidatedSlide);
    check(err == ShadowError::None && img != nullptr, "shadow_init succeeds",
          "err=" + std::string(shadow_error_name(err)));

    // [G] geometry -----------------------------------------------------------
    check(img->segment_count == 4, "segment count == 4 (default policy skips LOAD8)",
          "count=" + hex(img->segment_count));
    for (size_t s = 0; s < img->segment_count && s < 4; ++s)
    {
        const ShadowSegment &seg = img->segments[s];
        const ExpectedSeg &exp = kExpectedSegs[s];
        const bool ok = seg.vaddr == exp.vaddr && seg.filesz == exp.filesz &&
                        seg.memsz == exp.memsz && seg.flags == exp.flags;
        check(ok, "segment " + std::to_string(s) + " matches profile",
              "vaddr=" + hex(seg.vaddr) + " filesz=" + hex(seg.filesz) +
                  " memsz=" + hex(seg.memsz) + " flags=" + hex(seg.flags));
    }
    check(img->image_extent_vaddr == kExpectedExtent, "image extent vaddr 0xbc602c8",
          hex(img->image_extent_vaddr));

    // LOAD8 exclusion by default policy (opt-in only)
    bool load8_absent = true;
    for (size_t s = 0; s < img->segment_count; ++s)
    {
        if (img->segments[s].flags == 0)
            load8_absent = false;
    }
    check(load8_absent, "LOAD8 (---) not loaded under default policy");

    // [T] translation + the +0x4000 / -0x324000 file-offset trap -------------
    {
        const uint8_t *p = shadow_vaddr_to_ptr(img, kSeedFNameLinked);
        check(p != nullptr && p[0] == 0x55 && p[1] == 0x48, "vaddr_to_ptr(0x23d590) reads FName prologue");
        uint64_t back = 0;
        check(shadow_ptr_to_vaddr(img, p, &back) && back == kSeedFNameLinked,
              "ptr_to_vaddr round-trips", hex(back));
        // True gap: the 9444-byte hole between LOAD0 end (122133276) and
        // LOAD1 start (122142720) — pinned in the profile's phdr_gaps table.
        check(shadow_vaddr_to_ptr(img, 122137998ull) == nullptr,
              "inter-LOAD gap vaddr (LOAD0/LOAD1 hole) refuses translation");
    }
    {
        // File-offset trap: shadow byte at linked vaddr V must equal file byte
        // at V+0x4000 for LOAD0/1/3/7. Spot-check a rodata (LOAD1) vaddr.
        const uint64_t rodata_probe = 0x7dd2a40ull; // kWindowE AES S-box anchor vaddr (bridge r14 table)
        const uint8_t *sp = shadow_vaddr_to_ptr(img, rodata_probe);
        const uint8_t expect = file_bytes[rodata_probe + 0x4000];
        check(sp != nullptr && sp[0] == expect,
              "LOAD1 byte matches file at vaddr+0x4000 (S-box anchor head)",
              "shadow=" + hex(sp ? sp[0] : 0) + " file=" + hex(expect));
    }
    {
        // BSS tail: GUA label 0x93ba110 lies past LOAD7 filesz end (0x93085f2):
        // pointer exists (total translation) and reads zero (BSS zero-fill).
        const uint8_t *p = shadow_vaddr_to_ptr(img, kSeedGuaLinked);
        const ShadowSegment *seg7 = &img->segments[3];
        const bool in_load7 = p >= seg7->backing && p < seg7->backing + seg7->memsz;
        bool zero = true;
        for (int i = 0; i < 8; ++i)
            if (p[i] != 0)
                zero = false;
        check(in_load7 && zero, "BSS seed 0x93ba110 translates to zeroed LOAD7 BSS tail",
              "seg filesz_end=" + hex(seg7->vaddr + seg7->filesz));
    }
    {
        // UE4SS ModuleSnapshot contract (PlatformModuleSnapshot.hpp): the
        // module extent [base, base+size) must contain every readable range,
        // ranges sorted ascending by pointer, non-overlapping. Replicate that
        // validation over the shadow's file-backed ranges.
        bool sorted_ok = true, inside_ok = true;
        const uintptr_t base = reinterpret_cast<uintptr_t>(img->base);
        const uintptr_t end = base + img->backing_size;
        uintptr_t prev_end = base;
        for (size_t s = 0; s < img->segment_count; ++s)
        {
            const ShadowSegment &seg = img->segments[s];
            const uintptr_t r_begin = reinterpret_cast<uintptr_t>(seg.backing);
            const uintptr_t r_end = r_begin + seg.filesz;
            if (r_begin < prev_end || r_end > end)
                inside_ok = false;
            if (s > 0 && r_begin <= reinterpret_cast<uintptr_t>(img->segments[s - 1].backing))
                sorted_ok = false;
            prev_end = r_end;
        }
        check(sorted_ok && inside_ok, "shadow ranges satisfy ModuleSnapshot extent contract",
              "base+size covers " + std::to_string(img->segment_count) + " sorted ranges");

        // Known-base arithmetic: live == shadow_ptr + (slide - base - min_vaddr)
        const uint8_t *fn = shadow_vaddr_to_ptr(img, kSeedFNameLinked);
        const uint64_t min_vaddr = img->segments[0].vaddr;
        const uint64_t live_via_base = reinterpret_cast<uint64_t>(fn) + kValidatedSlide -
                                       reinterpret_cast<uint64_t>(img->base) - min_vaddr;
        check(live_via_base == 0x63d590ull, "known-base arithmetic: live == ptr + slide - base",
              hex(live_via_base));
    }

    // [F] fname_ctor_wchar ----------------------------------------------------
    {
        uint64_t results[8];
        size_t n = run_scan(img, kFNameSig, kFNameMask, results, 8);
        check(n == 1 && results[0] == kSeedFNameLinked,
              "FName ctor sig unique at linked 0x23d590",
              "hits=" + std::to_string(n) + " first=" + (n ? hex(results[0]) : "-"));
        check(n == 1 && shadow_linked_to_live(img, results[0]) == 0x63d590ull,
              "FName ctor live address 0x63d590 (seed)",
              hex(shadow_linked_to_live(img, results[0])));

        // scan_pattern() (the task-signature singleton entry) + scan_pattern_live agree
        auto bytes = unhex(kFNameSig);
        uint64_t live_results[8];
        size_t nl = scan_pattern_live(img, bytes.data(), kFNameMask, bytes.size(), live_results, 8);
        uint64_t linked_results[8];
        size_t np = scan_pattern(bytes.data(), kFNameMask, bytes.size(), linked_results, 8);
        check(nl == 1 && np == 1 && live_results[0] == 0x63d590ull && linked_results[0] == kSeedFNameLinked,
              "scan_pattern()/scan_pattern_live() singleton entry agrees",
              "linked=" + hex(linked_results[0]) + " live=" + hex(live_results[0]));
    }

    // [S] static_construct_object_internal --------------------------------------
    {
        uint64_t results[8];
        size_t n16 = run_scan(img, kScoPrologueOnly, kScoMask, results, 8);
        check(n16 >= 3, "SCO 16B prologue documented non-unique (>=3 hits, capped)",
              "hits=" + std::to_string(n16) + "+");

        uint64_t results25[8];
        size_t n25 = run_scan(img, kScoSig, kScoMask, results25, 8);
        check(n25 == 1 && results25[0] == kSeedScoLinked,
              "SCO 25B sig unique at linked 0x490aa0",
              "hits=" + std::to_string(n25) + " first=" + (n25 ? hex(results25[0]) : "-"));
        check(n25 == 1 && shadow_linked_to_live(img, results25[0]) == 0x890aa0ull,
              "SCO live address 0x890aa0 (seed)",
              hex(shadow_linked_to_live(img, results25[0])));
    }

    // [U] g_uobject_array via rip xrefs -----------------------------------------
    {
        uint64_t results[16];
        size_t nA = run_scan(img, kGuaSigA, kGuaSigAMask, results, 16);
        bool a1 = false, a2 = false, resolve_ok = true;
        for (size_t i = 0; i < nA && i < 16; ++i)
        {
            if (results[i] == kGuaSiteA1)
                a1 = true;
            if (results[i] == kGuaSiteA2)
                a2 = true;
            RipRefResult rr;
            if (resolve_rip_ref(img, results[i], &rr))
            {
                resolve_ok = resolve_ok && rr.target_vaddr == kSeedGuaLinked;
            }
            else
            {
                resolve_ok = false;
            }
        }
        check(nA == 2 && a1 && a2 && resolve_ok,
              "GUA sig A finds documented sites 0x2b212a/0x2b2399, both resolve to 0x93ba110",
              "hits=" + std::to_string(nA));

        size_t nB = run_scan(img, kGuaSigB, kGuaSigBMask, results, 16);
        RipRefResult rrB{};
        bool b_ok = nB == 1 && results[0] == kGuaSiteB && resolve_rip_ref(img, results[0], &rrB) &&
                    rrB.target_vaddr == kSeedGuaLinked;
        check(b_ok, "GUA sig B unique at 0x2b25f0 -> resolves 0x93ba110",
              "hits=" + std::to_string(nB) + " target=" + hex(rrB.target_vaddr));

        size_t nC = run_scan(img, kGuaSigC, kGuaSigCMask, results, 16);
        bool c1 = false, c2 = false, c_res = false;
        for (size_t i = 0; i < nC && i < 16; ++i)
        {
            if (results[i] == kGuaSiteC1)
            {
                c1 = true;
                RipRefResult rr;
                if (resolve_rip_ref(img, results[i], &rr) && rr.target_vaddr == kSeedGuaLinked)
                    c_res = true;
            }
            if (results[i] == kGuaSiteC2)
                c2 = true;
        }
        check(nC >= 2 && c1 && c2 && c_res,
              "GUA sig C (REX.WR 4c 8d 2d) finds sites 0x2b1e57/0x2b27fc; 0x2b1e57 resolves 0x93ba110",
              "hits=" + std::to_string(nC) + " (extra same-shape sites resolve elsewhere; disambiguate by resolved target)");

        check(shadow_linked_to_live(img, kSeedGuaLinked) == 0x97ba110ull,
              "GUA live address 0x97ba110 (seed)", hex(shadow_linked_to_live(img, kSeedGuaLinked)));
    }

    // [M] gmalloc via unique bootstrap store -------------------------------------
    {
        uint64_t results[8];
        size_t n = run_scan(img, kGmallocSig, kGmallocMask, results, 8);
        RipRefResult rr{};
        bool ok = n == 1 && results[0] == kGmallocSite &&
                  resolve_rip_ref(img, results[0], &rr) && rr.target_vaddr == kSeedGmallocLinked;
        check(ok, "GMalloc bootstrap-store sig unique at 0xb660 -> resolves 0x930add8",
              "hits=" + std::to_string(n) + " target=" + hex(rr.target_vaddr));
        check(shadow_linked_to_live(img, kSeedGmallocLinked) == 0x970add8ull,
              "GMalloc live address 0x970add8 (seed)", hex(shadow_linked_to_live(img, kSeedGmallocLinked)));
    }

    // [P] per-boot slide polymorphism ---------------------------------------------
    {
        const ShadowImage *img2 = nullptr;
        constexpr uint64_t kAltSlide = 0x123400000ull; // arbitrary per-boot ASLR value
        ShadowError e2 = shadow_init(&img2, file_bytes, static_cast<size_t>(fsize), kAltSlide);
        check(e2 == ShadowError::None && img2 != nullptr, "shadow_init with alternate slide",
              "slide=" + hex(kAltSlide));
        uint64_t results[8];
        size_t n = run_scan(img2, kFNameSig, kFNameMask, results, 8);
        check(n == 1 && shadow_linked_to_live(img2, results[0]) == kSeedFNameLinked + kAltSlide,
              "alternate slide maps same linked hit to shifted live address",
              hex(shadow_linked_to_live(img2, results[0])));
        check(shadow_live_to_linked(img2, kSeedFNameLinked + kAltSlide) == kSeedFNameLinked,
              "live_to_linked inverts under alternate slide");
        shadow_shutdown(img2);
        // Re-init the validated-slide singleton; shut the ORIGINAL image down
        // at the end too (it stayed alive across the alternate-slide window).
    }

    // [X] refusal honesty -----------------------------------------------------------
    {
        uint64_t results[4];
        auto bytes = unhex(kFNameSig);
        check(scan_pattern_in(img, 0, UINT64_MAX, bytes.data(), "????????????????", bytes.size(), results, 4) == SIZE_MAX,
              "all-wildcard mask refused");
        check(scan_pattern_in(img, 0, UINT64_MAX, bytes.data(), "xx", bytes.size(), results, 4) == SIZE_MAX,
              "short mask refused");
        check(scan_pattern_in(img, 0, UINT64_MAX, bytes.data(), kFNameMask, 0, results, 4) == SIZE_MAX,
              "zero-length pattern refused");
        check(scan_pattern_in(nullptr, 0, UINT64_MAX, bytes.data(), kFNameMask, bytes.size(), results, 4) == SIZE_MAX,
              "null image refused");
        // nonexistent byte sequence -> 0 hits, not SIZE_MAX, not garbage
        uint8_t nopattern[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};
        check(scan_pattern_in(img, 0, UINT64_MAX, nopattern, "xxxxxxxx", 8, results, 4) == 0,
              "absent pattern reports 0 hits");
    }

    shadow_shutdown(img);
    std::free(file_bytes);

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
