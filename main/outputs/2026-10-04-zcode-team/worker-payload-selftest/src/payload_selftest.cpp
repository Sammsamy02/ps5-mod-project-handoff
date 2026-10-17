// payload_selftest.cpp — worker-payload-selftest, 2026-10-05.
//
// Batched standalone payload self-test for the PS5 UE4SS runtime port.
// ONE binary runs FOUR checks and appends ONE bounded JSONL record; the
// dispatching runner tags the record with the exact built binary's sha256
// build id. This is SEPARATE-PROCESS evidence: the payload runs as its own
// process (pldmgr /loadpayload on console, plain child on host). It is NEVER
// game-process evidence and makes no game/console claims.
//
// Checks (batched, single dispatch):
//   1a  wstring_convert narrow->wide garbles on the PROSPERO target (SDK
//       prebuilt libc++ pairs 4-byte-unit conversion helpers with 2-byte
//       wchar_t). Host builds SKIP this assertion (host libc++ is correct)
//       and only verify 1b.
//   1b  direct byte-by-byte widen loop (push_back(static_cast<wchar_t>(c)))
//       produces the correct wide string. Verified on host AND target.
//   2   the loadlist rows mod/TownfallItemSpawner/1.0.0, file/lua_mod/
//       ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua/21415 and file/
//       lua_mod/ue4ss/Mods/TownfallItemSpawner/enabled.txt/0 parse into the
//       mod/file structures via the production single-header parser
//       modforge_loadlist.h (vendored from worker-7-installer, sha
//       b12a5ac5aebbc2635995657cdd4aed14314a9c3dfdba30c284a3ae2fb1f64a15).
//   3   the production NativeModSource (lineage-overlay Platform/
//       NativeModSource.hpp + its matching composition implementation)
//       built from those parsed rows: lua_mods() contains
//       name=TownfallItemSpawner path=ue4ss/Mods/TownfallItemSpawner and
//       read("ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua") returns
//       bytes BYTE-EQUAL to the bundle main.lua (21415 bytes, embedded at
//       build time from the actual file; sha pinned by the build receipt).
//       Finding the file on disk or the symbol in an object is NOT
//       sufficient — the returned bytes must compare equal.
//   4   modbridge::normalize_path_for_lua (the FIXED version:
//       path.string() with backslash->slash; verbatim semantics from
//       worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp, sha
//       c92c92ff…, qualified modbridge:: call sites) applied to the wide
//       path built by the 1b widen loop equals the registered native key
//       exactly.
//   5   range_verdict_semantics (TARGET-decisive; host runs the pure-header
//       logic subset): the ACTUAL rc_seed::check_pool_shape +
//       rc_seed::resolve_listing_gap (SeedValidation.hpp, sha db07ac60…)
//       driven over a thin binding to the SAME generic ::syscall trampoline
//       the production bridge uses (SDK libc.a syscalls.o `syscall`; no
//       mincore symbol) with kernel-header-sourced SYS_mincore. Cases:
//       (a) readable valid-pool positive (RW anon mmap + synthetic pool
//           words + published span) -> shape.ok true; resolve_listing_gap
//           returns ok WITHOUT consulting the verdict;
//       (b) unmapped negative (munmap hole) -> raw errno recorded (expect
//           ENOMEM) -> NotMapped -> refusal with the stronger note;
//       (c) mapped-but-unreadable: mprotect exec-only, or (if the weak
//           kernel_mprotect hook is absent -> ENOSYS, recorded) an
//           exec-only page from THIS payload's own sysctl KERN_PROC_VMMAP
//           listing; raw mincore rc recorded -> prot-blind (returns 0:
//           the (CaptureTruncated, Mapped, None) refusal asserted correct)
//           vs protection-aware (recorded only; future source-set
//           evidence; production code NOT changed here);
//       (d) len=0 / >16MiB -> Unavailable -> refuses; Structural failure
//           class refuses even with Mapped+ListedSpan.
//
// Bounded: one record <= ~4 KiB, one process run, exit 0 iff all hard checks
// pass (skipped-host never fails). No loops, no network, no timestamps in
// the record (the runner owns ordering identity).

#include "modforge_loadlist.h"
#include <Platform/NativeModSource.hpp>
#include "SeedValidation.hpp"  // ACTUAL production header, final-units sha db07ac60… (pinned by build receipt)

#include <algorithm>
#include <codecvt>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#if !defined(HOST_SELFTEST)
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <unistd.h>
#endif
#include <exception>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Deprecation warnings are intentional here: check 1a deliberately exercises
// the exact deprecated std::wstring_convert route the runtime used before the
// fix. Keep the route compilable and visible.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef HOST_SELFTEST
#define SELFTEST_TARGET "host"
#define SELFTEST_ON_TARGET_GARBLE_ASSERT 0
#else
#define SELFTEST_TARGET "prospero"
#define SELFTEST_ON_TARGET_GARBLE_ASSERT 1
#endif

