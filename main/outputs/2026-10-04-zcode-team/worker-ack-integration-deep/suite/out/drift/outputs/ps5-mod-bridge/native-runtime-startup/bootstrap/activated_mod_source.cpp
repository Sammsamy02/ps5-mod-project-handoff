#include "activated_mod_source.hpp"

#include <Platform/NativeModSource.hpp>
#include <Platform/NativeCppModPath.hpp>
#include <map>
#include <modforge_activation.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace modbridge::native
{
void ActivationDeleter::operator()(mfa_activation* activation) const noexcept
{
    mfa_release(activation);
}

std::shared_ptr<const RC::Platform::NativeModSource>
MakeActivatedUE4SSModSource(ActivatedMods activation, std::string_view expected_title,
                           const std::filesystem::path& activation_root)
{
    if (!activation || expected_title.empty() ||
        expected_title != std::string_view(mfa_title(activation.get())))
        throw std::invalid_argument("UE4SS activation must match the supplied game title");
    if (std::string_view(mfa_engine(activation.get())) != "unreal" ||
        !mfa_wants_framework(activation.get(), "ue4ss-ps5"))
        throw std::invalid_argument("Activation does not request the native UE4SS framework");

    std::shared_ptr<mfa_activation> owner(std::move(activation));
    std::vector<RC::Platform::NativeModFile> files;
    std::map<std::string, std::vector<const mfa_file*>> native_files;
    for (size_t i = 0; i < mfa_file_count(owner.get()); ++i)
    {
        const auto* file = mfa_file_at(owner.get(), i);
        const std::string_view path(file->path);
        if (!path.starts_with("ue4ss/")) continue;
        const std::string_view role(file->role);
        if (role == "native_plugin")
        {
            constexpr std::string_view prefix = "ue4ss/Mods/";
            if (!path.starts_with(prefix))
                throw std::invalid_argument("Native UE4SS module is outside the Mods layout");
            const auto suffix = path.substr(prefix.size());
            const auto slash = suffix.find('/');
            if (slash == suffix.npos || slash == 0 ||
                !suffix.substr(slash).starts_with("/dlls/") ||
                suffix.substr(slash + 6).empty() || suffix.substr(slash + 6).find('/') != path.npos)
                throw std::invalid_argument("Native UE4SS module must be directly under a mod's dlls directory");
            native_files[std::string(suffix.substr(0, slash))].push_back(file);
            continue;
        }
        if (role != "lua_mod" && role != "config" && role != "script")
            throw std::invalid_argument("Unsupported UE4SS activation file role: " + std::string(path));
        if (!file->data || file->size > std::numeric_limits<size_t>::max())
            throw std::invalid_argument("UE4SS file was not preloaded: " + std::string(path));
        files.push_back({std::string(path), {owner,
            std::string_view(reinterpret_cast<const char*>(file->data), static_cast<size_t>(file->size))}});
    }
    std::vector<RC::Platform::NativeCppModSpec> native_specs;
    if (!native_files.empty())
    {
        const auto& root = activation_root.native();
        if (activation_root.empty() || !activation_root.is_absolute() ||
            activation_root.lexically_normal() != activation_root || root.find('\0') != root.npos)
            throw std::invalid_argument("Native activation requires its explicit absolute normalized installation root");
        for (const auto& [name, entries] : native_files)
        {
            const mfa_file* selected = nullptr;
            const auto logical = std::string("ue4ss/Mods/") + name;
            for (const auto& candidate : RC::Platform::NativeCppModCandidates(name))
            {
                const auto key = logical + "/dlls/" + candidate;
                for (const auto* file : entries) if (key == file->path) { selected = file; break; }
                if (selected) break;
            }
            if (!selected)
                throw std::invalid_argument("Native UE4SS mod has no supported listed module candidate: " + name);
            native_specs.push_back({name, logical, activation_root / selected->path, selected->size});
        }
    }
    return RC::Platform::NativeModSource::Prepare(std::move(files), std::move(native_specs));
}
}
