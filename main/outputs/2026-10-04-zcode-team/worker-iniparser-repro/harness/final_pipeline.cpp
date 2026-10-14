// Reproduces the TARGET's exact emergency-message pipeline:
//   1. deserialize throws TokenParser syntax error on NUL-interleaved content
//   2. ensure_str(e.what())  -- PS5-broken from_bytes (NUL-interleave)
//   3. fmt::format(L"The IniParser failed to parse: {}")  -- concatenation
//   4. to_string(message)    -- PS5-broken to_bytes ([::2] stride)
//   5. ErrorObject::copy_error(strlen-capped memcpy)
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::u16string ps5_from_bytes(const std::string& in) {
    std::u16string out;
    for (size_t c = 0; c < in.size(); c += 8) {
        size_t n = std::min<size_t>(8, in.size() - c);
        uint32_t b[8] = {0};
        for (size_t i = 0; i < n; ++i) b[i] = (uint32_t)(unsigned char)in[c + i];
        const uint16_t* e = (const uint16_t*)b;
        for (size_t i = 0; i < 2 * n; ++i) out.push_back(e[i]);
    }
    return out;
}
static std::string ps5_to_bytes(const std::u16string& in) {
    std::string out;
    size_t i = 0;
    while (i < in.size()) {
        uint32_t u = 0;
        memcpy(&u, in.data() + i, std::min<size_t>(4, (in.size() - i) * 2));
        uint32_t ch = u & 0xFFFF;
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 3 < in.size()) { // surrogate +8 path (never hit here)
            i += 4; continue;
        }
        if (ch <= 0x7F) out.push_back((char)ch);
        else if (ch <= 0x7FF) { out.push_back((char)(0xC0|(ch>>6))); out.push_back((char)(0x80|(ch&0x3F))); }
        else { out.push_back((char)(0xE0|(ch>>12))); out.push_back((char)(0x80|((ch>>6)&0x3F))); out.push_back((char)(0x80|(ch&0x3F))); }
        i += 2;
    }
    return out;
}
int main() {
    const char* what_text = "Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey";
    // 2. target ensure_str
    std::u16string suffix = ps5_from_bytes(what_text);
    // 3. fmt concatenation
    std::u16string msg;
    for (const char* p = "The IniParser failed to parse: "; *p; ++p) msg.push_back(*p);
    msg += suffix;
    // 4. target to_string
    std::string stored_raw = ps5_to_bytes(msg);
    // 5. ErrorObject: strlen-capped
    const char* stored = stored_raw.c_str();
    printf("recovered e.what()          : %s\n", what_text);
    printf("full intended wide message  : The IniParser failed to parse: %s\n", what_text);
    printf("stored ErrorObject message  : '%s' (visible strlen %zu, raw %zu bytes)\n", stored, strlen(stored), stored_raw.size());
    printf("backend detail line         : 'Native UE4SS construction rejected: %s'\n", stored);
    // Compare against the live evidence from /tmp/l11run2-status.txt
    const char* expected = "TeIiasrfie opre ";
    printf("live evidence detail suffix : '%s'\n", expected);
    printf("MATCH: %s\n", strcmp(stored, expected) == 0 ? "YES (byte-exact)" : "NO");
    return 0;
}
