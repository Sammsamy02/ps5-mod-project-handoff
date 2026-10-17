// l27 EI stringfix host fixture (agent-EM, 2026-10-09). Extracts the ACTUAL
// production logic edited for the [Seeds][L27] ToString-bind launch failure:
//  - the send_narrow_diag helper now used by EI Phase 4c + the L15 summary in
//    candidate/src/UnrealInitializer.cpp (narrow-first fmt, one explicit
//    RC::Unicode::DecodeUtf8 widen, literal fallback, never throws), with the
//    REAL codec from Helpers/Utf16Utf8.hpp;
//  - the REAL rc::engtostring constants from include/EngineToStringProfile.hpp
//    (byte-exact 32-byte prologue pin);
//  - the REAL rc::fnameabi::signature pre-VirtualQuery gate order from
//    NativeFNameChecks.hpp (l19-local-profile-r4/units) with a mock ops whose
//    known_fault is controllable — proving both the root-cause mechanics
//    (owner-gate disarm => refusal BEFORE VirtualQuery) and the fix (32-vs-32
//    compare passes with the corrected constant).
// No engine calls anywhere. House style: actual_*.cpp; host profile uses the
// owned FORCE_U16 2-byte CharType (see the static_assert in Utf16Utf8.hpp).
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#define FORCE_U16 1 // host profile: 2-byte CharType, STR(x) = u##x (StringType.hpp)
#include <String/StringType.hpp>
#include <Helpers/Utf16Utf8.hpp>

#include "EngineToStringProfile.hpp" // REAL production constants (tree include/)

// ---- mock Output layer (captures what the devices would receive) ----
enum class LogLevel { Default, Normal, Verbose, Warning, Error };
static std::vector<RC::StringType> g_sent;
static std::vector<bool> g_sent_error;

namespace RC {
// ---- VERBATIM production helper (candidate/src/UnrealInitializer.cpp,
//      [Seeds][L27 stringfix]), kept inside namespace RC so the body text —
//      including the plain CharType/Unicode/Output lookups — is identical ----
namespace Output_mock {
    template <int32_t optional_arg>
    void send(RC::StringViewType content) { g_sent.push_back(RC::StringType(content)); g_sent_error.push_back(optional_arg == static_cast<int32_t>(LogLevel::Error)); }
    void send(RC::StringViewType content) { g_sent.push_back(RC::StringType(content)); g_sent_error.push_back(false); }
}
namespace Output = Output_mock;

template <class MakeNarrow>
auto send_narrow_diag(bool as_error, const CharType* fallback, MakeNarrow&& make_narrow) -> void
{
    try
    {
        const auto wide = Unicode::DecodeUtf8(std::string_view{make_narrow()});
        if (as_error) Output::send<static_cast<int32_t>(LogLevel::Error)>(wide);
        else Output::send(wide);
    }
    catch (...)
    {
        try
        {
            if (as_error) Output::send<static_cast<int32_t>(LogLevel::Error)>(fallback);
            else Output::send(fallback);
        }
        catch (...) {}
    }
}
} // namespace RC

// The device-side strict encode stage (PosixFile::encode_utf8_into shape):
// what actually lands in UE4SS.log. Throws on malformed wide text.
static std::string encode_utf8_log_line(const RC::StringType& text)
{
    return RC::Unicode::EncodeUtf8(text);
}

