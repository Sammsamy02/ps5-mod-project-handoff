#include <Unreal/VersionedContainer/Container.hpp>
#ifdef __PROSPERO__
#include <Unreal/VersionedContainer/UnrealVirtualImpl/NativeProvidedVirtualVC.hpp>
#endif
#include <Unreal/UnrealVersion.hpp>
#include <Unreal/VersionedContainer/UObject412.hpp>
#include <Unreal/VersionedContainer/UObject415.hpp>
#include <Unreal/VersionedContainer/UObject422.hpp>
#include <Unreal/VersionedContainer/UObject425.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual407.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual408.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual409.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual410.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual411.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual412.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual413.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual414.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual415.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual416.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual417.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual418.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual419.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual420.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual421.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual422.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual423.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual424.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual425.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual426.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual427.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual500.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual501.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual502.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual503.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual504.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual505.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual506.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual507.hpp>
#include <Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtual508.hpp>
#if PLATFORM_WINDOWS
#include <Unreal/Core/Windows/MinimalWindowsApi.hpp>
#endif

namespace RC::Unreal
{
    std::unique_ptr<UObjectBaseVC> Container::UnrealObjectVC;
    std::unique_ptr<UnrealVirtualBaseVC> Container::UnrealVirtualVC;

    auto Container::SetDerivedBaseObjects() -> void
    {
        if (Version::IsEqual(4, 7))
        {
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual407>();
#endif
        }
        else if (Version::IsEqual(4, 8))
        {
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual408>();
#endif
        }
        else if (Version::IsEqual(4, 9))
        {
          UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
          UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
          UnrealVirtualVC = std::make_unique<UnrealVirtual409>();
#endif
        }
        else if (Version::IsEqual(4, 10))
        {
          UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
          UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
          UnrealVirtualVC = std::make_unique<UnrealVirtual410>();
#endif
        }
        else if (Version::IsEqual(4, 11))
        {
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual411>();
#endif
        }
        else if (Version::IsEqual(4, 12))
        {
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual412>();
#endif
        }
        else if (Version::IsEqual(4, 13))
        {
            // FUObjectItem changed in this version
            UnrealObjectVC = std::make_unique<UObject415>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual413>();
#endif
        }
        else if (Version::IsEqual(4, 14))
        {
            UnrealObjectVC = std::make_unique<UObject415>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual414>();
#endif
        }
        else if (Version::IsEqual(4, 15))
        {
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual415>();
#endif
        }
        else if (Version::IsEqual(4, 16))
        {
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual416>();
#endif
        }
        else if (Version::IsEqual(4, 17))
        {
            // TLockFreePointerListUnordered was changed and is now a pointer surrounded by padding
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual417>();
#endif
        }
        else if (Version::IsEqual(4, 18))
        {
            // Using 4.12 since it seems to have the correct GUObjectArray definition
            UnrealObjectVC = std::make_unique<UObject412>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual418>();
#endif
        }
        else if (Version::IsEqual(4, 19))
        {
            UnrealObjectVC = std::make_unique<UObject422>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual419>();
#endif
        }
        else if (Version::IsEqual(4, 20))
        {
            // FFixedUObjectArray was replaced with FChunkedFixedUObjectArray
            UnrealObjectVC = std::make_unique<UObject422>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual420>();
#endif
        }
        else if (Version::IsEqual(4, 21))
        {
            UnrealObjectVC = std::make_unique<UObject422>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual421>();
#endif
        }
        else if (Version::IsEqual(4, 22))
        {
            // PLATFORM_CACHE_LINE_SIZE changed from 128 to 64
            UnrealObjectVC = std::make_unique<UObject422>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual422>();
#endif
        }
        else if (Version::IsEqual(4, 23))
        {
            UnrealObjectVC = std::make_unique<UObject422>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual423>();
#endif
        }
        else if (Version::IsEqual(4, 24))
        {
            UnrealObjectVC = std::make_unique<UObject422>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual424>();
#endif
        }
        else if (Version::IsEqual(4, 25))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual425>();
#endif
        }
        else if (Version::IsEqual(4, 26))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual426>();
#endif
        }
        else if (Version::IsEqual(4, 27))
        {
            // Type of FUObjectArray.ObjAvailableList was changed to TArray<int32>
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual427>();
#endif
        }
        else if (Version::IsEqual(5, 0))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual500>();
#endif
        }
        else if (Version::IsEqual(5, 1))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual501>();
#endif
        }
        else if (Version::IsEqual(5, 2))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual502>();
#endif
        }
        else if (Version::IsEqual(5, 3))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual503>();
#endif
        }
        else if (Version::IsEqual(5, 4))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual504>();
#endif
        }
        else if (Version::IsEqual(5, 5))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual505>();
#endif
        }
        else if (Version::IsEqual(5, 6))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual506>();
#endif
        }
        else if (Version::IsEqual(5, 7))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual507>();
#endif
        }
        else if (Version::IsEqual(5, 8))
        {
            UnrealObjectVC = std::make_unique<UObject425>();
#ifdef __PROSPERO__
            UnrealVirtualVC = std::make_unique<NativeProvidedVirtualVC>();
#else
            UnrealVirtualVC = std::make_unique<UnrealVirtual508>();
#endif
        }
        else
        {
            // If this code is ever reached then the version isn't explicitly supported
            // Using 4.25 as a long-shot, sometimes this will work but in the future it's likely that this will break
            // Update: It will break in 4.27+ because GUObjectArray is different
            throw std::runtime_error{"Engine version is not supported."};
        }
    }
}