#include "main_lua_bytes.hpp"  // generated at build time from the real bundle file

namespace {

const char kLoadlistText[] =
    "modforge-loadlist\t1\n"
    "mod\tTownfallItemSpawner\t1.0.0\n"
    "file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/Scripts/main.lua\t21415\n"
    "file\tlua_mod\tue4ss/Mods/TownfallItemSpawner/enabled.txt\t0\n";

const char kModName[] = "TownfallItemSpawner";
const char kModVersion[] = "1.0.0";
const char kMainLuaKey[] = "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua";
const char kEnabledKey[] = "ue4ss/Mods/TownfallItemSpawner/enabled.txt";
const char kModPath[] = "ue4ss/Mods/TownfallItemSpawner";
const char kNarrowSample[] = "TownfallItemSpawner";

// ---- fixed normalize_path_for_lua (verbatim semantics of the production
// ---- fix; see file header for provenance). NOT the buggy String.hpp route.
auto normalize_path_for_lua(const std::filesystem::path& path) -> std::string
{
    std::string utf8_path = path.string();
    std::replace(utf8_path.begin(), utf8_path.end(), '\\', '/');
    return utf8_path;
}

// ---- widen loop under test (the production fix route for narrow -> wide)
auto widen_loop(const std::string& narrow) -> std::wstring
{
    std::wstring wide;
    wide.reserve(narrow.size());
    for (unsigned char c : narrow)
        wide.push_back(static_cast<wchar_t>(c));
    return wide;
}

// ---- bounded JSON string builder
struct JsonLine {
    std::string text;
    explicit JsonLine(const char* marker) {
        text += "{\"marker\":\"";
        text += marker;
        text += "\",\"schema\":1,\"target\":\"" SELFTEST_TARGET "\",\"checks\":[";
    }
    void esc(const char* s) {
        for (const char* p = s; *p; ++p) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { text += '\\'; text += (char)c; }
            else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); text += b; }
            else text += (char)c;
        }
    }
    void kv_text(const char* key, const char* value, bool first, bool tail = false) {
        if (!first) text += ",";
        text += "\""; text += key; text += "\":";
        text += "\""; esc(value); text += "\"";
        if (tail) text += "";
    }
    void kv_num(const char* key, unsigned long long v, bool first) {
        if (!first) text += ",";
        char b[32]; std::snprintf(b, sizeof b, "%llu", v);
        text += "\""; text += key; text += "\":"; text += b;
    }
    void check(const char* id, const char* status, const std::string& detail_json) {
        if (text.back() != '[') text += ",";
        text += "{\"id\":\""; text += id; text += "\",\"status\":\"";
        text += status; text += "\"";
        if (!detail_json.empty()) { text += ",\"detail\":"; text += detail_json; }
        text += "}";
    }
    std::string finish(const char* expect_source, size_t expect_bytes,
                       const char* source_sha, size_t source_bytes) {
        text += "],\"source\":{";
        kv_text("main_lua_sha256", source_sha, true);
        kv_num("main_lua_bytes", (unsigned long long)source_bytes, false);
        text += "},\"expect\":{";
        kv_text("read_key", expect_source, true);
        kv_num("read_bytes", (unsigned long long)expect_bytes, false);
        text += "},\"sizeof_wchar_t\":";
        char b[16]; std::snprintf(b, sizeof b, "%zu", sizeof(wchar_t)); text += b;
        return text;  // root object left OPEN; main appends ,"all_pass":… and closes
    }
};

std::string quote(const std::string& s)
{
    JsonLine tmp("");
    tmp.text.clear();
    tmp.text += "\"";
    tmp.esc(s.c_str());
    tmp.text += "\"";
    return tmp.text;
}

// Render up to `max` wchar code units as decimal numbers (bounded, no raw bytes).
std::string wide_prefix(const std::wstring& w, size_t max)
{
    std::string out = "[";
    char b[24];
    for (size_t i = 0; i < w.size() && i < max; ++i) {
        if (i) out += ",";
        std::snprintf(b, sizeof b, "%u", (unsigned)w[i]);
        out += b;
    }
    out += "]";
    return out;
}

struct Check1aResult {
    bool ran = false;
    bool garbled_utf8_codec = false;     // codecvt_utf8<wchar_t>
    bool garbled_utf16_codec = false;    // codecvt_utf8_utf16<wchar_t>
    bool interleave_shape_utf8 = false;  // even=ASCII char, odd=NUL
    std::string detail_utf8_codec;
    std::string detail_utf16_codec;
    std::string threw;
};