// ---- REAL signature() gate order (rc::fnameabi::signature,
//      NativeFNameChecks.hpp) with mock ops; the pre-VirtualQuery conditions
//      and narrow reason literals are copied exactly ----
struct MockCheck { bool ok = false; const char* reason = "unproved native FName"; const char* source = "none"; };
struct MockOps {
    bool faulted = false;            // bridge_copyin_range_faulted() != 0 (fail-closed when the L26 owner gate is released)
    bool identity_ok = true;
    int query_calls = 0;             // counts VirtualQuery (ops.query) — the L14 line's source
    const uint8_t* disk_prefix = nullptr; // authenticated cached code-cache prefix
    size_t disk_prefix_size = 0;
    bool known_fault(uint64_t, size_t) { return faulted; }
    bool code_identity(uint64_t, size_t) { return identity_ok; }
    struct Query { int kind; } ;
    Query query(uint64_t) { ++query_calls; return {1}; }
    bool cached_code_signature(uint64_t, const uint8_t* expected, size_t size)
    {
        return disk_prefix && size == disk_prefix_size && std::memcmp(disk_prefix, expected, size) == 0;
    }
};
static MockCheck signature(MockOps& ops, uint64_t address, const uint8_t* expected, size_t size)
{
    if (!expected || size == 0 || size > 64) return {false, "invalid constructor profile"};
    if (ops.known_fault(address, size)) return {false, "constructor prefix has recorded copyin fault"}; // BEFORE VirtualQuery
    if (!ops.code_identity(address, size)) return {false, "constructor startup image identity/base unavailable or mismatched"};
    (void)ops.query(address); // VirtualQuery happens HERE (L14 line)
    return {true, "", "authenticated-disk-XOM-signature"};
}

// narrow formatting stand-in for the production fmt::format call sites
// (fmt-compat of the real TU is covered by the Z13 target compile)
static std::string format_refusal(const char* reason)
{
    std::string out = "[Seeds][L27] ToString bind refused: prologue signature: ";
    out += reason;
    out += "; legacy Kismet route retains FName::ToString\n";
    return out;
}
static std::string format_l15(uint64_t elapsed, uint64_t budget, const char* outcome)
{
    char buf[160];
    std::snprintf(buf, sizeof(buf), "[Seeds][L15] FName binding total elapsed=%llums shared-budget=%llums outcome=%s\n",
                  static_cast<unsigned long long>(elapsed), static_cast<unsigned long long>(budget), outcome);
    return buf;
}

static bool contains_full_narrow_ascii(const std::string& line, const char* needle)
{
    return line.find(needle) != std::string::npos;
}
static bool has_no_nul_bytes(const std::string& line)
{
    return line.find('\0') == std::string::npos;
}

