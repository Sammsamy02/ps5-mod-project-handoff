// Proves the STAGED fix resolves the live failure on the exact target pipeline:
// content: ensure_str(bytes) -> to_wstring -> Unicode::DecodeUtf8  (staged)
// error:   to_string(wide)    -> Unicode::EncodeUtf8            (staged)
#include <cstdio>
#include <string>
#include <IniParser/Ini.hpp>
#include <Helpers/String.hpp>
int main() {
    FILE* f = fopen(INI_PATH, "rb");
    std::string raw; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) raw.append(b, n);
    fclose(f);
    // 1) the content conversion the target performs, now via the staged header
    auto contents = RC::ensure_str(raw);
    std::printf("ensure_str produced %zu units (expected %zu, garbled would be %zu)\n",
                contents.size(), raw.size(), raw.size() * 2);
    try {
        RC::Ini::Parser parser;
        parser.parse(contents);
        auto w = [](const char* s){ std::u16string r; while (*s) r.push_back(*s++); return r; };
        std::printf("FIXED PIPELINE: parse OK; MajorVersion=%lld HookProcessInternal=%d RenderMode=",
                    (long long)parser.get_int64(w("EngineVersionOverride"), w("MajorVersion"), 0),
                    (int)parser.get_bool(w("Hooks"), w("HookProcessInternal"), true));
        for (auto c : parser.get_string(w("Debug"), w("RenderMode"))) putchar((char)c);
        std::printf("\n");
    } catch (const std::exception& e) { std::printf("FIXED PIPELINE STILL THROWS: %s\n", e.what()); return 1; }
    // 2) the error path with the staged to_string: full suffix retained
    auto wide_msg = RC::ensure_str(std::string("The IniParser failed to parse: Syntax error(8 : 2): Expected state NewLineStarted or StartOfFile, got CreateSectionKey"));
    auto stored = RC::to_string(wide_msg);
    std::printf("stored error message (staged): '%s'\n", stored.c_str());
    return 0;
}