// The expected correct wide form of kNarrowSample, via the widen loop.
Check1aResult check_wstring_convert_garbles()
{
    Check1aResult r;
    const std::string narrow(kNarrowSample);
    const std::wstring expected = widen_loop(narrow);
#if SELFTEST_ON_TARGET_GARBLE_ASSERT
    r.ran = true;
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv8;
        std::wstring got8 = conv8.from_bytes(narrow.data(), narrow.data() + narrow.size());
        r.garbled_utf8_codec = (got8 != expected);
        // Documented shape: 4-byte-unit writes into a 2-byte-wchar buffer
        // interleave a NUL after every character.
        bool shape = got8.size() == narrow.size() * 2;
        for (size_t i = 0; shape && i < got8.size(); ++i)
            shape = (got8[i] == (i % 2 == 0 ? (wchar_t)(unsigned char)narrow[i / 2] : (wchar_t)0));
        r.interleave_shape_utf8 = shape;
        r.detail_utf8_codec = "\"size\":" + std::to_string(got8.size()) +
                              ",\"prefix\":" + wide_prefix(got8, 12) +
                              ",\"interleave_nul_shape\":" + (shape ? "true" : "false");
    } catch (const std::exception& e) {
        r.garbled_utf8_codec = false;
        r.detail_utf8_codec = std::string("\"threw\":\"") + e.what() + "\"";
    }
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv16;
        std::wstring got16 = conv16.from_bytes(narrow.data(), narrow.data() + narrow.size());
        r.garbled_utf16_codec = (got16 != expected);
        r.detail_utf16_codec = "\"size\":" + std::to_string(got16.size()) +
                               ",\"prefix\":" + wide_prefix(got16, 12);
    } catch (const std::exception& e) {
        r.garbled_utf16_codec = false;
        r.detail_utf16_codec = std::string("\"threw\":\"") + e.what() + "\"";
    }
#endif
    return r;
}

struct ParsedMod { std::string name, version; };
struct ParsedFile { std::string role, path; unsigned long long size; };

// ===================== check 5: range_verdict_semantics ====================
//
// Thin binding over the SAME generic ::syscall trampoline the production
// bridge uses (SDK libc.a syscalls.o defines `syscall`; the retained SDK
// libc has NO mincore symbol). Semantics mirror the production
// bridge_kernel_range_verdict (runtime_start_bridge final-units): 16 MiB
// range cap, page-align-down, full-span probe, ENOMEM -> NotMapped, other
// errno -> Unavailable — plus RAW rc/errno capture for the record. The
// build receipt pins the call-site disassembly (args in the trampoline's
// register convention).
struct RawProbe {
    long rc;        // raw syscall return (LLONG_MIN sentinel: refused before syscall)
    int err;        // raw errno observed (or EINVAL for pre-syscall refusal)
    int verdict;    // 1 Mapped / 0 NotMapped / -1 Unavailable (RangeMappedVerdict codes)
};

#if !defined(HOST_SELFTEST)
RawProbe selftest_raw_mincore(uint64_t addr, uint64_t len)
{
    RawProbe out{LLONG_MIN, EINVAL, -1};
    static constexpr uint64_t kPage = 0x1000;
    static constexpr uint64_t kMaxRange = 0x1000000;  // 16 MiB, same as the bridge
    if (len == 0 || len > kMaxRange) return out;      // refused BEFORE any syscall
    const uint64_t start = addr & ~(kPage - 1);
    const uint64_t span = ((addr + len + kPage - 1) & ~(kPage - 1)) - start;
    static unsigned char vec[kMaxRange / kPage];      // 4 KiB static vec, same as bridge
    errno = 0;
    out.rc = ::syscall(SYS_mincore, static_cast<long>(start), static_cast<long>(span),
                       reinterpret_cast<long>(vec));
    out.err = errno;
    out.verdict = (out.rc == 0) ? 1 : (out.err == ENOMEM ? 0 : -1);
    return out;
}