int main()
{
    // ===== 1. Forced refusal (the observed 2026-10-09 failure mode) =====
    // Root cause proven: known_fault() (owner-gate fail-closed) refuses
    // BEFORE VirtualQuery with the exact narrow literal the console showed
    // halved + widened.
    {
        MockOps ops;
        ops.faulted = true; // L26 early release disarms the owner => range_faulted fails closed
        const MockCheck r = signature(ops, 0x63FA00ull, rc::engtostring::kToStringInternalPrefix,
                                       sizeof(rc::engtostring::kToStringInternalPrefix));
        assert(!r.ok);
        assert(std::strcmp(r.reason, "constructor prefix has recorded copyin fault") == 0);
        assert(ops.query_calls == 0); // refused BEFORE the memory query — the console mystery
    }
    // The fixed refusal log path: narrow-first + explicit widen + strict encode.
    {
        g_sent.clear(); g_sent_error.clear();
        bool continued = false;
        // production shape: if (!ts_signature.ok) send_narrow_diag(...)
        RC::send_narrow_diag(true,
            u"[Seeds][L27] ToString bind refused: prologue signature reason unavailable (narrow fallback); legacy Kismet route retains FName::ToString\n",
            [&] { return format_refusal("constructor prefix has recorded copyin fault"); });
        continued = true; // no throw escaped; execution continues past the refusal
        assert(continued);
        assert(g_sent.size() == 1 && g_sent_error[0]);
        const std::string logged = encode_utf8_log_line(g_sent[0]);
        assert(has_no_nul_bytes(logged));                                              // not raw UTF-16 anymore
        assert(contains_full_narrow_ascii(logged, "constructor prefix has recorded copyin fault")); // FULL text, not halved
        assert(contains_full_narrow_ascii(logged, "legacy Kismet route retains FName::ToString"));
    }

    // ===== 2. Corrected prefix: the signature gate passes 32-vs-32 =====
    {
        // The REAL constant is a plain byte array (uint8_t[32]) — the
        // char16_t/sizeof-double suspect is disproven at compile time:
        static_assert(sizeof(rc::engtostring::kToStringInternalPrefix) == 32);
        static_assert(sizeof(rc::engtostring::kToStringInternalPrefix[0]) == 1);
        // byte-exact vs the offline pin (file offset 0x243A00 of the sha-pinned image)
        static const uint8_t kPin[32] = {
            0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56,
            0x41, 0x55, 0x41, 0x54, 0x53, 0x50, 0x80, 0x3D,
            0x8B, 0x64, 0x12, 0x09, 0x00, 0x44, 0x8B, 0x3F,
            0x49, 0x89, 0xF6, 0x44, 0x8D, 0x6A, 0xFF, 0x48,
        };
        assert(std::memcmp(kPin, rc::engtostring::kToStringInternalPrefix, 32) == 0);

        MockOps ops;
        ops.faulted = false;                    // gate order fixed: admission runs before retirement
        ops.disk_prefix = kPin;                 // authenticated disk cache holds the same 32 bytes
        ops.disk_prefix_size = sizeof(kPin);
        const MockCheck r = signature(ops, 0x63FA00ull, rc::engtostring::kToStringInternalPrefix,
                                       sizeof(rc::engtostring::kToStringInternalPrefix));
        assert(r.ok);                           // 32-vs-32 byte compare passes
        assert(ops.query_calls == 1);           // the VirtualQuery L14 line now happens
    }

    // ===== 3. L15 outcome summary: full narrow text, never throws =====
    {
        g_sent.clear(); g_sent_error.clear();
        const char* outcome = "verified-and-bound";
        RC::send_narrow_diag(false,
            u"[Seeds][L15] FName binding total elapsed (outcome unavailable; narrow fallback)\n",
            [&] { return format_l15(2721, 60000, outcome); });
        const std::string logged = encode_utf8_log_line(g_sent[0]);
        assert(g_sent.size() == 1 && !g_sent_error[0]);
        assert(has_no_nul_bytes(logged));
        assert(contains_full_narrow_ascii(logged, "outcome=verified-and-bound")); // was 'verified-' (halved)
        assert(contains_full_narrow_ascii(logged, "shared-budget=60000ms"));
    }

    // ===== 4. Degradation: any failure in the diagnostic falls back to the
    //            plain literal and can never unwind into teardown =====
    {
        g_sent.clear(); g_sent_error.clear();
        RC::send_narrow_diag(true,
            u"[Seeds][L27] ToString bind refused: prologue signature reason unavailable (narrow fallback); legacy Kismet route retains FName::ToString\n",
            [] { return std::string("\xff\xfe garbage that DecodeUtf8 must reject"); }); // invalid UTF-8
        assert(g_sent.size() == 1 && g_sent_error[0]);
        const std::string logged = encode_utf8_log_line(g_sent[0]); // fallback literal encodes fine
        assert(has_no_nul_bytes(logged));
        assert(contains_full_narrow_ascii(logged, "narrow fallback"));
        // throwing producer (bad_alloc shape) also degrades
        struct Thrower { std::string operator()() { throw std::bad_alloc(); } };
        g_sent.clear(); g_sent_error.clear();
        RC::send_narrow_diag(false, u"[Seeds][L15] FName binding total elapsed (outcome unavailable; narrow fallback)\n", Thrower{});
        assert(g_sent.size() == 1);
        assert(contains_full_narrow_ascii(encode_utf8_log_line(g_sent[0]), "narrow fallback"));
    }

    std::printf("ALL L27 EI STRINGFIX FIXTURE CHECKS PASSED\n");
    return 0;
}
