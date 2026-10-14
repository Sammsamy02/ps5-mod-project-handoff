#include <Platform/NativeModSource.hpp>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace RC::Platform;
static NativeModFile file(std::string path, std::string content)
{
    auto bytes = std::make_shared<const std::string>(std::move(content));
    return {std::move(path), {bytes, *bytes}};
}
template<class F> static void rejects(F action)
{
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
int main()
{
    std::optional<NativeFileBuffer> retained;
    std::weak_ptr<const void> weak;
    {
        auto entry = file("ue4ss/Mods/Alpha/Scripts/main.lua", "return 17");
        weak = entry.buffer.owner;
        NativeModSource source({entry,
            file("ue4ss/Mods/Alpha/enabled.txt", ""),
            file("ue4ss/Mods/Beta/scripts/main.lua", "return 2"),
            file("ue4ss/Mods/Beta/enabled.txt", ""),
            file("ue4ss/Mods/Gamma/Scripts/main.lua", "return 3"),
            file("ue4ss/Mods/Disabled/scripts/main.lua", "return 4"),
            file("ue4ss/Mods/mods.txt", "\xef\xbb\xbfGamma : 1\r\nBeta : 1\nDisabled : 0\n")});
        assert(source.lua_mods().size() == 4);
        assert(source.lua_mods()[0].scripts_path == "ue4ss/Mods/Alpha/Scripts");
        assert((source.startup_mod_names() == std::vector<std::string>{"Gamma", "Beta", "Alpha"}));
        retained = source.read("ue4ss/Mods/Alpha/Scripts/main.lua");
        assert(retained && retained->bytes == "return 17");
        assert(!source.read("ue4ss/absent.ini"));
        rejects([&] { source.read("ue4ss/Mods/../outside"); });
        rejects([&] { source.read("/ue4ss/Mods/Alpha/Scripts/main.lua"); });
        rejects([&] { source.read("ue4ss//bad"); });
    }
    assert(!weak.expired() && retained->bytes == "return 17");
    retained.reset();
    assert(weak.expired());
    rejects([] { NativeModSource x({file("ue4ss/a", "1"), file("ue4ss/a", "2")}); });
    rejects([] { NativeModSource x({file("ue4ss/../bad", "1")}); });
    rejects([] { NativeModSource x({file("other/a", "1")}); });
    rejects([] { NativeModSource x({file(std::string("ue4ss/") + char(0xff), "1")}); });
    rejects([] { NativeModSource x({NativeModFile{"ue4ss/a", {{}, "abc"}}}); });
    rejects([] { NativeModSource x({file("ue4ss/Mods/A/Scripts/main.lua", ""),
        file("ue4ss/Mods/A/scripts/helper.lua", "")}); });
    NativeModSource marker({file("ue4ss/Mods/A/Scripts/main.lua", ""),
        file("ue4ss/Mods/A/enabled.txt", ""), file("ue4ss/Mods/mods.txt", "A : 0\n")});
    // Preserve UE4SS's marker fallback even for a mods.txt-disabled row.
    assert((marker.startup_mod_names() == std::vector<std::string>{"A"}));
    rejects([] { NativeModSource x({file("ue4ss/Mods/mods.txt", "A:yes\n")}); });
    rejects([] { NativeModSource x({file("ue4ss/Mods/mods.txt", std::string("A:1\0B:1", 7))}); });
    NativeModSource unicode({file("ue4ss/Mods/\xc3\xa9/Scripts/main.lua", ""),
        file("ue4ss/Mods/\xc3\xa9/enabled.txt", ""),
        file("ue4ss/Mods/Shared/Scripts/main.lua", ""), file("ue4ss/Mods/Shared/enabled.txt", "")});
    assert((unicode.startup_mod_names() == std::vector<std::string>{"\xc3\xa9"}));
}
