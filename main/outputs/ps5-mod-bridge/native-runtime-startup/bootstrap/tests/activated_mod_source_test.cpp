#include "activated_mod_source.hpp"
#include <Platform/NativeModSource.hpp>
#include <modforge_activation.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace modbridge::native;
namespace fs = std::filesystem;

static void put(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
    if (!file.good()) throw std::runtime_error("fixture write failed");
}

static ActivatedMods activate(const fs::path& root, const char* const* roles = nullptr)
{
    const std::string directory = root.string();
    mfa_config config{};
    config.root = directory.c_str();
    config.title_id = "PPSA00001";
    config.preload_roles = roles;
    mfa_activation* raw = nullptr;
    mfa_error error{};
    const int rc = mfa_activate(&config, &raw, &error);
    if (rc) throw std::runtime_error(error.message);
    return ActivatedMods(raw);
}

template<class Operation> static void rejects(Operation operation)
{
    bool refused = false;
    try { operation(); }
    catch (const std::invalid_argument&) { refused = true; }
    assert(refused);
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const fs::path root = argv[1];
    const std::string main = "return require('helper')\n";
    const std::string helper = "return 136\n";
    const std::string settings = "[General]\nEnableHotReloadSystem = 0\n";
    const std::string header = "modforge-loadlist\t1\ntitle\tPPSA00001\nengine\tunreal\nframework\tue4ss-ps5\n";
    std::string list = header;
    for (const auto& pair : {
        std::pair{"ue4ss/Mods/Example/Scripts/main.lua", main},
        std::pair{"ue4ss/Mods/Example/Scripts/helper.lua", helper},
        std::pair{"ue4ss/Mods/Example/enabled.txt", std::string{}},
        std::pair{"ue4ss/UE4SS-settings.ini", settings}})
    {
        put(root / pair.first, pair.second);
        list += "file\tconfig\t" + std::string(pair.first) + "\t" + std::to_string(pair.second.size()) + "\n";
    }
    // Asset files are not part of the UE4SS buffer source and need not be read.
    list += "file\tpak\tgame/Content/Paks/Example.pak\t123456\n";
    put(root / "loadlist.txt", list);
    auto source = MakeActivatedUE4SSModSource(activate(root), "PPSA00001");
    assert(source->lua_mods().size() == 1);
    assert(source->lua_mods()[0].name == "Example");
    assert(source->startup_mod_names() == std::vector<std::string>{"Example"});
    auto retained = source->read("ue4ss/Mods/Example/Scripts/main.lua");
    assert(retained && retained->bytes == main && retained->owner);
    assert(source->read("ue4ss/UE4SS-settings.ini")->bytes == settings);
    rejects([&] { MakeActivatedUE4SSModSource(activate(root), "PPSA00002"); });
    rejects([&] { MakeActivatedUE4SSModSource({}, "PPSA00001"); });
    // Keep the PAK listed-only in the source fixture so this activation can be
    // made without attempting to read it for this missing-buffer case.
    put(root / "loadlist.txt", list.substr(0, list.rfind("file\tpak\t")));
    const char* const preload_nothing[] = {nullptr};
    rejects([&] { MakeActivatedUE4SSModSource(activate(root, preload_nothing), "PPSA00001"); });
    put(root / "loadlist.txt", list + "file\tnative_plugin\tue4ss/Mods/Binary/dlls/main.prx\t4\n");
    rejects([&] { MakeActivatedUE4SSModSource(activate(root), "PPSA00001"); });
    put(root / "loadlist.txt", "modforge-loadlist\t1\ntitle\tPPSA00001\nengine\tunreal\n");
    rejects([&] { MakeActivatedUE4SSModSource(activate(root), "PPSA00001"); });

    fs::remove_all(root);
    // No remaining deployment files: actual activation-owned bytes must still
    // serve requires/settings, including after the source itself is destroyed.
    assert(source->read("ue4ss/Mods/Example/Scripts/helper.lua")->bytes == helper);
    std::weak_ptr<const void> lifetime = retained->owner;
    source.reset();
    assert(!lifetime.expired() && retained->bytes == main);
    retained.reset();
    assert(lifetime.expired());
}
