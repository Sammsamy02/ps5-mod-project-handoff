#include "modbridge/nexus.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <string>

// These exact public declarations let the preimplementation link control
// observe missing functions rather than a missing declaration diagnostic.
namespace modbridge {
bool nexus_download_link_url(std::string_view, std::uint64_t, std::uint64_t,
                            std::string_view, std::uint64_t,
                            std::span<char>, std::size_t&) noexcept;
bool nexus_select_download_uri(std::span<const std::uint8_t>,
                               std::span<char>, std::size_t&);
}

static bool zeroed(std::span<const char> bytes) {
    for (char byte : bytes) if (byte) return false;
    return true;
}

int main() {
    using namespace modbridge;
    std::array<char, 8192> out;
    std::size_t written = 99;
    out.fill('x');
    assert(nexus_download_link_url("sample-game", 42, 7, {}, 0, out, written));
    assert(std::string_view(out.data(), written) ==
           "https://api.nexusmods.com/v1/games/sample-game/mods/42/files/7/download_link");
    assert(out[written] == 0 && zeroed(std::span(out).subspan(written + 1)));
    out.fill('x');
    assert(nexus_download_link_url("game_1", 9223372036854775807ULL, 1,
        "a-Z_~.!@&?=/%+\\", 9223372036854775807ULL, out, written));
    assert(std::string_view(out.data(), written) ==
        "https://api.nexusmods.com/v1/games/game_1/mods/9223372036854775807/files/1/download_link?key=a-Z_~.%21%40%26%3F%3D%2F%25%2B%5C&expires=9223372036854775807");
    const auto reject_url = [&](std::string_view domain, std::uint64_t mod,
        std::uint64_t file, std::string_view key, std::uint64_t expires) {
        out.fill('x'); written = 99;
        assert(!nexus_download_link_url(domain, mod, file, key, expires, out, written));
        assert(written == 0 && zeroed(out));
    };
    reject_url("_game", 1, 1, {}, 0);
    reject_url("GAME", 1, 1, {}, 0);
    reject_url("a/b", 1, 1, {}, 0);
    reject_url(std::string(81, 'a'), 1, 1, {}, 0);
    reject_url("game", 0, 1, {}, 0);
    reject_url("game", 1, 0, {}, 0);
    reject_url("game", UINT64_MAX, 1, {}, 0);
    reject_url("game", 1, UINT64_MAX, {}, 0);
    reject_url("game", 1, 1, {}, 1);
    reject_url("game", 1, 1, "key", 0);
    reject_url("game", 1, 1, "key", UINT64_MAX);
    reject_url("game", 1, 1, " ", 1);
    reject_url("game", 1, 1, std::string("a\0b", 3), 1);
    reject_url("game", 1, 1, std::string(1, '\x7f'), 1);
    reject_url("game", 1, 1, std::string(1025, 'a'), 1);
    out.fill('x');
    assert(nexus_download_link_url("game", 1, 1, std::string(1024, '!'), 1, out, written));
    std::array<char, 20> small;
    small.fill('x'); written = 99;
    assert(!nexus_download_link_url("game", 1, 1, {}, 0, small, written));
    assert(written == 0 && zeroed(small));
    out.fill('x'); written = 99;
    assert(!nexus_download_link_url(std::string_view(out.data(), 4), 1, 1, {}, 0, out, written));
    assert(written == 99 && out.front() == 'x' && out.back() == 'x');
    out.fill('x'); written = 99;
    assert(!nexus_download_link_url("game", 1, 1, std::string_view(out.data(), 4), 1, out, written));
    assert(written == 99 && out.front() == 'x');

    const auto select = [&](const std::string& body, bool accepted) {
        out.fill('x'); written = 99;
        bool result = nexus_select_download_uri(
            {reinterpret_cast<const std::uint8_t*>(body.data()), body.size()}, out, written);
        assert(result == accepted);
        if (!result) assert(written == 0 && zeroed(out));
    };
    const std::string first = R"({"URI":"https://cdn.example.test/archive.zip?token=private","name":"Mirror","short_name":"M"})";
    select("[" + first + "]", true);
    assert(std::string_view(out.data(), written) == "https://cdn.example.test/archive.zip?token=private");
    assert(out[written] == 0 && zeroed(std::span(out).subspan(written + 1)));
    select("[" + first + R"(,{"URI":"http://cdn.example.test/bad","name":"B","short_name":"B"}])", false);
    select("[" + first + R"(,{"URI":"https://cdn.example.test/bad","name":7,"short_name":"B"}])", false);
    select("[" + first + R"(,{"URI":"https://cdn.example.test/bad","name":"B"}])", false);
    select("[]", false);
    select("{}", false);
    select("[" + first + "] trailing", false);
    select(R"([{"URI":"https://cdn.example.test/a","URI":"https://cdn.example.test/b","name":"M","short_name":"M"}])", false);
    select(R"([{"URI":"https://cdn.example.test/a","name":"M","short_name":"M","nested":{"x":1,"x":2}}])", false);
    select(R"([{"URI":"https://cdn.example.test/a","name":"M","short_name":"M","extra":{}}])", true);
    for (const auto uri : {"https://user@cdn.example.test/a", "https://cdn.example.test:444/a",
                           "https://cdn.example.test/a#fragment", "https://cdn.example.test/a b",
                           "https:///a", "https://cdn.example.test:/a"}) {
        select(Json::array({{{"URI",uri},{"name","M"},{"short_name","M"}}}).dump(), false);
    }
    for (const auto uri : {"https://cdn.example.test:443/a", "https://127.0.0.1/a",
                           "HTTPS://cdn.example.test/a", "https://[::1]/a"}) {
        select(Json::array({{{"URI",uri},{"name","M"},{"short_name","M"}}}).dump(), true);
    }
    select(Json::array({{{"URI","https://cdn.example.test/\\evil"},{"name","M"},{"short_name","M"}}}).dump(), false);
    select(Json::array({{{"URI","https://cdn.example.test/\n"},{"name","M"},{"short_name","M"}}}).dump(), false);
    select(Json::array({{{"URI","https://cdn.example.test/\x7f"},{"name","M"},{"short_name","M"}}}).dump(), false);
    auto mirrors = Json::array();
    for (unsigned i=0;i<32;++i) mirrors.push_back(Json::parse(first));
    select(mirrors.dump(), true);
    mirrors.push_back(Json::parse(first)); select(mirrors.dump(), false);
    const std::string prefix="https://cdn.example.test/";
    select(Json::array({{{"URI",prefix+std::string(8191-prefix.size(),'a')},{"name","M"},{"short_name","M"}}}).dump(), true);
    assert(written == 8191 && out[written] == 0);
    select(Json::array({{{"URI",prefix+std::string(8192-prefix.size(),'a')},{"name","M"},{"short_name","M"}}}).dump(), false);
    select(Json::array({{{"URI",prefix+"a"},{"name",std::string(257,'a')},{"short_name","M"}}}).dump(), false);
    select(Json::array({{{"URI",prefix+"a"},{"name","M"},{"short_name",std::string(65,'a')}}}).dump(), false);
    auto body = "["+first+"]";
    small.fill('x'); written=99;
    assert(!nexus_select_download_uri({reinterpret_cast<const std::uint8_t*>(body.data()),body.size()},small,written));
    assert(written==std::strlen("https://cdn.example.test/archive.zip?token=private") && zeroed(small));
    written=99;
    assert(!nexus_select_download_uri({reinterpret_cast<const std::uint8_t*>(body.data()),body.size()},{},written));
    assert(written==std::strlen("https://cdn.example.test/archive.zip?token=private"));
    std::memcpy(out.data(),body.data(),body.size()); written=99;
    assert(!nexus_select_download_uri({reinterpret_cast<const std::uint8_t*>(out.data()),body.size()},out,written));
    assert(written==99 && std::string_view(out.data(),body.size())==body);
    select(std::string(8U*1024U*1024U+1U,' '),false);
    select("["+first+", "+std::string(65,'[')+"0"+std::string(65,']')+"]",false);
    return 0;
}
