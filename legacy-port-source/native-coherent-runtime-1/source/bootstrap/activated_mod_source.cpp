#include "activated_mod_source.hpp"

#include <Platform/NativeModSource.hpp>
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
MakeActivatedUE4SSModSource(ActivatedMods activation, std::string_view expected_title)
{
    if (!activation || expected_title.empty() ||
        expected_title != std::string_view(mfa_title(activation.get())))
        throw std::invalid_argument("UE4SS activation must match the supplied game title");
    if (std::string_view(mfa_engine(activation.get())) != "unreal" ||
        !mfa_wants_framework(activation.get(), "ue4ss-ps5"))
        throw std::invalid_argument("Activation does not request the native UE4SS framework");

    std::shared_ptr<mfa_activation> owner(std::move(activation));
    std::vector<RC::Platform::NativeModFile> files;
    for (size_t i = 0; i < mfa_file_count(owner.get()); ++i)
    {
        const auto* file = mfa_file_at(owner.get(), i);
        const std::string_view path(file->path);
        if (!path.starts_with("ue4ss/")) continue;
        const std::string_view role(file->role);
        // Native binary mods need their own real PS5 runtime path. Silently
        // omitting them would misrepresent an incomplete activation as ready.
        if (role != "lua_mod" && role != "config" && role != "script")
            throw std::invalid_argument("Unsupported UE4SS activation file role: " + std::string(path));
        if (!file->data || file->size > std::numeric_limits<size_t>::max())
            throw std::invalid_argument("UE4SS file was not preloaded: " + std::string(path));
        files.push_back({std::string(path), {owner,
            std::string_view(reinterpret_cast<const char*>(file->data), static_cast<size_t>(file->size))}});
    }
    return std::make_shared<const RC::Platform::NativeModSource>(std::move(files));
}
}