// Bounded sysctl KERN_PROC_VMMAP walk of THIS payload process (the proven
// bridge walker shape: variable kve_structsize records; field offsets
// structsize@0 start@8 end@16 protection@56; kve_prot read=1 exec=4).
struct OwnVmmapFind {
    int sysctl_errno;
    uint32_t entries;
    bool exec_only_found;   // (prot & READ)==0 && (prot & EXEC)!=0
    uint64_t exec_only_addr;
    uint32_t exec_only_prot;
    bool text_prot_found;   // record containing the payload's own text page
    uint32_t text_prot;
};
OwnVmmapFind own_vmmap_exec_only(uint64_t text_page)
{
    OwnVmmapFind r{-1, 0, false, 0, 0, false, 0};
    static unsigned char buf[2048u * 1160u];  // pinned SDK sizeof(kinfo_vmentry)=1160
    const int pid = static_cast<int>(::getpid());
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, pid};
    size_t len = sizeof buf;
    if (::sysctl(mib, 4, buf, &len, nullptr, 0) != 0) { r.sysctl_errno = errno; return r; }
    r.sysctl_errno = 0;
    auto ld32 = [](const unsigned char* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    };
    auto ld64 = [&ld32](const unsigned char* p) {
        return (uint64_t)ld32(p) | ((uint64_t)ld32(p + 4) << 32);
    };
    size_t off = 0;
    while (off + 64 <= len) {
        const uint32_t ksz = ld32(buf + off);
        if (ksz < 64 || ksz > 1160 || (ksz & 3u) != 0) break;   // malformed: stop, never guess
        if (off + ksz > len) break;                             // truncated tail
        const uint64_t s = ld64(buf + off + 8);
        const uint64_t e = ld64(buf + off + 16);
        const uint32_t prot = ld32(buf + off + 56);
        if (r.entries < 2048u) ++r.entries;
        if (!r.exec_only_found && (prot & 1u) == 0 && (prot & 4u) != 0) {
            r.exec_only_found = true;
            r.exec_only_addr = s;
            r.exec_only_prot = prot;
        }
        if (!r.text_prot_found && s <= text_page && text_page < e) {
            r.text_prot_found = true;
            r.text_prot = prot;
        }
        off += ksz;
    }
    return r;
}
#endif  // !HOST_SELFTEST

struct RangeCaseOut {
    bool pass;
    const char* status;   // "pass" / "fail" / "skipped-host"
    std::string detail;   // bounded JSON object fragment (no braces)
};

// Pure rc_seed logic that runs on BOTH host and target (no syscalls).
// Simulated verdict enums are labelled as such in the detail.
RangeCaseOut range_logic_pure()
{
    using namespace rc_seed;
    RangeCaseOut out{true, "pass", ""};
    // (d-pure) Unavailable refuses a NotCovered listing gap even with ListedSpan.
    PoolShapeCheck not_covered{false, "nc", "published-readable-spans",
                               PoolShapeCheck::FailureClass::NotCovered};
    auto res_unavail = resolve_listing_gap(not_covered, RangeMappedVerdict::Unavailable,
                                           ReadableCoverageSource::ListedSpan);
    // (d-pure) Structural refuses even with Mapped + ListedSpan.
    PoolShapeCheck structural{false, "st", "structural", PoolShapeCheck::FailureClass::Structural};
    auto res_struct = resolve_listing_gap(structural, RangeMappedVerdict::Mapped,
                                          ReadableCoverageSource::ListedSpan);
    // (c-pure) The (CaptureTruncated, Mapped, None) refusal: mapped != readable
    // must refuse with no supported source — correct whether mincore proves
    // prot-blind or protection-aware.
    PoolShapeCheck truncated{false, "ct", "capture-truncated",
                             PoolShapeCheck::FailureClass::CaptureTruncated};
    auto res_ct = resolve_listing_gap(truncated, RangeMappedVerdict::Mapped,
                                      ReadableCoverageSource::None);
    // (b-pure) NotMapped refuses with the stronger note.
    auto res_nm = resolve_listing_gap(not_covered, RangeMappedVerdict::NotMapped,
                                      ReadableCoverageSource::None);
    out.pass = !res_unavail.ok && !res_struct.ok && !res_ct.ok && !res_nm.ok;
    out.status = out.pass ? "pass" : "fail";
    out.detail = std::string("{\"simulated\":\"verdict-enums\",\"refuses_unavailable\":")
                 + (res_unavail.ok ? "false" : "true")
                 + ",\"refuses_structural\":" + (res_struct.ok ? "false" : "true")
                 + ",\"refuses_truncated_mapped_none\":" + (res_ct.ok ? "false" : "true")
                 + ",\"refuses_notmapped\":" + (res_nm.ok ? "false" : "true") + "}";
    return out;
}

