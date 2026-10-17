#include <Unreal/UnrealVersion.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Helpers/Casting.hpp>

#define NOMINMAX
#include <Windows.h>

#include <fmt/core.h>
#include <fmt/xchar.h>

namespace RC::Unreal
{
    auto Version::Initialize(void* Address) -> VersionStatus
    {
        uint16_t MajorVersion = Helper::Casting::ptr_cast_deref<uint16_t>(Address);
        uint16_t MinorVersion = Helper::Casting::ptr_cast_deref<uint16_t>(Address, 0x2);
        uint16_t PatchVersion = Helper::Casting::ptr_cast_deref<uint16_t>(Address, 0x4);

        if (MajorVersion < 4 || MajorVersion > 5)
        {
            return {VersionStatus::FAILURE, fmt::format(STR("Could not determine Unreal Engine version: major version was too big or too small ({})"), MajorVersion)};
        }

        if (MinorVersion > 30)
        {
            return {VersionStatus::FAILURE, fmt::format(STR("Could not determine Unreal Engine version: minor version was too big ({})"), MinorVersion)};
        }

        if (PatchVersion > 10)
        {
            return {VersionStatus::FAILURE, fmt::format(STR("Could not determine Unreal Engine version: patch version was too big ({})"), PatchVersion)};
        }

        const File::CharType* Branch = Helper::Casting::ptr_cast_deref<const File::CharType*>(Address, 0x10);
        void* BranchTest = Helper::Casting::ptr_cast_deref_safe<void*>(Branch, 0x0, GetCurrentProcess());
        if (!BranchTest)
        {
            return {VersionStatus::FAILURE, fmt::format(STR("Could not determine Unreal Engine version: 'Branch' member variable FString data was nullptr"))};
        }

        auto BranchView = File::StringViewType{Branch};
        if (!BranchView.starts_with(STR("++")) && // Occurs in most games
            !BranchView.starts_with(STR("main")) && // Occurs in Dead Rock Galactic
            !BranchView.starts_with(STR("UE4"))) // Occurs in the 4.12 demo game
        {
            return {VersionStatus::FAILURE, fmt::format(STR("Could not determine Unreal Engine version: 'Branch' member variable FString does not start with '++'"))};
        }

        Major = static_cast<int32_t>(MajorVersion);
        Minor = static_cast<int32_t>(MinorVersion);

#if UNREAL_VERSION_RUN_TESTS == 1
        RunTests();
#endif

        return {VersionStatus::SUCCESS, STR("")};
    }

}
