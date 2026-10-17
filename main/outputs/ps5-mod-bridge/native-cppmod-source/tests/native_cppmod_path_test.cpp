#include <Platform/NativeCppModPath.hpp>
#include <Platform/CppModModule.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
static void put(const fs::path& path, const char* bytes = "native-placeholder")
{
    std::ofstream file(path, std::ios::binary);
    file << bytes;
    assert(file.good());
}
int main(int argc, char** argv)
{
    assert(argc == 2);
    const fs::path directory(argv[1]);
    fs::create_directories(directory);
    const auto choose = [&] { return RC::Platform::FindNativeCppModModule(directory, "Example"); };
    assert(!choose());
    put(directory / "Example.dll");
    assert(choose()->filename() == "Example.dll");
    put(directory / "main.dll", "MZrejected-Windows-binary");
    assert(choose()->filename() == "main.dll");
    bool refused_pe = false;
    try { RC::Platform::CppModModule::Open(*choose()); }
    catch (const std::runtime_error& e) { refused_pe = std::string(e.what()).find("Windows PE") != std::string::npos; }
    assert(refused_pe);
    for (const char* name : {"Example.sprx", "Example.prx", "main.sprx", "main.prx"})
    {
        put(directory / name);
        assert(choose()->filename() == name);
    }
    fs::remove(directory / "main.prx");
    fs::create_directory(directory / "main.prx");
    assert(choose()->filename() == "main.sprx");
    bool bad_name = false;
    try { RC::Platform::FindNativeCppModModule(directory, "../Other"); }
    catch (const std::invalid_argument&) { bad_name = true; }
    assert(bad_name);
    bool relative = false;
    try { RC::Platform::FindNativeCppModModule("relative", "Example"); }
    catch (const std::invalid_argument&) { relative = true; }
    assert(relative);
}
