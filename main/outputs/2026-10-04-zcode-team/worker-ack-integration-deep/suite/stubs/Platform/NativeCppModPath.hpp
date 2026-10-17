// Host-test stub mirroring the canonical NativeCppModPath.hpp surface used by
// activated_mod_source.cpp (candidate order included).
#pragma once

#include <array>
#include <string>
#include <string_view>

namespace RC::Platform
{
    inline auto NativeCppModCandidates(std::string_view mod_name) -> std::array<std::string, 6>
    {
        if (mod_name.empty() || mod_name == "." || mod_name == ".." ||
            mod_name.find_first_of("/\\\0", 0, 3) != mod_name.npos)
            throw std::invalid_argument("Native C++ mod requires one mod name");
        const std::string name(mod_name);
        return {"main.prx", "main.sprx", name + ".prx", name + ".sprx", "main.dll", name + ".dll"};
    }
}
