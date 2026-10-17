// Host reproduction of the PS5-target to_string behavior.
// PS5: CharType = wchar_t with sizeof==2 (short-wchar), and the SDK's
// prebuilt libc++ utf16_to_utf8 reads units with a 4-BYTE stride
// (movl + movzwl %r11w then +4/+8 pointer bumps), i.e. it consumes
// uint32-encoded units from a 2-byte-element array -> every OTHER char.
// This probe emulates BOTH the correct conversion and the PS5-stride bug.
#include <codecvt>
#include <locale>
#include <string>
#include <string_view>
#include <cstdio>
#include <cstdint>

using ShortWide = std::basic_string<uint16_t>; // stand-in for 2-byte wchar_t (wchar_t is 4B on macOS host)

// Faithful host port of the PS5 libc++ bug as disassembled in
// locale.cpp.o (sdk-v0.43) _ZNSt3__1L13utf16_to_utf8EPKjS1_RS1_PhS3_RS3_mNS_12codecvt_modeE:
// per unit: read 32-bit, truncate to 16 (movzwl %r11w), emit UTF-8,
// advance source by 4 bytes (surrogate low: +8).
static std::string ps5_stride_bug(const uint16_t* data, size_t n) {
    std::string out;
    size_t i = 0;
    while (i < n) {
        uint32_t unit32 = 0;
        memcpy(&unit32, data + i, 4);      // movl (%r14),%r11d  (4-byte load)
        uint32_t ch = unit32 & 0xFFFF;     // movzwl %r11w,%ebx
        if (ch > 0x7F && ch <= 0x7FF && i + 1 < n) {          // +4 path
            out.push_back((char)(0xC0 | (ch >> 6)));
            out.push_back((char)(0x80 | (ch & 0x3F)));
            i += 2;                        // addq $0x4,%r14 over uint16 array
        } else if (ch >= 0xD800 && ch <= 0xDBFF && i + 3 < n) { // +8 surrogate path
            uint32_t lo32 = 0; memcpy(&lo32, data + i + 2, 4);
            uint32_t lo = lo32 & 0xFFFF;
            if ((lo & 0xFC00) != 0xDC00) return "<invalid>";
            uint32_t c = 0x10000 + (((ch & 0x3FF) << 10) | (lo & 0x3FF));
            out.push_back((char)(0xF0 | (c >> 18)));
            out.push_back((char)(0x80 | ((c >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (c & 0x3F)));
            i += 4;                        // addq $0x8,%r14
        } else if (ch <= 0x7F && i + 1 < n) { // the +4 branch for ASCII (same stride)
            out.push_back((char)ch);
            i += 2;
        } else {
            return out + "<truncated>";
        }
    }
    return out;
}

int main() {
    // The exact wide message the PROSPERO emergency path formats:
    // "The IniParser failed to parse: " + e.what()
    const char16_t suffix[] = u"";          // observed: suffix contributed nothing
    std::u16string msg = u"The IniParser failed to parse: ";
    msg += suffix;

    ShortWide wide(msg.begin(), msg.end()); // short-wchar representation on target

    // correct conversion (what a working to_string would produce)
    std::string correct(wide.begin(), wide.end());

    // PS5 stride-bug conversion
    std::string garbled = ps5_stride_bug(wide.data(), wide.size());

    // Also: with a hypothetical NON-empty suffix, what would the garble look like?
    std::u16string msg2 = u"The IniParser failed to parse: Native UE4SS settings contain NUL";
    ShortWide wide2(msg2.begin(), msg2.end());
    std::string garbled2 = ps5_stride_bug(wide2.data(), wide2.size());

    std::printf("full wide message : %s\n", correct.c_str());
    std::printf("PS5 to_bytes out  : '%s'  (len %zu)\n", garbled.c_str(), garbled.size());
    std::printf("expect [::2]      : '%s'\n", [](const std::u16string& s){ std::string r; for (size_t i=0;i<s.size();i+=2) r.push_back((char)s[i]); return r; }(msg).c_str());
    std::printf("with real suffix  : '%s'\n", garbled2.c_str());
    return 0;
}
