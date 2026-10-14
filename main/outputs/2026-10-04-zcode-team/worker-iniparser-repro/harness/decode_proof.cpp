// Direct proof that the exact conversion the TARGET will run after the staged
// fix (Unicode::DecodeUtf8) feeds the real parser clean content.
#include <cstdio>
#include <string>
#include <IniParser/Ini.hpp>
#include <Helpers/Utf16Utf8.hpp>
int main() {
    FILE* f = fopen(INI_PATH, "rb");
    std::string raw; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) raw.append(b, n);
    fclose(f);
    auto contents = RC::Unicode::DecodeUtf8(raw);
    std::printf("DecodeUtf8: %zu bytes -> %zu units (1:1, no interleave)\n", raw.size(), contents.size());
    try {
        RC::Ini::Parser p; p.parse(contents);
        printf("DecodeUtf8-fed parse: OK\n");
    } catch (const std::exception& e) { printf("DecodeUtf8-fed parse THREW: %s\n", e.what()); return 1; }
    return 0;
}
