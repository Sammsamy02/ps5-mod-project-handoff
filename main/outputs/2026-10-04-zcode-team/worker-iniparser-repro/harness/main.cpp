// worker-iniparser-repro HOST REPRODUCTION HARNESS
// Compiles the REAL IniParser + ParserBase sources from the composed port tree
// (native-coherent-runtime-1/source/port/first/src) against the REAL staged
// UE4SS-settings.ini (1448 B, sha256 d8faa4ec...) and replays the target's
// exact conversion chain:
//   target: ensure_str(narrow)  -> std::wstring_convert<codecvt_utf8<wchar_t>>::from_bytes
//           -> PS5 SDK libc++ __codecvt_utf8<wchar_t>::do_in -> utf8_to_utf16(uint32_t*)
//           -> 4-BYTE-stride writes into a 2-byte-wchar buffer + /2 to_next fixup
//           => every char followed by a NUL element (garbled-but-parseable?)
//   target: to_string(wide)     -> codecvt_utf8_utf16<wchar_t>::do_out -> utf16_to_utf8(uint32_t*)
//           -> 4-BYTE-stride reads  => exactly [::2] of the true string
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <stdexcept>

#include <IniParser/Ini.hpp>
#include <Helpers/String.hpp>

static const std::string kIniPath = std::string(INI_PATH);

// Faithful host emulation of the PS5 libc++ short-wchar from_bytes bug
// (disassembled from sdk-v0.43 libc++.a locale.cpp.o):
// utf8_to_utf16(uint32_t* dest): writes each unit as 32-bit store, dest +4,
// wrapper halves to_nxt -> wstring elements = c0,0,c1,0,c2,0,...
static std::u16string ps5_broken_from_bytes(const std::string& in) {
    std::u16string out;
    for (size_t chunk = 0; chunk < in.size(); chunk += 8) {
        const size_t n = std::min<size_t>(8, in.size() - chunk);
        uint32_t buf[8];
        for (size_t i = 0; i < n; ++i) buf[i] = (uint32_t)(unsigned char)in[chunk + i];
        // to_nxt halved: 4n bytes -> 2n elements
        const uint16_t* elems = reinterpret_cast<const uint16_t*>(buf);
        for (size_t i = 0; i < 2 * n; ++i) out.push_back(elems[i]);
    }
    return out;
}

// Faithful host emulation of the PS5 libc++ short-wchar to_bytes bug:
// utf16_to_utf8(uint32_t* src): reads 32-bit per unit, truncates to 16 bits
// (movzwl), advances +4 (surrogate low: +8) -> consumes every OTHER element.
static std::string ps5_broken_to_bytes(const std::u16string& in) {
    std::string out;
    size_t i = 0;
    while (i < in.size()) {
        uint32_t unit = 0;
        memcpy(&unit, in.data() + i, std::min<size_t>(4, (in.size() - i) * 2));
        const uint32_t ch = unit & 0xFFFF;                    // movzwl %r11w
        if ((ch >= 0xD800 && ch <= 0xDBFF) && i + 3 < in.size()) {
            uint32_t lo = 0; memcpy(&lo, in.data() + i + 2, 4);
            lo &= 0xFFFF;
            if ((lo & 0xFC00) != 0xDC00) return out + "<invalid-surrogate>";
            uint32_t c = 0x10000 + (((ch & 0x3FF) << 10) | (lo & 0x3FF));
            out.push_back((char)(0xF0 | (c >> 18)));
            out.push_back((char)(0x80 | ((c >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (c & 0x3F)));
            i += 4;                                           // addq $0x8,%r14
        } else {                                              // BMP path (incl. ASCII)
            if (ch <= 0x7F) out.push_back((char)ch);
            else if (ch <= 0x7FF) { out.push_back((char)(0xC0 | (ch >> 6))); out.push_back((char)(0x80 | (ch & 0x3F))); }
            else { out.push_back((char)(0xE0 | (ch >> 12))); out.push_back((char)(0x80 | ((ch >> 6) & 0x3F))); out.push_back((char)(0x80 | (ch & 0x3F))); }
            i += 2;                                           // addq $0x4,%r14
        }
    }
    return out;
}

static void try_parse(const char* label, std::u16string& contents) {
    try {
        RC::Ini::Parser parser;
        parser.parse(contents);
        std::printf("[%s] parse COMPLETED (no exception)\n", label);
        // report what SettingsManager lookups would actually see
        auto wide = [](const char* s) { std::u16string r; while (*s) r.push_back(*s++); return r; };
        std::printf("[%s] parse OK; spot-checks (expected values in parentheses):\n", label);
        std::printf("    General.bUseUObjectArrayCache = %d (0)\n", (int)parser.get_bool(wide("General"), wide("bUseUObjectArrayCache"), true));
        std::printf("    General.SecondsToScanBeforeGivingUp = %lld (120)\n", (long long)parser.get_int64(wide("General"), wide("SecondsToScanBeforeGivingUp"), 30));
        std::printf("    EngineVersionOverride.MajorVersion = %lld (5)\n", (long long)parser.get_int64(wide("EngineVersionOverride"), wide("MajorVersion"), 0));
        std::printf("    Hooks.HookProcessInternal = %d (0)\n", (int)parser.get_bool(wide("Hooks"), wide("HookProcessInternal"), true));
        std::printf("    Debug.RenderMode = '%s' (ExternalThread)\n", [](const std::u16string& s){ std::string r; for (auto c : s) r.push_back((char)c); return r; }(parser.get_string(wide("Debug"), wide("RenderMode"))).c_str());
    } catch (const std::exception& e) {
        std::printf("[%s] parse THREW: what()='%s'\n", label, e.what());
        // the exact target emergency-message pipeline:
        auto wide_msg = RC::ensure_str(std::string("The IniParser failed to parse: ") + e.what());
        auto stored = ps5_broken_to_bytes(wide_msg);
        std::printf("[%s] stored ErrorObject message would be: '%s' (strlen %zu)\n",
                    label, stored.c_str(), std::strlen(stored.c_str()));
    }
}

int main() {
    std::printf("== worker-iniparser-repro host harness ==\n");
    FILE* f = fopen(kIniPath.c_str(), "rb");
    if (!f) { std::printf("cannot open ini at %s\n", kIniPath.c_str()); return 2; }
    std::string raw;
    char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) raw.append(buf, n);
    fclose(f);
    std::printf("ini bytes: %zu\n", raw.size());
    if (raw.find('\0') != raw.npos) { std::printf("raw ini contains NUL\n"); return 2; }

    // CASE A: correct conversion (what a working from_bytes gives the parser)
    std::u16string clean(raw.begin(), raw.end());
    try_parse("A-clean", clean);

    // CASE B: PS5-broken conversion (what the target actually feeds the parser)
    std::u16string garbled = ps5_broken_from_bytes(raw);
    try_parse("B-ps5garbled", garbled);

    // CASE C: what the target stores when deserialize throws a literal-message
    // exception (e.g. "Native UE4SS settings contain NUL")
    {
        auto wide_msg = RC::ensure_str(std::string("The IniParser failed to parse: Native UE4SS settings contain NUL"));
        auto stored = ps5_broken_to_bytes(wide_msg);
        std::printf("[C-literal] stored message would be: '%s' (strlen %zu)\n", stored.c_str(), std::strlen(stored.c_str()));
    }
    return 0;
}