#if !defined(HOST_SELFTEST)
// Live cases a-d: raw trampoline evidence over real memory on THIS target.
RangeCaseOut range_live_target()
{
    using namespace rc_seed;
    RangeCaseOut out{false, "fail", ""};
    std::string d;
    auto put = [&d](const std::string& key, const std::string& value, bool more) {
        d += "\"" + key + "\":" + value + (more ? "," : "");
    };
    bool pass = true;

    // ---- case a: readable valid-pool positive (RW anon mmap; the
    // non-executable mmap path works per the sdk-mman-no-provider-1 spike).
    bool a_ok = false;
    std::string a_detail = "{\"mapped\":false";
    void* page_a = mmap(nullptr, 0x4000, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (page_a != MAP_FAILED) {
        // Synthetic valid pool words in REAL mapped memory: Blocks[0] word +
        // a 'None'-shaped u16 entry header (h>>6==4, h&1==0). rc_seed itself
        // never dereferences; the readback proves the RW mapping is live.
        const uint64_t blocks0 = reinterpret_cast<uint64_t>(page_a) + 0x100;
        static_cast<uint64_t*>(page_a)[0] = blocks0;
        static_cast<uint16_t*>(page_a)[32] = static_cast<uint16_t>(4u << 6);
        const uint64_t readback = static_cast<volatile uint64_t*>(page_a)[0];
        const AddressSpan span_a{reinterpret_cast<uint64_t>(page_a),
                                 reinterpret_cast<uint64_t>(page_a) + 0x4000};
        const PoolShapeCheck shape = check_pool_shape(
            1u, (1u << 16) | 7u, blocks0, &span_a, 1,
            reinterpret_cast<uint64_t>(page_a) + 0x400000);
        // resolve_listing_gap must admit WITHOUT consulting the verdict.
        const auto gap = resolve_listing_gap(shape, RangeMappedVerdict::NotMapped,
                                             ReadableCoverageSource::None);
        const bool rb_ok = readback == blocks0;
        a_ok = rb_ok && shape.ok && gap.ok && gap.bounds_source != nullptr &&
               std::string_view(gap.bounds_source) == "published-readable-spans";
        a_detail = std::string("{\"mapped\":true,\"readback_ok\":") + (rb_ok ? "true" : "false") +
                   ",\"shape_ok\":" + (shape.ok ? "true" : "false") +
                   ",\"gap_ok_without_verdict\":" + (gap.ok ? "true" : "false");
        munmap(page_a, 0x4000);
    } else {
        a_detail += ",\"mmap_errno\":" + std::to_string(errno);
    }
    put("case_a_readable_positive", a_detail + "}", true);
    pass = pass && a_ok;

    // ---- case b: unmapped negative - a munmap hole is CERTAIN unmapped.
    bool b_ok = false;
    std::string b_detail = "{\"setup\":false}";
    void* two = mmap(nullptr, 0x2000, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);
    if (two != MAP_FAILED) {
        void* hole = reinterpret_cast<char*>(two) + 0x1000;
        munmap(hole, 0x1000);
        const RawProbe pb = selftest_raw_mincore(reinterpret_cast<uint64_t>(hole), 0x1000);
        const AddressSpan span_none{1, 2};  // covers nothing relevant
        const PoolShapeCheck shape = check_pool_shape(
            1u, (1u << 16) | 7u, reinterpret_cast<uint64_t>(hole), &span_none, 1,
            reinterpret_cast<uint64_t>(hole) + 0x100000);
        const bool not_covered = !shape.ok &&
            shape.failure_class == PoolShapeCheck::FailureClass::NotCovered;
        const auto res = resolve_listing_gap(shape, static_cast<RangeMappedVerdict>(pb.verdict),
                                             ReadableCoverageSource::None);
        const bool stronger_note = res.note != nullptr &&
            std::string_view(res.note).starts_with("kernel range verdict: NOT mapped");
        b_ok = pb.verdict == 0 && pb.err == ENOMEM && not_covered && !res.ok && stronger_note;
        b_detail = std::string("{\"setup\":true,\"raw_rc\":") + std::to_string(pb.rc) +
                   ",\"raw_errno\":" + std::to_string(pb.err) +
                   ",\"verdict\":" + std::to_string(pb.verdict) +
                   ",\"shape_not_covered\":" + (not_covered ? "true" : "false") +
                   ",\"refused_stronger_note\":" +
                   ((!res.ok && stronger_note) ? "true" : "false") + "}";
        munmap(two, 0x1000);
    }
    put("case_b_unmapped_negative", b_detail, true);
    pass = pass && b_ok;

    // ---- case c: mapped-but-unreadable - exec-only construction, then the
    // RAW verdict record. mprotect-to-exec may ENOSYS (weak kernel_mprotect
    // hook absent per the sdk-mman spike); fall back to an exec-only page
    // from THIS payload's own sysctl KERN_PROC_VMMAP listing.
    void* page_c = mmap(nullptr, 0x1000, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    int mprotect_rc = -1, mprotect_err = 0;
    bool exec_only = false, via_mprotect = false;
    uint64_t exec_addr = 0;
    uint32_t exec_prot = 0;
    const uint64_t text_probe = reinterpret_cast<uint64_t>(&selftest_raw_mincore) & ~0xFFFull;
    OwnVmmapFind walk{0, 0, false, 0, 0, false, 0};
    if (page_c != MAP_FAILED) {
        errno = 0;
        mprotect_rc = mprotect(page_c, 0x1000, PROT_EXEC);
        mprotect_err = errno;
        if (mprotect_rc == 0) {
            exec_only = true;
            via_mprotect = true;
            exec_addr = reinterpret_cast<uint64_t>(page_c);
            exec_prot = 4u;  // constructed exec-only by mprotect
        }
    }
    if (!exec_only) {
        walk = own_vmmap_exec_only(text_probe);
        if (walk.exec_only_found) {
            exec_only = true;
            exec_addr = walk.exec_only_addr;
            exec_prot = walk.exec_only_prot;
        }
    }
    int prot_behavior = 0;  // 0 inconclusive / 1 prot-blind / 2 protection-aware
    long c_rc = LLONG_MIN; int c_err = 0;
    if (exec_only) {
        const RawProbe pc = selftest_raw_mincore(exec_addr, 0x1000);
        c_rc = pc.rc; c_err = pc.err;
        if (pc.rc == 0) prot_behavior = 1;             // 0 on exec-only: prot-BLIND
        else if (pc.err == ENOMEM) prot_behavior = 2;  // errno on mapped page: protection-AWARE
    }
    // The pinned-surface (CaptureTruncated, Mapped, None) refusal is asserted
    // in BOTH decisive branches: prot-blind makes the refusal CORRECT
    // (mapped!=readable), protection-aware would only license a FUTURE
    // source-set extension (production code unchanged here).
    {
        PoolShapeCheck truncated{false, "ct", "capture-truncated",
                                 PoolShapeCheck::FailureClass::CaptureTruncated};
        const auto res_ct = resolve_listing_gap(truncated, RangeMappedVerdict::Mapped,
                                                ReadableCoverageSource::None);
        const bool refusal_ok = !res_ct.ok;
        pass = pass && refusal_ok;
        char addr_buf[32];
        snprintf(addr_buf, sizeof addr_buf, "0x%llx", (unsigned long long)exec_addr);
        std::string c_detail = std::string("{\"constructed\":") + (exec_only ? "true" : "false") +
            ",\"via\":\"" + (via_mprotect ? "mprotect" : (exec_only ? "own-vmmap" : "none")) + "\"" +
            ",\"mprotect_rc\":" + std::to_string(mprotect_rc) +
            ",\"mprotect_errno\":" + std::to_string(mprotect_err) +
            ",\"sysctl_errno\":" + std::to_string(walk.sysctl_errno) +
            ",\"vmmap_entries\":" + std::to_string(walk.entries) +
            ",\"exec_addr\":\"" + addr_buf + "\"" +
            ",\"kve_prot\":" + std::to_string(exec_prot) +
            ",\"raw_rc\":" + std::to_string(c_rc) +
            ",\"raw_errno\":" + std::to_string(c_err) +
            ",\"text_kve_prot_found\":" + (walk.text_prot_found ? "true" : "false") +
            ",\"text_kve_prot\":" + std::to_string(walk.text_prot) +
            ",\"prot_behavior\":" + std::to_string(prot_behavior) +
            ",\"refusal_asserted\":" + (refusal_ok ? "true" : "false") + "}";
        put("case_c_exec_only", c_detail, true);
    }
    if (page_c != MAP_FAILED) munmap(page_c, 0x1000);

    // ---- case d: unsupported request classes refuse BEFORE any syscall;
    // a live Unavailable verdict refuses the listing gap (even ListedSpan).
    const RawProbe d0 = selftest_raw_mincore(0x1000, 0);          // len == 0
    const RawProbe d1 = selftest_raw_mincore(0x1000, 0x2000000);  // 32 MiB > 16 MiB
    const AddressSpan span_x{1, 2};
    const PoolShapeCheck nc = check_pool_shape(
        1u, (1u << 16) | 7u, 0x50000, &span_x, 1, 0x1000000);
    const auto res_d = resolve_listing_gap(nc, RangeMappedVerdict::Unavailable,
                                           ReadableCoverageSource::ListedSpan);
    const bool d0_ok = d0.rc == LLONG_MIN && d0.verdict == -1;
    const bool d1_ok = d1.rc == LLONG_MIN && d1.verdict == -1;
    const bool d_ok = d0_ok && d1_ok && !res_d.ok;
    put("case_d_unsupported", std::string("{\"len0_pre_syscall_refused\":") +
        (d0_ok ? "true" : "false") +
        ",\"oversize_pre_syscall_refused\":" + (d1_ok ? "true" : "false") +
        ",\"live_unavailable_refuses\":" + (!res_d.ok ? "true" : "false") + "}", false);
    pass = pass && d_ok;

    out.pass = pass;
    out.status = pass ? "pass" : "fail";
    out.detail = d;
    return out;
}
#endif  // !HOST_SELFTEST

struct ParseResult {
    bool ok = false;
    ParsedMod mod;
    std::vector<ParsedFile> files;
    std::string detail;
};

// Check 2: the three loadlist rows parse into the mod/file structures.
ParseResult check_parser()
{
    ParseResult r;
    mf_reader reader;
    mf_record rec;
    int rc;
    int saw_mod = 0, saw_main = 0, saw_enabled = 0;
    mf_reader_init(&reader, kLoadlistText, std::strlen(kLoadlistText));
    while ((rc = mf_next(&reader, &rec)) > 0) {
        if (rec.kind == MF_REC_MOD) {
            ++saw_mod;
            r.mod = {std::string(rec.f[0].p, rec.f[0].n), std::string(rec.f[1].p, rec.f[1].n)};
        } else if (rec.kind == MF_REC_FILE) {
            ParsedFile f{std::string(rec.f[0].p, rec.f[0].n),
                         std::string(rec.f[1].p, rec.f[1].n), rec.size};
            if (f.role == "lua_mod" && f.path == kMainLuaKey && f.size == 21415) ++saw_main;
            else if (f.role == "lua_mod" && f.path == kEnabledKey && f.size == 0) ++saw_enabled;
            r.files.push_back(std::move(f));
        }
    }
    char lineinfo[64];
    std::snprintf(lineinfo, sizeof lineinfo, "rc=%d line=%d", rc, reader.line);
    r.ok = (rc == 0 && saw_mod == 1 && saw_main == 1 && saw_enabled == 1 &&
            r.files.size() == 2 &&
            r.mod.name == kModName && r.mod.version == kModVersion);
    r.detail = std::string("\"mf_result\":\"") + lineinfo + "\",\"mod_rows\":" + std::to_string(saw_mod) +
               ",\"file_rows\":" + std::to_string(r.files.size()) +
               ",\"main_row\":" + std::to_string(saw_main) +
               ",\"enabled_row\":" + std::to_string(saw_enabled);
    return r;
}

}  // namespace

int main(int argc, char** argv)
{
    // Sink: argv[1] (target default /data/modbridge-selftest.jsonl). One
    // appended line, bounded; stdout mirror for host capture.
    const char* sink = argc > 1 ? argv[1] : "/data/modbridge-selftest.jsonl";

    JsonLine line("modbridge.payloadselftest");
    bool all_pass = true;

    // ---- Check 1b first: the widen loop is the reference for 1a/4.
    const std::string narrow(kNarrowSample);
    const std::wstring widened = widen_loop(narrow);
    const std::wstring expected = L"TownfallItemSpawner";
    {
        bool pass = widened == expected && widened.size() == narrow.size();
        line.check("1b_widen_loop", pass ? "pass" : "fail",
                   std::string("{\"size\":") + std::to_string(widened.size()) + "}");
        all_pass = all_pass && pass;
    }

    // ---- Check 1a: wstring_convert garbles on-target (skipped on host).
    {
        Check1aResult r = check_wstring_convert_garbles();
        if (!r.ran) {
            line.check("1a_wstring_convert_garbles_on_target", "skipped-host", "");
        } else {
            bool pass = r.garbled_utf8_codec && r.garbled_utf16_codec;
            std::string detail = "{\"codecvt_utf8\":{" + r.detail_utf8_codec + "},\"codecvt_utf8_utf16\":{" +
                                 r.detail_utf16_codec + "}}";
            line.check("1a_wstring_convert_garbles_on_target", pass ? "pass" : "fail", detail);
            all_pass = all_pass && pass;
        }
    }

    // ---- Check 2: parser.
    ParseResult parsed = check_parser();
    line.check("2_loadlist_parse", parsed.ok ? "pass" : "fail",
               "{" + parsed.detail + "}");
    all_pass = all_pass && parsed.ok;

    // ---- Check 3: production NativeModSource over parsed rows.
    std::string read_key_reported = kMainLuaKey;
    size_t read_bytes_reported = 0;
    bool source_ok = false;
    std::string source_detail;
    {
        try {
            std::shared_ptr<const std::string> main_bytes = std::make_shared<const std::string>(
                reinterpret_cast<const char*>(kMainLuaBytes), kMainLuaBytesLen);
            std::shared_ptr<const std::string> empty_bytes = std::make_shared<const std::string>();
            std::vector<RC::Platform::NativeModFile> files;
            for (const ParsedFile& f : parsed.files) {
                if (f.role != "lua_mod") continue;
                if (f.path == kMainLuaKey)
                    files.push_back({f.path, {main_bytes, *main_bytes}});
                else if (f.path == kEnabledKey)
                    files.push_back({f.path, {empty_bytes, *empty_bytes}});
            }
            RC::Platform::NativeModSource source(std::move(files));
            const std::vector<RC::Platform::NativeLuaMod>& mods = source.lua_mods();
            bool mods_ok = mods.size() == 1 &&
                           mods[0].name == kModName &&
                           mods[0].path == kModPath;
            auto buf = source.read(kMainLuaKey);
            bool read_ok = buf.has_value() &&
                           buf->bytes.size() == kMainLuaBytesLen &&
                           std::memcmp(buf->bytes.data(), kMainLuaBytes, kMainLuaBytesLen) == 0;
            // Also serve the enabled marker through the same source.
            bool enabled_ok = source.read(kEnabledKey).has_value() &&
                              source.read(kEnabledKey)->bytes.empty();
            source_ok = mods_ok && read_ok && enabled_ok;
            read_bytes_reported = buf ? buf->bytes.size() : 0;
            source_detail = std::string("\"lua_mods\":") + std::to_string(mods.size()) +
                            ",\"mods_ok\":" + (mods_ok ? "true" : "false") +
                            ",\"read_ok\":" + (read_ok ? "true" : "false") +
                            ",\"read_bytes\":" + std::to_string(read_bytes_reported) +
                            ",\"enabled_ok\":" + (enabled_ok ? "true" : "false") +
                            ",\"bytes_equal\":" + (read_ok ? "true" : "false");
        } catch (const std::exception& e) {
            source_detail = std::string("\"threw\":\"") + e.what() + "\"";
        }
        line.check("3_source_lookup_bytes_equal", source_ok ? "pass" : "fail", "{" + source_detail + "}");
        all_pass = all_pass && source_ok;
    }

    // ---- Check 4: fixed normalize_path_for_lua over the widened wide path.
    {
        bool key_ok = false;
        std::string detail;
        try {
            const std::wstring wide_mod = widen_loop(kModPath);
            const std::wstring wide_file = widen_loop(kMainLuaKey);
            const std::string norm_mod = normalize_path_for_lua(std::filesystem::path(wide_mod));
            const std::string norm_file = normalize_path_for_lua(std::filesystem::path(wide_file));
            key_ok = norm_mod == kModPath && norm_file == kMainLuaKey;
            detail = "\"norm_mod\":" + quote(norm_mod) +
                     ",\"norm_file\":" + quote(norm_file) +
                     ",\"equals_registered\":" + (key_ok ? "true" : "false");
        } catch (const std::exception& e) {
            detail = std::string("\"threw\":\"") + e.what() + "\"";
        }
        line.check("4_normalize_key_equality", key_ok ? "pass" : "fail", "{" + detail + "}");
        all_pass = all_pass && key_ok;
    }

    // ---- Check 5: range_verdict_semantics (TARGET-decisive; host runs the
    // pure rc_seed logic subset; raw syscall cases are target evidence).
    {
        RangeCaseOut logic = range_logic_pure();
#if !defined(HOST_SELFTEST)
        RangeCaseOut live = range_live_target();
        const bool pass5 = logic.pass && live.pass;
        std::string detail5 = "{\"pure\":" + logic.detail + ",\"live\":{" + live.detail + "}}";
        line.check("5_range_verdict_semantics", pass5 ? "pass" : "fail", detail5);
        all_pass = all_pass && pass5;
#else
        line.check("5_range_verdict_semantics", logic.status,
                   std::string("{\"pure\":") + logic.detail +
                   ",\"live\":\"skipped-host-raw-syscall-cases-are-target-evidence\"}");
        all_pass = all_pass && logic.pass;
#endif
    }

    (void)read_key_reported;
    std::string record = line.finish(kMainLuaKey, 21415, kMainLuaSha256, kMainLuaBytesLen);
#if !defined(HOST_SELFTEST)
    {
        // The SYS_mincore number actually compiled from the SDK header
        // (kernel-header-sourced, never hand-typed — same discipline as the
        // production bridge).
        char sysnum[48];
        std::snprintf(sysnum, sizeof sysnum, ",\"sys_mincore\":%d", static_cast<int>(SYS_mincore));
        record.insert(record.size(), sysnum);  // root object still open here
    }
#endif
    record += ",\"all_pass\":";
    record += all_pass ? "true" : "false";
    record += "}\n";

    std::fputs(record.c_str(), stdout);
    bool wrote = false;
    if (std::FILE* out = std::fopen(sink, "a")) {
        wrote = std::fputs(record.c_str(), out) >= 0;
        std::fclose(out);
    }
    return (all_pass && wrote) ? 0 : 1;
}
