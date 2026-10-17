#include <cstdio>
#include <string>
#include <IniParser/Ini.hpp>
#include <Helpers/String.hpp>

static std::u16string garble(const std::string& in) {
    std::u16string out;
    for (size_t chunk = 0; chunk < in.size(); chunk += 8) {
        const size_t n = std::min<size_t>(8, in.size() - chunk);
        uint32_t buf[8] = {0};
        for (size_t i = 0; i < n; ++i) buf[i] = (uint32_t)(unsigned char)in[chunk + i];
        const uint16_t* e = (const uint16_t*)buf;
        for (size_t i = 0; i < 2 * n; ++i) out.push_back(e[i]);
    }
    return out;
}
static void t(const char* label, const std::string& raw) {
    auto g = garble(raw);
    try { RC::Ini::Parser p; p.parse(g); std::printf("[%s] OK\n", label); }
    catch (const std::exception& e) { std::printf("[%s] THROW: %s\n", label, e.what()); }
}
int main() {
    t("s1-section-only", "[General]\n");
    t("s2-key", "[General]\nKey=0\n");
    t("s3-comment-then", "; comment\n[General]\nKey=0\n");
    t("s4-bracket-comment", "; no [Overrides] section\n[General]\nKey=0\n");
    t("s5-comment-with-colon", "; (SettingsManager.cpp:60-68). note\n[General]\nKey=0\n");
    t("s6-blank-line", "; a\n\n[General]\nKey=0\n");
    t("s7-real-head", "; UE4SS-settings.ini - composed native runtime (PS5) first-run configuration.\n; Consumed ONLY from the logical activation key \"ue4ss/UE4SS-settings.ini\"\n; (SettingsManager.cpp:60-68). Every key below is parsed by the retained\n; SettingsManager::deserialize; anything unlisted keeps its code default.\n; NOTE: no [Overrides] section: native activation refuses non-empty\n; ModsFolderPath / ControllingModsTxt (SettingsManager.cpp:112-113) and the\n; retained parser wedges on a section that contains only comments.\n\n[General]\nbUseUObjectArrayCache=false\n");
    t("s8-lastline-no-nl", "[General]\nKey=0");
    t("s9-key-8chars", "[General]\nABCDEFGH=0\n");
    t("s10-key-9chars", "[General]\nABCDEFGHI=0\n");
    return 0;
}
