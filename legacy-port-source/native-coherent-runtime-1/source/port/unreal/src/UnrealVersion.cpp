#include <Unreal/UnrealVersion.hpp>

namespace RC::Unreal
{
    int32_t Version::Major{-1};
    int32_t Version::Minor{-1};
    bool Version::DebugBuild{false};
    bool Version::NonShipping{false};

    VersionStatus::VersionStatus(StatusCode StatusParam, StringType ErrorMessageParam) : Status(StatusParam), ErrorMessage(std::move(ErrorMessageParam)) {}


#if UNREAL_VERSION_RUN_TESTS == 1
    auto Version::RunTests() -> void
    {
        int32_t StashedMajor = Major;
        int32_t StashedMinor = Minor;

        Major = 4;
        Minor = 25;

        if (!IsEqual(4, 25))
        {
            throw std::runtime_error{"[UnrealVersion] Failed test #1 - !is_equal(4, 25) || Test ran with major = 4 and minor = 25"};
        }

        if (!IsAtLeast(4, 24))
        {
            throw std::runtime_error{"[UnrealVersion] Failed test #2 - !is_atleast(4, 24) || Test ran with major = 4 and minor = 25"};
        }

        if (IsAtMost(4, 24))
        {
            throw std::runtime_error{"[UnrealVersion] Failed test #3 - is_atmost(4, 24) || Test ran with major = 4 and minor = 25"};
        }

        if (IsBelow(4, 24))
        {
            throw std::runtime_error{"[UnrealVersion] Failed test #4 - is_below(4, 24) || Test ran with major = 4 and minor = 25"};
        }

        if (!IsAbove(4, 24))
        {
            throw std::runtime_error{"[UnrealVersion] Failed test #5 - !is_above(4, 24) || Test ran with major = 4 and minor = 25"};
        }

        Major = StashedMajor;
        Minor = StashedMinor;
    }
#endif
}
